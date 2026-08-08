//
//  detail/memory/tracker.hpp
//  Allocation Tracker / Leak Detector / Peak Usage / free checkers
//
//  Why there is a pointer set as well as the in-block header
//  --------------------------------------------------------
//  The header alone cannot answer "did I allocate this?". Reading
//  payload - offset for a pointer we did not allocate means reading memory in
//  front of somebody else's allocation, which is exactly the kind of thing this
//  file exists to catch.
//
//  It matters because tracking is not on for the whole program. Allocations
//  made before the first measured scope have no header; frees arriving during a
//  scope may belong to them. Getting that wrong is not a false report, it is
//  heap corruption -- the system allocator would be handed the payload pointer
//  instead of the raw one.
//
//  So a set of live payload pointers decides which path a free takes. It is
//  filled with platform::rawAllocate, never with the allocator being tracked,
//  so it cannot recurse into itself -- the reason the design wanted to avoid a
//  side table in the first place.
//

#ifndef TESTRIXA_DETAIL_MEMORY_TRACKER_HPP
#define TESTRIXA_DETAIL_MEMORY_TRACKER_HPP

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <ostream>
#include <string>

#include <testrixa/detail/memory/allocator.hpp>
#include <testrixa/detail/memory/block.hpp>

TRX_BEGIN_NAMESPACE

namespace memory {

// ---------------------------------------------------------------------------
// Checker selection
// ---------------------------------------------------------------------------
enum Check : std::uint32_t {
    CheckTrack       = 1u << 0,   // required by everything below
    CheckLeak        = 1u << 1,
    CheckDoubleFree  = 1u << 2,
    CheckRedzone     = 1u << 3,
    CheckPeak        = 1u << 4,
    // Fills a freed block with a pattern and holds it in quarantine instead of
    // returning it. Two things follow: the address cannot be handed out again
    // while it is held, so a double free is unambiguous rather than a guess;
    // and if the pattern is disturbed before the block is finally released,
    // somebody wrote through a dangling pointer.
    CheckPatternFill = 1u << 5,
    // Records where each allocation came from. By far the most expensive
    // checker -- more than all the others together -- so it is never on by
    // default. The workflow is: a run reports a leak, you turn this on and run
    // again to find out where it came from.
    CheckBacktrace   = 1u << 6
    // Invalid Free is deliberately absent. Telling "never allocated by anyone"
    // apart from "allocated before tracking started" needs whole-program
    // coverage, which arrives with malloc interposition. A flag that quietly
    // detects nothing is worse than no flag.
};

constexpr std::uint32_t PresetFast     = CheckTrack | CheckLeak | CheckPeak;
constexpr std::uint32_t PresetDefault  = CheckTrack | CheckLeak | CheckDoubleFree
                                       | CheckRedzone | CheckPeak;
constexpr std::uint32_t PresetParanoid = PresetDefault | CheckPatternFill;

struct Config {
    std::uint32_t checks  = PresetDefault;
    std::size_t   redzone = 32;

    // How much freed memory to hold back before really releasing it. Zero
    // disables quarantine: double free then depends on the address not being
    // reused, and use-after-free is not detected at all.
    std::size_t   quarantineBytes = 8u << 20;   // 8 MB

    std::uint8_t  fillFreed = 0xDD;             // "dead"

    int           backtraceFrames = 0;          // 0 disables capture
    int           backtraceSkip   = 3;          // drop the allocator's own frames
};

// ---------------------------------------------------------------------------
// What a measured scope reports
// ---------------------------------------------------------------------------
struct Report {
    std::size_t allocations   = 0;
    std::size_t frees         = 0;
    std::size_t leakedBlocks  = 0;
    std::size_t leakedBytes   = 0;
    std::size_t peakBytes     = 0;
    std::size_t largestBlock  = 0;
    std::size_t doubleFrees   = 0;
    std::size_t overflows     = 0;
    std::size_t useAfterFrees = 0;

    bool clean() const {
        return leakedBlocks == 0 && doubleFrees == 0
            && overflows == 0 && useAfterFrees == 0;
    }
};

namespace detail {

// ---------------------------------------------------------------------------
// LivePointerSet -- open addressing over raw memory
// ---------------------------------------------------------------------------
class LivePointerSet {
public:
    void insert(void* pointer) {
        if ((m_count + 1) * 4 >= m_capacity * 3) grow();
        if (place(m_slots, m_capacity, pointer)) m_count++;
    }

    bool contains(void* pointer) const {
        if (!m_slots) return false;
        std::size_t index = hash(pointer) & (m_capacity - 1);
        for (std::size_t probe = 0; probe < m_capacity; ++probe) {
            void* slot = m_slots[index];
            if (slot == nullptr) return false;
            if (slot == pointer) return true;
            index = (index + 1) & (m_capacity - 1);
        }
        return false;
    }

    // Tombstone-free removal: rehash the run that follows the hole.
    void erase(void* pointer) {
        if (!m_slots) return;
        std::size_t index = hash(pointer) & (m_capacity - 1);
        for (std::size_t probe = 0; probe < m_capacity; ++probe) {
            void* slot = m_slots[index];
            if (slot == nullptr) return;
            if (slot == pointer) break;
            index = (index + 1) & (m_capacity - 1);
        }
        if (m_slots[index] != pointer) return;

        m_slots[index] = nullptr;
        m_count--;

        std::size_t scan = (index + 1) & (m_capacity - 1);
        while (m_slots[scan]) {
            void* moved = m_slots[scan];
            m_slots[scan] = nullptr;
            m_count--;
            if (place(m_slots, m_capacity, moved)) m_count++;
            scan = (scan + 1) & (m_capacity - 1);
        }
    }

    void release() {
        if (m_slots) platform::rawFree(m_slots);
        m_slots = nullptr;
        m_capacity = 0;
        m_count = 0;
    }

    std::size_t size() const { return m_count; }

private:
    static std::size_t hash(void* pointer) {
        std::uint64_t value = (std::uint64_t)(std::uintptr_t)pointer;
        value ^= value >> 33;
        value *= 0xff51afd7ed558ccdull;
        value ^= value >> 33;
        return (std::size_t)value;
    }

    static bool place(void** slots, std::size_t capacity, void* pointer) {
        std::size_t index = hash(pointer) & (capacity - 1);
        for (std::size_t probe = 0; probe < capacity; ++probe) {
            if (slots[index] == nullptr) { slots[index] = pointer; return true; }
            if (slots[index] == pointer) return false;
            index = (index + 1) & (capacity - 1);
        }
        return false;
    }

    void grow() {
        const std::size_t capacity = m_capacity ? m_capacity * 2 : 1024;
        void** slots = (void**)platform::rawAllocate(capacity * sizeof(void*));
        if (!slots) return;                       // out of memory: stop growing
        for (std::size_t i = 0; i < capacity; ++i) slots[i] = nullptr;

        for (std::size_t i = 0; i < m_capacity; ++i) {
            if (m_slots[i]) place(slots, capacity, m_slots[i]);
        }
        if (m_slots) platform::rawFree(m_slots);
        m_slots    = slots;
        m_capacity = capacity;
    }

    void**      m_slots    = nullptr;
    std::size_t m_capacity = 0;
    std::size_t m_count    = 0;
};

} // namespace detail

// ---------------------------------------------------------------------------
// Tracker
// ---------------------------------------------------------------------------
class Tracker {
public:
    // Immortal on purpose.
    //
    // The hook can still be called by static destructors at exit -- anything
    // holding a tracked block frees it then. A function-local static would
    // already be gone by that point, and locking a destroyed std::mutex ends
    // the process with "mutex lock failed: Invalid argument". Leaking the
    // tracker is the cheapest way to outlive every user of it.
    static Tracker& instance() {
        static Tracker* tracker = construct();
        return *tracker;
    }

    // Installed for as long as any tracked block is still alive, which is not
    // the same as "for as long as we are recording": a leak outlives its scope
    // and its free must still find the header.
    void beginRecording(const Config& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_recording == 0) {
            m_config = config;               // frozen while blocks are live
            if (!m_installed) {
                m_previous  = install(Allocator{ &Tracker::allocateHook, &Tracker::deallocateHook });
                m_installed = true;
            }
        }
        m_recording++;
    }

    void endRecording() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_recording > 0) m_recording--;
        uninstallIfIdleLocked();
    }

    bool recording() const { return m_recording > 0; }

    std::uint64_t sequence() const { return m_sequence; }

    // Everything a Report needs, sampled under the lock.
    void sample(std::uint64_t sinceSequence, Report& report) {
        std::lock_guard<std::mutex> lock(m_mutex);
        report.leakedBlocks = 0;
        report.leakedBytes  = 0;
        for (BlockHeader* header = m_head; header; header = header->next) {
            if (header->sequence >= sinceSequence) {
                report.leakedBlocks++;
                report.leakedBytes += header->size;
            }
        }
    }

    // realloc has to know whether a pointer is one of ours, and how big it was.
    // Assumes default alignment, which is what the malloc shim always uses --
    // realloc on an over-aligned `new` allocation is undefined anyway.
    bool trackedSize(void* payload, std::size_t& sizeOut) {
        if (!payload) return false;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_installed || !m_live.contains(payload)) return false;
        BlockHeader* header = headerOf(payload, platform::defaultAlignment(), m_config.redzone);
        if (header->magic != kLiveMagic) return false;
        sizeOut = header->size;
        return true;
    }

    static bool isAllocatorFrame(const std::string& symbol) {
        static const char* internals[] = {
            "trxOperatorNew", "testrixa6memory", "testrixa::memory",
            "_Znwm", "_Znam", "_ZnwmSt11align_val_t", "_ZnamSt11align_val_t",
            "operator new",
        };
        for (const char* needle : internals) {
            if (symbol.find(needle) != std::string::npos) return true;
        }
        return false;
    }

    // Everything still live, which after a phase means everything that leaked.
    // Prints origins when they were captured, and says how to get them when
    // they were not -- a leak report with no location is only half an answer.
    void reportLeaks(std::ostream& out, std::size_t maximum) {
        Bypass guard;                       // symbol lookup allocates
        std::lock_guard<std::mutex> lock(m_mutex);

        std::size_t shown = 0, total = 0;
        const std::uint64_t now = platform::nowNanos();

        for (BlockHeader* header = m_head; header; header = header->next) {
            total++;
            if (shown >= maximum) continue;
            shown++;

            out << "  leak #" << header->sequence
                << "  " << header->size << " bytes"
                << "  age=" << (now - header->allocatedAt) / 1000000 << "ms";
            if (header->frames == 0) out << "  (origin not recorded)";
            out << std::endl;

            // Drop the leading allocator frames by symbol rather than by a
            // fixed count. A fixed skip cannot be right at every optimisation
            // level: too small and the reader wades through operator new, too
            // large and it eats the frame they were looking for. Symbols say
            // which is which.
            int first = 0;
            while (first < header->frames && isAllocatorFrame(
                       platform::describeFrame(header->backtrace[first]))) {
                first++;
            }
            if (first == header->frames) first = 0;   // all filtered: show it raw

            for (int i = first; i < header->frames; ++i) {
                out << "      " << platform::describeFrame(header->backtrace[i]) << std::endl;
            }
        }

        if (total > shown) {
            out << "  ... and " << (total - shown) << " more" << std::endl;
        }
        if (total > 0 && !(m_config.checks & CheckBacktrace)) {
            out << "  rerun with --mem.backtrace=16 to record where these came from"
                << std::endl;
        } else if (total > 0 && !platform::backtraceSupported()) {
            // Capture was asked for and could not be delivered. Saying only
            // "origin not recorded" would read as a mistake the reader made,
            // when in fact this build has nothing to capture with (musl, for
            // one, ships no <execinfo.h>).
            out << "  this platform cannot capture backtraces, so origins are "
                   "unavailable however --mem.backtrace is set"
                << std::endl;
        }
    }

    std::size_t liveBlocks()   const { return m_liveBlocks; }
    std::size_t allocations()  const { return m_allocations; }
    std::size_t frees()        const { return m_frees; }
    std::size_t doubleFrees()  const { return m_doubleFrees; }
    std::size_t overflows()    const { return m_overflows; }
    std::size_t useAfterFrees() const { return m_useAfterFrees; }

    // Releases everything quarantine is holding, checking each block's pattern
    // on the way out. A scope has to do this before it reports: a write through
    // a dangling pointer is only visible when the block is finally released,
    // and a report taken first would say the scope was clean.
    void flushQuarantine() {
        std::lock_guard<std::mutex> lock(m_mutex);
        quarantineDrainLocked(0);
    }
    std::size_t largestBlock() const { return m_largestBlock; }

    std::size_t liveBytes()    const { return m_liveBytes; }
    std::size_t peakBytes()    const { return m_peakBytes; }
    void        resetPeak()          { m_peakBytes = m_liveBytes; }
    // Same treatment as the peak: a scope reports its own largest block,
    // not the largest the process has ever seen.
    void        resetLargest()       { m_largestBlock = 0; }

private:
    Tracker() = default;

    static Tracker* construct() {
        void* storage = platform::rawAllocate(sizeof(Tracker));
        return new (storage) Tracker();     // never destroyed, see instance()
    }

    // A freed block goes here instead of back to the system. Its payload is
    // painted first, so a write through a dangling pointer shows up when the
    // block is finally released.
    //
    // The queue reuses the header's `next` link: the block is out of the live
    // list by now, so the pointer is free.
    void quarantinePushLocked(BlockHeader* header) {
        header->next = nullptr;
        if (m_quarantineTail) m_quarantineTail->next = header;
        else                  m_quarantineHead = header;
        m_quarantineTail = header;
        m_quarantineBytes += header->size;
    }

    // True if the freed pattern is still intact.
    bool freedPatternIntact(const BlockHeader* header) const {
        const std::uint8_t* payload = (const std::uint8_t*)header
                                    + payloadOffset(header->alignment, header->redzone);
        for (std::size_t i = 0; i < header->size; ++i) {
            if (payload[i] != m_config.fillFreed) return false;
        }
        return true;
    }

    void quarantineDrainLocked(std::size_t downTo) {
        while (m_quarantineHead && m_quarantineBytes > downTo) {
            BlockHeader* header = m_quarantineHead;
            m_quarantineHead = header->next;
            if (!m_quarantineHead) m_quarantineTail = nullptr;
            m_quarantineBytes -= header->size;

            if ((m_config.checks & CheckPatternFill) && !freedPatternIntact(header)) {
                m_useAfterFrees++;
            }

            const std::size_t alignment = header->alignment;
            if (header->backtrace) platform::rawFree(header->backtrace);
            header->magic = 0;
            platform::rawFreeAligned(header, alignment);
        }
    }

    // Recording has stopped and the last tracked block just came back, so the
    // hook has nothing left to do. endRecording() cannot do this on its own:
    // a block that outlives its scope is freed later, and that free is the
    // moment the allocator becomes removable again.
    void uninstallIfIdleLocked() {
        if (m_recording == 0 && m_installed && m_liveBlocks == 0) {
            // Everything held back has to go now, or quarantine turns into a
            // leak of its own.
            quarantineDrainLocked(0);
            install(m_previous);
            m_installed = false;
            m_live.release();
            m_freedRecently.release();
        }
    }

    static void* allocateHook(std::size_t bytes, std::size_t alignment) {
        return instance().doAllocate(bytes, alignment);
    }

    static void deallocateHook(void* pointer, std::size_t bytes, std::size_t alignment) {
        instance().doDeallocate(pointer, bytes, alignment);
    }

    void* doAllocate(std::size_t bytes, std::size_t alignment) {
        // Nothing below may be seen by the hook again.
        Bypass guard;

        if (alignment < platform::defaultAlignment()) alignment = platform::defaultAlignment();

        const std::size_t redzone = (m_config.checks & CheckRedzone) ? m_config.redzone : 0;
        const std::size_t total   = totalBlockSize(bytes, alignment, redzone);

        void* raw = platform::rawAllocateAligned(total, alignment);
        if (!raw) return nullptr;

        BlockHeader* header = (BlockHeader*)raw;
        header->magic       = kLiveMagic;
        header->size        = bytes;
        header->alignment   = alignment;
        header->redzone     = redzone;
        header->allocatedAt = platform::nowNanos();
        header->threadId    = platform::currentThreadId();
        header->reserved    = 0;
        header->previous    = nullptr;
        header->next        = nullptr;
        header->backtrace   = nullptr;
        header->frames      = 0;

        if ((m_config.checks & CheckBacktrace) && m_config.backtraceFrames > 0) {
            // Not `bytes` -- that is this function's parameter, and holding
            // two different sizes under one name is worth renaming even where
            // the compiler stays quiet. MSVC /W4 does not (C4457).
            const std::size_t frameBytes =
                (std::size_t)m_config.backtraceFrames * sizeof(void*);
            header->backtrace = (void**)platform::rawAllocate(frameBytes);
            if (header->backtrace) {
                header->frames = platform::captureBacktrace(
                    header->backtrace, m_config.backtraceFrames, m_config.backtraceSkip);
            }
        }

        paintRedzones(header);

        void* payload = payloadOf(raw, alignment, redzone);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            header->sequence = m_sequence++;

            header->next = m_head;
            if (m_head) m_head->previous = header;
            m_head = header;

            m_live.insert(payload);
            // The address may have been freed earlier and is now live again.
            // Leaving it in the freed set would make its next free look like a
            // double free -- and the block would not be released.
            m_freedRecently.erase(payload);
            m_liveBlocks++;
            m_allocations++;
            m_liveBytes += bytes;
            if (m_liveBytes > m_peakBytes)   m_peakBytes = m_liveBytes;
            if (bytes > m_largestBlock)      m_largestBlock = bytes;
        }

        return payload;
    }

    void doDeallocate(void* payload, std::size_t, std::size_t alignment) {
        Bypass guard;

        if (!payload) return;
        if (alignment < platform::defaultAlignment()) alignment = platform::defaultAlignment();

        bool ours = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ours = m_live.contains(payload);
        }

        if (!ours) {
            // Either allocated before tracking started, or never allocated at
            // all. Only the second is a fault, and only a double free of one of
            // ours can be told apart from it -- see below.
            if (m_config.checks & CheckDoubleFree) {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_freedRecently.contains(payload)) {
                    m_doubleFrees++;
                    return;                       // do not free it a second time
                }
            }
            platform::rawFreeAligned(payload, alignment);
            return;
        }

        bool freedLast = false;
        BlockHeader* header = headerOf(payload, alignment, m_config.redzone);

        if (header->magic != kLiveMagic) {
            // The set said it is ours but the header disagrees: the header was
            // overwritten. Report and leak it rather than unlink through
            // pointers that can no longer be trusted.
            std::lock_guard<std::mutex> lock(m_mutex);
            m_overflows++;
            m_live.erase(payload);
            return;
        }

        if (m_config.checks & CheckRedzone) {
            if (inspectRedzones(header) != Guard::Intact) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_overflows++;
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (header->previous) header->previous->next = header->next;
            else                  m_head = header->next;
            if (header->next)     header->next->previous = header->previous;

            m_live.erase(payload);
            if (m_config.checks & CheckDoubleFree) m_freedRecently.insert(payload);

            m_liveBlocks--;
            m_frees++;
            m_liveBytes -= header->size;

            freedLast = true;
        }

        header->magic = kFreedMagic;

        const bool quarantining = (m_config.checks & CheckPatternFill)
                                && m_config.quarantineBytes > 0;
        if (quarantining) {
            std::uint8_t* body = (std::uint8_t*)header
                               + payloadOffset(header->alignment, header->redzone);
            std::memset(body, m_config.fillFreed, header->size);

            std::lock_guard<std::mutex> lock(m_mutex);
            quarantinePushLocked(header);
            quarantineDrainLocked(m_config.quarantineBytes);
        } else {
            if (header->backtrace) platform::rawFree(header->backtrace);
            platform::rawFreeAligned(header, alignment);
        }

        if (freedLast) {
            std::lock_guard<std::mutex> lock(m_mutex);
            uninstallIfIdleLocked();
        }
    }

    mutable std::mutex     m_mutex;
    Config                 m_config;
    Allocator              m_previous{};
    bool                   m_installed  = false;
    unsigned               m_recording  = 0;

    BlockHeader*           m_head       = nullptr;
    detail::LivePointerSet m_live;
    detail::LivePointerSet m_freedRecently;

    std::uint64_t          m_sequence     = 1;
    std::size_t            m_liveBlocks   = 0;
    std::size_t            m_allocations  = 0;
    std::size_t            m_frees        = 0;
    std::size_t            m_doubleFrees  = 0;
    std::size_t            m_overflows    = 0;
    std::size_t            m_useAfterFrees = 0;
    BlockHeader*           m_quarantineHead = nullptr;
    BlockHeader*           m_quarantineTail = nullptr;
    std::size_t            m_quarantineBytes = 0;
    std::size_t            m_liveBytes    = 0;
    std::size_t            m_peakBytes    = 0;
    std::size_t            m_largestBlock = 0;
};

// ---------------------------------------------------------------------------
// Scope -- turns recording on for a region and reports what happened in it
// ---------------------------------------------------------------------------
class Scope {
public:
    explicit Scope(const Config& config = Config())
    : m_tracker(Tracker::instance()) {
        m_tracker.beginRecording(config);
        m_sequence     = m_tracker.sequence();
        m_allocations  = m_tracker.allocations();
        m_frees        = m_tracker.frees();
        m_doubleFrees  = m_tracker.doubleFrees();
        m_overflows    = m_tracker.overflows();
        m_useAfterFrees = m_tracker.useAfterFrees();
        m_tracker.resetPeak();
        m_tracker.resetLargest();
        m_peakBase     = m_tracker.peakBytes();
    }

    ~Scope() { m_tracker.endRecording(); }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    Report report() {
        m_tracker.flushQuarantine();

        Report result;
        result.allocations  = m_tracker.allocations()  - m_allocations;
        result.frees        = m_tracker.frees()        - m_frees;
        result.doubleFrees  = m_tracker.doubleFrees()  - m_doubleFrees;
        result.overflows    = m_tracker.overflows()    - m_overflows;
        result.useAfterFrees = m_tracker.useAfterFrees() - m_useAfterFrees;
        result.peakBytes    = m_tracker.peakBytes()    - m_peakBase;
        result.largestBlock = m_tracker.largestBlock();
        m_tracker.sample(m_sequence, result);
        return result;
    }

private:
    Tracker&      m_tracker;
    std::uint64_t m_sequence     = 0;
    std::size_t   m_allocations  = 0;
    std::size_t   m_frees        = 0;
    std::size_t   m_doubleFrees  = 0;
    std::size_t   m_overflows    = 0;
    std::size_t   m_useAfterFrees = 0;
    std::size_t   m_peakBase     = 0;
};

// Runs `body` with tracking on and hands back what happened.
//
// Do not assert inside `body`: the framework's own bookkeeping allocates, and
// while the scope is open those allocations are the code under test's.
template <typename Body>
inline Report measure(Body&& body, const Config& config = Config()) {
    Scope scope(config);
    body();
    return scope.report();
}

} // namespace memory

TRX_END_NAMESPACE

#endif // TESTRIXA_DETAIL_MEMORY_TRACKER_HPP

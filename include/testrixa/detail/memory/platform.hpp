//
//  detail/memory/platform.hpp
//  Platform Adapter
//
//  Everything the memory suite needs from the operating system, in one place.
//
//  The critical member is rawAllocate/rawFree. The tracker's own bookkeeping
//  must never travel through the allocator it is tracking, or the first
//  allocation recurses forever. These two go straight to the C allocator and
//  are never intercepted.
//

#ifndef TESTRIXA_DETAIL_MEMORY_PLATFORM_HPP
#define TESTRIXA_DETAIL_MEMORY_PLATFORM_HPP

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <functional>
#include <string>
#include <cstdio>

#if defined(_MSC_VER)
    // <windows.h> is a public-header include, so it has to be tamed before it
    // reaches a consumer:
    //
    //   NOMINMAX             it defines min/max as macros, which break
    //                        std::min/std::max and anything with a member of
    //                        that name -- in the consumer's code, not ours.
    //   WIN32_LEAN_AND_MEAN  drops Winsock, OLE, RPC and the rest. The only
    //                        thing wanted from here is
    //                        CaptureStackBackTrace.
    //
    // Both are defined only if the consumer has not already made their own
    // choice, and neither is left defined afterwards -- undefining a macro the
    // consumer set would be the same class of pollution.
#   ifndef NOMINMAX
#       define NOMINMAX
#       define TRX_DEFINED_NOMINMAX
#   endif
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#       define TRX_DEFINED_LEAN_AND_MEAN
#   endif
#   include <windows.h>
    // No <dbghelp.h>: nothing here symbolises a frame. describeFrame has no
    // MSVC branch, so a Windows backtrace is addresses and nothing else --
    // the same shape as a stripped binary elsewhere. Adding SymFromAddr is a
    // separate piece of work; carrying the header and its link for a call that
    // does not exist is not.
#   ifdef TRX_DEFINED_NOMINMAX
#       undef NOMINMAX
#       undef TRX_DEFINED_NOMINMAX
#   endif
#   ifdef TRX_DEFINED_LEAN_AND_MEAN
#       undef WIN32_LEAN_AND_MEAN
#       undef TRX_DEFINED_LEAN_AND_MEAN
#   endif
#elif defined(__has_include)
#   if __has_include(<execinfo.h>)
#       include <execinfo.h>
#       define TRX_HAS_EXECINFO 1
#   endif
#endif

#include <testrixa/traits.h>

TRX_BEGIN_NAMESPACE

namespace memory {
namespace platform {

// Raw memory, straight from the C allocator.
//
// Never routed through testrixa's allocator hook: this is what the tracker
// itself allocates from, and what the hook falls back to when tracking is off.
inline void* rawAllocate(std::size_t bytes) {
    return std::malloc(bytes);
}

// gcc pairs an allocation with its deallocation by the *declared* function it
// traces back to. Once it inlines through our replaced `operator new` -- which
// really does hand back malloc'd memory -- it sees `operator new` reaching
// `free` and reports -Wmismatched-new-delete. The pairing is correct here by
// construction: rawAllocate is malloc, rawFree is free, and nothing that did
// not come from the former is ever passed to the latter.
//
// Suppressed at the one function whose entire job is that pairing, rather than
// project-wide: a consumer's own mismatched new/delete must still be reported.
// Found on gcc 15 (Alpine); gcc 12 and 16 do not inline far enough to reach it,
// which is exactly why it had to be silenced rather than waited out -- whether
// it fires depends on the inliner, so any consumer with -Werror could hit it.
#if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
inline void rawFree(void* pointer) {
    std::free(pointer);
}
#if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic pop
#endif

// Over-aligned raw memory. Kept separate because the aligned C11/C++17 entry
// points are not available everywhere and the fallback has to remember the
// original pointer to free it.
inline void* rawAllocateAligned(std::size_t bytes, std::size_t alignment) {
    if (alignment <= alignof(std::max_align_t)) {
        return rawAllocate(bytes);
    }

#if defined(_MSC_VER)
    return _aligned_malloc(bytes, alignment);
#else
    void* result = nullptr;
    // posix_memalign wants a power-of-two multiple of sizeof(void*)
    if (alignment < sizeof(void*)) alignment = sizeof(void*);
    if (posix_memalign(&result, alignment, bytes) != 0) return nullptr;
    return result;
#endif
}

inline void rawFreeAligned(void* pointer, std::size_t alignment) {
    if (alignment <= alignof(std::max_align_t)) {
        rawFree(pointer);
        return;
    }

#if defined(_MSC_VER)
    _aligned_free(pointer);
#else
    rawFree(pointer);
#endif
}

inline std::uint64_t nowNanos() {
    return (std::uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline std::uint32_t currentThreadId() {
    return (std::uint32_t)std::hash<std::thread::id>()(std::this_thread::get_id());
}

// ---------------------------------------------------------------------------
// Backtraces
//
// Off unless asked for: capturing costs more than every other checker put
// together, which is why --mem.backtrace defaults to 0. When a leak does turn
// up, the second run with it on is what says where.
// ---------------------------------------------------------------------------
inline bool backtraceSupported() {
#if defined(TRX_HAS_EXECINFO) || defined(_MSC_VER)
    return true;
#else
    return false;
#endif
}

// Returns how many frames were written. `skip` drops the allocator's own
// frames, which are never what the reader is looking for.
inline int captureBacktrace(void** frames, int maximum, int skip) {
    if (maximum <= 0) return 0;

#if defined(TRX_HAS_EXECINFO)
    // Capture into a local buffer first so `skip` frames can be dropped
    // without the caller having to over-allocate.
    const int wanted = maximum + skip;
    void* raw[128];
    const int limit = wanted < 128 ? wanted : 128;
    const int got = ::backtrace(raw, limit);
    if (got <= skip) return 0;

    int kept = got - skip;
    if (kept > maximum) kept = maximum;
    std::memcpy(frames, raw + skip, (std::size_t)kept * sizeof(void*));
    return kept;
#elif defined(_MSC_VER)
    return (int)CaptureStackBackTrace((ULONG)skip, (ULONG)maximum, frames, nullptr);
#else
    (void)frames; (void)skip;
    return 0;
#endif
}

// One frame as text. Falls back to the raw address when no symbol is available
// -- a stripped binary should still say something rather than nothing.
inline std::string describeFrame(void* frame) {
#if defined(TRX_HAS_EXECINFO)
    // backtrace_symbols allocates with malloc. Callers run this under a Bypass
    // so the tracker does not see it.
    char** text = ::backtrace_symbols(&frame, 1);
    if (text) {
        std::string result(text[0] ? text[0] : "");
        std::free(text);
        if (!result.empty()) return result;
    }
#endif
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%p", frame);
    return std::string(buffer);
}

inline std::size_t defaultAlignment() {
    return alignof(std::max_align_t);
}

// Rounds up to a multiple of alignment. Used to keep the block header from
// disturbing the alignment of the pointer handed back to the caller.
inline std::size_t alignUp(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace platform
} // namespace memory

TRX_END_NAMESPACE

#endif // TESTRIXA_DETAIL_MEMORY_PLATFORM_HPP

//
//  testQuarantine.cpp
//  testrixa
//
//  Quarantine and pattern fill.
//
//  A freed block is painted and held instead of being returned to the system.
//  Two things follow, and both are tested here:
//
//    - the address cannot be handed out again while the block is held, so a
//      double free is unambiguous instead of depending on whether the
//      allocator happened to reuse the address;
//    - if the pattern is disturbed before the block is finally released,
//      somebody wrote through a dangling pointer.
//
//  The write-after-free cases deliberately write to freed memory. That memory
//  is testrixa's own quarantine, not the system allocator's, so the write lands
//  somewhere we own -- but the compiler does not know that, hence the barriers.
//

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <testrixa/memory.h>
#include <testrixa/testrixa.h>

namespace {

inline void escape(void* pointer) {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "g"(pointer) : "memory");
#else
    static volatile void* sink;
    sink = pointer;
#endif
}

// The out-of-bounds writes below are the behaviour under test: they land in
// testrixa's own guard bytes, which is memory we allocated. Marked so the
// sanitizers do not stop a run that is deliberately stepping over the line --
// the alternative is excluding these files from sanitizer builds entirely,
// which would also exclude the tracker code they exercise.
#if defined(__clang__) || defined(__GNUC__)
__attribute__((no_sanitize("undefined", "address")))
#endif
inline void writeOutOfBounds(char* base, std::ptrdiff_t offset, char value) {
    base[offset] = value;
}

testrixa::memory::Config paranoid() {
    testrixa::memory::Config config;
    config.checks = testrixa::memory::PresetParanoid;
    return config;
}

} // namespace

TESTCASE_BEGIN(testQuarantine)

TESTCASE_BASIC(testQuarantine) {
    using namespace testrixa::memory;

    // The next two groups read and write freed memory on purpose -- that is
    // the behaviour being detected. gcc is right to object; the memory belongs
    // to testrixa's quarantine rather than the system allocator, which the
    // compiler has no way to know. Silence only these diagnostics, only here.
#if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wuse-after-free"
#endif

    TESTCASE_GROUP_START(A_FREED_BLOCK_IS_PAINTED) {
        // Reading freed memory is exactly what this feature exists to catch,
        // so read it on purpose and confirm the paint went down.
        unsigned char first = 0;
        const Report report = measure([&]{
            unsigned char* block = new unsigned char[32];
            escape(block);
            std::memset(block, 'A', 32);
            delete[] block;
            escape(block);
            first = block[0];      // dangling on purpose: quarantine still owns it
            escape(&first);
        }, paranoid());

        Config config = paranoid();
        CHECK_SAME(first, config.fillFreed);
        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
    } TESTCASE_GROUP_END(A_FREED_BLOCK_IS_PAINTED)

    TESTCASE_GROUP_START(WRITING_AFTER_FREE_IS_CAUGHT) {
        const Report report = measure([]{
            unsigned char* block = new unsigned char[32];
            escape(block);
            delete[] block;
            escape(block);
            writeOutOfBounds((char*)block, 4, 0x11);   // disturbs the freed pattern
            escape(block);
        }, paranoid());

        // Detected when quarantine finally releases the block, which the scope
        // forces on the way out.
        CHECK_SAME(report.useAfterFrees, (std::size_t)1);
        CHECK(!report.clean());
    } TESTCASE_GROUP_END(WRITING_AFTER_FREE_IS_CAUGHT)

#if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic pop
#endif

    TESTCASE_GROUP_START(AN_UNTOUCHED_FREED_BLOCK_REPORTS_NOTHING) {
        const Report report = measure([]{
            unsigned char* block = new unsigned char[32];
            escape(block);
            delete[] block;
        }, paranoid());

        CHECK_SAME(report.useAfterFrees, (std::size_t)0);
        CHECK_SAME(report.leakedBlocks,  (std::size_t)0);
        CHECK(report.clean());
    } TESTCASE_GROUP_END(AN_UNTOUCHED_FREED_BLOCK_REPORTS_NOTHING)

    TESTCASE_GROUP_START(QUARANTINE_KEEPS_THE_ADDRESS_OUT_OF_CIRCULATION) {
        // With the block held, a fresh allocation of the same size cannot be
        // handed the address that was just freed. That is what makes a double
        // free unambiguous rather than a guess about allocator behaviour.
        void* freed = nullptr;
        void* reused = nullptr;
        const Report report = measure([&]{
            unsigned char* first = new unsigned char[32];
            freed = first;
            escape(first);
            delete[] first;

            unsigned char* second = new unsigned char[32];
            reused = second;
            escape(second);
            delete[] second;
        }, paranoid());

        CHECK(freed != reused);
        CHECK_SAME(report.allocations, (std::size_t)2);
    } TESTCASE_GROUP_END(QUARANTINE_KEEPS_THE_ADDRESS_OUT_OF_CIRCULATION)

    TESTCASE_GROUP_START(QUARANTINE_DRAINS_AT_ITS_LIMIT) {
        // A small limit forces eviction inside the scope. Nothing may leak:
        // held blocks are released, not forgotten.
        Config config = paranoid();
        config.quarantineBytes = 1024;

        const Report report = measure([]{
            for (int i = 0; i < 64; ++i) {
                unsigned char* block = new unsigned char[256];
                escape(block);
                delete[] block;
            }
        }, config);

        CHECK_SAME(report.allocations,   (std::size_t)64);
        CHECK_SAME(report.frees,         (std::size_t)64);
        CHECK_SAME(report.leakedBlocks,  (std::size_t)0);
        CHECK_SAME(report.useAfterFrees, (std::size_t)0);
    } TESTCASE_GROUP_END(QUARANTINE_DRAINS_AT_ITS_LIMIT)

    TESTCASE_GROUP_START(QUARANTINE_CAN_BE_TURNED_OFF) {
        Config config = paranoid();
        config.quarantineBytes = 0;

        const Report report = measure([]{
            unsigned char* block = new unsigned char[32];
            escape(block);
            delete[] block;
        }, config);

        CHECK_SAME(report.leakedBlocks,  (std::size_t)0);
        CHECK_SAME(report.useAfterFrees, (std::size_t)0);
    } TESTCASE_GROUP_END(QUARANTINE_CAN_BE_TURNED_OFF)

    TESTCASE_GROUP_START(THE_DEFAULT_PRESET_DOES_NOT_PAINT) {
        // Pattern fill costs a memset per free, so it is paranoid-only. The
        // default preset must not pay for it.
        Config config;
        CHECK_SAME((config.checks & (std::uint32_t)CheckPatternFill), (std::uint32_t)0);

        Config strict = paranoid();
        CHECK((strict.checks & (std::uint32_t)CheckPatternFill) != 0);
    } TESTCASE_GROUP_END(THE_DEFAULT_PRESET_DOES_NOT_PAINT)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testQuarantine) {
    TESTCASE_RETURN
}

TESTCASE_END(testQuarantine)

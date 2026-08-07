//
//  testShim.cpp
//  testrixa
//
//  C allocation coverage through <testrixa/malloc_shim.h>.
//
//  Note the include order: system headers, then testrixa's own headers, and the
//  shim last. The shim redefines malloc and friends as macros, so anything
//  parsed after it would see the macro instead of the declaration.
//

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <testrixa/memory.h>
#include <testrixa/testrixa.h>

#include <testrixa/malloc_shim.h>

namespace {

inline void escape(void* pointer) {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "g"(pointer) : "memory");
#else
    static volatile void* sink;
    sink = pointer;
#endif
}

} // namespace

TESTCASE_BEGIN(testShim)

TESTCASE_BASIC(testShim) {
    using namespace testrixa::memory;

    TESTCASE_GROUP_START(MALLOC_AND_FREE_ARE_TRACKED) {
        const Report report = inspect([]{
            void* block = malloc(64);
            escape(block);
            free(block);
        });

        CHECK_SAME(report.allocations,  (std::size_t)1);
        CHECK_SAME(report.frees,        (std::size_t)1);
        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
    } TESTCASE_GROUP_END(MALLOC_AND_FREE_ARE_TRACKED)

    TESTCASE_GROUP_START(A_LEAKED_MALLOC_IS_CAUGHT) {
        void* leaked = nullptr;
        const Report report = inspect([&]{
            leaked = malloc(48);
            escape(leaked);
        });

        CHECK_SAME(report.leakedBlocks, (std::size_t)1);
        CHECK_SAME(report.leakedBytes,  (std::size_t)48);

        free(leaked);
    } TESTCASE_GROUP_END(A_LEAKED_MALLOC_IS_CAUGHT)

    TESTCASE_GROUP_START(CALLOC_ZEROES_AND_IS_TRACKED) {
        bool zeroed = true;
        const Report report = inspect([&]{
            unsigned char* block = (unsigned char*)calloc(16, 4);
            escape(block);
            for (int i = 0; i < 64; ++i) if (block[i] != 0) zeroed = false;
            free(block);
        });

        CHECK(zeroed);
        CHECK_SAME(report.allocations,  (std::size_t)1);
        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
    } TESTCASE_GROUP_END(CALLOC_ZEROES_AND_IS_TRACKED)

    TESTCASE_GROUP_START(CALLOC_REFUSES_AN_OVERFLOWING_SIZE) {
        // count * size wraps; returning a short buffer here is how heap
        // overflows get written on purpose.
        void* block = calloc((std::size_t)-1 / 2 + 1, 4);
        CHECK(block == nullptr);
    } TESTCASE_GROUP_END(CALLOC_REFUSES_AN_OVERFLOWING_SIZE)

    TESTCASE_GROUP_START(REALLOC_GROWS_AND_KEEPS_CONTENT) {
        bool preserved = true;
        const Report report = inspect([&]{
            char* block = (char*)malloc(8);
            escape(block);
            for (int i = 0; i < 8; ++i) block[i] = (char)('a' + i);

            char* grown = (char*)realloc(block, 64);
            escape(grown);
            for (int i = 0; i < 8; ++i) if (grown[i] != (char)('a' + i)) preserved = false;

            free(grown);
        });

        CHECK(preserved);
        // one for the original, one for the grown block
        CHECK_SAME(report.allocations,  (std::size_t)2);
        CHECK_SAME(report.frees,        (std::size_t)2);
        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
    } TESTCASE_GROUP_END(REALLOC_GROWS_AND_KEEPS_CONTENT)

    TESTCASE_GROUP_START(REALLOC_SHRINKS_WITHOUT_READING_PAST_THE_NEW_SIZE) {
        bool preserved = true;
        const Report report = inspect([&]{
            char* block = (char*)malloc(64);
            escape(block);
            std::memset(block, 'z', 64);

            char* shrunk = (char*)realloc(block, 8);
            escape(shrunk);
            for (int i = 0; i < 8; ++i) if (shrunk[i] != 'z') preserved = false;

            free(shrunk);
        });

        CHECK(preserved);
        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
    } TESTCASE_GROUP_END(REALLOC_SHRINKS_WITHOUT_READING_PAST_THE_NEW_SIZE)

    TESTCASE_GROUP_START(REALLOC_EDGE_CASES) {
        const Report report = inspect([]{
            // realloc(nullptr, n) is malloc(n)
            void* fresh = realloc(nullptr, 32);
            escape(fresh);
            // realloc(p, 0) releases and yields null
            void* gone = realloc(fresh, 0);
            escape(gone);
        });

        CHECK_SAME(report.allocations,  (std::size_t)1);
        CHECK_SAME(report.frees,        (std::size_t)1);
        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
    } TESTCASE_GROUP_END(REALLOC_EDGE_CASES)

    TESTCASE_GROUP_START(STRDUP_IS_TRACKED) {
        bool copied = false;
        const Report report = inspect([&]{
            char* copy = strdup("testrixa");
            escape(copy);
            copied = (std::strcmp(copy, "testrixa") == 0);
            free(copy);
        });

        CHECK(copied);
        CHECK_SAME(report.allocations,  (std::size_t)1);
        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
        // strlen("testrixa") + 1
        CHECK_SAME(report.largestBlock, (std::size_t)9);
    } TESTCASE_GROUP_END(STRDUP_IS_TRACKED)

    TESTCASE_GROUP_START(STRNDUP_STOPS_AT_THE_LIMIT) {
        bool truncated = false;
        const Report report = inspect([&]{
            char* copy = strndup("testrixa", 4);
            escape(copy);
            truncated = (std::strcmp(copy, "test") == 0);
            free(copy);
        });

        CHECK(truncated);
        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
    } TESTCASE_GROUP_END(STRNDUP_STOPS_AT_THE_LIMIT)

    TESTCASE_GROUP_START(FREEING_A_POINTER_WE_NEVER_SAW_IS_SAFE) {
        // Allocated before the scope opened, so the tracker has no record of
        // it. The free must reach the system allocator untouched rather than
        // be treated as one of ours.
        //
        // Not std::malloc: `malloc` is a macro here, so `std::malloc(32)`
        // expands to `std::::testrixa::...` and does not compile. That is the
        // ordering hazard the shim header warns about, met head on. The raw
        // entry point says what is meant anyway -- allocate past the tracker.
        void* outside = testrixa::memory::platform::rawAllocate(32);
        const Report report = inspect([&]{
            free(outside);
        });

        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
        CHECK_SAME(report.doubleFrees,  (std::size_t)0);
    } TESTCASE_GROUP_END(FREEING_A_POINTER_WE_NEVER_SAW_IS_SAFE)

    TESTCASE_GROUP_START(MALLOC_ZERO_RETURNS_A_USABLE_POINTER) {
        const Report report = inspect([]{
            void* block = malloc(0);
            escape(block);
            free(block);
        });

        CHECK_SAME(report.allocations, (std::size_t)1);
        CHECK_SAME(report.frees,       (std::size_t)1);
    } TESTCASE_GROUP_END(MALLOC_ZERO_RETURNS_A_USABLE_POINTER)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testShim) {
    TESTCASE_RETURN
}

TESTCASE_END(testShim)

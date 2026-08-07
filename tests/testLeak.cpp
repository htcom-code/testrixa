//
//  testLeak.cpp
//  testrixa
//
//  Allocation Tracker / Leak Detector / redzone / double free.
//
//  Assertions live outside the measured callable on purpose: the framework's
//  own bookkeeping allocates, and anything allocated while a scope is open
//  belongs to the code under test as far as the tracker is concerned.
//

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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

// Forces a value to be materialised here, so the optimiser cannot sink the
// computation that produced it past a later deallocation.
inline void escapeValue(std::uintptr_t value) {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "r"(value) : "memory");
#else
    static volatile std::uintptr_t sink;
    sink = value;
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

struct alignas(64) OverAligned {
    char payload[64];
};

} // namespace

TESTCASE_BEGIN(testLeak)

TESTCASE_BASIC(testLeak) {
    using namespace testrixa::memory;

    TESTCASE_GROUP_START(BALANCED_ALLOCATION_LEAKS_NOTHING) {
        const Report report = inspect([]{
            int* value = new int(1);
            escape(value);
            delete value;
        });

        CHECK_SAME(report.allocations,  (std::size_t)1);
        CHECK_SAME(report.frees,        (std::size_t)1);
        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
        CHECK(report.clean());
    } TESTCASE_GROUP_END(BALANCED_ALLOCATION_LEAKS_NOTHING)

    TESTCASE_GROUP_START(A_LEAK_IS_COUNTED_AND_SIZED) {
        int* leaked = nullptr;
        const Report report = inspect([&]{
            leaked = new int(2);
            escape(leaked);
        });

        CHECK_SAME(report.allocations,  (std::size_t)1);
        CHECK_SAME(report.frees,        (std::size_t)0);
        CHECK_SAME(report.leakedBlocks, (std::size_t)1);
        CHECK_SAME(report.leakedBytes,  sizeof(int));
        CHECK(!report.clean());

        // A leaked block outlives its scope, so the tracker has to stay
        // installed until it comes back. Freeing it here used to hand the
        // payload pointer to the system allocator -- heap corruption, not a
        // bad report. This is the regression guard for that.
        delete leaked;
    } TESTCASE_GROUP_END(A_LEAK_IS_COUNTED_AND_SIZED)

    TESTCASE_GROUP_START(MANY_LEAKS_ARE_ALL_COUNTED) {
        int* leaked[4] = { nullptr, nullptr, nullptr, nullptr };
        const Report report = inspect([&]{
            for (int i = 0; i < 4; ++i) {
                leaked[i] = new int(i);
                escape(leaked[i]);
            }
        });

        CHECK_SAME(report.leakedBlocks, (std::size_t)4);
        CHECK_SAME(report.leakedBytes,  sizeof(int) * 4);

        for (int i = 0; i < 4; ++i) delete leaked[i];
    } TESTCASE_GROUP_END(MANY_LEAKS_ARE_ALL_COUNTED)

    TESTCASE_GROUP_START(ARRAY_AND_ALIGNED_FORMS_ARE_TRACKED) {
        bool aligned = false;
        const Report report = inspect([&]{
            int* numbers = new int[8];
            escape(numbers);
            delete[] numbers;

            OverAligned* object = new OverAligned();
            // As an integer, not a pointer: the optimiser is free to sink this
            // past the delete, and reading a pointer value after deallocation
            // is undefined (gcc -Wuse-after-free says so).
            const std::uintptr_t address = (std::uintptr_t)object;
            escapeValue(address);
            escape(object);
            delete object;
            aligned = (address % 64) == 0;
        });

        CHECK_SAME(report.allocations,  (std::size_t)2);
        CHECK_SAME(report.frees,        (std::size_t)2);
        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
        // The block header must not disturb the alignment the caller asked for.
        CHECK(aligned);
    } TESTCASE_GROUP_END(ARRAY_AND_ALIGNED_FORMS_ARE_TRACKED)

    TESTCASE_GROUP_START(STL_ALLOCATIONS_ARE_TRACKED) {
        const Report report = inspect([]{
            std::vector<int> numbers;
            numbers.reserve(64);
            escape(numbers.data());
        });

        CHECK(report.allocations >= 1);
        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
    } TESTCASE_GROUP_END(STL_ALLOCATIONS_ARE_TRACKED)

    TESTCASE_GROUP_START(PEAK_TRACKS_THE_HIGH_WATER_MARK) {
        const Report report = inspect([]{
            char* first = new char[1024];
            escape(first);
            delete[] first;
            char* second = new char[512];
            escape(second);
            delete[] second;
        });

        // Sequential, not simultaneous: the peak is the larger block, not the sum.
        CHECK(report.peakBytes >= 1024);
        CHECK(report.peakBytes <  1536);
    } TESTCASE_GROUP_END(PEAK_TRACKS_THE_HIGH_WATER_MARK)

    // The next three groups do on purpose what the checkers are meant to catch:
    // write past the end, write before the start, free twice. The compiler is
    // right to complain -- the writes land in testrixa's own guard bytes, which
    // is memory we allocated, and the second free never reaches the system
    // allocator. Silence only these diagnostics, only here.
#if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Warray-bounds"
#   pragma GCC diagnostic ignored "-Wstringop-overflow"
#   pragma GCC diagnostic ignored "-Wuse-after-free"
#endif

    TESTCASE_GROUP_START(WRITING_PAST_THE_END_IS_CAUGHT) {
        const Report report = inspect([]{
            char* buffer = new char[8];
            escape(buffer);
            writeOutOfBounds(buffer, 8, 'x');   // one past the end: the rear guard
            escape(buffer);
            delete[] buffer;
        });

        CHECK_SAME(report.overflows, (std::size_t)1);
        CHECK(!report.clean());
    } TESTCASE_GROUP_END(WRITING_PAST_THE_END_IS_CAUGHT)

    TESTCASE_GROUP_START(WRITING_BEFORE_THE_START_IS_CAUGHT) {
        const Report report = inspect([]{
            char* buffer = new char[8];
            escape(buffer);
            writeOutOfBounds(buffer, -1, 'x');  // underflow: the front guard
            escape(buffer);
            delete[] buffer;
        });

        CHECK_SAME(report.overflows, (std::size_t)1);
    } TESTCASE_GROUP_END(WRITING_BEFORE_THE_START_IS_CAUGHT)

    TESTCASE_GROUP_START(A_CLEAN_BLOCK_REPORTS_NO_OVERFLOW) {
        const Report report = inspect([]{
            char* buffer = new char[8];
            escape(buffer);
            for (int i = 0; i < 8; ++i) buffer[i] = (char)i;   // exactly in bounds
            escape(buffer);
            delete[] buffer;
        });

        CHECK_SAME(report.overflows, (std::size_t)0);
    } TESTCASE_GROUP_END(A_CLEAN_BLOCK_REPORTS_NO_OVERFLOW)

    TESTCASE_GROUP_START(FREEING_TWICE_IS_CAUGHT) {
        const Report report = inspect([]{
            int* value = new int(5);
            escape(value);
            ::operator delete(value);
            ::operator delete(value);   // the tracker must not release it again
        });

        CHECK_SAME(report.doubleFrees, (std::size_t)1);
        CHECK(!report.clean());
    } TESTCASE_GROUP_END(FREEING_TWICE_IS_CAUGHT)

#if defined(__GNUC__) && !defined(__clang__)
#   pragma GCC diagnostic pop
#endif

    TESTCASE_GROUP_START(REDZONE_CAN_BE_TURNED_OFF) {
        Config config;
        config.redzone = 0;
        config.checks &= ~(std::uint32_t)CheckRedzone;

        const Report report = measure([]{
            char* buffer = new char[8];
            escape(buffer);
            delete[] buffer;
        }, config);

        CHECK_SAME(report.allocations,  (std::size_t)1);
        CHECK_SAME(report.leakedBlocks, (std::size_t)0);
        CHECK_SAME(report.overflows,    (std::size_t)0);
    } TESTCASE_GROUP_END(REDZONE_CAN_BE_TURNED_OFF)

    TESTCASE_GROUP_START(TRACKING_IS_OFF_OUTSIDE_A_SCOPE) {
        const bool before = Tracker::instance().recording();
        {
            Scope scope;
            CHECK(Tracker::instance().recording());
        }
        CHECK_SAME(Tracker::instance().recording(), before);
    } TESTCASE_GROUP_END(TRACKING_IS_OFF_OUTSIDE_A_SCOPE)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testLeak) {
    TESTCASE_RETURN
}

TESTCASE_END(testLeak)


TESTCASE_BEGIN(testLeakMacros)

TESTCASE_BASIC(testLeakMacros) {
    TESTCASE_RETURN
}

// Runs only when the memory phase is asked for (--mem / --only=memory).
TESTCASE_MEMORY(testLeakMacros) {
    TESTCASE_GROUP_START(CHECK_MACROS) {
        CHECK_NO_LEAK([]{
            int* values = new int[10];
            escape(values);
            delete[] values;
        });

        CHECK_ALLOC_COUNT([]{
            int* value = new int(1);
            escape(value);
            delete value;
        }, 1);

        CHECK_PEAK_UNDER([]{
            char* buffer = new char[256];
            escape(buffer);
            delete[] buffer;
        }, 4096);

        CHECK_NO_OVERFLOW([]{
            char* buffer = new char[16];
            escape(buffer);
            delete[] buffer;
        });

        CHECK_NO_DOUBLE_FREE([]{
            int* value = new int(1);
            escape(value);
            delete value;
        });

        CHECK_MEMORY_CLEAN([]{
            std::string text(128, 'x');
            escape(&text[0]);
        });
    } TESTCASE_GROUP_END(CHECK_MACROS)

    TESTCASE_GROUP_START(A_DELIBERATE_LEAK_IS_ASSERTABLE) {
        // Reclaimed right after, so the suite itself stays clean.
        static int* leaked = nullptr;
        CHECK_LEAK_COUNT([]{ leaked = new int(9); escape(leaked); }, 1);
        delete leaked;
    } TESTCASE_GROUP_END(A_DELIBERATE_LEAK_IS_ASSERTABLE)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testLeakMacros) {
    TESTCASE_RETURN
}

TESTCASE_END(testLeakMacros)

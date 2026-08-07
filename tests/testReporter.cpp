//
//  testReporter.cpp
//  testrixa
//
//  The memory phase reports memory, not pass/fail counts.
//
//  Until this existed the MEMORY table borrowed the basic one and showed
//  Total/Success/Fail -- which says whether the assertions held and nothing
//  about what the code under test actually allocated. The numbers here are the
//  point of running the phase at all, so they get pinned.
//
//  These run in the memory phase (--mem / --only=memory) so the table this file
//  is about is the one it renders.
//

#include <cstddef>
#include <cstdint>

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

} // namespace

TESTCASE_BEGIN(testReporter)

TESTCASE_BASIC(testReporter) {
    TESTCASE_RETURN
}

TESTCASE_MEASURE(testReporter) {
    TESTCASE_RETURN
}

TESTCASE_MEMORY(testReporter) {
    using namespace testrixa::memory;

    TESTCASE_GROUP_START(A_CHECK_FEEDS_THE_GROUP_TALLY) {
        // Every memory CHECK routes its report into the current group, so the
        // table shows the work even when the assertion is uninteresting.
        CHECK_NO_LEAK([]{
            char* block = new char[512];
            escape(block);
            delete[] block;
        });
        CHECK_NO_LEAK([]{
            char* block = new char[128];
            escape(block);
            delete[] block;
        });
        // The group's row should now read Alloc=2 Free=2 Leak=0, MaxBlock 512B.
    } TESTCASE_GROUP_END(A_CHECK_FEEDS_THE_GROUP_TALLY)

    TESTCASE_GROUP_START(BYTES_ARE_PRINTED_FOR_PEOPLE) {
        // 4096 has to read as 4.0K, not as 4096. Six digits of bytes in a
        // column is not something anyone reads.
        CHECK_SAME(humanBytes(0),      std::string("0B"));
        CHECK_SAME(humanBytes(512),    std::string("512B"));
        CHECK_SAME(humanBytes(1024),   std::string("1.0K"));
        CHECK_SAME(humanBytes(4096),   std::string("4.0K"));
        CHECK_SAME(humanBytes(1536),   std::string("1.5K"));
        CHECK_SAME(humanBytes(1048576), std::string("1.0M"));
    } TESTCASE_GROUP_END(BYTES_ARE_PRINTED_FOR_PEOPLE)

    TESTCASE_GROUP_START(THE_TALLY_MERGES_RATHER_THAN_SUMS_PEAKS) {
        // Two sequential 1KB allocations peak at 1KB, not 2KB. Summing peaks
        // would report memory that never existed at one time.
        testrixa::MemoryTally first;
        first.allocations = 1; first.peakBytes = 1024; first.largestBlock = 1024;

        testrixa::MemoryTally second;
        second.allocations = 1; second.peakBytes = 1024; second.largestBlock = 512;

        first.merge(second);

        CHECK_SAME(first.allocations,  (std::size_t)2);   // counts add
        CHECK_SAME(first.peakBytes,    (std::size_t)1024); // peak is a maximum
        CHECK_SAME(first.largestBlock, (std::size_t)1024); // so is the largest block
    } TESTCASE_GROUP_END(THE_TALLY_MERGES_RATHER_THAN_SUMS_PEAKS)

    TESTCASE_GROUP_START(FAULTS_ARE_COLLAPSED_INTO_ONE_NUMBER) {
        // double free, overflow and use-after-free share a column: they are
        // usually zero, and any of them being non-zero means the same thing --
        // go look.
        testrixa::MemoryTally tally;
        tally.faults = 0;

        testrixa::MemoryTally faulted;
        faulted.faults = 3;
        tally.merge(faulted);

        CHECK_SAME(tally.faults, (std::size_t)3);
    } TESTCASE_GROUP_END(FAULTS_ARE_COLLAPSED_INTO_ONE_NUMBER)

    TESTCASE_RETURN
}

TESTCASE_END(testReporter)

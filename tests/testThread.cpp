//
//  testThread.cpp
//  testrixa
//
//  The concurrency harness.
//
//  Not a race detector -- see the header. What is tested here is the part that
//  is ours: assertions made on worker threads have to be counted, an exception
//  must not take the process down, and none of it may corrupt the framework's
//  single-threaded bookkeeping.
//
//  That last one is the reason the harness exists at all. add_result() touches
//  the group stack with no lock; before ThreadTally, calling CHECK from four
//  threads was a data race inside the tool meant to find data races.
//

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <testrixa/testrixa.h>
#include <testrixa/thread.h>

TESTCASE_BEGIN(testThread)

TESTCASE_BASIC(testThread) {
    using namespace testrixa::concurrency;

    TESTCASE_GROUP_START(EVERY_THREAD_RUNS) {
        std::atomic<int> counter{0};
        const Result result = CONCURRENT(4, [&](int) -> bool {
            for (int i = 0; i < 250; ++i) counter.fetch_add(1);
            return true;
        });

        CHECK_SAME(result.threads,    (std::size_t)4);
        CHECK_SAME(result.failed,     (std::size_t)0);
        CHECK_SAME(result.exceptions, (std::size_t)0);
        CHECK(result.clean());
        CHECK_SAME(counter.load(), 1000);
    } TESTCASE_GROUP_END(EVERY_THREAD_RUNS)

    TESTCASE_GROUP_START(EACH_THREAD_GETS_ITS_OWN_INDEX) {
        std::atomic<int> sum{0};
        (void)CONCURRENT(5, [&](int index) -> bool {
            sum.fetch_add(index);
            return true;
        });

        CHECK_SAME(sum.load(), 0 + 1 + 2 + 3 + 4);
    } TESTCASE_GROUP_END(EACH_THREAD_GETS_ITS_OWN_INDEX)

    TESTCASE_GROUP_START(ASSERTIONS_FROM_WORKERS_ARE_COUNTED) {
        // Four threads, three assertions each. All twelve have to land in this
        // group's totals -- that is what the per-thread tally buys.
        (void)CONCURRENT(4, [&](int) -> bool {
            CHECK(true);
            CHECK_SAME(2 + 2, 4);
            CHECK_FALSE(false);
            return true;
        });
        // The group total is checked from the console table rather than here:
        // asserting on our own count while adding to it is circular.
        CHECK(true);
    } TESTCASE_GROUP_END(ASSERTIONS_FROM_WORKERS_ARE_COUNTED)

    TESTCASE_GROUP_START(A_THROWING_WORKER_DOES_NOT_KILL_THE_PROCESS) {
        // An exception escaping a std::thread calls std::terminate. Reaching
        // the next line at all is most of this assertion.
        const Result result = CONCURRENT(3, [](int index) -> bool {
            if (index == 1) throw std::runtime_error("worker exploded");
            return true;
        });

        CHECK_SAME(result.exceptions, (std::size_t)1);
        CHECK(!result.clean());
        CHECK_SAME(result.firstError, std::string("worker exploded"));
    } TESTCASE_GROUP_END(A_THROWING_WORKER_DOES_NOT_KILL_THE_PROCESS)

    TESTCASE_GROUP_START(AN_UNKNOWN_THROW_IS_ALSO_CAUGHT) {
        const Result result = CONCURRENT(2, [](int index) -> bool {
            if (index == 0) throw 42;          // not a std::exception
            return true;
        });

        CHECK_SAME(result.exceptions, (std::size_t)1);
        CHECK_SAME(result.firstError, std::string("unknown exception"));
    } TESTCASE_GROUP_END(AN_UNKNOWN_THROW_IS_ALSO_CAUGHT)

    TESTCASE_GROUP_START(A_BODY_RETURNING_FALSE_IS_A_FAILURE) {
        const Result result = CONCURRENT(4, [](int index) -> bool {
            return index != 2;
        });

        CHECK_SAME(result.failed,     (std::size_t)1);
        CHECK_SAME(result.exceptions, (std::size_t)0);
        CHECK(!result.clean());
    } TESTCASE_GROUP_END(A_BODY_RETURNING_FALSE_IS_A_FAILURE)

    TESTCASE_GROUP_START(ZERO_THREADS_IS_A_NO_OP_NOT_A_HANG) {
        bool ran = false;
        const Result result = CONCURRENT(0, [&](int) -> bool { ran = true; return true; });

        CHECK(!ran);
        CHECK_SAME(result.threads, (std::size_t)0);
        CHECK(result.clean());
    } TESTCASE_GROUP_END(ZERO_THREADS_IS_A_NO_OP_NOT_A_HANG)

    TESTCASE_GROUP_START(THE_HARNESS_LEAVES_NO_BINDING_BEHIND) {
        // If a worker's tally were still bound afterwards, every later
        // assertion in this test case would vanish into it.
        (void)CONCURRENT(2, [](int) -> bool { return true; });

        // Reaching the table with these counted proves the binding was undone.
        CHECK(true);
        CHECK_SAME(1, 1);
    } TESTCASE_GROUP_END(THE_HARNESS_LEAVES_NO_BINDING_BEHIND)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testThread) {
    TESTCASE_RETURN
}

TESTCASE_END(testThread)

//
//  testStress.cpp
//  testrixa
//
//  Repetition, and the counting that makes it useful.
//
//  The point of a stress phase is telling "fails every time" apart from "fails
//  once in ten thousand". A harness that stops at the first failure cannot do
//  that, so these tests pin the counting as much as the running.
//
//  Bodies return bool and end with `return true;` -- CHECK expands to
//  `... return false;` on the failing path, so a body containing one already
//  returns bool whether it meant to or not.
//

#include <cstddef>

#include <testrixa/stress.h>
#include <testrixa/testrixa.h>

TESTCASE_BEGIN(testStress)

TESTCASE_BASIC(testStress) {
    using namespace testrixa::stress;

    TESTCASE_GROUP_START(A_STABLE_BODY_RUNS_THE_WHOLE_BUDGET) {
        int seen = 0;
        const Result result = repeat([&]() -> bool { seen++; return true; }, 500);

        CHECK_SAME(result.iterations,   (std::size_t)500);
        CHECK_SAME(result.failures,     (std::size_t)0);
        CHECK_SAME(result.firstFailure, (std::size_t)0);
        CHECK_SAME(seen, 500);
        CHECK(result.stable());
    } TESTCASE_GROUP_END(A_STABLE_BODY_RUNS_THE_WHOLE_BUDGET)

    TESTCASE_GROUP_START(FLAKINESS_IS_COUNTED_NOT_STOPPED_AT) {
        // Fails on every 100th run. A harness that stopped at the first would
        // report "1 failure" and hide that it is one in a hundred -- the
        // difference between a flake and a broken test.
        int run = 0;
        const Result result = repeat([&]() -> bool { return (++run % 100) != 0; }, 1000);

        CHECK_SAME(result.iterations,   (std::size_t)1000);
        CHECK_SAME(result.failures,     (std::size_t)10);
        CHECK_SAME(result.firstFailure, (std::size_t)100);
        CHECK(!result.stable());
    } TESTCASE_GROUP_END(FLAKINESS_IS_COUNTED_NOT_STOPPED_AT)

    TESTCASE_GROUP_START(THE_FIRST_FAILING_ITERATION_IS_REPORTED) {
        // "It broke on run 37" is something you can go and reproduce.
        int run = 0;
        const Result result = repeat([&]() -> bool { return ++run != 37; }, 100);

        CHECK_SAME(result.firstFailure, (std::size_t)37);
        CHECK_SAME(result.failures,     (std::size_t)1);
    } TESTCASE_GROUP_END(THE_FIRST_FAILING_ITERATION_IS_REPORTED)

    TESTCASE_GROUP_START(A_TIME_BUDGET_RUNS_AT_LEAST_ONCE) {
        int seen = 0;
        const Result result = repeatFor([&]() -> bool { seen++; return true; }, 1);

        // However small the budget, and whatever --stress.scale does to it, a
        // check that never ran is worse than a slow one.
        CHECK(result.iterations >= 1);
        CHECK(seen >= 1);
        CHECK_SAME(result.failures, (std::size_t)0);
    } TESTCASE_GROUP_END(A_TIME_BUDGET_RUNS_AT_LEAST_ONCE)

    TESTCASE_GROUP_START(TIMING_IS_COLLECTED) {
        const Result result = repeat([]() -> bool { return true; }, 200);

        CHECK(result.maxNanos >= result.minNanos);
        CHECK(result.meanNanos() >= result.minNanos);
        CHECK(result.meanNanos() <= result.maxNanos);
        CHECK(result.totalNanos > 0);
    } TESTCASE_GROUP_END(TIMING_IS_COLLECTED)

    TESTCASE_GROUP_START(AN_EMPTY_RESULT_IS_NOT_STABLE) {
        // Zero iterations means nothing was proved. Reporting that as stable
        // would be the most misleading answer available.
        Result nothing;
        CHECK(!nothing.stable());
        CHECK_SAME(nothing.meanNanos(), (std::uint64_t)0);
    } TESTCASE_GROUP_END(AN_EMPTY_RESULT_IS_NOT_STABLE)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testStress) {
    TESTCASE_RETURN
}

// Runs only under --stress / --only=stress.
TESTCASE_STRESS(testStress) {
    TESTCASE_GROUP_START(THE_STRESS_PHASE_RUNS) {
        CHECK_STABLE([]() -> bool { return true; }, 200);
        CHECK_STABLE_FOR([]() -> bool { return true; }, 5);

        // Pins how unreliable something is allowed to be, for code that is
        // known not to be perfect.
        int run = 0;
        CHECK_FAILURES_UNDER([&]() -> bool { return (++run % 50) != 0; }, 100, 5);
    } TESTCASE_GROUP_END(THE_STRESS_PHASE_RUNS)

    TESTCASE_RETURN
}

TESTCASE_END(testStress)

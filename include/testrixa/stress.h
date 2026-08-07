//
//  stress.h
//  Stress Test -- repetition
//
//  "Stress" could mean three different things and they are three different
//  components, so this one is pinned to the first:
//
//    repetition   run the same thing until it breaks or the budget runs out
//    load         many threads at once            -> <testrixa/thread.h>
//    resource     allocation pressure, low memory -> install a failing
//                 allocator through <testrixa/memory.h>
//
//  Repetition is the one nothing else covers and the one that hurts most
//  often: the test that passes locally and fails in CI every twentieth run.
//
//      TESTCASE_STRESS(testX) {
//          TESTCASE_GROUP_START(hammer) {
//              CHECK_STABLE([]{ return parse("1,2,3") == 3; }, 10000);
//              CHECK_STABLE_FOR([]{ return tick(); }, 250);   // milliseconds
//          } TESTCASE_GROUP_END(hammer)
//          TESTCASE_RETURN
//      }
//
//  The body returns bool and takes no arguments. It must not contain
//  assertions: a CHECK inside a body that runs ten thousand times floods the
//  report with the same line, and the count is the answer anyway.
//
//  Opt-in, like the memory phase: --only=stress or --stress. Repetition is
//  slow by construction and a plain run must not pay for it.
//

#ifndef TESTRIXA_STRESS_H
#define TESTRIXA_STRESS_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <testrixa/options.h>
#include <testrixa/testrixa.h>

TRX_BEGIN_NAMESPACE

namespace stress {

// ---------------------------------------------------------------------------
// Command line surface: --stress.*
// ---------------------------------------------------------------------------
inline options::OptionGroup optionGroup("stress", "Stress Test");

inline options::IntOption optionScale(
    optionGroup, "scale", 100, 1, 10000,
    "percent of each declared budget to actually run (10 = a tenth)");

inline options::IntOption optionStopAfter(
    optionGroup, "stop-after", 0, 0, 1000000,
    "give up after this many failures (0 = run the whole budget)");

struct Result {
    std::size_t   iterations   = 0;
    std::size_t   failures     = 0;
    std::size_t   firstFailure = 0;   // 1-based; 0 when nothing failed
    std::uint64_t minNanos     = 0;
    std::uint64_t maxNanos     = 0;
    std::uint64_t totalNanos   = 0;

    bool stable() const { return failures == 0 && iterations > 0; }

    std::uint64_t meanNanos() const {
        return iterations ? totalNanos / iterations : 0;
    }
};

namespace detail {

inline std::size_t scaled(std::size_t budget) {
    const std::size_t percent = (std::size_t)optionScale.value();
    const std::size_t value = budget * percent / 100;
    return value ? value : 1;          // never scale a budget away entirely
}

template <typename Body>
inline void oneRun(Body& body, Result& result) {
    const auto start = std::chrono::steady_clock::now();
    const bool ok = body();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    const std::uint64_t nanos = (std::uint64_t)
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();

    result.iterations++;
    result.totalNanos += nanos;
    if (result.iterations == 1 || nanos < result.minNanos) result.minNanos = nanos;
    if (nanos > result.maxNanos) result.maxNanos = nanos;

    if (!ok) {
        result.failures++;
        // The first failing iteration is the reproducible one -- "it broke on
        // run 6421" is something you can go and look at.
        if (result.firstFailure == 0) result.firstFailure = result.iterations;
    }
}

inline bool giveUp(const Result& result) {
    const std::size_t limit = (std::size_t)optionStopAfter.value();
    return limit > 0 && result.failures >= limit;
}

} // namespace detail

// Runs `body` a fixed number of times.
//
// Does not stop at the first failure. "Failed once in ten thousand" is what
// flakiness looks like, and a run that stops at the first one cannot tell it
// apart from "fails every time".
template <typename Body>
inline Result repeat(Body body, std::size_t iterations) {
    Result result;
    const std::size_t budget = detail::scaled(iterations);
    for (std::size_t i = 0; i < budget; ++i) {
        detail::oneRun(body, result);
        if (detail::giveUp(result)) break;
    }
    return result;
}

// Runs `body` until the time budget is spent. At least once, however small the
// budget: a scale factor must not turn a check into a no-op.
template <typename Body>
inline Result repeatFor(Body body, std::size_t milliseconds) {
    Result result;
    const std::size_t budget = detail::scaled(milliseconds);
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(budget);
    do {
        detail::oneRun(body, result);
        if (detail::giveUp(result)) break;
    } while (std::chrono::steady_clock::now() < deadline);
    return result;
}

} // namespace stress

TRX_END_NAMESPACE

// ---------------------------------------------------------------------------
// Checks
// ---------------------------------------------------------------------------
#define TRX_STRESS_REPEAT(body, iterations) \
    (::testrixa::stress::repeat(body, (std::size_t)(iterations)))

#define TRX_STRESS_REPEAT_FOR(body, milliseconds) \
    (::testrixa::stress::repeatFor(body, (std::size_t)(milliseconds)))

#define TRX_CHECK_STABLE(body, iterations, ...) \
    TRX_CHECK_SAME(TRX_STRESS_REPEAT(body, iterations).failures, (std::size_t)0, ##__VA_ARGS__)

#define TRX_CHECK_STABLE_FOR(body, milliseconds, ...) \
    TRX_CHECK_SAME(TRX_STRESS_REPEAT_FOR(body, milliseconds).failures, (std::size_t)0, ##__VA_ARGS__)

// For code that is expected to be unreliable: pins how unreliable.
#define TRX_CHECK_FAILURES_UNDER(body, iterations, limit, ...) \
    TRX_CHECK(TRX_STRESS_REPEAT(body, iterations).failures <= (std::size_t)(limit), ##__VA_ARGS__)

#ifndef TESTRIXA_NO_SHORT_MACROS

#define STRESS_REPEAT(body, iterations)              TRX_STRESS_REPEAT(body, iterations)
#define STRESS_REPEAT_FOR(body, milliseconds)        TRX_STRESS_REPEAT_FOR(body, milliseconds)
#define CHECK_STABLE(body, iterations, ...)          TRX_CHECK_STABLE(body, iterations, ##__VA_ARGS__)
#define CHECK_STABLE_FOR(body, milliseconds, ...)    TRX_CHECK_STABLE_FOR(body, milliseconds, ##__VA_ARGS__)
#define CHECK_FAILURES_UNDER(body, iterations, limit, ...) \
    TRX_CHECK_FAILURES_UNDER(body, iterations, limit, ##__VA_ARGS__)

#endif // TESTRIXA_NO_SHORT_MACROS

#endif // TESTRIXA_STRESS_H

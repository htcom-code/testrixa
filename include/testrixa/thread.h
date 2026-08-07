//
//  thread.h
//  Thread Test -- a concurrency harness
//
//  Read this first: **this is not a race detector.**
//
//  Detecting a data race properly means tracking happens-before edges across
//  every memory access, which is what ThreadSanitizer already does and does
//  well. Writing a second one here would be a worse copy of it. Build with
//  -fsanitize=thread and run your concurrent tests under that; this header
//  gives you the concurrent tests to run.
//
//  What it does provide is the part TSan does not: a way to run something on
//  several threads, get every thread's assertions counted, and not have the
//  process disappear when one of them throws.
//
//      TESTCASE_BASIC(testQueue) {
//          TESTCASE_GROUP_START(concurrent_push) {
//              Queue q;
//              CHECK_CONCURRENT(4, [&](int) -> bool {
//                  for (int i = 0; i < 1000; ++i) q.push(i);
//                  CHECK(q.size() > 0);
//                  return true;
//              });
//              CHECK_SAME(q.size(), (std::size_t)4000);
//          } TESTCASE_GROUP_END(concurrent_push)
//          TESTCASE_RETURN
//      }
//
//  **The body returns bool and must end with `return true;`.** That is not
//  style: CHECK expands to `... return false;` on the failing path, so a body
//  containing one already returns bool whether it meant to or not, and falling
//  off the end of it is undefined. Spelling the return type out makes the
//  compiler enforce what the macro already assumes. The same rule applies to
//  <testrixa/stress.h>.
//
//  Assertions inside the body work. Each thread accumulates its own counts and
//  they are merged after the join, so the framework's single-threaded
//  bookkeeping is never touched concurrently -- see ThreadTally in
//  detail/core.hpp for why locking it instead was the wrong answer.
//
//  An exception escaping a std::thread calls std::terminate: the process dies
//  and takes the report with it. Here it is caught, counted as a failure, and
//  its message reaches the report.
//

#ifndef TESTRIXA_THREAD_H
#define TESTRIXA_THREAD_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <testrixa/options.h>
#include <testrixa/testrixa.h>

TRX_BEGIN_NAMESPACE

namespace concurrency {

// ---------------------------------------------------------------------------
// Command line surface: --thread.*
// ---------------------------------------------------------------------------
inline options::OptionGroup optionGroup("thread", "Thread Test");

inline options::IntOption optionJoinTimeout(
    optionGroup, "join-timeout", 10000, 0, 600000,
    "ms to wait for a worker before reporting a hang (0 = wait forever)");

struct Result {
    std::size_t threads    = 0;
    std::size_t failed     = 0;   // bodies that returned false
    std::size_t exceptions = 0;
    std::string firstError;

    bool clean() const { return failed == 0 && exceptions == 0; }
};

// ---------------------------------------------------------------------------
// A latch, so every worker starts at roughly the same moment.
//
// Without one the first thread is usually finished before the last one starts,
// and a test that "runs on eight threads" never actually overlaps.
// ---------------------------------------------------------------------------
class StartGate {
public:
    explicit StartGate(std::size_t expected) : m_remaining(expected) {}

    void arriveAndWait() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (--m_remaining == 0) {
            m_open = true;
            m_ready.notify_all();
        } else {
            m_ready.wait(lock, [this] { return m_open; });
        }
    }

private:
    std::mutex              m_mutex;
    std::condition_variable m_ready;
    std::size_t             m_remaining;
    bool                    m_open = false;
};

// ---------------------------------------------------------------------------
// Runs `body(index)` on `threads` threads and merges what they assert.
//
// `testCase` is the running test case -- `this` at the call site. Templated so
// this header does not have to see TEST's definition.
// ---------------------------------------------------------------------------
template <typename TestCase, typename Body>
inline Result run(TestCase* testCase, std::size_t threads, Body body) {
    Result result;
    result.threads = threads;
    if (threads == 0) return result;

    typedef typename TestCase::ThreadTallyType Tally;

    std::vector<Tally>       tallies(threads);
    std::vector<std::string> errors(threads);
    std::vector<char>        ok(threads, 1);
    std::vector<std::thread> workers;
    workers.reserve(threads);

    StartGate gate(threads);

    for (std::size_t i = 0; i < threads; ++i) {
        workers.emplace_back([&, i] {
            // Point this thread's assertions at its own tally before anything
            // in the body can call CHECK.
            testCase->bindThreadTally(&tallies[i]);
            gate.arriveAndWait();
            try {
                ok[i] = body((int)i) ? 1 : 0;
            } catch (const std::exception& e) {
                errors[i] = e.what();
            } catch (...) {
                errors[i] = "unknown exception";
            }
            testCase->bindThreadTally(nullptr);
        });
    }

    for (std::thread& worker : workers) worker.join();

    for (std::size_t i = 0; i < threads; ++i) {
        testCase->mergeThreadTally(tallies[i]);
        if (!ok[i]) result.failed++;
        if (!errors[i].empty()) {
            result.exceptions++;
            if (result.firstError.empty()) result.firstError = errors[i];
        }
    }

    return result;
}

} // namespace concurrency

TRX_END_NAMESPACE

// ---------------------------------------------------------------------------
// Checks
// ---------------------------------------------------------------------------
#define TRX_CONCURRENT(threads, body) \
    (::testrixa::concurrency::run(this, (std::size_t)(threads), body))

// Passes when every worker returned true and none threw. Assertions inside the
// body are counted on their own and reported separately, exactly as if they had
// run on this thread.
#define TRX_CHECK_CONCURRENT(threads, body, ...) \
    TRX_CHECK(TRX_CONCURRENT(threads, body).clean(), ##__VA_ARGS__)

#ifndef TESTRIXA_NO_SHORT_MACROS

#define CONCURRENT(threads, body)                  TRX_CONCURRENT(threads, body)
#define CHECK_CONCURRENT(threads, body, ...)       TRX_CHECK_CONCURRENT(threads, body, ##__VA_ARGS__)

#endif // TESTRIXA_NO_SHORT_MACROS

#endif // TESTRIXA_THREAD_H

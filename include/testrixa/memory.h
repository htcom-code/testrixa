//
//  memory.h
//  Memory Suite -- public API
//
//      #include <testrixa/memory.h>
//
//      TESTCASE_BASIC(testAlloc) {
//          TESTCASE_GROUP_START(lifecycle) {
//              CHECK_NO_LEAK([]{ int* p = new int[10]; delete[] p; });
//              CHECK_LEAK_COUNT([]{ (void)new int; }, 1);
//              CHECK_BALANCED([]{ std::string s(64, 'x'); });
//              CHECK_PEAK_UNDER([]{ std::vector<int> v(16); }, 4096);
//          } TESTCASE_GROUP_END(lifecycle)
//          TESTCASE_RETURN
//      }
//
//  Do not put assertions inside the measured callable. The framework's own
//  bookkeeping allocates, and while the scope is open those allocations count
//  as the code under test's.
//
//  Coverage, in full:
//    - C++ new/delete           complete, including array, nothrow and
//                               over-aligned forms
//    - C malloc/free            only in translation units that include
//                               <testrixa/malloc_shim.h>
//    - prebuilt .a / .so        not seen at all
//
//  "no leaks" therefore means "no leaks within what is tracked", and the report
//  prints that coverage line every run rather than letting the reader assume
//  otherwise.
//
//  Pattern fill and quarantine (CHECK_NO_USE_AFTER_FREE) are paranoid-only:
//  they cost a memset per free and hold freed memory back, so the default
//  preset does not pay for them.
//
//  --mem.backtrace=N records where each allocation came from and is off even
//  under paranoid: it costs more than every other checker together. The
//  workflow is a run that reports a leak, then a second run with it on. Build
//  with -O0 or -O1 and -g for that second run -- at -O2 the frames the reader
//  wants have usually been inlined away, and no allocator can recover them.
//

#ifndef TESTRIXA_MEMORY_H
#define TESTRIXA_MEMORY_H

#include <cstddef>

#include <testrixa/options.h>
#include <testrixa/detail/memory/tracker.hpp>

TRX_BEGIN_NAMESPACE

namespace memory {

// ---------------------------------------------------------------------------
// Command line surface: --mem.*
//
// inline variables, so the group exists once in the program no matter how many
// translation units include this header. They are declared in dependency order
// -- the group before the options that attach to it.
// ---------------------------------------------------------------------------
inline options::OptionGroup optionGroup("mem", "Memory Suite");

inline options::StringOption optionPreset(
    optionGroup, "preset", "default", "fast|default|paranoid",
    "which checkers to run");

inline options::IntOption optionRedzone(
    optionGroup, "redzone", 32, 0, 4096,
    "guard bytes on each side of an allocation (0=off)");

inline options::IntOption optionBacktrace(
    optionGroup, "backtrace", 0, 0, 64,
    "frames to record per allocation (0=off, costs more than everything else)");

inline options::IntOption optionQuarantine(
    optionGroup, "quarantine", 8, 0, 1024,
    "MB of freed memory to hold back (0=off, weakens double-free)");

// Builds the configuration the command line asks for.
inline Config configuration() {
    Config config;

    const std::string& preset = optionPreset.value();
    if      (preset == "fast")     config.checks = PresetFast;
    else if (preset == "paranoid") config.checks = PresetParanoid;
    else                           config.checks = PresetDefault;

    config.redzone = (std::size_t)optionRedzone.value();
    if (config.redzone == 0) config.checks &= ~(std::uint32_t)CheckRedzone;

    config.quarantineBytes = (std::size_t)optionQuarantine.value() * 1024u * 1024u;

    config.backtraceFrames = (int)optionBacktrace.value();
    if (config.backtraceFrames > 0) config.checks |= (std::uint32_t)CheckBacktrace;
    else                            config.checks &= ~(std::uint32_t)CheckBacktrace;

    return config;
}

// measure() with the command line's configuration applied.
template <typename Body>
inline Report inspect(Body&& body) {
    return measure(body, configuration());
}

// Hands the report to the running test case so the memory phase can show what
// the code under test did, then returns it for the assertion to look at.
//
// Templated on the test case type rather than taking TEST*: this header is
// included by consumers who may not have pulled in the runner yet, and the
// template is only instantiated where the type is complete anyway.
template <typename TestCase>
inline Report record(TestCase* testCase, const Report& report) {
    typename TestCase::MemoryTally tally;
    tally.allocations  = report.allocations;
    tally.frees        = report.frees;
    tally.leakedBlocks = report.leakedBlocks;
    tally.leakedBytes  = report.leakedBytes;
    tally.peakBytes    = report.peakBytes;
    tally.largestBlock = report.largestBlock;
    tally.faults       = report.doubleFrees + report.overflows + report.useAfterFrees;
    testCase->add_memory(tally);
    return report;
}

} // namespace memory

TRX_END_NAMESPACE

// ---------------------------------------------------------------------------
// Canonical checks
//
// Each one runs the callable inside a tracking scope and asserts on one field
// of the report, reusing the ordinary CHECK machinery so failures are counted,
// located and printed like every other check.
// ---------------------------------------------------------------------------
#define TRX_MEMORY_REPORT(body) \
    (::testrixa::memory::record(this, ::testrixa::memory::inspect(body)))

#define TRX_CHECK_NO_LEAK(body, ...) \
    TRX_CHECK_SAME(TRX_MEMORY_REPORT(body).leakedBlocks, (std::size_t)0, ##__VA_ARGS__)

#define TRX_CHECK_LEAK_COUNT(body, expected, ...) \
    TRX_CHECK_SAME(TRX_MEMORY_REPORT(body).leakedBlocks, (std::size_t)(expected), ##__VA_ARGS__)

#define TRX_CHECK_ALLOC_COUNT(body, expected, ...) \
    TRX_CHECK_SAME(TRX_MEMORY_REPORT(body).allocations, (std::size_t)(expected), ##__VA_ARGS__)

#define TRX_CHECK_BALANCED(body, ...) \
    TRX_CHECK(TRX_MEMORY_REPORT(body).leakedBlocks == 0, ##__VA_ARGS__)

#define TRX_CHECK_PEAK_UNDER(body, limit, ...) \
    TRX_CHECK(TRX_MEMORY_REPORT(body).peakBytes <= (std::size_t)(limit), ##__VA_ARGS__)

#define TRX_CHECK_NO_OVERFLOW(body, ...) \
    TRX_CHECK_SAME(TRX_MEMORY_REPORT(body).overflows, (std::size_t)0, ##__VA_ARGS__)

#define TRX_CHECK_NO_DOUBLE_FREE(body, ...) \
    TRX_CHECK_SAME(TRX_MEMORY_REPORT(body).doubleFrees, (std::size_t)0, ##__VA_ARGS__)

// Needs --mem.preset=paranoid: pattern fill costs a memset per free, so the
// default preset does not pay for it and this check would always pass.
#define TRX_CHECK_NO_USE_AFTER_FREE(body, ...) \
    TRX_CHECK_SAME(TRX_MEMORY_REPORT(body).useAfterFrees, (std::size_t)0, ##__VA_ARGS__)

#define TRX_CHECK_MEMORY_CLEAN(body, ...) \
    TRX_CHECK(TRX_MEMORY_REPORT(body).clean(), ##__VA_ARGS__)

// ---------------------------------------------------------------------------
// Short aliases -- see <testrixa/testrixa.h> for the naming contract.
// ---------------------------------------------------------------------------
#ifndef TESTRIXA_NO_SHORT_MACROS

#define MEMORY_REPORT(body)                     TRX_MEMORY_REPORT(body)
#define CHECK_NO_LEAK(body, ...)                TRX_CHECK_NO_LEAK(body, ##__VA_ARGS__)
#define CHECK_LEAK_COUNT(body, expected, ...)   TRX_CHECK_LEAK_COUNT(body, expected, ##__VA_ARGS__)
#define CHECK_ALLOC_COUNT(body, expected, ...)  TRX_CHECK_ALLOC_COUNT(body, expected, ##__VA_ARGS__)
#define CHECK_BALANCED(body, ...)               TRX_CHECK_BALANCED(body, ##__VA_ARGS__)
#define CHECK_PEAK_UNDER(body, limit, ...)      TRX_CHECK_PEAK_UNDER(body, limit, ##__VA_ARGS__)
#define CHECK_NO_OVERFLOW(body, ...)            TRX_CHECK_NO_OVERFLOW(body, ##__VA_ARGS__)
#define CHECK_NO_DOUBLE_FREE(body, ...)         TRX_CHECK_NO_DOUBLE_FREE(body, ##__VA_ARGS__)
#define CHECK_NO_USE_AFTER_FREE(body, ...)      TRX_CHECK_NO_USE_AFTER_FREE(body, ##__VA_ARGS__)
#define CHECK_MEMORY_CLEAN(body, ...)           TRX_CHECK_MEMORY_CLEAN(body, ##__VA_ARGS__)

#endif // TESTRIXA_NO_SHORT_MACROS

#endif // TESTRIXA_MEMORY_H

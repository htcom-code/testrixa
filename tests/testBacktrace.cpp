//
//  testBacktrace.cpp
//  testrixa
//
//  Recording where an allocation came from.
//
//  A leak count without a location is half an answer. This is the half that
//  says where, and it is off by default because capturing costs more than
//  every other checker put together -- the workflow is "a run reports a leak,
//  turn this on, run again".
//
//  The tests leak deliberately and then reclaim, so the suite stays clean while
//  still exercising the path that only live blocks reach.
//

#include <cstddef>
#include <sstream>
#include <string>

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

testrixa::memory::Config withBacktrace(int frames) {
    testrixa::memory::Config config;
    config.checks |= (std::uint32_t)testrixa::memory::CheckBacktrace;
    config.backtraceFrames = frames;
    return config;
}

} // namespace

TESTCASE_BEGIN(testBacktrace)

TESTCASE_BASIC(testBacktrace) {
    using namespace testrixa::memory;

    // Backtrace support is a platform property, not a promise. glibc and macOS
    // have <execinfo.h>; musl does not, so an Alpine build has nothing to
    // capture with. Both are supported states, and the documented contract is
    // that tracking and leak detection work either way -- only the stacks are
    // missing. Asserting support unconditionally made the suite fail on musl
    // for doing exactly what the README says it does.
    TESTCASE_GROUP_START(THE_PLATFORM_REPORTS_ITS_CAPABILITY_HONESTLY) {
        void* frames[8] = {};
        const int got = platform::captureBacktrace(frames, 8, 0);

        if (platform::backtraceSupported()) {
            CHECK(got > 0);
            CHECK(frames[0] != nullptr);
            CHECK(!platform::describeFrame(frames[0]).empty());
        } else {
            // The claim and the behaviour have to agree. A platform that says
            // it cannot capture must not quietly return frames, and one that
            // returns none must not claim it can.
            CHECK_SAME(got, 0);
        }
    } TESTCASE_GROUP_END(THE_PLATFORM_REPORTS_ITS_CAPABILITY_HONESTLY)

    TESTCASE_GROUP_START(A_LEAK_CARRIES_ITS_ORIGIN) {
        int* leaked = nullptr;
        const Report report = measure([&]{
            leaked = new int(7);
            escape(leaked);
        }, withBacktrace(8));

        CHECK_SAME(report.leakedBlocks, (std::size_t)1);

        std::ostringstream out;
        Tracker::instance().reportLeaks(out, 10);
        const std::string text = out.str();

        // The leak itself is found on every platform -- that part does not
        // depend on backtraces.
        CHECK(text.find("leak #") != std::string::npos);
        CHECK(text.find("4 bytes") != std::string::npos);

        if (platform::backtraceSupported()) {
            CHECK(text.find("origin not recorded") == std::string::npos);
        } else {
            // Where it cannot capture, it has to say why. "origin not
            // recorded" on its own reads as something the reader got wrong.
            CHECK(text.find("cannot capture backtraces") != std::string::npos);
        }

        delete leaked;
    } TESTCASE_GROUP_END(A_LEAK_CARRIES_ITS_ORIGIN)

    TESTCASE_GROUP_START(WITHOUT_CAPTURE_IT_SAYS_SO_AND_SAYS_HOW) {
        // Silence would leave the reader thinking the origin is unknowable.
        int* leaked = nullptr;
        const Report report = measure([&]{
            leaked = new int(9);
            escape(leaked);
        });                                  // default config: capture off

        CHECK_SAME(report.leakedBlocks, (std::size_t)1);

        std::ostringstream out;
        Tracker::instance().reportLeaks(out, 10);
        const std::string text = out.str();

        CHECK(text.find("origin not recorded") != std::string::npos);
        CHECK(text.find("--mem.backtrace") != std::string::npos);

        delete leaked;
    } TESTCASE_GROUP_END(WITHOUT_CAPTURE_IT_SAYS_SO_AND_SAYS_HOW)

    TESTCASE_GROUP_START(ALLOCATOR_FRAMES_ARE_FILTERED_OUT) {
        // A fixed skip count cannot be right at every optimisation level, so
        // the leading allocator frames are dropped by symbol instead. Whatever
        // survives, the first line must not be operator new.
        int* leaked = nullptr;
        (void)measure([&]{
            leaked = new int(11);
            escape(leaked);
        }, withBacktrace(8));

        std::ostringstream out;
        Tracker::instance().reportLeaks(out, 10);
        const std::string text = out.str();

        if (platform::backtraceSupported()) {
            const std::string::size_type frame = text.find("      ");
            CHECK(frame != std::string::npos);
            if (frame != std::string::npos) {
                const std::string firstLine = text.substr(frame, text.find('\n', frame) - frame);
                CHECK(firstLine.find("_Znwm")          == std::string::npos);
                CHECK(firstLine.find("trxOperatorNew") == std::string::npos);
            }
        } else {
            // Nothing to filter, but the leak still has to be reported -- the
            // alternative would be a group that passes by testing nothing.
            CHECK(text.find("leak #") != std::string::npos);
        }

        delete leaked;
    } TESTCASE_GROUP_END(ALLOCATOR_FRAMES_ARE_FILTERED_OUT)

    TESTCASE_GROUP_START(THE_REPORT_IS_CAPPED) {
        // A program that leaks thousands of blocks must not print thousands of
        // stacks; the first few plus a count is what anyone reads.
        int* leaked[8] = {};
        (void)measure([&]{
            for (int i = 0; i < 8; ++i) { leaked[i] = new int(i); escape(leaked[i]); }
        }, withBacktrace(4));

        std::ostringstream out;
        Tracker::instance().reportLeaks(out, 3);
        const std::string text = out.str();

        CHECK(text.find("and 5 more") != std::string::npos);

        for (int i = 0; i < 8; ++i) delete leaked[i];
    } TESTCASE_GROUP_END(THE_REPORT_IS_CAPPED)

    TESTCASE_GROUP_START(CAPTURE_IS_OFF_BY_DEFAULT) {
        Config plain;
        CHECK_SAME(plain.backtraceFrames, 0);
        CHECK_SAME((plain.checks & (std::uint32_t)CheckBacktrace), (std::uint32_t)0);

        // Even the paranoid preset leaves it off: it is the one checker whose
        // cost is not worth paying until you already know there is a leak.
        Config strict;
        strict.checks = PresetParanoid;
        CHECK_SAME((strict.checks & (std::uint32_t)CheckBacktrace), (std::uint32_t)0);
    } TESTCASE_GROUP_END(CAPTURE_IS_OFF_BY_DEFAULT)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testBacktrace) {
    TESTCASE_RETURN
}

TESTCASE_END(testBacktrace)

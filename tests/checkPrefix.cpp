//
//  checkPrefix.cpp
//  testrixa
//
//  Compile-only contract check for the macro naming scheme.
//
//  Built by `make check-prefix` with -fsyntax-only and deliberately NOT linked
//  into testAll.out: it must not add a test case to the runner's output.
//
//  It asserts two things that nothing else covers:
//    1. TESTRIXA_NO_SHORT_MACROS really removes the short aliases.
//    2. The canonical TRX_ names are sufficient on their own -- a consumer who
//       turns the aliases off can still write a complete test case.
//

#define TESTRIXA_NO_SHORT_MACROS
#include <testrixa/testrixa.h>
#include <testrixa/mock.h>

#include <cstdint>

// 1. the short aliases must be gone
#ifdef CHECK
#   error "TESTRIXA_NO_SHORT_MACROS did not suppress CHECK"
#endif
#ifdef CHECK_SAME
#   error "TESTRIXA_NO_SHORT_MACROS did not suppress CHECK_SAME"
#endif
#ifdef TESTCASE_BEGIN
#   error "TESTRIXA_NO_SHORT_MACROS did not suppress TESTCASE_BEGIN"
#endif
#ifdef TEST_RUN
#   error "TESTRIXA_NO_SHORT_MACROS did not suppress TEST_RUN"
#endif
#ifdef TGS
#   error "TESTRIXA_NO_SHORT_MACROS did not suppress TGS"
#endif
#ifdef TUPLE_SIZE
#   error "TESTRIXA_NO_SHORT_MACROS did not suppress TUPLE_SIZE"
#endif
// MOCK_METHOD is gmock's macro too, so this alias is the likeliest of all of
// them to collide -- the escape hatch has to work for it.
#ifdef MOCK_METHOD
#   error "TESTRIXA_NO_SHORT_MACROS did not suppress MOCK_METHOD"
#endif
#ifdef MOCK_BEGIN
#   error "TESTRIXA_NO_SHORT_MACROS did not suppress MOCK_BEGIN"
#endif
#ifdef CHECK_CALLED
#   error "TESTRIXA_NO_SHORT_MACROS did not suppress CHECK_CALLED"
#endif

// 2. the canonical names must carry a full test case on their own
struct PrefixClock {
    virtual ~PrefixClock() {}
    virtual std::uint64_t now() const = 0;
    virtual void sleep(int ms) = 0;
};

TRX_MOCK_BEGIN(PrefixMockClock, PrefixClock)
    TRX_MOCK_CONST_METHOD(now,   std::uint64_t, (),       ())
    TRX_MOCK_METHOD      (sleep, void,          (int ms), (ms))
TRX_MOCK_END

TRX_TESTCASE_BEGIN(checkPrefix)

TRX_TESTCASE_BASIC(checkPrefix) {
    TRX_TESTCASE_GROUP_START(canonical_names) {
        TRX_CHECK(true);
        TRX_CHECK_TRUE(true);
        TRX_CHECK_FALSE(false);
        TRX_CHECK_SAME(1, 1);
        TRX_CHECK_SAME_TRUE(1, 1);
        TRX_CHECK_SAME_FALSE(1, 2);

        const char lhs[4] = {'a', 'b', 'c', '\0'};
        const char rhs[4] = {'a', 'b', 'c', '\0'};
        TRX_CHECK_MEM_SAME(lhs, rhs, sizeof(lhs));
        TRX_CHECK_MEM_SAME_TRUE(lhs, rhs, sizeof(lhs));
    } TRX_TESTCASE_GROUP_END(canonical_names)

    TRX_TESTCASE_GROUP_START(canonical_mock) {
        PrefixMockClock clock;
        clock.nowMock.returns(1);
        clock.sleep(5);

        TRX_CHECK_CALLED(clock.sleepMock);
        TRX_CHECK_NOT_CALLED(clock.nowMock);
        TRX_CHECK_CALLED_TIMES(clock.sleepMock, 1);
        TRX_CHECK_LAST_CALL(clock.sleepMock, 5);
        TRX_CHECK_NO_UNSTUBBED_CALLS(clock.nowMock);
    } TRX_TESTCASE_GROUP_END(canonical_mock)

    TRX_TESTCASE_RETURN
}

TRX_TESTCASE_MEASURE(checkPrefix) {
    TRX_TESTCASE_GROUP_START(canonical_timing) {
        auto retval = TRX_CHECK_TIME_FUNCTION([](int i) -> int { return i + 1; }, 1);
        TRX_CHECK(TRX_TUPLE_SIZE(retval) == 2);
        TRX_CHECK(TRX_TUPLE_FIRST(retval));
        TRX_CHECK_SAME(TRX_TUPLE_SECOND(retval), 2);

        TRX_CHECK_TIME_FUNCTION_DESC("described", [] {});
        TRX_CHECK_TIME_FUNCTION_FORCE("forced", 3, [] {});
    } TRX_TESTCASE_GROUP_END(canonical_timing)

    TRX_TESTCASE_RETURN
}

TRX_TESTCASE_END(checkPrefix)

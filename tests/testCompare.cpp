//
//  testCompare.cpp
//  testrixa
//
//  CHECK_SAME across mixed types.
//
//  The prototype compared with a plain ==, so the usual arithmetic conversions
//  applied and `CHECK_SAME(-1, 4294967295u)` passed: the int converts to
//  unsigned and the bit patterns match. That rule is correct for arithmetic and
//  wrong for an assertion, and a test framework that answers wrong about
//  equality is worse than no framework.
//
//  These cases pin the fixed behaviour. Every negative-vs-unsigned pair here
//  would have passed as "equal" before.
//

#include <climits>
#include <cstddef>
#include <cstdint>

#include <testrixa/testrixa.h>

namespace {

enum PlainEnum      { PlainOne = 1, PlainTwo = 2 };
enum class TypedEnum : int { One = 1, Two = 2 };

} // namespace

TESTCASE_BEGIN(testCompare)

TESTCASE_BASIC(testCompare) {

    TESTCASE_GROUP_START(NEGATIVE_NEVER_EQUALS_UNSIGNED) {
        // The headline case: -1 and UINT_MAX share a bit pattern, not a value.
        CHECK_SAME_FALSE(-1, 4294967295u);
        CHECK_SAME_FALSE(4294967295u, -1);

        CHECK_SAME_FALSE(-1, (unsigned char)255);
        CHECK_SAME_FALSE(-1, (unsigned short)65535);
        CHECK_SAME_FALSE((std::int64_t)-1, (std::uint64_t)UINT64_MAX);
        CHECK_SAME_FALSE(-1, (std::size_t)SIZE_MAX);

        // A negative value is not equal to zero either, however it converts.
        CHECK_SAME_FALSE(-1, 0u);
    } TESTCASE_GROUP_END(NEGATIVE_NEVER_EQUALS_UNSIGNED)

    TESTCASE_GROUP_START(MIXED_SIGNEDNESS_STILL_COMPARES_EQUAL_VALUES) {
        // Fixing the negative case must not break the ordinary one.
        CHECK_SAME(1, 1u);
        CHECK_SAME(1u, 1);
        CHECK_SAME((std::size_t)42, 42);
        CHECK_SAME(42, (std::size_t)42);
        CHECK_SAME((std::int64_t)7, (std::uint32_t)7);
        CHECK_SAME(0, 0u);

        CHECK_SAME_FALSE(1, 2u);
        CHECK_SAME_FALSE(2u, 1);
    } TESTCASE_GROUP_END(MIXED_SIGNEDNESS_STILL_COMPARES_EQUAL_VALUES)

    TESTCASE_GROUP_START(SAME_SIGNEDNESS_IS_UNTOUCHED) {
        CHECK_SAME(-1, -1);
        CHECK_SAME(INT_MIN, INT_MIN);
        CHECK_SAME(0u, 0u);
        CHECK_SAME(UINT_MAX, UINT_MAX);
        CHECK_SAME_FALSE(-1, 1);
        CHECK_SAME_FALSE(1u, 2u);
    } TESTCASE_GROUP_END(SAME_SIGNEDNESS_IS_UNTOUCHED)

    TESTCASE_GROUP_START(FLOATING_POINT_IS_UNTOUCHED) {
        CHECK_SAME(1.5, 1.5);
        CHECK_SAME(1.0f, 1.0);
        CHECK_SAME(-1.0, -1.0);
        // A float still compares against an integer the way the language says.
        CHECK_SAME(2.0, 2);
        CHECK_SAME_FALSE(2.5, 2);
        // -1.0 vs unsigned goes through floating point, not the integer rule.
        CHECK_SAME_FALSE(-1.0, 1u);
    } TESTCASE_GROUP_END(FLOATING_POINT_IS_UNTOUCHED)

    TESTCASE_GROUP_START(BOOL_AND_CHAR) {
        CHECK_SAME(true, 1);
        CHECK_SAME(false, 0);
        CHECK_SAME(true, 1u);
        CHECK_SAME_FALSE(false, 1);
        CHECK_SAME_FALSE(true, -1);

        CHECK_SAME((char)'A', 65);
        CHECK_SAME_FALSE((char)'A', 66);
    } TESTCASE_GROUP_END(BOOL_AND_CHAR)

    TESTCASE_GROUP_START(ENUMS_COMPARE_BY_UNDERLYING_VALUE) {
        CHECK_SAME(PlainOne, 1);
        CHECK_SAME(1, PlainOne);
        CHECK_SAME(PlainTwo, 2u);
        CHECK_SAME_FALSE(PlainOne, 2);
        CHECK_SAME(PlainOne, PlainOne);
        CHECK_SAME_FALSE(PlainOne, PlainTwo);

        CHECK_SAME(TypedEnum::One, 1);
        CHECK_SAME_FALSE(TypedEnum::One, 2);
        CHECK_SAME(TypedEnum::Two, TypedEnum::Two);
    } TESTCASE_GROUP_END(ENUMS_COMPARE_BY_UNDERLYING_VALUE)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testCompare) {
    TESTCASE_RETURN
}

TESTCASE_END(testCompare)

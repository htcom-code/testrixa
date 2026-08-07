//
//  testrixa.h
//  testcase suit
//
//  Created by htjulia on 2/27/26.
//

/*
    compile macro flag option(for command line) : -DTEST_RUN_TERM // add flags.
*/



#ifndef TESTRIXA_TESTRIXA_H
#define TESTRIXA_TESTRIXA_H

#include <testrixa/detail/core.hpp>

/*
    Macro naming
    ------------
    Every public macro exists twice:

      TRX_CHECK(expr)   canonical, always defined, cannot collide
      CHECK(expr)       short alias, defined unless TESTRIXA_NO_SHORT_MACROS

    The short names are the ones you normally write. Define
    TESTRIXA_NO_SHORT_MACROS before including this header when they clash with
    another framework or with your own code -- names like CHECK and TEST_RUN
    are common enough that the collision is a matter of when, not if.

        #define TESTRIXA_NO_SHORT_MACROS
        #include <testrixa/testrixa.h>

        TRX_TESTCASE_BEGIN(testAA)
        ...
*/

/*
    example : testAA.cpp

    #include <testrixa/testrixa.h>

    // Beginnings and endings macro must always exist in pairs.
    TESTCASE_BEGIN(testAA)

    // basic check area
    TESTCASE_BASIC(testAA) {
        TESTCASE_GROUP_START(#group_name) {
            CHECK(1=1);
            CHECK_SAME(1, 1);
            .....
         } TESTCASE_GROUP_END(#group_name)

        TESTCASE_RETURN
    }

    // time measure area
    TESTCASE_MEASURE(testAA) {
        TESTCASE_GROUP_START(#group_name) {
            CHECK_TIME_FUNCTION([]{}{
                // time measure code.
            });
            .....
        } TESTCASE_GROUP_END(#group_name)

        TESTCASE_RETURN
    }

    TESTCASE_END(testAA)
 */


// ============================================================================
// Canonical API
// ============================================================================

//use make class macro
#define TRX_TESTCASE_BEGIN(testcase)        INTERNAL_TESTCASE_BEGIN(testcase)
#define TRX_TESTCASE_END(testcase)          INTERNAL_TESTCASE_END(testcase)
#define TRX_TESTCASE_BASIC(testcase)        INTERNAL_TESTCASE_BASIC(testcase)
#define TRX_TESTCASE_MEASURE(testcase)      INTERNAL_TESTCASE_MEASURE(testcase)
// Optional third phase. A test case without one behaves exactly as before.
#define TRX_TESTCASE_MEMORY(testcase)       INTERNAL_TESTCASE_MEMORY(testcase)

// Fixture. Both optional, both run around every phase -- each phase is a
// separate run of the code under test and gets the same starting state.
// Members declared after TESTCASE_BEGIN are the fixture's state.
#define TRX_TESTCASE_STRESS(testcase)       INTERNAL_TESTCASE_STRESS(testcase)
#define TRX_TESTCASE_SETUP(testcase)        INTERNAL_TESTCASE_SETUP(testcase)
#define TRX_TESTCASE_TEARDOWN(testcase)     INTERNAL_TESTCASE_TEARDOWN(testcase)

#define TRX_TESTCASE_GROUP_START(groupname) INTERNAL_TESTCASE_GROUP_START(groupname)
#define TRX_TESTCASE_GROUP_END(groupname)   INTERNAL_TESTCASE_GROUP_END(groupname)

#define TRX_TESTCASE_RETURN INTERNAL_TESTCASE_RETURN

/*
    check support macro
 */
#define TRX_CHECK_TRUE(expr, ...)  \
    CHECK_IMP(expr, true, ##__VA_ARGS__)

#define TRX_CHECK_FALSE(expr, ...) \
    CHECK_IMP(expr, false, ##__VA_ARGS__)

#define TRX_CHECK(expr, ...) \
    TRX_CHECK_TRUE(expr, ##__VA_ARGS__)

#define TRX_CHECK_SAME_TRUE(source, target, ...) \
    CHECK_SAME_IMPL(source, target, true,  ##__VA_ARGS__)

#define TRX_CHECK_SAME_FALSE(source, target, ...) \
    CHECK_SAME_IMPL(source, target, false, ##__VA_ARGS__)

#define TRX_CHECK_SAME(source, target, ...)  \
    CHECK_SAME_IMPL(source, target, true,  ##__VA_ARGS__)

#define TRX_CHECK_MEM_SAME_TRUE(source, target, size, ...) \
    CHECK_MEM_SAME_IMPL(source, target, size, true,  ##__VA_ARGS__)

#define TRX_CHECK_MEM_SAME_FALSE(source, target, size, ...) \
    CHECK_MEM_SAME_IMPL(source, target, size, false, ##__VA_ARGS__)

#define TRX_CHECK_MEM_SAME(source, target, size, ...) \
    CHECK_MEM_SAME_IMPL(source, target, size, true,  ##__VA_ARGS__)

/*
 Time Measure support macro

 example : measure class member method
    // std::string length loop test
    std::string test_string("aaaaaaa");
    auto retval = CHECK_TIME_OBJECT(&std::string::length, test_string);

    // tuple size is 2 <bool, return method>
    // retval is tuple<bool, std::string_size_type>.
    CHECK(TUPLE_SIZE(retval) == 2);
    CHECK(TUPLE_FIRST(retval));
    CHECK_SAME(TUPLE_SECOND(retval), 7);

 example : templeat ambious method call
    auto retval = CHECK_TIME_FUNCTION(std::bind<std::string(int)>(std::to_string, std::placeholders::_1), (int)32);
    CHECK(TUPLE_SIZE(retval) == 2);
    CHECK(TUPLE_FIRST(retval));
    CHECK_SAME(TUPLE_SECOND(retval), "32");

 example : ramda method call
    auto retval = CHECK_TIME_FUNCTION([](int i)->std::string{return std::to_string(i);}, 32);
    CHECK(TUPLE_SIZE(retval) == 2);
    CHECK(TUPLE_FIRST(retval));
    CHECK_SAME(TUPLE_SECOND(retval), "32");


 more example is testTEST.cpp

 */

#define TRX_CHECK_TIME_OBJECT(func, ins_obj, ...) \
    INTERNAL_CHECK_TIME_FORCE(#func, m_loop, func, ins_obj, ##__VA_ARGS__)

#define TRX_CHECK_TIME_OBJECT_DESC(desc, func, ins_obj, ...) \
    INTERNAL_CHECK_TIME_FORCE(desc, m_loop, func, ins_obj, ##__VA_ARGS__)

#define TRX_CHECK_TIME_FUNCTION_DESC(desc, func, ...) \
    INTERNAL_CHECK_TIME_FUNCTION_FORCE(desc, m_loop, func, ##__VA_ARGS__)

#define TRX_CHECK_TIME_FUNCTION(func, ...) \
    INTERNAL_CHECK_TIME_FUNCTION_FORCE(#func, m_loop, func, ##__VA_ARGS__)

#define TRX_CHECK_TIME_FORCE(desc, loop, func, ins_obj, ...) \
    INTERNAL_CHECK_TIME_FORCE(desc, loop, func, ins_obj, ##__VA_ARGS__)

#define TRX_CHECK_TIME_FUNCTION_FORCE(desc, times, func, ...) \
    INTERNAL_CHECK_TIME_FUNCTION_FORCE(desc, times, func, ##__VA_ARGS__)


// Yields the testAll status (0 == everything passed), so it works both as a
// statement and as a value:
//
//     TEST_RUN("", true, true, true, 100);              // as before
//     return TEST_RUN("", true, true, true, 100) ? 1 : 0;   // propagate to the shell
#define TRX_TEST_RUN(case, fail_stop, basic, measure, loop)     \
    (TEST::Initialization(),                                    \
     TEST::testAll(case, fail_stop, basic, measure, loop))


// ============================================================================
// Short aliases
// ============================================================================

#ifndef TESTRIXA_NO_SHORT_MACROS

#define TESTCASE_BEGIN(testcase)        TRX_TESTCASE_BEGIN(testcase)
#define TESTCASE_END(testcase)          TRX_TESTCASE_END(testcase)
#define TESTCASE_BASIC(testcase)        TRX_TESTCASE_BASIC(testcase)
#define TESTCASE_MEASURE(testcase)      TRX_TESTCASE_MEASURE(testcase)
#define TESTCASE_MEMORY(testcase)       TRX_TESTCASE_MEMORY(testcase)
#define TESTCASE_STRESS(testcase)       TRX_TESTCASE_STRESS(testcase)
#define TESTCASE_SETUP(testcase)        TRX_TESTCASE_SETUP(testcase)
#define TESTCASE_TEARDOWN(testcase)     TRX_TESTCASE_TEARDOWN(testcase)

#define TESTCASE_GROUP_START(groupname) TRX_TESTCASE_GROUP_START(groupname)
#define TESTCASE_GROUP_END(groupname)   TRX_TESTCASE_GROUP_END(groupname)

// short name
#define TCB(c) TRX_TESTCASE_BEGIN(c)
#define TCE(c) TRX_TESTCASE_END(c)
#define TGS(g) TRX_TESTCASE_GROUP_START(g)
#define TGE(g) TRX_TESTCASE_GROUP_END(g)

#define TESTCASE_RETURN TRX_TESTCASE_RETURN

#define CHECK_TRUE(expr, ...)                   TRX_CHECK_TRUE(expr, ##__VA_ARGS__)
#define CHECK_FALSE(expr, ...)                  TRX_CHECK_FALSE(expr, ##__VA_ARGS__)
#define CHECK(expr, ...)                        TRX_CHECK(expr, ##__VA_ARGS__)

#define CHECK_SAME_TRUE(source, target, ...)    TRX_CHECK_SAME_TRUE(source, target, ##__VA_ARGS__)
#define CHECK_SAME_FALSE(source, target, ...)   TRX_CHECK_SAME_FALSE(source, target, ##__VA_ARGS__)
#define CHECK_SAME(source, target, ...)         TRX_CHECK_SAME(source, target, ##__VA_ARGS__)

#define CHECK_MEM_SAME_TRUE(source, target, size, ...)  TRX_CHECK_MEM_SAME_TRUE(source, target, size, ##__VA_ARGS__)
#define CHECK_MEM_SAME_FALSE(source, target, size, ...) TRX_CHECK_MEM_SAME_FALSE(source, target, size, ##__VA_ARGS__)
#define CHECK_MEM_SAME(source, target, size, ...)       TRX_CHECK_MEM_SAME(source, target, size, ##__VA_ARGS__)

#define CHECK_TIME_OBJECT(func, ins_obj, ...)           TRX_CHECK_TIME_OBJECT(func, ins_obj, ##__VA_ARGS__)
#define CHECK_TIME_OBJECT_DESC(desc, func, ins_obj, ...) TRX_CHECK_TIME_OBJECT_DESC(desc, func, ins_obj, ##__VA_ARGS__)
#define CHECK_TIME_FUNCTION_DESC(desc, func, ...)       TRX_CHECK_TIME_FUNCTION_DESC(desc, func, ##__VA_ARGS__)
#define CHECK_TIME_FUNCTION(func, ...)                  TRX_CHECK_TIME_FUNCTION(func, ##__VA_ARGS__)
#define CHECK_TIME_FORCE(desc, loop, func, ins_obj, ...) TRX_CHECK_TIME_FORCE(desc, loop, func, ins_obj, ##__VA_ARGS__)
#define CHECK_TIME_FUNCTION_FORCE(desc, times, func, ...) TRX_CHECK_TIME_FUNCTION_FORCE(desc, times, func, ##__VA_ARGS__)

#define TEST_RUN(case, fail_stop, basic, measure, loop) \
    TRX_TEST_RUN(case, fail_stop, basic, measure, loop)

#endif // TESTRIXA_NO_SHORT_MACROS


#endif // TESTRIXA_TESTRIXA_H

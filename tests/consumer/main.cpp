//
//  main.cpp
//  testrixa consumer smoke test
//
//  One translation unit that does what a real consumer does: pull in the
//  public headers, define main through TEST_RUN_TERM, write a test case, and
//  use the memory suite. If the installed package is missing a header or the
//  exported target has no include directory, this fails to compile.
//

#define TEST_RUN_TERM
#include <testrixa/testrixa.h>
#include <testrixa/memory.h>

TESTCASE_BEGIN(consumerSmoke)

TESTCASE_BASIC(consumerSmoke) {
    TESTCASE_GROUP_START(PUBLIC_API) {
        CHECK(true);
        CHECK_SAME(1 + 1, 2);
    } TESTCASE_GROUP_END(PUBLIC_API)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(consumerSmoke) {
    TESTCASE_RETURN
}

TESTCASE_MEMORY(consumerSmoke) {
    TESTCASE_GROUP_START(MEMORY_API) {
        CHECK_NO_LEAK([]{
            int* values = new int[4];
#if defined(__clang__) || defined(__GNUC__)
            asm volatile("" : : "g"(values) : "memory");
#endif
            delete[] values;
        });
    } TESTCASE_GROUP_END(MEMORY_API)

    TESTCASE_RETURN
}

TESTCASE_END(consumerSmoke)

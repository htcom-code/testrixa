//
//  testExample.cpp
//  testrixa
//
//  Created by htjulia on 2/28/26.
//
//  The smallest complete testrixa program: one file, one test case, runnable.
//
//  TEST_RUN_TERM pulls in main() and the terminal runner, so this single
//  translation unit builds into an executable on its own:
//
//      c++ -std=c++17 -I../include -o example.out testExample.cpp
//      ./example.out -h
//

#define TEST_RUN_TERM
#include <testrixa/testrixa.h>

TESTCASE_BEGIN(testExample)

TESTCASE_BASIC(testExample) {
    
    TESTCASE_RETURN
}

TESTCASE_MEASURE(testExample) {
    
    TESTCASE_RETURN
}


TESTCASE_END(testExample)

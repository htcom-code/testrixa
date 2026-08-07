//
//  testMain.cpp
//  testrixa
//
//  The translation unit that supplies main().
//
//  TEST_RUN_TERM makes <testrixa/testrixa.h> emit the terminal runner: main(),
//  the argument parser and the -l/-h listings. Exactly one TU in a program may
//  define it.
//
//  The prototype produced this object by compiling internal/TEST.hpp directly
//  with `-x c++`. A real source file says the same thing without asking the
//  build system to treat a header as a translation unit.
//

#define TEST_RUN_TERM
#include <testrixa/testrixa.h>

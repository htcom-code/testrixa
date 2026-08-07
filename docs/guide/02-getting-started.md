# 02. Getting Started

From an empty directory to a test suite you can run, in five steps. Everything
below was executed as written; the output is the real output.

Assumes testrixa is installed — see [01. Installation](01-installation.md).
If you have not installed it, add `-I/path/to/testrixa/include` to every
compile command here.

- [1. The code under test](#1-the-code-under-test)
- [2. The first test](#2-the-first-test)
- [3. Watch it fail](#3-watch-it-fail)
- [4. More than one test file](#4-more-than-one-test-file)
- [5. A Makefile](#5-a-makefile)

## 1. The code under test

```sh
mkdir -p mylib/tests && cd mylib
```

`stats.h` — something worth testing:

```cpp
#ifndef STATS_H
#define STATS_H

#include <vector>

inline int sum(const std::vector<int>& values) {
    int total = 0;
    for (int v : values) total += v;
    return total;
}

inline double mean(const std::vector<int>& values) {
    if (values.empty()) return 0.0;
    return (double)sum(values) / (double)values.size();
}

#endif
```

## 2. The first test

`tests/testStats.cpp`:

```cpp
#define TEST_RUN_TERM
#include <testrixa/testrixa.h>

#include "../stats.h"

TESTCASE_BEGIN(testStats)

TESTCASE_BASIC(testStats) {
    TESTCASE_GROUP_START(SUM_ADDS_EVERYTHING) {
        CHECK_SAME(sum({1, 2, 3}), 6);
        CHECK_SAME(sum({}), 0);
        CHECK_SAME(sum({-1, 1}), 0);
    } TESTCASE_GROUP_END(SUM_ADDS_EVERYTHING)

    TESTCASE_GROUP_START(MEAN_HANDLES_THE_EMPTY_CASE) {
        CHECK_SAME(mean({2, 4}), 3.0);
        CHECK_SAME(mean({}), 0.0);      // not a division by zero
    } TESTCASE_GROUP_END(MEAN_HANDLES_THE_EMPTY_CASE)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testStats) {
    TESTCASE_RETURN
}

TESTCASE_END(testStats)
```

Four things are doing work here:

| | |
|---|---|
| `#define TEST_RUN_TERM` | Pulls in `main()` and the terminal runner. **Exactly one** file in the program defines it. |
| `TESTCASE_BEGIN` / `TESTCASE_END` | Generate a class that registers itself. There is no list of tests to maintain. |
| `TESTCASE_BASIC` / `TESTCASE_MEASURE` | The two phases that always run. Both must exist, even if one is empty. |
| `TESTCASE_GROUP_START` / `_END` | A group is one row in the report. Name it after what should be true. |

Build and run:

```sh
c++ -std=c++17 -o tests/testAll.out tests/testStats.cpp
./tests/testAll.out
```

```
[testStats::BASIC]
----------------------------------------------------------------------------------
| Group Name                                     | Lvl|   Total| Success|    Fail|
----------------------------------------------------------------------------------
| SUM_ADDS_EVERYTHING                            |   0|       3|       3|       0|
|--------------------------------------------------------------------------------|
| MEAN_HANDLES_THE_EMPTY_CASE                    |   0|       2|       2|       0|
|--------------------------------------------------------------------------------|
| Total Test case                                     |       5|       5|       0|
----------------------------------------------------------------------------------

ALL test case finised total[1] success[1] fail[0]
```

```sh
echo $?      # 0
```

**Check the exit code, not the table.** A run that failed, took a bad argument,
or matched no test case at all is non-zero — that is what your CI reads.

## 3. Watch it fail

A test you have never seen fail is a test you have no reason to trust. Break
`mean` on purpose:

```cpp
inline double mean(const std::vector<int>& values) {
    return (double)sum(values) / (double)values.size();   // no empty guard
}
```

```sh
c++ -std=c++17 -o tests/testAll.out tests/testStats.cpp && ./tests/testAll.out
```

```
| MEAN_HANDLES_THE_EMPTY_CASE                    |   0|       2|       1|       1|
|  fail[    1] CHECK_TRUE [expr:(mean({}), 0.0) value:(nan, 0.000000)] testStats.cpp:17
|--------------------------------------------------------------------------------|
| Total Test case                                     |       5|       4|       1|
----------------------------------------------------------------------------------

ALL test case finised total[1] success[0] fail[1]
```

```sh
echo $?      # 1
```

The line gives you both expressions, both values, the file and the line —
`nan` against `0.000000`, which is the bug.

By default a failing test case stops at its first failure and the runner moves
on to the next one. With only one test case here, `total[1] fail[1]` is the
whole story; with several you would see `not running[N]` for the ones that were
skipped. Pass `-s` to run every assertion regardless and see everything that is
broken at once:

```sh
./tests/testAll.out -s
```

Put the guard back before moving on.

## 4. More than one test file

One test case per file is the usual arrangement. Add `tests/testMore.cpp`:

```cpp
#include <testrixa/testrixa.h>          // note: no TEST_RUN_TERM here

#include "../stats.h"

TESTCASE_BEGIN(testMore)

TESTCASE_BASIC(testMore) {
    TESTCASE_GROUP_START(SUM_IGNORES_ORDER) {
        CHECK_SAME(sum({1, 2, 3}), sum({3, 2, 1}));
    } TESTCASE_GROUP_END(SUM_IGNORES_ORDER)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testMore) {
    TESTCASE_RETURN
}

TESTCASE_END(testMore)
```

**`TEST_RUN_TERM` goes in one file only.** Defining it twice gives you two
`main()` functions and a link error.

```sh
c++ -std=c++17 -o tests/testAll.out tests/testStats.cpp tests/testMore.cpp
./tests/testAll.out -l
```

```
Test case list: 
  testMore                      : ./tests/testAll.out testMore
  testStats                     : ./tests/testAll.out testStats
```

Both registered themselves. Run one:

```sh
./tests/testAll.out testStats
```

## 5. A Makefile

`tests/Makefile`:

```make
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
TARGET    = testAll.out
SRCS      = testStats.cpp testMore.cpp
OBJS      = $(SRCS:.cpp=.o)

# Every object depends on the header under test. Without this, editing stats.h
# leaves stale objects behind and you test the previous version without knowing.
DEPS      = ../stats.h

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
```

```sh
make -C tests test
```

The `DEPS` line is not decoration. testrixa's own build once lacked header
dependencies: a rename left stale objects behind, only one file rebuilt, and
the runner reported "Test case None" — which looked like a framework bug and
was not. If your headers are not in the dependency list, you will eventually
test something you no longer have.

For CMake instead of make, see
[09. Build Integration](09-build-integration.md).

## What you have now

A test binary that registers its own tests, tells you where a failure is, and
returns a usable exit code. That is the whole of the basic phase.

## Next

- [03. Writing Tests](03-writing-tests.md) — every assertion, phases, fixtures
- [04. Running Tests](04-running-tests.md) — the command line in full
- [05. Memory and Leaks](05-memory.md) — the part a unit-test framework usually
  does not have

## Related

- [Guide index](README.md)

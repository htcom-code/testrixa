# testrixa

[![CI](https://github.com/htcom-code/testrixa/actions/workflows/ci.yml/badge.svg)](https://github.com/htcom-code/testrixa/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![header-only](https://img.shields.io/badge/header--only-no%20dependencies-brightgreen.svg)](#installing)

A C/C++ testing platform in header files. Unit tests, benchmarks, memory and
leak checking, stress, concurrency, mocks, fixtures and reporting — one
library, no dependencies, C++17.

**Contents** · [Why](#why) · [Installing](#installing) ·
[The shape of a test](#the-shape-of-a-test) · [Memory and leaks](#memory-and-leaks) ·
[Mocks](#mocks) · [Stress](#stress) · [Concurrency](#concurrency) ·
[Running](#running) · [What it does not do](#what-it-does-not-do) ·
[Requirements](#requirements) · [Building testrixa itself](#building-testrixa-itself)

```cpp
#define TEST_RUN_TERM
#include <testrixa/testrixa.h>

int add(int a, int b) { return a + b; }

TESTCASE_BEGIN(testAdd)

TESTCASE_BASIC(testAdd) {
    TESTCASE_GROUP_START(adds_two_numbers) {
        CHECK_SAME(add(2, 2), 4);
        CHECK(add(0, 0) == 0);
        CHECK_FALSE(add(1, 1) == 3);
    } TESTCASE_GROUP_END(adds_two_numbers)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(testAdd) {
    TESTCASE_GROUP_START(how_fast) {
        CHECK_TIME_FUNCTION([] { return add(2, 2); });
    } TESTCASE_GROUP_END(how_fast)

    TESTCASE_RETURN
}

TESTCASE_END(testAdd)
```

```sh
c++ -std=c++17 -I path/to/testrixa/include -o test.out test.cpp && ./test.out
```

```
[testAdd::BASIC]
----------------------------------------------------------------------------------
| Group Name                                     | Lvl|   Total| Success|    Fail|
----------------------------------------------------------------------------------
| adds_two_numbers                               |   0|       3|       3|       0|
|--------------------------------------------------------------------------------|
| Total Test case                                     |       3|       3|       0|
----------------------------------------------------------------------------------

[testAdd::MEASURE] format[mm:ss.zzz.µµµ.nnn], [D]Default Loop, [C]Custom Loop
-------------------------------------------------------------------------------------------------
| Group Name / Description          | Loop|          Average|                       Source| Line|
-------------------------------------------------------------------------------------------------
| how_fast                                                                                      |
| [D][] { return add(2, 2); }       |  100|00:00.000.000.021|                     test.cpp|   20|
|-----------------------------------------------------------------------------------------------|
| Measure case : 1                                                                              |
-------------------------------------------------------------------------------------------------

ALL test case finised total[1] success[1] fail[0]
```

No `main()` to write, no registration list to keep up to date, no build system
required. `TEST_RUN_TERM` in exactly one translation unit pulls in `main()`;
every `TESTCASE_BEGIN` registers itself.

📖 **[User Guide](docs/guide/README.md)** — installing it on your system,
writing and running tests, each component in depth, build and CI integration,
and what the error messages mean. Start at
[01. Installation](docs/guide/01-installation.md).

## Why

Most projects reach for a unit-test framework, then a separate benchmark
library, then a sanitizer build for memory, then something else again for
concurrency — four tools with four ways of reporting and four things to install.
testrixa is one header set that covers those axes with a single test-case shape
and a single report.

That does not make it better than the specialists at what they specialise in.
Where a mature tool already does the job properly, this defers to it rather than
writing a worse copy — see [What it does not do](#what-it-does-not-do), which is
the most important section here.

## Installing

Header-only. Any of these works.

**Copy the headers.** Drop `include/testrixa` where your compiler looks and add
`-I`. Nothing else is needed.

**make.**

```sh
make install PREFIX=/usr/local        # copies headers only
c++ -std=c++17 -I/usr/local/include -o test.out test.cpp
```

**CMake** — as a subdirectory or a `FetchContent`:

```cmake
add_subdirectory(third_party/testrixa)
target_link_libraries(my_tests PRIVATE testrixa::testrixa)
```

or installed, via `find_package`:

```sh
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local && cmake --build build && cmake --install build
```

```cmake
find_package(testrixa 0.1 REQUIRED)
target_link_libraries(my_tests PRIVATE testrixa::testrixa)
```

The interface target carries the include path and `cxx_std_17`. Pulling
testrixa in with `add_subdirectory` does not build its tests or examples.

## The shape of a test

A **test case** is a class the macros write for you. It has up to four
**phases**, all optional except the ones you use:

| Phase | Macro | Runs |
|---|---|---|
| Basic | `TESTCASE_BASIC` | always |
| Measure | `TESTCASE_MEASURE` | always |
| Memory | `TESTCASE_MEMORY` | only with `--mem` / `--only=memory` |
| Stress | `TESTCASE_STRESS` | only with `--stress` / `--only=stress` |

Memory and stress are opt-in because both are slow by construction, and an
ordinary run must not pay for them.

Inside a phase, **groups** are the unit the report prints a row for:

```cpp
TESTCASE_GROUP_START(name) {
    ...
} TESTCASE_GROUP_END(name)
```

Members declared after `TESTCASE_BEGIN` are the fixture's state. `TESTCASE_SETUP`
and `TESTCASE_TEARDOWN` run around **each phase** — every phase is a separate
run of the code under test and gets the same starting state. Both return `bool`.

```cpp
TESTCASE_BEGIN(testThing)

std::string m_resource;

TESTCASE_SETUP(testThing)    { m_resource = "ready"; return true; }
TESTCASE_TEARDOWN(testThing) { m_resource.clear();   return true; }
```

### Assertions

| | |
|---|---|
| `CHECK(expr)` | `CHECK_TRUE` / `CHECK_FALSE` |
| `CHECK_SAME(a, b)` | `CHECK_SAME_TRUE` / `CHECK_SAME_FALSE` |
| `CHECK_MEM_SAME(a, b, size)` | raw memory comparison |
| `CHECK_TIME_FUNCTION(fn, ...)` | time a callable |
| `CHECK_TIME_OBJECT(&T::method, obj, ...)` | time a member function |

Every one takes an optional trailing message. A failing check prints the
expression, both values, the file and the line.

Each public macro exists twice: `TRX_CHECK` is canonical and always defined,
`CHECK` is the short alias. Define `TESTRIXA_NO_SHORT_MACROS` before including
to keep only the prefixed names — names like `CHECK` and `MOCK_METHOD` collide
with other frameworks eventually.

## Memory and leaks

`<testrixa/memory.h>` replaces the global allocator for the duration of a
scope and reports what happened inside it.

```cpp
#include <testrixa/memory.h>

TESTCASE_MEMORY(testThing) {
    TESTCASE_GROUP_START(memory) {
        CHECK_NO_LEAK([] {
            int* values = new int[32];
            delete[] values;
        });

        int* leaked = nullptr;
        CHECK_LEAK_COUNT([&] { leaked = new int(9); }, 1);
        delete leaked;

        CHECK_PEAK_UNDER([] {
            int* values = new int[16];
            delete[] values;
        }, 1024);
    } TESTCASE_GROUP_END(memory)

    TESTCASE_RETURN
}
```

`CHECK_NO_LEAK` · `CHECK_LEAK_COUNT` · `CHECK_ALLOC_COUNT` · `CHECK_BALANCED` ·
`CHECK_PEAK_UNDER` · `CHECK_NO_OVERFLOW` · `CHECK_NO_DOUBLE_FREE` ·
`CHECK_NO_USE_AFTER_FREE` · `CHECK_MEMORY_CLEAN`, and `MEMORY_REPORT(body)` for
the whole report.

What it sees, and what it does not, is printed above every memory table:

```
coverage: C++ new/delete = full | C malloc = only where <testrixa/malloc_shim.h>
is included | prebuilt libraries = not tracked
```

For C allocation, include `<testrixa/malloc_shim.h>` **last** in the translation
unit you want covered — it redefines `malloc`/`free`/`realloc`/`calloc`/`strdup`
as macros, so anything included after it would be rewritten too.

Tuning: `--mem.preset=fast|default|paranoid`, `--mem.redzone=<bytes>`,
`--mem.quarantine=<MB>`, `--mem.backtrace=<frames>`. Backtraces cost more than
every other check combined, so they default to off; turn them on when you have
a leak and need to know where it came from.

`CHECK_NO_USE_AFTER_FREE` needs `--mem.preset=paranoid`. Detecting a write to
freed memory requires filling it with a pattern on every free, which the default
preset does not pay for — without it the check would pass by never looking.

## Mocks

`<testrixa/mock.h>` generates a test double for a virtual interface.

```cpp
#include <testrixa/mock.h>

struct Clock {
    virtual ~Clock() {}
    virtual std::uint64_t now() const = 0;
    virtual void sleep(int ms) = 0;
};

MOCK_BEGIN(MockClock, Clock)
    MOCK_CONST_METHOD(now,   std::uint64_t, (),       ())
    MOCK_METHOD      (sleep, void,          (int ms), (ms))
MOCK_END
```

```cpp
MockClock clock;
clock.nowMock.returns(1000);

Scheduler scheduler(&clock);
scheduler.tick(250);

CHECK_CALLED_TIMES(clock.nowMock, 1);
CHECK_LAST_CALL(clock.sleepMock, 250);
```

Each mocked method gets a `<method>Mock` member. Configure it with `returns`,
`returnsOnce` (a scripted sequence), or `does` (a body that sees the arguments);
question it with `calls()`, `lastCall()`, `unconfiguredCalls()` and the
`CHECK_CALLED*` macros.

The parameter list is written twice — `(int ms)` declares the override, `(ms)`
forwards it. The preprocessor cannot derive the second from the first without a
macro argument-counting library, and the duplication is checked by the compiler
on that line. It also means any number of parameters and that the parentheses
protect commas, so `(std::map<int,int> keys, int which)` passes through intact.

There is no `EXPECT_CALL`. Mocks record and the test asserts afterwards, the
same shape the memory suite uses. That keeps mocks free of any dependency on
the framework — the recorder, `mock::Method`, also works on its own against a
function-pointer seam. The cost is worth naming: forget to verify and nothing
tells you.

## Stress

Repetition — the test that passes locally and fails in CI every twentieth run.

```cpp
#include <testrixa/stress.h>

TESTCASE_STRESS(testThing) {
    TESTCASE_GROUP_START(repeat) {
        CHECK_STABLE([]() -> bool { return parse("1,2,3"); }, 10000);
        CHECK_STABLE_FOR([]() -> bool { return parse("x"); }, 250);   // ms

        // For code known to be unreliable: pins how unreliable it may be.
        CHECK_FAILURES_UNDER([]() -> bool { return parse("1"); }, 100, 5);
    } TESTCASE_GROUP_END(repeat)

    TESTCASE_RETURN
}
```

It does not stop at the first failure. "Failed once in ten thousand" is what
flakiness looks like, and a run that stops cannot tell it apart from "fails
every time". The first failing iteration is reported, because "it broke on run
6421" is something you can go and reproduce.

`--stress.scale=<percent>` shrinks every declared budget at once, never to zero:
a check that did not run is worse than a slow one.

## Concurrency

```cpp
#include <testrixa/thread.h>

CHECK_CONCURRENT(4, [&](int index) -> bool {
    for (int i = 0; i < 1000; ++i) queue.push(i);
    CHECK(queue.size() > 0);
    return true;
});
```

**This is not a race detector.** Detecting races properly means tracking
happens-before across every memory access, which ThreadSanitizer already does
well; build with `-fsanitize=thread` and run your concurrent tests under it.
What this gives you is the part TSan does not: a way to run something on several
threads, have every thread's assertions counted, and not lose the process when
one of them throws (an exception escaping a `std::thread` calls
`std::terminate`).

Bodies return `bool` and end with `return true;`. That is not style — `CHECK`
expands to `... return false;` on the failing path, so a body containing one
already returns `bool` whether it meant to or not.

## Running

```
./test.out                      every test case, basic + measure
./test.out -l                   list test cases
./test.out testAdd              one test case
./test.out -s                   do not stop at the first failure
./test.out -d                   detail: average, min, max
./test.out -t=30                measure loop count
./test.out --only=basic,memory  choose phases
./test.out --skip=measure       or drop them
./test.out --mem                the memory phase
./test.out --stress             the stress phase
./test.out --report-junit=out.xml
./test.out -h                   everything, including component options
```

Component options are namespaced — `--mem.*`, `--stress.*`, `--thread.*` — and
each component registers its own, so `-h` always lists what is actually there.
An unrecognised option is an error, never a silent no-op.

The exit code is non-zero when anything failed, when the arguments were wrong,
and when nothing ran at all — a test binary that matched no test case must not
report success.

### In CI

`--report-junit=<path>` writes JUnit XML: one `<testsuite>` per test case and
phase, one `<testcase>` per group, failures carrying the same text the console
shows. GitHub Actions, GitLab CI and Jenkins all read it directly.

Failing to write that file is an error, not a warning. A missing report reads to
CI as "nothing failed", and a silent false pass is the worst outcome available.

## What it does not do

The honest list, and the reason in each case.

- **It is not a race detector.** Use ThreadSanitizer; `<testrixa/thread.h>` is
  the harness you run under it.
- **It does not replace ASan/UBSan.** The memory suite catches leaks, double
  frees, overflow into its own redzones and writes to quarantined memory. It
  does not see stack overflows, uninitialised reads, or undefined behaviour.
  Run both.
- **Coverage and profiling are not here yet.** Planned, not written.
- **Mocks cover virtual interfaces and function-pointer seams.** Free functions
  with no seam need linker substitution, and non-virtual member functions need
  code generation; neither is in scope. Call-ordering expectations are not
  either.
- **C allocation is tracked only where `<testrixa/malloc_shim.h>` is included**,
  and never inside prebuilt libraries. The report says so every time it prints.
- **Tests run in one thread, one after another.** The whole suite here runs in
  under a second; parallel execution would be pure complexity.
- **Windows has no sanitizers and no `make` here.** The suite builds and passes
  under MSVC through CMake, but `ci/run.sh` — the sanitizers, the consumer
  checks, the GNU Makefile — is a POSIX shell script and does not run there.
  Backtraces are addresses without symbols: nothing calls `SymFromAddr` yet.

## Requirements

C++17. No dependencies beyond the standard library.

| | Tested |
|---|---|
| Compilers | gcc 12, clang 14, MSVC 19.51 — warning-free at `-Wall -Wextra -Werror` / `/W4 /WX` |
| Platforms | Linux (Debian bookworm), macOS, Windows Server 2025 |
| Sanitizers | ASan, UBSan, TSan — Linux only |
| Build paths | make and CMake on Linux/macOS; **CMake only on Windows** |

Leak backtraces need `<execinfo.h>` (glibc, macOS). Without it — musl, and
Windows — tracking and leak detection still work; only the captured stacks are
missing, and the leak report says so.

## Building testrixa itself

```sh
make check          # contracts, CLI regression, suite, memory phase, example
make test           # the suite only
make ci             # both compilers in Docker, CMake, sanitizers, consumers
```

or with CMake:

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

`make ci` is the gate. It runs everything under gcc and clang from a clean copy
of the tree, because the two disagree often enough that checking one proves
little — gcc has rejected code clang accepted silently, including a real
use-after-free.

What that covers today: 112 groups across 17 test suites, a 98-assertion
regression over the command line, two compile-only contract checks (the macro
prefix scheme, and that including testrixa puts nothing in the consumer's global
namespace), an installed-package consumer and a subdirectory consumer, all three
sanitizers, and both compilers.

## Contributing

Bug reports and pull requests are welcome. [CONTRIBUTING.md](CONTRIBUTING.md)
covers the contracts a change must not break, and why a new check has to be
proved capable of failing before it is trusted.

The reports worth the most are the ones the project cannot find by itself: a
check that stayed silent when it should have fired, or something that compiles
here and breaks in your build.

- [ROADMAP.md](ROADMAP.md) — what is planned, and what was declined with the reason
- [CHANGELOG.md](CHANGELOG.md) — notable changes
- [SECURITY.md](SECURITY.md) — private reporting, and the threat model of a
  library that replaces the global allocator
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)

## License

MIT — see [LICENSE](LICENSE).

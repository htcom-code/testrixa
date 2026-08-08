# 12. Coverage and Profiling

Two questions the pass/fail report does not answer: **what did the tests never
reach**, and **where did the time go**. Neither is measured by testrixa — the
compiler and the platform profilers already do that properly. What this page
covers is getting answers that are about *your* code.

- [Coverage](#coverage)
- [Why the filter matters](#why-the-filter-matters)
- [Without CMake](#without-cmake)
- [Coverage in CI](#coverage-in-ci)
- [Profiling](#profiling)

## Coverage

```cmake
add_executable(my_tests t.cpp)
target_link_libraries(my_tests PRIVATE testrixa::testrixa)

testrixa_add_coverage(my_tests)
```

```sh
cmake -B build
cmake --build build --target coverage
```

```
------------------------------------------------------------------------------
File                                       Lines    Exec  Cover   Missing
------------------------------------------------------------------------------
lib.h                                          5       5   100%
t.cpp                                          9       9   100%
------------------------------------------------------------------------------
TOTAL                                         14      14   100%
------------------------------------------------------------------------------
lines: 100.0% (14 out of 14)
functions: 100.0% (4 out of 4)
branches: 36.7% (36 out of 98)
```

`testrixa_add_coverage` adds `--coverage` to the target — compile **and** link,
since the instrumentation needs both — and creates a target that runs the tests
and reports through [gcovr](https://gcovr.com).

| | |
|---|---|
| `testrixa_add_coverage(tgt)` | target named `coverage` |
| `testrixa_add_coverage(tgt NAME cov)` | different target name |
| `testrixa_add_coverage(tgt EXCLUDE ".*/generated/.*")` | extra exclusions |

Requires **gcovr** (`pip install gcovr`, or your package manager). If it is not
found, CMake warns and creates no target — a coverage target that reports
nothing looks exactly like one that reported good news.

**MSVC is not supported.** gcov has no MSVC equivalent; the usual answer there
is [OpenCppCoverage](https://github.com/OpenCppCoverage/OpenCppCoverage), which
is a separate tool with its own invocation. The function says so and creates no
target rather than pretending.

## Why the filter matters

testrixa is header-only, so **every one of its headers is compiled into your
test binary** and the instrumentation counts them. Whether that reaches your
report depends on where testrixa lives:

| How you brought testrixa in | Naive `gcovr` |
|---|---|
| `make install`, or `-I` from outside the project | already correct — gcovr's root filter drops it |
| **vendored** (`third_party/testrixa`) | **counts testrixa's headers** |
| **FetchContent** (`build/_deps/…`) | same |

Measured on the 14-line example above, vendored and unfiltered:

```
third_party/testrixa/detail/core.hpp     ...
third_party/testrixa/detail/options.hpp   60   0   0%
TOTAL                                    973 353  36%
```

**36%, when the code under test is 100% covered.** The other 959 lines are
testrixa's internals — code you did not write, cannot change, and are not
testing.

Which layout you picked is not something you should have to think about at
report time, so `testrixa_add_coverage` excludes testrixa either way.

## Without CMake

The exclusion is one flag. Everything else is ordinary gcovr:

```sh
c++ -std=c++17 --coverage -O0 -g -I third_party -o my_tests t.cpp
./my_tests

gcovr --root . --exclude ".*/testrixa/.*" --print-summary
```

With clang, tell gcovr which gcov to use — `gcov` and `llvm-cov` are not
interchangeable, and a clang-built binary needs the latter:

```sh
gcovr --root . --gcov-executable "llvm-cov gcov" --exclude ".*/testrixa/.*"
```

## Coverage in CI

Coverage is **not** printed in testrixa's own console table, on purpose. A
number that is always on screen tends to become a target, and a suite tuned to
raise a percentage is not the same as a suite that checks more.

Treat it as a separate report:

```yaml
- name: coverage
  run: |
    cmake -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --target coverage
```

For a machine-readable artifact, gcovr writes Cobertura, JSON, HTML and more —
add the format flags to your own invocation rather than the helper's:

```sh
gcovr --root . --exclude ".*/testrixa/.*" --cobertura coverage.xml
```

⚠️ **A coverage gate is a policy, not a fact.** testrixa deliberately ships no
`CHECK_COVERAGE_ABOVE`. Coverage counts lines the tests *reached*, not lines the
tests *checked* — a run that executes everything and asserts nothing scores
100%. Use it to find what was never reached, and let the assertions say whether
the behaviour is right.

## Profiling

**testrixa does not profile, and will not.** The measure phase answers "how
long did this take"; "where did the time go inside it" is a sampling
profiler's job, and `perf`, Instruments and VTune do it well.

The two work together. Use the measure phase to find *what* is slow:

```cpp
TESTCASE_MEASURE(testParser) {
    TESTCASE_GROUP_START(speed) {
        CHECK_TIME_FUNCTION([] { return parse(sample); });
    } TESTCASE_GROUP_END(speed)

    TESTCASE_RETURN
}
```

```sh
./my_tests -m -d          # measure phase only, with min/max
```

then a profiler to find *where*:

```sh
# Linux
perf record -g ./my_tests -m -t=1000
perf report

# macOS
xcrun xctrace record --template 'Time Profiler' --launch ./my_tests -- -m -t=1000
```

Two things help when reading that profile:

- **Raise the loop count** (`-t=1000`) so the code under test dominates the
  samples rather than the runner's setup.
- **Run one test case** (`./my_tests testParser -m`) so the report is not
  spread across every benchmark in the suite.

The measure phase times a callable in isolation, so the numbers it prints are
already about your code. The profile is not filtered that way — testrixa's own
frames will appear in it, mostly around table formatting between measurements.

## Related

- [04. Running Tests](04-running-tests.md) — the measure phase, `-t`, `-d`
- [10. CI Integration](10-ci-integration.md)
- [Guide index](README.md)

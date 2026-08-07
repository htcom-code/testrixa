# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

While the version is `0.x`, the public API may change between minor releases.

## [Unreleased]

First public baseline of **testrixa** — a header-only C++17 testing platform
covering unit tests, benchmarks, memory and leak checking, stress, concurrency,
mocks, fixtures and reporting, with no dependencies beyond the standard library.

It grew out of a two-axis prototype (unit tests and benchmarks) that was
converted rather than copied: the layout, namespace, macro naming and build were
each changed in a separate step, with the output compared character by character
against a baseline at every one.

### Added

- **Unit tests and benchmarks.** Test cases register themselves — no `main()` to
  write and no list to keep up to date. Four phases per test case (basic,
  measure, memory, stress), the last two opt-in because both are slow by
  construction.
- **Memory Suite.** Global `operator new`/`delete` replacement with an intrusive
  block header, redzones, a live-block list, quarantine with pattern fill for
  use-after-free detection, peak tracking, and symbol-filtered backtraces.
  `CHECK_NO_LEAK` · `CHECK_LEAK_COUNT` · `CHECK_ALLOC_COUNT` · `CHECK_BALANCED`
  · `CHECK_PEAK_UNDER` · `CHECK_NO_OVERFLOW` · `CHECK_NO_DOUBLE_FREE` ·
  `CHECK_NO_USE_AFTER_FREE` · `CHECK_MEMORY_CLEAN`.
- **C allocation coverage** via `<testrixa/malloc_shim.h>`, which redefines
  `malloc`/`calloc`/`realloc`/`free`/`strdup`/`strndup` in the translation units
  that include it last.
- **Mock.** Macros generate a test double for a virtual interface; the recorder
  behind them, `mock::Method`, also stands alone against a function-pointer
  seam. Verify-after rather than expect-before, so a mock has no dependency on
  the framework.
- **Stress.** Repetition with a count or a time budget, counting failures
  instead of stopping at the first one, and reporting the first failing
  iteration.
- **Thread harness.** Runs a body on several threads with a start gate, counts
  each thread's assertions through a per-thread tally, and catches exceptions
  that would otherwise call `std::terminate`. It is not a race detector and says
  so in its first line.
- **Fixtures.** `TESTCASE_SETUP` / `TESTCASE_TEARDOWN` run around every phase.
- **Reporting.** Console tables per phase, a memory table that prints its own
  coverage, and JUnit XML via `--report-junit=<path>` for CI.
- **A namespaced option registry.** Components register their own
  `--<component>.<key>=<value>` options and appear in `-h` automatically;
  `main()` never has to be edited again. Unknown options are an error.
- **Build integration.** GNU make and CMake, an installable package with
  `find_package(testrixa)` support, and `add_subdirectory` that does not leak
  our tests and examples into a consumer's build.
- **`TESTRIXA_NO_SHORT_MACROS`** to keep only the `TRX_`-prefixed macro names.

### Fixed

Defects found and fixed during the conversion and the work that followed. Listed
because each one is a class of mistake the tests now pin.

- The exported CMake target carried no include directory, so every
  `find_package()` consumer failed —`include(GNUInstallDirs)` came after the
  target was described and `CMAKE_INSTALL_INCLUDEDIR` expanded to nothing.
- A global `using namespace std;` in a public header dropped all of `std` into
  every consumer's global namespace.
- Sanitizer flags never reached the link, so every sanitizer build failed with
  undefined references from the runtime.
- The option dispatcher claimed any argument containing a dot, so
  `--report-junit=/tmp/out.xml` was read as a component option and a working
  core flag failed.
- `CHECK` inside a lambda made the lambda return `bool`, so a harness body that
  fell off the end was undefined. Bodies now return `bool` explicitly.
- Test-case failures did not reach the process exit code.
- `-Wno-unused-parameter` in our own build hid two warnings in public headers
  that consumers building with `-Wextra` saw.
- Assertions on a mock's call count treated a `void` method with nothing
  configured as an unconfigured call, which would have made
  `CHECK_NO_UNSTUBBED_CALLS` unusable on most interfaces.
- `mock::Method::resetCalls()` left a `returnsOnce` queue spent, so a second
  phase ran against a mock configured differently from the first.

[Unreleased]: https://github.com/htcom-code/testrixa/commits/main

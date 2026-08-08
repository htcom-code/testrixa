# testrixa User Guide

Practical how-tos for developers using testrixa to test their own C or C++
code. The focus is "how do I do this".

New here? Start with [01. Installation](01-installation.md), then
[02. Getting Started](02-getting-started.md) — together they take you from an
empty machine to a test suite you can run.

Every command and code example in these guides was executed before it was
written down, on the toolchains listed in
[01](01-installation.md#verified-toolchains).

## Core

| # | Doc | Contents |
|---|---|---|
| 01 | [Installation](01-installation.md) | Prerequisites per OS · where the headers go · five ways to install · verifying it |
| 02 | [Getting Started](02-getting-started.md) | An empty directory to a passing test: the code, the test, watching it fail, several files, a Makefile |
| 03 | [Writing Tests](03-writing-tests.md) | Test-case anatomy · phases · groups · every assertion · fixtures · benchmarks · macro collisions |
| 04 | [Running Tests](04-running-tests.md) | The command line · phase selection · exit codes · component options · JUnit XML · sanitizers |

## Components

| # | Doc | Contents |
|---|---|---|
| 05 | [Memory and Leaks](05-memory.md) | The nine memory checks · the report object · C allocation via the shim · presets · finding where a leak came from |
| 06 | [Mock](06-mock.md) | Generating a double for a virtual interface · configuring · verifying · function-pointer seams · threads |
| 07 | [Stress](07-stress.md) | Repetition, counting failures rather than stopping at the first · time budgets · scaling in CI |
| 08 | [Concurrency](08-concurrency.md) | Running a body on N threads with assertions counted · why it is not a race detector |

## Integration

| # | Doc | Contents |
|---|---|---|
| 09 | [Build Integration](09-build-integration.md) | make · CMake (`find_package`, `add_subdirectory`, `FetchContent`) · Meson · Bazel · rules that apply to all |
| 10 | [CI Integration](10-ci-integration.md) | GitHub Actions · GitLab CI · Jenkins · JUnit reports · the mistakes that block merges |
| 11 | [Troubleshooting](11-troubleshooting.md) | Build and link errors · "nothing runs" · checks that seem wrong · platform notes |
| 12 | [Coverage and Profiling](12-coverage.md) | `testrixa_add_coverage` · why the filter matters · coverage in CI · using the measure phase with `perf`/Instruments |

## Where to look for what

| I want to… | |
|---|---|
| install it | [01](01-installation.md) |
| write my first test | [02](02-getting-started.md) |
| know every assertion available | [03](03-writing-tests.md#assertions) |
| find a memory leak | [05](05-memory.md) |
| find out *where* a leak came from | [05](05-memory.md#finding-where-a-leak-came-from) |
| replace a dependency in a test | [06](06-mock.md) |
| chase a flaky test | [07](07-stress.md) |
| test something concurrent | [08](08-concurrency.md) |
| put it in my CMake project | [09](09-build-integration.md#cmake) |
| get failures onto my pull requests | [10](10-ci-integration.md) |
| understand an error message | [11](11-troubleshooting.md) |
| find out what the tests never reached | [12](12-coverage.md) |
| profile the code under test | [12](12-coverage.md#profiling) |

## Related

- [README](../../README.md) — what testrixa is, and what it deliberately does not do
- [CONTRIBUTING.md](../../CONTRIBUTING.md) — changing testrixa itself
- [ROADMAP.md](../../ROADMAP.md) — what is planned, and what was declined

# Contributing to testrixa

Thanks for your interest in improving **testrixa**. This guide covers what the
project expects of a change and how to check one before you send it.

## Getting started

Header-only C++17. A compiler and `make` are enough; CMake is optional and only
needed for the consumer checks.

```sh
git clone https://github.com/htcom-code/testrixa
cd testrixa
make check      # contracts, CLI regression, suite, memory phase, example
```

`make check` covers only the compiler you happen to be running. Before opening a
PR, run the real gate:

```sh
make ci         # both compilers in Docker, CMake, all sanitizers, both consumers
```

That is the same `ci/run.sh` the workflow runs, from a clean copy of the tree —
a green run locally means a green run in CI. It needs Docker; without it, run
`make check` under both compilers and each sanitizer by hand:

```sh
make check CXX=g++ && make check CXX=clang++
make check SANITIZER=address && make check SANITIZER=undefined && make check SANITIZER=thread
```

## Where things live

| Header | Responsibility |
|---|---|
| `testrixa.h` | The macro API — the umbrella header. Every public macro is named here, twice. |
| `detail/core.hpp` | The runner: test-case registration, phases, groups, result tallies, the console tables, JUnit XML, fixtures. |
| `options.h` · `detail/options.hpp` | The `--<component>.<key>` registry components register into. |
| `memory.h` · `detail/memory/*` | Memory Suite. `allocator.hpp` replaces global new/delete, `block.hpp` is the intrusive header and redzones, `tracker.hpp` is the live list and quarantine, `platform.hpp` is everything OS-specific. |
| `malloc_shim.h` | C allocation coverage. Include **last** — it redefines `malloc` and friends as macros. |
| `mock.h` | Test doubles: the class-generating macros and `mock::Method`. |
| `stress.h` | Repetition. |
| `thread.h` | The concurrency harness. Not a detector. |
| `traits.h` | Compiler, standard and platform detection. |

Tests live in `tests/`, one file per area, and every test file is listed in both
`tests/Makefile` and `tests/CMakeLists.txt` — **both**, or one build silently
stops covering it.

## Contracts a change must not break

Each of these has a test that pins it, and each has been broken by accident at
least once. They are the promises a consumer relies on.

1. **Nothing new in the consumer's global namespace.** `core.hpp` once carried a
   global `using namespace std;` in a header-only library, so every consumer
   with their own `string` or `count` failed to compile — and none of our own
   tests could have noticed, because our sources are inside `namespace
   testrixa`. `tests/checkNamespace.cpp` pins it. **Adding a public header means
   adding it there too**; `stress.h` and `thread.h` were missing from that list
   for two merge requests, and the contract covered less than it looked like.
2. **Every macro exists twice.** `TRX_CHECK` is canonical and always defined;
   `CHECK` is a short alias that `TESTRIXA_NO_SHORT_MACROS` removes. Names like
   `CHECK` and `MOCK_METHOD` collide with other frameworks eventually, so the
   escape hatch has to stay complete. `tests/checkPrefix.cpp` pins it.
3. **No new warnings.** `-Wall -Wextra -Werror` under both gcc and clang. Do not
   suppress a warning in our own build to make it quiet: `-Wno-unused-parameter`
   once hid two warnings in public headers that consumers building with
   `-Wextra` saw and we did not.
4. **Unknown input is rejected, never ignored.** An option that silently does
   nothing is the worst outcome available — the run looks fine and checks
   nothing.
5. **Failure reaches the exit code.** Failures, a bad command line, and *nothing
   having run at all* are all non-zero.
6. **The report says what it does not cover.** Every memory table prints its own
   coverage line. If a change alters what is tracked, that line changes with it.
7. **Verify after, do not expect before.** Every component hands back a result
   object the test asserts on — `measure() → Report`, `repeat() → Result`,
   `Method`. A new component follows the same shape rather than reporting
   failures from a destructor, which would need a global registry.

## Writing a check

A testing tool's own failure mode is a check that watches nothing. It reports
zero problems, exactly like a check that works.

**So prove it can fail.** Break the thing it guards on purpose, watch the check
fire, then put it back — and say so in the PR. The project has done this twice
and both times it was the only thing that made the result mean anything:

- The global `using namespace std;` fix was verified by compiling a consumer
  against the *pre-fix* headers and seeing 5 errors.
- `mock::Method`'s thread safety was verified by removing its lock, watching
  ThreadSanitizer report 16 data races, and restoring it.

Assertions belong where the reader is looking, and a group name should say what
is true rather than what is exercised — `A_BODY_RETURNING_FALSE_IS_A_FAILURE`,
not `test_body_false`.

Avoid assertions that depend on which phase is running. "setUp ran 3 times"
passed on a full run and failed under `--mem`; the invariant
`setUpCalls - tearDownCalls == 1` holds either way.

## Documentation

Update the README when the public API, the command line or the support matrix
changes, and add an entry to `CHANGELOG.md` under `[Unreleased]`.

**Compile every example before writing it down.** The README's examples were all
built and run under both compilers with `-Wall -Wextra -Werror` first, and that
caught four wrong ones — including a macro that did not exist. Documentation
whose examples do not compile is worse than none: it costs the reader the time
it takes to find out.

Documentation is written in English. Internal design notes under `docs/` may be
Korean.

## Commits, branches and PRs

- **Commits** follow Conventional Commits: `type(scope): subject` (`feat`,
  `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `perf`, `ci`). The
  subject is imperative, lower case, no trailing period, 50 characters or fewer.
  The body says **why**.
- **Branches** are `<type>/<short-description>` in kebab-case.
- **`main` is the only integration branch.** Fork, branch off `main`, and open
  the PR against `main`. Direct pushes are blocked; the PR has to satisfy the
  template checklist and the `ci-complete` check.

## License

By contributing, you agree that your contributions are licensed under the
project's [MIT License](LICENSE).

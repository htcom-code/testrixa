# 11. Troubleshooting

Errors people actually hit, and what each one means.

- [Build errors](#build-errors)
- [Link errors](#link-errors)
- [Nothing runs](#nothing-runs)
- [Checks that seem wrong](#checks-that-seem-wrong)
- [Platform-specific](#platform-specific)
- [Still stuck](#still-stuck)

## Build errors

### `fatal error: testrixa/testrixa.h: No such file or directory`

The include path points at the wrong level. Every include is written
`#include <testrixa/…>`, so what goes on the path is the **parent** of the
`testrixa/` directory:

```sh
c++ -I/usr/local/include ...        # correct — headers are in .../include/testrixa/
c++ -I/usr/local/include/testrixa   # wrong
```

Check what is actually there:

```sh
ls /usr/local/include/testrixa/testrixa.h
```

### `use of undeclared identifier 'CHECK'` / `'CHECK' was not declared` — after defining `TESTRIXA_NO_SHORT_MACROS`

That is what the macro does. Use the canonical names: `TRX_CHECK`,
`TRX_TESTCASE_BEGIN`, `TRX_CHECK_SAME`, and so on. They are complete — a whole
test case can be written with them.

### `error: 'MOCK_METHOD' macro redefined`

gmock defines it too. Turn off the short aliases in that translation unit:

```cpp
#define TESTRIXA_NO_SHORT_MACROS
#include <testrixa/testrixa.h>
#include <testrixa/mock.h>
```

### `error: comparison of integer expressions of different signedness`

`CHECK_SAME(v.size(), 3)` compares `size_t` against `int`. Cast the literal:

```cpp
CHECK_SAME(v.size(), (std::size_t)3);
```

### `too many arguments provided to function-like macro invocation` (clang)<br>`macro "MOCK_METHOD" passed 5 arguments, but takes just 4` (gcc)

A comma inside a **return type** split the macro argument. Parameter lists are
protected by their parentheses; return types are not:

```cpp
// breaks
MOCK_METHOD(all, std::map<int,int>, (), ())

// works
typedef std::map<int,int> IntMap;
MOCK_METHOD(all, IntMap, (), ())
```

The same applies to an interface named with template arguments in
`MOCK_BEGIN`.

### Errors inside `<string>`, `<vector>` or another standard header

Almost always `<testrixa/malloc_shim.h>` included too early. It redefines
`malloc` and friends as macros, so **everything included after it gets
rewritten** — including standard headers.

```cpp
#include <testrixa/testrixa.h>
#include <testrixa/memory.h>
#include "thing_under_test.h"
#include <string>

#include <testrixa/malloc_shim.h>   // last. always last.
```

### `error: unexpected ';' before '}'` on a `CHECK_NO_LEAK` line

`CHECK_NO_LEAK` and friends take a **callable**, not a block. Passing a block
confuses the preprocessor and the diagnostic points at punctuation rather than
at the real problem:

```cpp
CHECK_NO_LEAK([] { delete new int(1); });   // yes
CHECK_NO_LEAK({ delete new int(1); });      // no
```

### `error: control reaches end of non-void function` in a stress or concurrency body

`CHECK` expands to `... return false;`, so a body containing one returns `bool`.
Declare it and end with `return true;`:

```cpp
CHECK_CONCURRENT(4, [&](int) -> bool {
    CHECK(queue.size() > 0);
    return true;
});
```

### `static assertion failed … returns() on a method that returns void`

Use `does()`. There is nothing to return.

### `arguments of this method are not recorded`

One of the parameters is not copy-constructible — `std::unique_ptr`, for
instance — so the mock counts calls but cannot store arguments. `calls()` and
`called()` still work; `lastCall()` cannot. Check with
`decltype(m.fooMock)::recordsArguments`.

## Link errors

### `undefined reference to '__asan_report_load4'` (or `__ubsan_…`, `__tsan_…`)

The sanitizer flag reached the compile but not the link. It has to be on both:

```sh
c++ -std=c++17 -fsanitize=address -c -o test.o test.cpp
c++ -fsanitize=address -o test.out test.o        # here too
```

### `duplicate symbol '_main'`

`TEST_RUN_TERM` is defined in more than one translation unit. It goes in exactly
one.

### `undefined reference to 'pthread_create'`

Add `-pthread` to the link. Needed when anything includes
`<testrixa/thread.h>`.

### `undefined reference to 'main'`

The opposite: no translation unit defines `TEST_RUN_TERM`.

## Nothing runs

### `Test case None`, or a test case you wrote is missing from `-l`

Three causes, in order of likelihood:

1. **The file is not in the build.** It compiles in your editor and is absent
   from the source list. `-l` shows what actually registered.
2. **Stale object files.** Your build has no header dependencies, a header
   changed, and the old objects were kept. `make clean` and rebuild — if that
   fixes it, add the dependencies (see
   [09. Build Integration](09-build-integration.md)). This exact symptom cost
   real time in this repository once, and looked like a framework bug.
3. **`TESTCASE_BEGIN` and `TESTCASE_END` names do not match.**

### The memory or stress phase does nothing

Both are opt-in:

```sh
./testAll.out --mem
./testAll.out --stress
./testAll.out --only=basic,measure,memory,stress
```

A plain run never executes them. This is the single most common "the leak check
isn't working" report.

### Exit code 1 with no visible failure

Three things produce it: a failure, a bad command line, and **no test case
having run at all**. The last one usually means a mistyped test-case name:

```sh
./testAll.out testStat     # typo — matches nothing
echo $?                    # 1
```

## Checks that seem wrong

### `CHECK_NO_USE_AFTER_FREE` never fails

It needs the paranoid preset:

```sh
./testAll.out --mem --mem.preset=paranoid
```

Detecting a write to freed memory means filling freed blocks with a pattern,
which the default preset does not pay for. Without it the check passes by never
looking.

### A leak in a library I link against is not reported

It cannot be. That library was compiled without testrixa's headers, so its
allocations never go through the replaced `operator new`. The coverage line
above every memory table says this on every run.

### `malloc` in my C code is not tracked

Only translation units that include `<testrixa/malloc_shim.h>` are covered, and
it must be included last. Coverage is per file.

### The leak report says "origin not recorded"

Backtraces are off by default because they cost more than every other check
combined:

```sh
./testAll.out --mem --mem.backtrace=16
```

If it says the platform *cannot capture* backtraces instead, you are on a build
with no `<execinfo.h>` — musl, typically. Leak detection still works; only the
stacks are unavailable.

### A benchmark got slower and nothing failed

Timings are measurements, not assertions. Nothing fails because a number moved.
To gate on it, read the value and assert yourself.

### Assertions inside a thread body do not appear

They do — each thread keeps its own tally and they are merged after the join.
If the count looks wrong, check that the body returns `true` on the path you
expect; a body that returned `false` early stopped before the rest of its
assertions.

## Platform-specific

### macOS: `.dSYM` directories appearing

clang writes them when you build with `-g`, which sanitizer builds do. Harmless;
add `rm -rf *.dSYM` to your `clean` target and `*.dSYM/` to `.gitignore`.

### Alpine / musl: no backtraces

musl ships no `<execinfo.h>`. Everything else works; the leak report says so
explicitly rather than leaving you to guess.

### Windows: no sanitizer, no `make`

Both are real limits rather than bugs. `ci/run.sh` and the GNU Makefile are
POSIX shell scripts; **CMake is the only build path on Windows**, and the
sanitizers are run on Linux only. The suite itself builds and passes under MSVC.

### Windows: `--mem.backtrace` prints addresses, not names

Expected. `CaptureStackBackTrace` collects the frames but nothing resolves them
to symbols yet. Leak detection is unaffected — only the origin lines are raw
addresses.

### Windows: `ctest` skips the CLI regression

`cli_test.sh` is a shell script and runs through the bash that Git for Windows
installs. If CMake warned that it found no bash, install Git for Windows or run
it by hand: `bash tests/cli_test.sh <path-to-runner>`.

### `ctest` runs no memory tests

CTest sees one test per `add_test`. Register the phases separately:

```cmake
add_test(NAME unit   COMMAND mylib_tests)
add_test(NAME memory COMMAND mylib_tests --mem)
```

## Still stuck

Open an issue with a **minimal reproducer** — a single self-contained `.cpp`
that can be compiled — plus your compiler version, `-std=`, warning flags, and
OS.

The two most valuable reports are:

- **A check that stayed silent.** A leak, overflow, double free or mock call
  that should have been reported and was not. The suite looks green either way,
  so this class of bug is invisible from the inside.
- **Something that compiles here and breaks in your build** — a name leaked
  into your global namespace, a macro collision, a warning under your flags.

## Related

- [Guide index](README.md)
- [README](../../README.md#what-it-does-not-do)

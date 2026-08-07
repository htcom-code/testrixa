# 05. Memory and Leaks

`<testrixa/memory.h>` replaces the global allocator for the duration of a scope
and reports what happened inside it.

- [The shortest version](#the-shortest-version)
- [What it sees](#what-it-sees)
- [The checks](#the-checks)
- [The report object](#the-report-object)
- [C allocation](#c-allocation)
- [Presets and tuning](#presets-and-tuning)
- [Finding where a leak came from](#finding-where-a-leak-came-from)
- [What it does not catch](#what-it-does-not-catch)

## The shortest version

```cpp
#include <testrixa/memory.h>
#include <testrixa/testrixa.h>

TESTCASE_MEMORY(testThing) {
    TESTCASE_GROUP_START(NOTHING_LEAKS) {
        CHECK_NO_LEAK([] {
            int* values = new int[32];
            delete[] values;
        });
    } TESTCASE_GROUP_END(NOTHING_LEAKS)

    TESTCASE_RETURN
}
```

```sh
./testAll.out --mem
```

**The memory phase is opt-in.** Without `--mem` or `--only=memory` the body
above never runs. If a leak check seems to be doing nothing, that is the first
thing to check.

The argument is a **callable**, not a block — `[] { … }`, not `{ … }`.

## What it sees

Printed above every memory table, on every run:

```
coverage: C++ new/delete = full | C malloc = only where <testrixa/malloc_shim.h>
is included | prebuilt libraries = not tracked
```

| | |
|---|---|
| `new` / `delete`, all 20 forms | tracked, always |
| `malloc` / `free` / `realloc` / `calloc` / `strdup` / `strndup` | only in translation units that include `<testrixa/malloc_shim.h>` |
| Allocation inside a library you link against | **not tracked** — it was compiled without our headers |

The third row is the one that surprises people. If the code under test lives in
a prebuilt `.a` or `.so`, its allocations are invisible here.

## The checks

Every one takes a callable and an optional message.

| | Asserts |
|---|---|
| `CHECK_NO_LEAK(body)` | nothing allocated inside was still live at the end |
| `CHECK_LEAK_COUNT(body, n)` | exactly `n` blocks leaked — for a leak you mean |
| `CHECK_ALLOC_COUNT(body, n)` | exactly `n` allocations happened |
| `CHECK_BALANCED(body)` | allocations and frees matched |
| `CHECK_PEAK_UNDER(body, bytes)` | peak live bytes stayed under a limit |
| `CHECK_NO_OVERFLOW(body)` | nothing wrote past an allocation into its redzone |
| `CHECK_NO_DOUBLE_FREE(body)` | nothing was freed twice |
| `CHECK_NO_USE_AFTER_FREE(body)` | nothing wrote to freed memory — **needs `--mem.preset=paranoid`** |
| `CHECK_MEMORY_CLEAN(body)` | all of the above at once |

```cpp
// a leak you know about and want pinned
int* leaked = nullptr;
CHECK_LEAK_COUNT([&] { leaked = new int(9); }, 1);
delete leaked;

// a budget
CHECK_PEAK_UNDER([] {
    std::vector<int> v(100);
}, 4096);
```

⚠️ `CHECK_NO_USE_AFTER_FREE` needs the paranoid preset. Detecting a write to
freed memory means filling it with a pattern on every free, which the default
does not pay for — **without the preset the check passes by never looking.**

## The report object

When one assertion is not enough:

```cpp
const testrixa::memory::Report report = MEMORY_REPORT([] {
    int* a = new int(1);
    delete a;
    (void)new int(2);          // leaked on purpose
});

CHECK_SAME(report.allocations,  (std::size_t)2);
CHECK_SAME(report.frees,        (std::size_t)1);
CHECK_SAME(report.leakedBlocks, (std::size_t)1);
CHECK(report.peakBytes >= 4);
CHECK_FALSE(report.clean());
```

| Field | |
|---|---|
| `allocations` · `frees` | counts inside the scope |
| `leakedBlocks` · `leakedBytes` | still live when it ended |
| `peakBytes` · `largestBlock` | high-water mark, and the biggest single block |
| `overflows` · `doubleFrees` · `useAfterFrees` | fault counts |
| `clean()` | no leaks, no double frees, no overflows, no use-after-free |

All of them are `std::size_t`, so compare against a cast literal:
`CHECK_SAME(report.frees, (std::size_t)1)`.

This is the same shape the rest of the platform uses — the tool records, the
test asserts. Nothing fails on its own.

## C allocation

To cover `malloc` and friends, include the shim **last** in that translation
unit:

```cpp
#include <testrixa/testrixa.h>
#include <testrixa/memory.h>
#include "thing_under_test.h"

#include <testrixa/malloc_shim.h>   // last, always
```

It redefines `malloc`, `calloc`, `realloc`, `free`, `strdup` and `strndup` as
macros, so anything included **after** it gets rewritten too — including
standard headers, which is how you get errors that make no sense.

Coverage is per translation unit. A `.c` or `.cpp` file compiled without the
shim keeps allocating invisibly, and the coverage line stays honest about it.

## Presets and tuning

```sh
./testAll.out --mem --mem.preset=paranoid
```

| Preset | Runs |
|---|---|
| `fast` | tracking and leak detection only |
| `default` | + redzones, double-free, invalid-free, peak |
| `paranoid` | + pattern fill (use-after-free detection) |

| Option | Default | |
|---|---|---|
| `--mem.redzone=<bytes>` | 32 | Guard bytes each side of an allocation. `0` disables overflow detection. |
| `--mem.quarantine=<MB>` | 8 | Freed memory held back rather than returned. `0` weakens double-free detection. |
| `--mem.backtrace=<frames>` | 0 | Frames recorded per allocation. **Costs more than every other check combined.** |

In code, per scope:

```cpp
testrixa::memory::Config config;
config.checks |= (std::uint32_t)testrixa::memory::CheckBacktrace;
config.backtraceFrames = 16;

const auto report = testrixa::memory::measure([] { /* … */ }, config);
```

## Finding where a leak came from

Backtraces are off by default because they cost more than everything else. Turn
them on once you know there is a leak:

```sh
./testAll.out --mem --mem.backtrace=16
```

```
  leak #1  4 bytes  age=0ms
      myLib::Parser::parse(std::string const&)  parser.cpp:88
      main                                      main.cpp:12
```

The allocator's own frames are dropped by symbol rather than by a fixed count —
a fixed skip is wrong at some optimisation level, and this was calibrated at
`-O0`, `-O1` and `-O2` before it was believed.

Without capture, the report says so and says how to change it:

```
  leak #1  4 bytes  age=0ms  (origin not recorded)
  rerun with --mem.backtrace=16 to record where these came from
```

On a platform with no backtrace support at all — musl, for instance, which
ships no `<execinfo.h>` — it says that instead, rather than leaving you to
wonder what you configured wrongly:

```
  this platform cannot capture backtraces, so origins are unavailable
  however --mem.backtrace is set
```

## What it does not catch

Run a sanitizer too. These are different questions, not competing answers.

| Not caught here | Use |
|---|---|
| Reads or writes out of bounds on the stack or globals | AddressSanitizer |
| Use of uninitialised memory | MemorySanitizer / valgrind |
| Undefined behaviour generally | UndefinedBehaviorSanitizer |
| Data races | ThreadSanitizer |
| Allocation inside prebuilt libraries | nothing here — recompile with the headers |

ASan and `--mem` run together happily; testrixa's own CI does exactly that on
every commit.

## Next

- [06. Mock](06-mock.md)
- [04. Running Tests](04-running-tests.md) — the `--mem.*` options in context

## Related

- [Guide index](README.md)
- [SECURITY.md](../../SECURITY.md) — why this belongs in test binaries only

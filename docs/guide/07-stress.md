# 07. Stress

Repetition — the test that passes locally and fails in CI every twentieth run.

## What "stress" means here

Three different things get called stress testing, and they are three different
components:

| | |
|---|---|
| **Repetition** | run the same thing until it breaks or the budget runs out → **this guide** |
| **Load** | many threads at once → [08. Concurrency](08-concurrency.md) |
| **Resource pressure** | allocation failure, low memory → install a failing allocator through [`<testrixa/memory.h>`](05-memory.md) |

Repetition is the one nothing else covers, and the one that hurts most often.

## Using it

```cpp
#include <testrixa/stress.h>

TESTCASE_STRESS(testParser) {
    TESTCASE_GROUP_START(PARSING_IS_STABLE) {
        CHECK_STABLE([]() -> bool { return parse("1,2,3") == 3; }, 10000);
        CHECK_STABLE_FOR([]() -> bool { return tick(); }, 250);   // milliseconds
    } TESTCASE_GROUP_END(PARSING_IS_STABLE)

    TESTCASE_RETURN
}
```

```sh
./testAll.out --stress
```

**The stress phase is opt-in**, like memory. Repetition is slow by construction
and a plain run must not pay for it.

| | |
|---|---|
| `CHECK_STABLE(body, iterations)` | run it `n` times, fail if any run failed |
| `CHECK_STABLE_FOR(body, ms)` | run it for a time budget instead |
| `CHECK_FAILURES_UNDER(body, iterations, limit)` | for code known to be unreliable: pins how unreliable it may be |
| `STRESS_REPEAT(body, n)` / `STRESS_REPEAT_FOR(body, ms)` | return the `Result` instead of asserting |

### The body

Returns `bool` and takes no arguments. It must end with `return true;`.

That is not style. `CHECK` expands to `... return false;` on the failing path,
so a body containing one already returns `bool` whether it meant to or not, and
falling off the end of it is undefined. Spelling the return type out makes the
compiler enforce what the macro already assumes.

**Do not put assertions in a stress body.** A `CHECK` inside something that runs
ten thousand times floods the report with the same line, and the failure count
is the answer anyway.

## It does not stop at the first failure

"Failed once in ten thousand" *is* what flakiness looks like, and a run that
stops at the first failure cannot tell it apart from "fails every time".

```cpp
const auto result = STRESS_REPEAT([&]() -> bool { return flaky(); }, 1000);

CHECK_SAME(result.iterations, (std::size_t)1000);
CHECK_SAME(result.failures,   (std::size_t)10);      // one in a hundred
CHECK_SAME(result.firstFailure, (std::size_t)100);   // and it broke on run 100
```

`firstFailure` is 1-based and `0` when nothing failed. "It broke on run 6421" is
something you can go and reproduce.

### `Result`

| | |
|---|---|
| `iterations` | how many ran |
| `failures` | how many returned false |
| `firstFailure` | the first failing iteration, 1-based; `0` if none |
| `minNanos` · `maxNanos` · `totalNanos` · `meanNanos()` | timing |
| `stable()` | no failures **and** at least one iteration ran |

`stable()` is false for an empty result on purpose. Zero iterations proves
nothing, and reporting that as stable would be the most misleading answer
available.

## Options

| | Default | |
|---|---|---|
| `--stress.scale=<percent>` | 100 | Shrink every declared budget at once. `10` runs a tenth. |
| `--stress.stop-after=<n>` | 0 | Give up after this many failures. `0` runs the whole budget. |

```sh
./testAll.out --stress --stress.scale=10     # a quick pass
./testAll.out --stress                       # the real thing
```

`--stress.scale` **never scales a budget to zero**. A check that did not run is
worse than a slow one, so the minimum is always one iteration.

That makes it useful in CI: declare honest budgets in the tests, run them at
`--stress.scale=5` on every commit and at 100 nightly.

## Next

- [08. Concurrency](08-concurrency.md)

## Related

- [Guide index](README.md)

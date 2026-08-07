# 08. Concurrency

`<testrixa/thread.h>` runs a body on several threads and counts what every one
of them asserted.

## Read this first: it is not a race detector

Detecting a data race properly means tracking happens-before edges across every
memory access. **ThreadSanitizer already does that, and does it well.** Writing
a second one here would be a worse copy.

```sh
c++ -std=c++17 -fsanitize=thread -fno-omit-frame-pointer -g -O1 \
    -o testAll.out testConcurrent.cpp
./testAll.out
```

What this header provides is the part TSan does not: a way to *run* something
concurrently, get every thread's assertions counted, and not lose the process
when one of them throws.

## Using it

```cpp
#include <testrixa/thread.h>

TESTCASE_GROUP_START(CONCURRENT_PUSH) {
    Queue queue;

    CHECK_CONCURRENT(4, [&](int index) -> bool {
        for (int i = 0; i < 1000; ++i) queue.push(index * 1000 + i);
        CHECK(queue.size() > 0);
        return true;
    });

    CHECK_SAME(queue.size(), (std::size_t)4000);
} TESTCASE_GROUP_END(CONCURRENT_PUSH)
```

| | |
|---|---|
| `CHECK_CONCURRENT(threads, body)` | passes when every worker returned true and none threw |
| `CONCURRENT(threads, body)` | returns the `Result` instead of asserting |

The body takes the thread index and returns `bool`, ending with `return true;`
— same rule and same reason as [stress](07-stress.md): `CHECK` expands to
`... return false;`, so the body already returns `bool`.

Assertions inside the body **work and are counted**. Each thread accumulates its
own tally and they are merged after the join.

### `Result`

| | |
|---|---|
| `threads` | how many were started |
| `failed` | bodies that returned false |
| `exceptions` | bodies that threw |
| `firstError` | the first exception's message |
| `clean()` | none failed and none threw |

```cpp
const auto result = CONCURRENT(3, [](int index) -> bool {
    if (index == 1) throw std::runtime_error("worker exploded");
    return true;
});

CHECK_SAME(result.exceptions, (std::size_t)1);
CHECK_SAME(result.firstError, std::string("worker exploded"));
```

## Three things it handles that a raw `std::thread` does not

**Assertions from workers are counted.** `add_result()` touches the group stack
with no lock; before per-thread tallies existed, calling `CHECK` from four
threads was a data race *inside the tool meant to find data races*. Each thread
now has its own tally, merged after the join — no lock on the hot path, so
benchmark numbers do not quietly become a mutex's numbers.

**An exception does not kill the process.** An exception escaping a
`std::thread` calls `std::terminate`: the process dies and takes the report with
it. Here it is caught, counted, and its message reaches the result.

**Workers actually overlap.** A start gate holds every thread until all of them
have arrived. Without one, the first thread is usually finished before the last
one starts, and a test that "runs on eight threads" never overlaps at all.

## Options

| | Default | |
|---|---|---|
| `--thread.join-timeout=<ms>` | 10000 | How long to wait for a worker before reporting a hang. `0` waits forever. |

## Zero threads is a no-op, not a hang

```cpp
const auto result = CONCURRENT(0, [&](int) -> bool { return true; });
CHECK(result.clean());
CHECK_SAME(result.threads, (std::size_t)0);
```

Useful when the thread count comes from a parameter.

## Mocks are safe here

`mock::Method` is thread-safe, so a mock can be handed to concurrent code and
its counts will be right. See [06. Mock](06-mock.md#threads).

## Next

- [09. Build Integration](09-build-integration.md)

## Related

- [Guide index](README.md)
- [04. Running Tests](04-running-tests.md#sanitizers) — building with TSan

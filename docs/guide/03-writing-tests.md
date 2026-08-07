# 03. Writing Tests

The anatomy of a test case, every assertion, and the fixture hooks.

- [Anatomy](#anatomy)
- [Phases](#phases)
- [Groups](#groups)
- [Assertions](#assertions)
- [Fixtures](#fixtures)
- [Benchmarks](#benchmarks)
- [Macro names and collisions](#macro-names-and-collisions)
- [Conventions that hold up](#conventions-that-hold-up)

## Anatomy

```cpp
#define TEST_RUN_TERM            // one file in the program, no more
#include <testrixa/testrixa.h>

TESTCASE_BEGIN(testThing)        // opens a class

int m_counter = 0;               // members here are the fixture's state

TESTCASE_SETUP(testThing)    { m_counter = 0; return true; }
TESTCASE_TEARDOWN(testThing) { return true; }

TESTCASE_BASIC(testThing) {
    TESTCASE_GROUP_START(WHAT_SHOULD_BE_TRUE) {
        CHECK(true);
    } TESTCASE_GROUP_END(WHAT_SHOULD_BE_TRUE)

    TESTCASE_RETURN              // required, closes the phase
}

TESTCASE_MEASURE(testThing) {
    TESTCASE_RETURN
}

TESTCASE_END(testThing)          // closes the class
```

`TESTCASE_BEGIN` and `TESTCASE_END` must pair, and the name must match
everywhere. The class registers itself at static-initialisation time by pushing
onto a linked list, so there is no registry to edit and no `main()` to touch.

`TESTCASE_RETURN` closes each phase body. Leaving it out is a compile error,
not a silent no-op.

## Phases

A test case has up to four phases. `BASIC` and `MEASURE` must both be present,
even if one is empty; `MEMORY` and `STRESS` are optional.

| Phase | Macro | When it runs |
|---|---|---|
| Basic | `TESTCASE_BASIC` | always |
| Measure | `TESTCASE_MEASURE` | always |
| Memory | `TESTCASE_MEMORY` | only with `--mem` or `--only=memory` |
| Stress | `TESTCASE_STRESS` | only with `--stress` or `--only=stress` |

Memory and stress are opt-in because both are slow by construction. A plain run
must not pay for them, or people stop running it.

**Each phase is a separate run of the code under test**, with `setUp` and
`tearDown` around it. State does not carry from one phase to the next.

## Groups

A group is one row in the report and the unit that failures are counted
against.

```cpp
TESTCASE_GROUP_START(EMPTY_INPUT_IS_NOT_AN_ERROR) {
    CHECK_SAME(parse(""), 0);
} TESTCASE_GROUP_END(EMPTY_INPUT_IS_NOT_AN_ERROR)
```

The name is not quoted and both ends must match. Groups can nest — the report
shows the depth in the `Lvl` column.

Name a group after **what should be true**, not what the code does.
`EMPTY_INPUT_IS_NOT_AN_ERROR` tells you what broke when it fails;
`test_parse_2` does not.

## Assertions

Every one takes an optional trailing message, and every one has a `TRX_`-prefixed
twin.

### Truth

| | |
|---|---|
| `CHECK(expr)` | expression is true |
| `CHECK_TRUE(expr)` | same thing, spelled out |
| `CHECK_FALSE(expr)` | expression is false |

### Equality

| | |
|---|---|
| `CHECK_SAME(a, b)` | `a == b` |
| `CHECK_SAME_TRUE(a, b)` | same thing, spelled out |
| `CHECK_SAME_FALSE(a, b)` | `a != b` |

`CHECK_SAME` prints both values on failure, which `CHECK(a == b)` cannot.
Prefer it whenever you are comparing.

⚠️ **Signed/unsigned comparison.** `CHECK_SAME(v.size(), 3)` compares
`size_t` against `int`, which `-Wsign-compare` rejects under `-Werror`. Cast the
literal:

```cpp
CHECK_SAME(v.size(), (std::size_t)3);
```

### Raw memory

```cpp
char lhs[4] = {'a','b','c','\0'};
char rhs[4] = {'a','b','c','\0'};
CHECK_MEM_SAME(lhs, rhs, sizeof(lhs));
```

`CHECK_MEM_SAME_TRUE` and `CHECK_MEM_SAME_FALSE` also exist.

### With a message

```cpp
CHECK_SAME(parse("1,2"), 2, "comma-separated pairs");
```

### From other components

Each has its own guide.

| Header | Assertions |
|---|---|
| `<testrixa/memory.h>` | `CHECK_NO_LEAK` · `CHECK_LEAK_COUNT` · `CHECK_ALLOC_COUNT` · `CHECK_BALANCED` · `CHECK_PEAK_UNDER` · `CHECK_NO_OVERFLOW` · `CHECK_NO_DOUBLE_FREE` · `CHECK_NO_USE_AFTER_FREE` · `CHECK_MEMORY_CLEAN` → [05](05-memory.md) |
| `<testrixa/mock.h>` | `CHECK_CALLED` · `CHECK_NOT_CALLED` · `CHECK_CALLED_TIMES` · `CHECK_LAST_CALL` · `CHECK_NO_UNSTUBBED_CALLS` → [06](06-mock.md) |
| `<testrixa/stress.h>` | `CHECK_STABLE` · `CHECK_STABLE_FOR` · `CHECK_FAILURES_UNDER` → [07](07-stress.md) |
| `<testrixa/thread.h>` | `CHECK_CONCURRENT` → [08](08-concurrency.md) |

## Fixtures

Members declared after `TESTCASE_BEGIN` are the fixture's state. `TESTCASE_SETUP`
and `TESTCASE_TEARDOWN` run around **every phase**, and both return `bool`.

```cpp
TESTCASE_BEGIN(testDatabase)

Connection* m_connection = nullptr;

TESTCASE_SETUP(testDatabase) {
    m_connection = Connection::open(":memory:");
    return m_connection != nullptr;      // false aborts the phase
}

TESTCASE_TEARDOWN(testDatabase) {
    delete m_connection;
    m_connection = nullptr;
    return true;
}

TESTCASE_BASIC(testDatabase) {
    TESTCASE_GROUP_START(A_FRESH_CONNECTION_IS_EMPTY) {
        CHECK_SAME(m_connection->rowCount(), 0);
    } TESTCASE_GROUP_END(A_FRESH_CONNECTION_IS_EMPTY)

    TESTCASE_RETURN
}
```

Returning `false` from `setUp` stops that phase — use it when the phase cannot
run meaningfully, rather than letting every assertion fail one by one.

⚠️ **Do not assert on how many times the hooks ran.** "setUp ran 3 times" is
true on a full run and false under `--mem`, because the number of phases
changed. Assert the invariant instead:

```cpp
CHECK_SAME(m_setUpCalls - m_tearDownCalls, 1);   // holds in every phase
```

That mistake was made here and caught by the CLI regression, not by the suite.

## Benchmarks

The measure phase times things. It runs a loop (100 iterations by default,
`-t=N` to change) and reports the average.

```cpp
TESTCASE_MEASURE(testStats) {
    TESTCASE_GROUP_START(HOW_FAST_IS_SUM) {
        CHECK_TIME_FUNCTION([] { return sum({1, 2, 3}); });
    } TESTCASE_GROUP_END(HOW_FAST_IS_SUM)

    TESTCASE_RETURN
}
```

| | |
|---|---|
| `CHECK_TIME_FUNCTION(fn, args...)` | time a callable |
| `CHECK_TIME_FUNCTION_DESC(desc, fn, args...)` | …with a label instead of the expression text |
| `CHECK_TIME_OBJECT(&T::method, obj, args...)` | time a member function |
| `CHECK_TIME_FUNCTION_FORCE(desc, times, fn, ...)` | override the loop count for this one |

Each returns a tuple of `(ok, result)`; `TUPLE_FIRST` and `TUPLE_SECOND` read
it:

```cpp
auto r = CHECK_TIME_FUNCTION([](int i) { return i + 1; }, 1);
CHECK(TUPLE_FIRST(r));
CHECK_SAME(TUPLE_SECOND(r), 2);
```

`-d` adds min and max to the report alongside the average.

These are **timings, not assertions** — nothing fails because something got
slower. To fail on a regression, assert on the value yourself.

## Macro names and collisions

Every public macro exists twice:

```cpp
TRX_CHECK(expr)      // canonical, always defined
CHECK(expr)          // short alias, unless TESTRIXA_NO_SHORT_MACROS
```

Names like `CHECK`, `TEST_RUN` and `MOCK_METHOD` collide with other frameworks
eventually — `MOCK_METHOD` is gmock's, exactly. If you need both in one
translation unit:

```cpp
#define TESTRIXA_NO_SHORT_MACROS
#include <testrixa/testrixa.h>

TRX_TESTCASE_BEGIN(testThing)
...
```

The `TRX_` names are complete: a test case can be written entirely with them.

## Conventions that hold up

These come from mistakes made in this repository.

1. **Name a group after what should be true.** The name is what you read when
   it fails.
2. **Prefer `CHECK_SAME` to `CHECK(a == b)`.** Only the first prints both
   values.
3. **Make a new check fail once, on purpose.** A check that watches nothing
   reports zero problems, exactly like one that works. Break the thing it
   guards, watch it fire, put it back.
4. **Do not assert on phase-dependent counts.** They change with `--mem` and
   `--only`.
5. **Assertions inside a `CHECK_CONCURRENT` or stress body need a `bool`
   return.** `CHECK` expands to `... return false;`, so the body already
   returns `bool` whether it meant to or not — spell the return type out and
   end with `return true;`.

## Next

- [04. Running Tests](04-running-tests.md) — the command line
- [05. Memory and Leaks](05-memory.md)

## Related

- [Guide index](README.md)

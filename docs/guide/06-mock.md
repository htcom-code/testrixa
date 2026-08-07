# 06. Mock

`<testrixa/mock.h>` generates a test double for a virtual interface, and gives
you a recorder you can also use on its own.

- [Declaring a mock](#declaring-a-mock)
- [Configuring it](#configuring-it)
- [Asking what happened](#asking-what-happened)
- [Function-pointer seams](#function-pointer-seams)
- [Threads](#threads)
- [What it does not do](#what-it-does-not-do)

## Declaring a mock

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

Each mocked method gets a `<method>Mock` member — `nowMock`, `sleepMock` — and
that object is where everything happens.

### The four arguments

`MOCK_METHOD(name, returnType, (parameters), (arguments))`

The parameter list is written twice: `(int ms)` declares the override, `(ms)`
forwards it to the recorder. The preprocessor cannot derive the second from the
first without a macro argument-counting library, which is about 150 lines of
expansion nobody can debug when it misfires. The duplication is checked by the
compiler on that line, so it cannot go quietly wrong.

It buys two things. Any number of parameters, and the parentheses protect
commas:

```cpp
MOCK_METHOD(lookup, std::string, (std::map<int,int> keys, int which), (keys, which))
```

An arity-numbered macro cannot take that — the comma inside `std::map<int,int>`
would split the macro argument.

### `const`, `noexcept` and the rest

| | |
|---|---|
| `MOCK_METHOD(...)` | plain |
| `MOCK_CONST_METHOD(...)` | `const` member function |
| `MOCK_METHOD_SPEC(name, ret, (params), (args), specs)` | anything else — `noexcept`, `const noexcept`, `final` |

```cpp
MOCK_METHOD_SPEC(tick, void, (), (), noexcept)
```

### Limits, all of them loud at compile time

- A **return type containing a comma** needs a typedef. The parameter list does
  not — it is parenthesised.
- So does an **interface named with template arguments**.
- The interface must be **default-constructible**.
- **Default arguments** cannot be given in the parameter list.
- A method **returning a reference** needs `does()`, not `returns()`.

## Configuring it

```cpp
MockClock clock;
clock.nowMock.returns(1000);
```

| | |
|---|---|
| `returns(value)` | every call returns this |
| `returnsOnce(value)` | the next call returns this; queue several to script a sequence |
| `does(body)` | run this instead; the body receives the arguments |
| `doesOnce(body)` | as above, for one call |

```cpp
// "fails the first time, succeeds the second" — the shape of every retry test
clock.nowMock.returnsOnce(0);
clock.nowMock.returnsOnce(0);
clock.nowMock.returns(1000);

// a body sees the arguments and decides
store.lookupMock.does([](std::map<int,int> keys, int which) -> std::string {
    return std::to_string(keys[which]);
});
```

An unconfigured method returns a value-initialised `Ret` — `0`, `nullptr`,
`false`, an empty string. If the return type cannot be default-constructed and
nothing was configured, the mock prints which method it was and aborts, rather
than handing back something invented.

## Asking what happened

Verification happens **after** the call, not before it.

```cpp
MockClock clock;
clock.nowMock.returns(1000);

Scheduler scheduler(&clock);
scheduler.tick(250);

CHECK_CALLED_TIMES(clock.nowMock, 1);
CHECK_LAST_CALL(clock.sleepMock, 250);
CHECK_NO_UNSTUBBED_CALLS(clock.nowMock);
```

| Macro | |
|---|---|
| `CHECK_CALLED(m)` | called at least once |
| `CHECK_NOT_CALLED(m)` | never called |
| `CHECK_CALLED_TIMES(m, n)` | called exactly `n` times |
| `CHECK_LAST_CALL(m, args...)` | the most recent call had these arguments |
| `CHECK_NO_UNSTUBBED_CALLS(m)` | nothing had to invent a return value |

And directly on the recorder:

| | |
|---|---|
| `calls()` | how many times |
| `called()` | at least once |
| `call(i)` / `lastCall()` | the arguments, as a `std::tuple` |
| `lastCallWas(args...)` | comparison, false when never called |
| `unconfiguredCalls()` | calls that fell back to an invented return value |
| `resetCalls()` | forget the calls, keep the configuration |
| `recordsArguments` | compile-time: whether arguments could be recorded |

```cpp
CHECK_SAME(std::get<0>(clock.sleepMock.call(0)), 10);
```

### Why there is no `EXPECT_CALL`

Expecting up front means the mock reports a failure from its own destructor,
which means it has to know which test case is running, which means a global
registry — and an assertion firing during stack unwinding is its own problem.
Recording instead keeps a mock a plain object with no dependency on the
framework, and puts the assertions where the reader is already looking. It is
the same shape `measure() → Report` uses in the memory suite.

**The cost is real: forget to verify and nothing tells you.**
`CHECK_NO_UNSTUBBED_CALLS` is the closest thing to a strict mock here — it
fails when the test exercised a path it never described.

### Arguments that cannot be copied

A method taking `std::unique_ptr` is worth mocking. Rather than refusing to
compile, such a method **counts its calls and drops the arguments**:

```cpp
MOCK_BEGIN(MockSink, Sink)
    MOCK_METHOD(take, void, (std::unique_ptr<int> value), (std::move(value)))
MOCK_END

static_assert(!decltype(sink.takeMock)::recordsArguments, "");
CHECK_CALLED_TIMES(sink.takeMock, 2);      // still counted
```

Calling `lastCall()` on such a method is a compile error with a message saying
why, not a silent empty tuple.

## Function-pointer seams

`mock::Method` does not depend on the generated class, so it works wherever the
code under test already routes through a replaceable pointer — the arrangement
`<testrixa/memory.h>` uses for `install()`:

```cpp
typedef int (*ComputeFn)(int);
ComputeFn g_compute = &realCompute;
int compute(int v) { return g_compute(v); }

// in the test
static testrixa::mock::Method<int(int)> fakeCompute("compute");
static int fakeComputeThunk(int v) { return fakeCompute.invoke(v); }

fakeCompute.returns(7);
g_compute = &fakeComputeThunk;

CHECK_SAME(compute(21), 7);
CHECK_LAST_CALL(fakeCompute, 21);

g_compute = &realCompute;                  // put it back
```

## Threads

`mock::Method` is thread-safe — unusually for this framework, where the runner
is single-threaded by design. A test double is precisely the object you hand to
concurrent code, and a race inside it would be a bug in the tool meant to find
bugs.

```cpp
MockClock clock;
clock.nowMock.returns(5);

CHECK_CONCURRENT(4, [&](int) -> bool {
    for (int i = 0; i < 250; ++i) clock.now();
    return true;
});

CHECK_CALLED_TIMES(clock.nowMock, 1000);
```

The lock is not what costs: recording a call appends to a vector, which is
dearer than an uncontended mutex. Bodies run with the lock released, so a body
may call back into its own mock without deadlocking.

The lock makes `Method` non-copyable, and therefore a generated mock class
non-copyable too. Hold mocks by reference or pointer.

## What it does not do

| | Why |
|---|---|
| Mock non-virtual member functions | Needs code generation |
| Mock free functions with no seam | Needs linker substitution, which couples the library to your build |
| Call-ordering expectations | Outside the verify-after model |

If you need one of the first two today, introduce a seam: an interface, or a
replaceable function pointer. That is a change to the code under test, and
usually an improvement to it.

## Next

- [07. Stress](07-stress.md)
- [08. Concurrency](08-concurrency.md)

## Related

- [Guide index](README.md)

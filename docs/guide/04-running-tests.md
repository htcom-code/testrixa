# 04. Running Tests

The command line, phase selection, exit codes and the JUnit report.

- [Selecting what runs](#selecting-what-runs)
- [Output](#output)
- [Exit codes](#exit-codes)
- [Component options](#component-options)
- [JUnit XML](#junit-xml)
- [Sanitizers](#sanitizers)

## Selecting what runs

```sh
./testAll.out                     # every test case: basic + measure
./testAll.out -l                  # list the registered test cases
./testAll.out testStats           # one test case, by exact name
./testAll.out -h                  # everything below, including component options
```

### By phase

```sh
./testAll.out -b                  # basic only
./testAll.out -m                  # measure only
./testAll.out --mem               # memory only   (= --only=memory)
./testAll.out --stress            # stress only   (= --only=stress)

./testAll.out --only=basic,memory
./testAll.out --skip=measure
```

Memory and stress are **opt-in**. A plain run never executes them, so a memory
phase you wrote is not being run unless you asked for it — check with
`--only=memory` if a leak test seems to be doing nothing.

Test-case selection is an **exact match** on the name. There is no pattern
matching yet; the whole suite here runs in well under a second, so running all
of it is rarely worse than selecting part of it.

### Other switches

| | |
|---|---|
| `-s` / `--no_stop` | Do not stop a test case at its first failure. Use this when you want the full list of what is broken. |
| `-t=N` / `--times=N` | Measure loop count (default 100, range 1–999999). |
| `-d` / `--detail` | Add min and max to the measure table, not just the average. |

## Output

One table per test case and phase.

```
[testStats::BASIC]
----------------------------------------------------------------------------------
| Group Name                                     | Lvl|   Total| Success|    Fail|
----------------------------------------------------------------------------------
| SUM_ADDS_EVERYTHING                            |   0|       3|       3|       0|
```

`Lvl` is the nesting depth of the group. `Total` counts assertions, not groups.

A failure prints under its group:

```
|  fail[    1] CHECK_TRUE [expr:(mean({}), 0.0) value:(nan, 0.000000)] testStats.cpp:17
```

— the expressions, both values, the file and the line.

The memory phase prints a different table, and it prints **what it does not
cover** every time:

```
[testLeak::MEMORY] coverage: C++ new/delete = full | C malloc = only where
<testrixa/malloc_shim.h> is included | prebuilt libraries = not tracked
----------------------------------------------------------------------------------
| Group Name                   |   Alloc|    Free|    Leak|       Peak|   MaxBlock|
```

That line is not boilerplate. A memory checker that quietly watches less than
you assume is worse than none, so it says so on every run.

## Exit codes

| Code | Meaning |
|---|---|
| `0` | Everything that ran, passed |
| `1` | Something failed, the command line was wrong, **or nothing ran at all** |

The last one matters. A binary that matched no test case must not report
success — a typo in a name would otherwise look like a clean run forever.

```sh
./testAll.out doesNotExist; echo $?     # 1
./testAll.out --nope=1;     echo $?     # 1
```

Unknown options are an **error**, never ignored. An option that silently does
nothing is the worst outcome available: the run looks fine and checks nothing.

**Read the exit code, not the table.** In CI that is the only thing that
decides pass or fail.

## Component options

Each component registers its own namespaced options, so `-h` always lists what
is actually compiled in.

```
  Memory Suite (--mem.*)
    --mem.preset=fast|default|paranoid  which checkers to run [default]
    --mem.redzone=<int>               guard bytes on each side of an allocation (0=off) [32]
    --mem.backtrace=<int>             frames to record per allocation (0=off, costs more than everything else) [0]
    --mem.quarantine=<int>            MB of freed memory to hold back (0=off, weakens double-free) [8]

  Stress Test (--stress.*)
    --stress.scale=<int>              percent of each declared budget to actually run (10 = a tenth) [100]
    --stress.stop-after=<int>         give up after this many failures (0 = run the whole budget) [0]

  Thread Test (--thread.*)
    --thread.join-timeout=<int>       ms to wait for a worker before reporting a hang (0 = wait forever) [10000]
```

The value in brackets is the current default. Details are in each component's
guide: [memory](05-memory.md), [stress](07-stress.md),
[concurrency](08-concurrency.md).

Useful in practice:

```sh
./testAll.out --mem --mem.preset=paranoid     # every checker, including pattern fill
./testAll.out --mem --mem.backtrace=16        # find out where a leak came from
./testAll.out --stress --stress.scale=10      # a tenth of every stress budget
```

## JUnit XML

```sh
./testAll.out --report-junit=results.xml
```

One `<testsuite>` per test case and phase, one `<testcase>` per group, failures
carrying the same text the console shows. GitHub Actions, GitLab CI and Jenkins
all read this format directly.

```sh
./testAll.out --only=basic,measure,memory,stress --report-junit=results.xml
```

Combine it with phase selection so the report covers the phases you care about
— a report from a plain run contains no memory or stress results, because those
phases did not run.

**Failing to write the file is an error, not a warning.** A missing report reads
to CI as "nothing failed", and a silent false pass is the outcome worth
preventing hardest.

See [10. CI Integration](10-ci-integration.md) for wiring it up.

## Sanitizers

testrixa's memory suite and the sanitizers answer different questions, and both
are worth running.

| | Catches |
|---|---|
| testrixa `--mem` | leaks, double frees, invalid frees, overflow into its redzones, writes to quarantined memory, peak usage |
| AddressSanitizer | out-of-bounds on stack/globals/heap, use-after-return, use-after-scope |
| UndefinedBehaviorSanitizer | signed overflow, bad shifts, misaligned access, invalid casts |
| ThreadSanitizer | data races — testrixa's `<testrixa/thread.h>` is a harness, **not** a detector |

Build your tests with one at a time. Flags must reach **both** the compile and
the link:

```sh
c++ -std=c++17 -fsanitize=address -fno-omit-frame-pointer -g -O1 \
    -o testAll.out testStats.cpp
```

Getting that wrong produces undefined references from the sanitizer runtime,
which is exactly how this project's own build was broken once.

**ASan and `--mem` run together.** Both intercept allocation, and the obvious
worry is that one blinds the other — it does not. testrixa's own CI builds the
suite with `-fsanitize=address` and runs `--mem` under it on every commit; the
leak counts, sizes and peak figures come out the same as without ASan.

What does change is where each one points. testrixa reports the leak in terms
of your test (`leak #1  4 bytes  age=0ms`); ASan reports memory errors in terms
of the process. They are complementary, not redundant.

ThreadSanitizer is the one to run on its own build, since it needs the whole
program instrumented to say anything useful.

## Next

- [05. Memory and Leaks](05-memory.md)
- [10. CI Integration](10-ci-integration.md)

## Related

- [Guide index](README.md)

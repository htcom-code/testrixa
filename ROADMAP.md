# testrixa Roadmap

Where this is going, and — just as deliberately — where it is not. Several
things below were considered and declined; they are recorded with their reasons
so the decision is on the record rather than implied by absence.

This is a living document, not a release schedule.

## Legend

| Status | Meaning |
|---|---|
| ✅ Shipped | On `main`. Listed for context. |
| 🛠 Planned | Committed, not yet scheduled or implemented. |
| 🔎 Candidate | Under evaluation — inclusion not yet decided. |
| 🚫 Not planned | Considered and declined, with the reason. |

---

## Components

testrixa was scoped as ten components plus two later ones. The ten are done.

### ✅ Shipped

Unit test · Benchmark · Memory check · Leak detection · Stress · Thread harness
· Mock · Fixture · Reporter · Runner — the ten the project was scoped around —
plus Coverage integration.

### ✅ Shipped — Coverage

`testrixa_add_coverage(target)` instruments a test target and reports through
gcovr. See [docs/guide/12-coverage.md](docs/guide/12-coverage.md).

Nothing here measures coverage — the compiler does. What it adds is that the
number is about **your** code. testrixa is header-only, so its headers compile
into your binary and the instrumentation counts them; vendored or fetched, that
reaches your report. Measured on a 14-line example whose own coverage is 100%,
an unfiltered run reported **36% over 973 lines**. The exclusion is applied
regardless of how testrixa was brought in, because which layout you picked is
not something you should have to think about at report time.

Two decisions worth stating:

- **The number is not printed in the console table.** A figure that is always
  on screen tends to become a target, and a suite tuned to raise a percentage
  is not the same as a suite that checks more.
- **There is no coverage gate macro.** Coverage counts lines the tests
  *reached*, not lines they *checked* — a run that executes everything and
  asserts nothing scores 100%. Making that a pass condition would reward the
  wrong thing.

MSVC is not covered: gcov has no MSVC equivalent. OpenCppCoverage is the usual
answer there and is a separate tool.

### 🔎 Candidate — Per-test-case coverage attribution

"Which lines did *this* test case cover, and did it cover anything the others
did not." **This is the one coverage question only testrixa can answer** — the
runner is the only thing that knows where a test case begins and ends — and it
finds tests that add nothing.

Not built yet because the cost is real and nobody has asked. It needs
`__gcov_reset`/`__gcov_dump` on gcc and `__llvm_profile_reset_counters` on
clang — different APIs, absent on MSVC — plus a dump per test case, which is
not cheap. Same reason as Runner pattern selection below: it will be built when
something needs it.

### 🔎 Candidate — Allocation histogram

Size-class distribution over a scope. The tracker already sees every allocation,
so the data is there; what is not settled is whether the answer is worth a table
that most runs would ignore.

### 🔎 Candidate — Lifetime checker

How long allocations live, to find the block that is freed correctly but far too
late. Same data source as the histogram, same open question.

### 🔎 Candidate — `malloc` interposition

Today C allocation is covered only where `<testrixa/malloc_shim.h>` is included,
because it works by redefining `malloc` as a macro. True interposition
(`LD_PRELOAD`, `-Wl,--wrap`, or `DYLD_INSERT_LIBRARIES`) would cover prebuilt
libraries too.

The reason it is not shipped: every mechanism is platform-specific and couples
the library to the consumer's link step, which is exactly what "header-only"
promises not to do. It would likely arrive as an *optional* companion rather
than part of the headers.

### 🔎 Candidate — Runner pattern selection

`--only-case=mem*` instead of one exact test-case name. Not written because
nothing has needed it yet: the suite runs in under a second, so running all of
it is rarely worse than selecting part of it.

---

## Platform support

### ✅ Current baseline

gcc 12 and clang 14 on Linux and macOS, `-Wall -Wextra -Werror` clean, verified
under AddressSanitizer, UndefinedBehaviorSanitizer and ThreadSanitizer.

### ✅ Shipped — Windows / MSVC

Builds and passes under MSVC 19.51 on Windows Server 2025, through CMake, with
`/W4 /WX`. A CI job gates it like the others.

The first run was a discovery, not a formality: fourteen defects came out, and
only two were Windows-specific. The rest were ordinary bugs — silent narrowing,
name shadowing, a `bool` compared against an `unsigned` — that gcc 12, gcc 15,
gcc 16, clang 14 and clang 21 had all walked past. A fifth compiler earns its
place the same way the second one did.

Still missing there, and worth knowing before relying on it:

- **No sanitizers.** ASan/UBSan/TSan run on Linux only.
- **No `make`.** `ci/run.sh` and the GNU Makefile are POSIX shell; CMake is the
  only build path on Windows.
- **Backtraces are addresses without symbols.** Nothing calls `SymFromAddr`.

### 🔎 Candidate — Windows backtrace symbolisation

`CaptureStackBackTrace` gives addresses; turning them into names needs dbghelp
and `SymFromAddr`. Worth doing, but it is the difference between a leak report
that says `0x7ff6...` and one that names your function — not between working
and not working.

### 🔎 Candidate — Older standards

C++17 is the floor and `traits.h` still carries pre-17 fallbacks from the
prototype. Supporting C++11/14 would mean giving up `if constexpr`, inline
variables and fold expressions, which several components lean on. Not currently
worth it; recorded because the fallbacks in the source suggest otherwise.

---

## Deliberately not planned

### 🚫 A profiler

The measure phase already answers "how long did this take", and it times a
callable in isolation so the figure is about your code. "Where did the time go
inside it" is a sampling profiler's question, and `perf`, Instruments and VTune
answer it well.

The difference from coverage is worth spelling out, because both started as the
same kind of plan. Coverage had something for testrixa to do: our headers were
corrupting the consumer's number, so there was a thin layer that only we could
write correctly. A profile has no equivalent — nothing we do distorts it in a
way only we can undo, so a wrapper would add a step and no information.

[docs/guide/12-coverage.md](docs/guide/12-coverage.md) shows how to use the
measure phase and an external profiler together, including the two flags that
make the profile readable (`-t=1000` to let the code under test dominate the
samples, and a single test-case name to keep the report narrow).

### 🚫 A race detector

ThreadSanitizer tracks happens-before across every memory access and does it
well. Writing a second one here would be a worse copy. `<testrixa/thread.h>`
is the harness you run *under* TSan, and its first line says so.

### 🚫 Replacing ASan / UBSan

The memory suite catches leaks, double frees, overflow into its own redzones and
writes to quarantined memory. Stack overflows, uninitialised reads and
undefined behaviour belong to the sanitizers. Run both.

### 🚫 Parallel test execution

The whole suite runs in under a second. Parallelism would add scheduling, output
interleaving and a new class of flakiness, and buy nothing.

### 🚫 Mocking non-virtual member functions

Cannot be done without code generation, which would make testrixa something you
install and run rather than something you include.

### 🚫 Mocking free functions with no seam

Needs linker substitution or weak symbols, which couples the library to the
consumer's build. Where the code under test already routes through a replaceable
function pointer, `mock::Method` works today.

### 🚫 Mock call-ordering expectations

Outside the verify-after model: enforcing order means the mock reports failures
from its own destructor, which means it has to know which test case is running,
which means a global registry. That trade was declined once already when the
verification model was chosen.

---

## Project

### 🛠 Planned — a published release

There is no tagged release yet, and nothing is published to any package index.
Header-only C++ has no single obvious distribution channel; the candidates are a
GitHub release tarball, vcpkg, and Conan. Undecided.

### 🔎 Candidate — per-component guides

The README covers ten components in one file. Deeper usage guides under `docs/`
would relieve it, at the cost of documentation that can drift from the code.
The current mitigation — every example is compiled before it is written down —
would have to extend to those too.

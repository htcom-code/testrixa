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
· Mock · Fixture · Reporter · Runner.

### 🛠 Planned — Coverage

Line and branch coverage, reported next to the test results rather than as a
separate artifact a person has to go and find. The likely shape is a thin
reader over `gcov`/`llvm-cov` output rather than instrumentation of our own —
the compilers already do the hard part, and a home-grown instrumenter would be a
worse version of theirs.

Open question: whether coverage belongs in the console table at all, or only in
the machine-readable report. A number that is always on screen tends to become a
target.

### 🛠 Planned — Profiler

Beyond the measure phase's wall-clock timing: where the time went, not just how
much there was. Same principle as coverage — lean on `perf`, `Instruments` and
friends rather than sampling ourselves.

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

### 🛠 Planned — Windows / MSVC

MSVC and Win32 code paths exist in the headers — `traits.h` detects the
compiler, `platform.hpp` has `_aligned_malloc` and `dbghelp` branches — and
**none of it has ever been compiled**. Calling that "support" would be a claim
the project cannot back, so the README says it is untested instead.

What it needs: a Windows CI job, then whatever that job finds. Experience from
the Linux/macOS split suggests the first run is a discovery, not a formality.

### 🔎 Candidate — Older standards

C++17 is the floor and `traits.h` still carries pre-17 fallbacks from the
prototype. Supporting C++11/14 would mean giving up `if constexpr`, inline
variables and fold expressions, which several components lean on. Not currently
worth it; recorded because the fallbacks in the source suggest otherwise.

---

## Deliberately not planned

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

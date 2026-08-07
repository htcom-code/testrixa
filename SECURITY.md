# Security Policy

## Reporting a vulnerability

Please report security issues **privately** — do not open a public issue for an
unpatched vulnerability.

- Use GitHub's **"Report a vulnerability"** (Security → Advisories) on this
  repository:
  <https://github.com/htcom-code/testrixa/security/advisories/new>, or
- email the maintainer at **htjulia1@gmail.com**.

Include a description, the affected version or commit, and a minimal reproducer
— a single self-contained `.cpp` we can compile is ideal. We will acknowledge
the report and work with you on a fix and coordinated disclosure.

## Supported versions

testrixa is pre-1.0. Fixes land on the active line, and the public API may
change between minor versions.

| Version | Supported |
|---|---|
| 0.1.x | ✅ |

## Threat model — a test-only library that replaces the allocator

testrixa is meant for **test binaries, not production ones**, and the reason is
worth stating rather than assuming. To do its job it takes over machinery the
rest of the program relies on:

- **It replaces global `operator new` and `operator delete`** — all twenty
  forms, routed through function pointers that a test can swap at run time
  (`memory::install()`). Anything linked into the same binary allocates through
  testrixa once `<testrixa/memory.h>` is part of the link.
- **`<testrixa/malloc_shim.h>` redefines `malloc`, `calloc`, `realloc`, `free`,
  `strdup` and `strndup` as macros.** Any code compiled after that include in
  the same translation unit is rewritten.
- **Tracked allocations carry an intrusive header and redzones**, so the layout
  of a live allocation is not what the platform allocator would have produced.
- **Freed memory is held in quarantine** and overwritten with a pattern, so a
  use-after-free reads or writes memory testrixa still owns.

None of that is a defect — it is how the checks work. It does mean that linking
testrixa into a **production** binary widens that binary's attack surface for no
benefit, and we do not support doing so.

**Assumed trusted:** the test source, the build, and anything else linked into
the test binary. testrixa does not attempt to contain code running inside its
own process, and there is no sandbox — a test can do anything the process can.

### In scope

Bugs in testrixa with security impact, including:

- **A check that can be made to stay silent.** A leak, double free, redzone
  overflow or use-after-free that testrixa fails to report is the most serious
  class of bug here: the suite is green either way, so the failure is invisible
  by construction.
- **Memory-safety faults in the tracker itself** — the block header, redzone and
  quarantine code does pointer arithmetic by hand. A crash or corruption
  triggered by ordinary allocation patterns counts.
- **The report writer** — `--report-junit=<path>` writes a file whose contents
  include test names and failure text. Escaping or path handling that can be
  driven somewhere unintended is in scope.
- **Option parsing** — argument handling that reads out of bounds or accepts
  what it should reject.

### Out of scope

- **A test doing what test code can inherently do.** Test source is trusted;
  see above.
- **Production use.** Linking a test-only allocator replacement into a shipped
  binary is a configuration choice we advise against, not a vulnerability.
- **Bugs the checks are documented not to catch** — stack overflows,
  uninitialised reads, C allocation in a translation unit that does not include
  `<testrixa/malloc_shim.h>`, allocation inside prebuilt libraries, data races
  (`<testrixa/thread.h>` is a harness, not a detector). The report prints its
  own coverage line for exactly this reason. If you find a gap that the
  documentation claims is covered, that *is* in scope — report it.
- **Windows.** The MSVC and Win32 code paths have never been compiled. Findings
  there are welcome as bugs; they are not regressions.

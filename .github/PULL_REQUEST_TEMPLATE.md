<!--
Thanks for contributing to testrixa. Keep the title in Conventional Commits
form, e.g. `fix(mock): rewind the returnsOnce queue on resetCalls`.
Delete any section that does not apply.
-->

## What & why

<!-- What does this change do, and what problem does it solve? -->

## Type of change

- [ ] `fix` — bug fix (no new API)
- [ ] `feat` — new capability
- [ ] `perf` — performance, no observable behaviour change
- [ ] `docs` — documentation only
- [ ] `refactor` / `chore` / `test` / `ci` — no behaviour change

## Verification

`make ci` is the gate. It runs both compilers, CMake, all three sanitizers and
both consumer paths from a clean copy of the tree — the same script CI runs, so
a green run here means a green run there.

- [ ] `make ci` passes
- [ ] New behaviour has a test, and the test fails without the change

<!--
"It passes" is not the same as "it is checked". A check that watches nothing
also reports zero problems. If this PR adds one, break the thing it guards on
purpose, confirm it fails, put it back — and say so below.
-->

- [ ] If this adds a check: it was disproved once (broken deliberately, seen to
      fail, restored) — say what you broke and what it reported

## Contracts

These are the promises a consumer relies on. Each has a test that pins it, and
each has been broken accidentally at least once.

- [ ] **Nothing new in the consumer's global namespace.** A new public header
      must be added to `tests/checkNamespace.cpp` — two headers were once
      missing from it and the contract covered less than it appeared to.
- [ ] **`TESTRIXA_NO_SHORT_MACROS` still removes every short alias.** New macros
      need both a `TRX_` name and an entry in `tests/checkPrefix.cpp`.
- [ ] **No new warnings.** `-Wall -Wextra -Werror` on gcc and clang, and the
      public headers are compiled by consumers who may be stricter than we are.
- [ ] **Unknown input is still rejected**, never silently ignored.
- [ ] **The report still says what it does not cover**, if this changes what is
      tracked or measured.

## Docs

- [ ] README updated if the public API, CLI or support matrix changed
- [ ] CHANGELOG entry added under `[Unreleased]`
- [ ] Any example added to the docs was compiled and run before being written down

## Related

<!-- Closes #123, related issues. -->

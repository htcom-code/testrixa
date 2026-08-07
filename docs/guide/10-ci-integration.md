# 10. CI Integration

Running testrixa in CI and getting failures shown where people look.

- [The shape of a CI run](#the-shape-of-a-ci-run)
- [GitHub Actions](#github-actions)
- [GitLab CI](#gitlab-ci)
- [Jenkins](#jenkins)
- [Things that bite](#things-that-bite)

## The shape of a CI run

Three decisions matter more than which CI system you use.

**1. Run the opt-in phases explicitly.** A plain run executes basic and measure
only. The memory and stress tests you wrote do not run in CI unless you ask:

```sh
./testAll.out                            # basic + measure
./testAll.out --mem                      # memory
./testAll.out --stress --stress.scale=10 # stress, at a tenth of the budget
```

**2. Read the exit code.** It is non-zero for a failure, a bad argument, **and
for nothing having run at all**. The last one catches a build that silently
stopped registering tests.

**3. Emit JUnit XML.** Every CI system reads it, and it puts failures on the
pull request rather than at line 4,000 of a log:

```sh
./testAll.out --only=basic,measure,memory,stress --report-junit=results.xml
```

Failing to write that file is an error, not a warning. A missing report reads
to CI as "nothing failed".

## GitHub Actions

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:          # no paths filter — see "Things that bite"

concurrency:
  group: ci-${{ github.ref }}
  cancel-in-progress: true

permissions:
  contents: read

jobs:
  test:
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        cxx: [g++, clang++]
    steps:
      - uses: actions/checkout@v4

      - name: build and run
        run: |
          make -C tests CXX=${{ matrix.cxx }}
          ./tests/testAll.out
          ./tests/testAll.out --mem

      - name: JUnit report
        if: always()          # a failed run is exactly when you want the report
        run: ./tests/testAll.out --only=basic,measure,memory --report-junit=results.xml

      - uses: actions/upload-artifact@v4
        if: always()
        with:
          name: results-${{ matrix.cxx }}
          path: results.xml

  sanitizers:
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        san: [address, undefined, thread]
    steps:
      - uses: actions/checkout@v4
      - run: |
          make -C tests SANITIZER=${{ matrix.san }}
          ./tests/testAll.out
          ./tests/testAll.out --mem

  ci-complete:
    if: always()
    needs: [test, sanitizers]
    runs-on: ubuntu-latest
    steps:
      - run: |
          for r in "${{ needs.test.result }}" "${{ needs.sanitizers.result }}"; do
            if [ "$r" = "failure" ] || [ "$r" = "cancelled" ]; then exit 1; fi
          done
```

**Mark `ci-complete` as the required status check**, not the matrix jobs. It
reports even when everything upstream is skipped, so a required check never gets
stuck — and you can restructure the jobs above without touching branch
protection.

### Pinning the compiler version

`ubuntu-latest` moves when GitHub moves it. If your README claims a support
matrix, pin the toolchain so CI is testing what you promise:

```yaml
    runs-on: ubuntu-latest
    container: debian:bookworm-slim      # gcc 12, clang 14
    steps:
      - run: |
          apt-get update -qq
          apt-get install -y -qq --no-install-recommends g++ clang make cmake git
      - uses: actions/checkout@v4        # after git is installed
```

That is what testrixa's own workflow does.

## GitLab CI

```yaml
stages: [verify]

verify:
  stage: verify
  image: debian:bookworm-slim
  before_script:
    - apt-get update -qq
    - apt-get install -y -qq --no-install-recommends g++ clang make cmake
  script:
    - make -C tests
    - ./tests/testAll.out
    - ./tests/testAll.out --mem
    - ./tests/testAll.out --only=basic,measure,memory --report-junit=results.xml
  artifacts:
    when: always
    reports:
      junit: results.xml
```

`reports: junit:` is what puts failures on the merge request page. `when:
always` matters — without it the report is dropped exactly when the job failed
and you needed it.

## Jenkins

```groovy
pipeline {
    agent any
    stages {
        stage('test') {
            steps {
                sh 'make -C tests'
                sh './tests/testAll.out'
                sh './tests/testAll.out --mem'
                sh './tests/testAll.out --only=basic,measure,memory --report-junit=results.xml'
            }
        }
    }
    post {
        always {
            junit 'results.xml'
        }
    }
}
```

## Things that bite

**Do not put a `paths:` filter on `pull_request`.** If the workflow is skipped
on a docs-only PR, a required status check sits at "Expected — waiting for
status" forever and the merge is blocked for anyone who is not an admin.
Filtering `push` is fine — that runs after the merge, so it is not a gate.

**The report needs `if: always()` / `when: always`.** A report produced only on
success is a report you never read.

**Opt-in phases are opt-in in CI too.** The most common way a memory suite
provides no value is that nobody passed `--mem`.

**One sanitizer per build.** They instrument the same things; combining them
gives you a slower build and less clarity. Run them as a matrix instead.

**Stress on every commit, at a fraction.** Declare honest budgets in the tests
and run `--stress.scale=5` per commit, 100 nightly. `--stress.scale` never
scales a budget to zero, so the checks still run.

**A green CI with no test cases is still green.** The exit code covers this —
"nothing ran" is non-zero — but only if you read it rather than the log.

## Related

- [04. Running Tests](04-running-tests.md) — phases, exit codes, JUnit
- [09. Build Integration](09-build-integration.md)
- [Guide index](README.md)

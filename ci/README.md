# CI

Two entry points, one script.

| | |
|---|---|
| `ci/run.sh` | everything a verification run covers |
| `ci/Dockerfile` | the toolchain it needs, already unpacked |

`make ci` runs the script in that image locally. `.github/workflows/ci.yml` runs
the same script in a container with the same packages. Neither has steps of its
own, so they cannot drift apart.

```
make ci            # both compilers, clean copy of the tree, ~2m30s
make ci-image      # rebuild the image after editing ci/Dockerfile
ci/run.sh g++      # one compiler, in the current environment
```

## What the script covers

For each compiler: `make check` (contract checks, CLI regression, suite, memory
phase, example) and a CMake configure/build/`ctest`. Then, once: the three
sanitizers, an installed-package consumer, and a subdirectory consumer.

## Why the image exists

The toolchain is baked into image layers rather than installed per run. An
earlier arrangement installed it with apt on every job and spent 535 of 687
seconds doing so, measured from job log timestamps. Caching the `.deb` files did
not help — the bottleneck was `dpkg` unpacking, not downloading, so the cache
saved nothing and cost 36s per job to carry 135MB around. Layers are unpacked
once per host and then reused, which is the only thing that removes the work.

## Why two compilers

They disagree. gcc has rejected code clang accepted silently, including a real
use-after-free and a `#if` on an undefined macro that had quietly selected a
deprecated branch. A third compiler found more: gcc 15 rejects what gcc 12 and
16 accept, because whether `-Wmismatched-new-delete` fires depends on how far
the inliner gets. Checking one proves little, so `ci/run.sh` runs both by
default.

## The workflow

`.github/workflows/ci.yml` runs `ci/run.sh` in `debian:bookworm-slim` with the
same four packages this Dockerfile installs. It does not use this image: there
is no registry to pull it from, and bookworm plus apt reproduces it closely
enough that the compiler versions match what the README claims.

That pinning is deliberate. `ubuntu-latest` carries newer compilers and moves
when GitHub moves it, so CI would drift away from the documented support matrix
with neither of them saying so out loud.

## Running the workflow's job locally

Before changing the workflow, run its body in the same container it uses:

```sh
docker run --rm --platform linux/arm64 -v "$PWD":/src:ro -w / debian:bookworm-slim bash -c '
  apt-get update -qq
  apt-get install -y -qq --no-install-recommends g++ clang make cmake ca-certificates git
  mkdir -p /work && cp -a /src/. /work/ && cd /work
  ci/run.sh'
```

On Apple Silicon, pass `--platform linux/arm64`. Without it Docker may pick the
amd64 image and run it under qemu, which is several times slower and cannot run
a ThreadSanitizer binary at all.

# testrixa CI toolchain
#
# The whole point of this image is that the packages are already unpacked.
#
# The previous pipeline installed the toolchain with apt on every job: 535 of
# 687 seconds, measured from the job log timestamps. Caching the .deb files did
# not help -- a follow-up experiment showed the bottleneck was dpkg unpacking,
# not downloading, so the cache saved nothing and cost 36s per job to carry
# 135MB around. Baking the packages into image layers is the only thing that
# removes that work, because the layers are unpacked once per host and reused.
#
# Build:
#   docker build -t testrixa-ci:1 ci
#
# Use locally:
#   make ci            # both compilers, in this image
#
# Use from a runner: set image: testrixa-ci:1 and pull_policy: if-not-present,
# so the runner takes the local copy instead of looking for a registry.
#
# Also usable verbatim as a GitHub Actions container after the migration.

FROM debian:bookworm-slim

# g++ and clang both, because they disagree: gcc has caught warnings clang
# never reported, including a real use-after-free. Checking one proves little.
RUN apt-get update -qq \
 && apt-get install -y -qq --no-install-recommends \
      g++ \
      clang \
      make \
      cmake \
      ca-certificates \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

# Fail the build here rather than in a pipeline job if a package is missing.
RUN g++ --version && clang++ --version && make --version && cmake --version

WORKDIR /work

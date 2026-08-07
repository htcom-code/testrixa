# testrixa -- top-level GNU Makefile
#
# CMake is the path a consumer's build system takes. This is the path a person
# takes: a header-only library should be usable with a compiler and make, and
# nothing else. Everything here works without CMake installed.
#
#   make            build the test suite and the example
#   make test       run the suite (basic + measure phases)
#   make memtest    run the memory phase
#   make check      everything: contracts, CLI regression, suite, memory, example
#   make install    copy the headers under $(PREFIX)
#   make uninstall  remove them again
#   make clean      remove build output
#
# Overridable: CXX, CXXFLAGS, PREFIX, DESTDIR.
#
# Requires GNU make (uses .PHONY, $(MAKE), pattern-free recursion into
# subdirectories). Tested with GNU make 3.81 (macOS) and 4.3 (Debian).
#
# There is no CI. `make check` is the gate, and it only covers the compiler you
# happen to be running. The library has to build under both gcc and clang, and
# the two disagree often enough that checking one proves little -- gcc found six
# warnings clang never reported. Before opening a merge request:
#
#   docker run --rm -v "$PWD":/src:ro -w / debian:bookworm-slim bash -c '
#     apt-get update -qq && \
#     apt-get install -y -qq --no-install-recommends g++ clang make cmake ca-certificates && \
#     cp -r /src /work && cd /work && \
#     for CXX in g++ clang++; do make clean >/dev/null; make check CXX=$$CXX || exit 1; done'

CXX     ?= c++
PREFIX  ?= /usr/local
DESTDIR ?=

INCLUDE_DIR = include
HEADER_DEST = $(DESTDIR)$(PREFIX)/include

CI_IMAGE  ?= testrixa-ci:1

# SANITIZER=address|undefined|thread -- propagated to compile *and* link.
#   make check SANITIZER=address
SANITIZER ?=

.PHONY: all build test memtest check check-prefix check-namespace example \
        ci ci-image install uninstall clean help

all: build

# ---------------------------------------------------------------------------
# build / run
# ---------------------------------------------------------------------------
build:
	$(MAKE) -C tests CC="$(CXX)" SANITIZER="$(SANITIZER)"
	$(MAKE) -C examples CXX="$(CXX)" SANITIZER="$(SANITIZER)"

test: build
	cd tests && ./testAll.out

# The memory phase is opt-in, so it needs a run of its own.
memtest: build
	cd tests && ./testAll.out --mem

example: build
	cd examples && ./example.out

# Contract checks are compile-only: building them is the assertion.
check-prefix:
	$(MAKE) -C tests check-prefix CC="$(CXX)" SANITIZER="$(SANITIZER)"

check-namespace:
	$(MAKE) -C tests check-namespace CC="$(CXX)" SANITIZER="$(SANITIZER)"

# What CI runs, minus the CMake half.
check: check-prefix check-namespace build
	$(MAKE) -C tests check-cli
	cd tests && ./testAll.out
	cd tests && ./testAll.out --mem
	cd examples && ./example.out

# ---------------------------------------------------------------------------
# ci -- what `make check` cannot do on its own
#
# `make check` only ever covers the compiler you happen to be running. gcc and
# clang disagree often enough that one of them proves little: gcc has rejected
# code clang accepted silently, including a genuine use-after-free. This runs
# the whole thing under both, in the image the pipeline uses, from a clean copy
# of the tree so nothing local leaks in.
# ---------------------------------------------------------------------------
ci-image:
	docker build -t $(CI_IMAGE) ci

# The tree is copied into the container rather than mounted read-write, so a
# run cannot leave build output or a half-installed prefix behind in the repo.
#
# TSAN_SKIP=1: the image is amd64 and this host is arm64, so it runs emulated,
# and a ThreadSanitizer binary needs a memory layout the emulator does not give
# it ("cannot execute binary file"). Run TSan natively instead:
#   make check SANITIZER=thread
# `cp -a /src/. /work/` copies the contents: the image already creates /work,
# so `cp -r /src /work` would nest the tree at /work/src instead.
ci:
	@docker image inspect $(CI_IMAGE) >/dev/null 2>&1 || $(MAKE) ci-image
	docker run --rm -e TSAN_SKIP=1 -v "$(CURDIR)":/src:ro -w / $(CI_IMAGE) \
		bash -c 'cp -a /src/. /work/ && cd /work && ci/run.sh'

# ---------------------------------------------------------------------------
# install -- header-only, so this is a copy
#
# No CMake package files are written here. A consumer who wants
# find_package(testrixa) should install through CMake; this target is for
# `#include <testrixa/testrixa.h>` plus -I$(PREFIX)/include.
# ---------------------------------------------------------------------------
install:
	@echo "installing headers into $(HEADER_DEST)"
	@cd $(INCLUDE_DIR) && find testrixa -type d -exec mkdir -p "$(HEADER_DEST)/{}" \;
	@cd $(INCLUDE_DIR) && find testrixa -type f -name '*.h' -o -type f -name '*.hpp' | \
		while read -r f; do \
			mkdir -p "$(HEADER_DEST)/$$(dirname "$$f")"; \
			cp "$$f" "$(HEADER_DEST)/$$f"; \
		done
	@echo "done. build with: $(CXX) -std=c++17 -I$(PREFIX)/include ..."

uninstall:
	@echo "removing $(HEADER_DEST)/testrixa"
	@rm -rf "$(HEADER_DEST)/testrixa"

# ---------------------------------------------------------------------------
clean:
	$(MAKE) -C tests clean
	$(MAKE) -C examples clean
	rm -rf build

help:
	@echo "targets: all build test memtest check install uninstall clean"
	@echo "vars   : CXX=$(CXX) PREFIX=$(PREFIX) DESTDIR=$(DESTDIR)"

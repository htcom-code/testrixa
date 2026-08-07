#!/bin/sh
#
#  ci/run.sh
#  testrixa
#
#  Everything a verification run has to cover, in one script, so the same steps
#  run locally (`make ci`) and in GitHub Actions. A workflow that inlines its own
#  steps drifts from what people run by hand; this one cannot.
#
#  Expects the toolchain already present -- see ci/Dockerfile. Run it from the
#  root of a writable copy of the tree.
#
#  usage: ci/run.sh [compiler ...]      (default: g++ clang++)
#

set -e

COMPILERS="${*:-g++ clang++}"

echo "== toolchain =="
for cxx in $COMPILERS; do
    "$cxx" --version | head -1
done
cmake --version | head -1
make --version | head -1

for cxx in $COMPILERS; do
    echo
    echo "== $cxx : make =="
    make clean >/dev/null 2>&1 || true
    # contracts, CLI regression, suite, memory phase, example -- no CMake here,
    # which is the point: header-only has to mean a compiler is enough.
    make check CXX="$cxx"

    echo
    echo "== $cxx : cmake + ctest =="
    rm -rf build
    cmake -S . -B build -DCMAKE_CXX_COMPILER="$cxx" >/dev/null
    cmake --build build >/dev/null
    ( cd build && ctest --output-on-failure )
    rm -rf build
done

# The memory suite does pointer arithmetic by hand -- block headers, redzone
# offsets, payload alignment. That is exactly the code a sanitizer finds bugs
# in, and it is the code most worth guarding. Both are clean today; the point is
# that they stay that way.
#
# ThreadSanitizer joins the list now that the suite has concurrent tests. It was
# left out before on purpose -- with nothing concurrent to watch it guarded
# nothing -- and it is the only real race detector here: thread.h is a harness,
# not a detector, and says so.
#
# TSAN_SKIP=1 for hosts where it cannot run. Instrumented binaries need a memory
# layout an emulated container does not provide, which is exactly the case in
# the amd64 CI image on an arm64 host.
SANITIZERS="address undefined"
[ "${TSAN_SKIP:-0}" = "1" ] || SANITIZERS="$SANITIZERS thread"

for san in $SANITIZERS; do
    echo
    echo "== sanitizer: $san =="
    make clean >/dev/null 2>&1 || true
    make -C tests CC=g++ SANITIZER="$san" >/dev/null
    ./tests/testAll.out -s
    ./tests/testAll.out --mem -s
done
make clean >/dev/null 2>&1 || true

# add_subdirectory hides a broken export; only a consumer built against an
# installed package notices, and one did once -- the exported target carried no
# include directory at all and every find_package() user failed.
echo
echo "== installed package =="
rm -rf build stage consumer-build
cmake -S . -B build -DTESTRIXA_BUILD_TESTS=OFF -DTESTRIXA_BUILD_EXAMPLES=OFF >/dev/null
cmake --install build --prefix "$PWD/stage" >/dev/null
cmake -S tests/consumer -B consumer-build -DCMAKE_PREFIX_PATH="$PWD/stage" >/dev/null
cmake --build consumer-build >/dev/null
./consumer-build/consumer_smoke --only=basic,memory >/dev/null
echo "installed package builds and runs"

# A consumer pulling testrixa in as a subdirectory must not inherit our tests
# and examples: the symptom of getting that wrong shows up in their repository,
# not ours.
echo
echo "== subdirectory consumer =="
rm -rf /tmp/sub && mkdir -p /tmp/sub
cp -r . /tmp/sub/testrixa
rm -rf /tmp/sub/testrixa/build /tmp/sub/testrixa/stage /tmp/sub/testrixa/consumer-build
cat > /tmp/sub/CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.14)
project(subdir_consumer LANGUAGES CXX)
add_subdirectory(testrixa)
add_executable(sub_smoke main.cpp)
target_link_libraries(sub_smoke PRIVATE testrixa::testrixa)
EOF
printf '#define TEST_RUN_TERM\n#include <testrixa/testrixa.h>\n' > /tmp/sub/main.cpp
cmake -S /tmp/sub -B /tmp/sub/build >/dev/null
cmake --build /tmp/sub/build --target sub_smoke >/dev/null
/tmp/sub/build/sub_smoke -l >/dev/null
if cmake --build /tmp/sub/build --target help | grep -qE 'testrixa_tests|testrixa_example'; then
    echo "testrixa's own targets leaked into the consumer build" >&2
    exit 1
fi
echo "no target leakage"

rm -rf build stage consumer-build
echo
echo "all green: $COMPILERS"

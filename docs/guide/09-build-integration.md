# 09. Build Integration

Wiring testrixa into a project that already has a build.

- [make](#make)
- [CMake](#cmake)
- [Meson](#meson)
- [Bazel](#bazel)
- [Rules that apply to all of them](#rules-that-apply-to-all-of-them)

## make

```make
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
LDFLAGS  ?= -pthread                # only if you use <testrixa/thread.h>
INCS      = -I$(HOME)/.local/include -I..

TARGET = testAll.out
SRCS   = testMain.cpp testParser.cpp testStore.cpp
OBJS   = $(SRCS:.cpp=.o)

# Every object depends on every header it could see. Without this a header edit
# leaves stale objects behind and you test the previous version.
HDRS = ../parser.h ../store.h

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $(INCS) -c -o $@ $<

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf *.dSYM              # clang -g leaves these on macOS
```

`-pthread` is needed only if something includes `<testrixa/thread.h>`, but it
is harmless otherwise.

### Sanitizer builds

Flags have to reach **both** compile and link:

```make
SANITIZER ?=
ifneq ($(SANITIZER),)
SANFLAGS = -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g -O1
else
SANFLAGS = -O2
endif

CXXFLAGS = -std=c++17 $(SANFLAGS) -Wall -Wextra
LDFLAGS  = $(SANFLAGS) -pthread
```

```sh
make test SANITIZER=address
```

Putting the flag only on the compile line produces undefined references from
the sanitizer runtime — a link error that looks like a missing library and is
not.

## CMake

### Against an installed package

```cmake
cmake_minimum_required(VERSION 3.14)
project(mylib_tests LANGUAGES CXX)

find_package(testrixa 0.1 REQUIRED)

enable_testing()

add_executable(mylib_tests
    testMain.cpp
    testParser.cpp
    testStore.cpp)

target_link_libraries(mylib_tests PRIVATE testrixa::testrixa)
target_compile_options(mylib_tests PRIVATE -Wall -Wextra)

add_test(NAME mylib_tests COMMAND mylib_tests)
```

```sh
cmake -B build -DCMAKE_PREFIX_PATH=$HOME/.local
cmake --build build
ctest --test-dir build --output-on-failure
```

The imported target carries the include path and `cxx_std_17`; you do not set
either.

### Vendored, with `add_subdirectory`

```cmake
add_subdirectory(third_party/testrixa)
target_link_libraries(mylib_tests PRIVATE testrixa::testrixa)
```

testrixa builds its own tests and examples **only when it is the top-level
project**, so this adds no targets to your build. If you ever see
`testrixa_tests` in your target list, something set `TESTRIXA_BUILD_TESTS` on.

### `FetchContent`

```cmake
include(FetchContent)
FetchContent_Declare(
    testrixa
    GIT_REPOSITORY https://github.com/htcom-code/testrixa
    GIT_TAG        v0.1.0
)
FetchContent_MakeAvailable(testrixa)
```

Pin `GIT_TAG` to a tag or a commit. `main` means your build changes when
somebody else pushes.

### Threads

If you use `<testrixa/thread.h>`:

```cmake
find_package(Threads REQUIRED)
target_link_libraries(mylib_tests PRIVATE testrixa::testrixa Threads::Threads)
```

### Registering phases with CTest

CTest sees one test per `add_test`. The opt-in phases need their own entries:

```cmake
add_test(NAME unit   COMMAND mylib_tests)
add_test(NAME memory COMMAND mylib_tests --mem)
add_test(NAME stress COMMAND mylib_tests --stress --stress.scale=10)
```

Otherwise `ctest` runs the basic and measure phases only, and the memory tests
you wrote never execute.

## Meson

testrixa ships no Meson support, but a header-only library needs none:

```meson
project('mylib', 'cpp', default_options: ['cpp_std=c++17'])

testrixa = declare_dependency(
  include_directories: include_directories('third_party/testrixa/include'))

thread_dep = dependency('threads')

t = executable('mylib_tests',
               ['testMain.cpp', 'testParser.cpp'],
               dependencies: [testrixa, thread_dep])

test('mylib_tests', t)
test('memory', t, args: ['--mem'])
```

## Bazel

Same idea — the headers are the whole library:

```python
cc_library(
    name = "testrixa",
    hdrs = glob(["third_party/testrixa/include/**/*.h",
                 "third_party/testrixa/include/**/*.hpp"]),
    includes = ["third_party/testrixa/include"],
)

cc_test(
    name = "mylib_tests",
    srcs = ["testMain.cpp", "testParser.cpp"],
    deps = [":testrixa"],
    linkopts = ["-pthread"],
)
```

## Rules that apply to all of them

**1. `TEST_RUN_TERM` in exactly one translation unit.** It pulls in `main()`.
Two of them is a link error; none of them is a binary with no entry point.

**2. Header dependencies, or you will test the previous version.** This is not
hypothetical: testrixa's own build once lacked them, a rename left stale objects
behind, and the runner reported "Test case None" — which looked like a framework
bug for a while and was a Makefile bug.

**3. Do not put testrixa on a production build's include path.** It replaces
global `operator new`/`delete`, and `<testrixa/malloc_shim.h>` redefines
`malloc`. That is how the checks work and why it belongs to test binaries.
Keep it in the test target's include path, not the library's.

**4. `-Wall -Wextra` should be clean.** testrixa's public headers are built
warning-free under `-Wall -Wextra -Werror` on gcc 12–16 and clang 14–21. A
warning coming from inside `testrixa/` is worth reporting — a suppression in our
own build once hid two warnings that consumers saw and we did not.

**5. Every test file goes in the build twice** — once in the source list, once
in whatever dependency list your build uses. A file added to only one of them
compiles but never runs, and nothing says so.

## Next

- [10. CI Integration](10-ci-integration.md)

## Related

- [01. Installation](01-installation.md)
- [Guide index](README.md)

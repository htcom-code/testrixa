# 01. Installation

testrixa is header-only. There is nothing to compile, nothing to link, and no
dependency beyond the C++17 standard library — "installing" means putting the
headers somewhere your compiler looks.

- [What you need first](#what-you-need-first)
- [Where the headers go](#where-the-headers-go)
- [Five ways to install](#five-ways-to-install)
- [Verifying the install](#verifying-the-install)
- [Uninstalling](#uninstalling)

## What you need first

A C++17 compiler and, for two of the five methods, `make` or CMake.

| | Command | |
|---|---|---|
| **macOS** | `xcode-select --install` — gives you clang, `make` and the SDK.<br>CMake, if you want it: `brew install cmake` | ✅ run |
| **Debian / Ubuntu** | `sudo apt-get install g++ make cmake` | ✅ run |
| **Fedora / RHEL** | `sudo dnf install gcc-c++ make cmake` | ✅ run |
| **Alpine** | `apk add g++ make cmake` | ✅ run |
| **Arch** | `sudo pacman -S gcc make cmake` | not run |
| **Windows** | Visual Studio with the "Desktop development with C++" workload (MSVC + CMake). MinGW-w64 and WSL also work; WSL then follows the Linux row. | ✅ run (MSVC) |

✅ means the command was executed on that system and the resulting toolchain
built and ran the whole suite. Only the Arch row is untested — the container
image available for testing could not install those packages. The Windows row
covers MSVC through CMake; MinGW-w64 has not been tried.

Check what you have:

```sh
c++ --version      # or g++ --version
make --version
cmake --version    # optional
```

### Verified toolchains

These were built and run in full, not inferred from a support matrix:

| Platform | Compiler | Result |
|---|---|---|
| macOS 26, arm64 | Apple clang 21 | full suite |
| Debian 12 (bookworm) | gcc 12, clang 14 | full suite, CMake, ASan/UBSan/TSan, both consumer paths |
| Fedora 43 | gcc 16 | full suite |
| Alpine (musl) | gcc 15 | full suite; no backtraces — see below |
| Windows Server 2025 | MSVC 19.51 | suite + memory + stress + CLI regression, via CMake; no sanitizers |

Anything C++17 and reasonably recent should work. gcc 12 through 16 and clang 14
through 21 are the range actually exercised.

### Alpine and other musl systems

musl ships no `<execinfo.h>`, so a musl build **cannot capture backtraces**.
Everything else works: allocation tracking, leak detection, double-free and
overflow checks, quarantine, all of it. Only the "where did this leak come
from" stacks are unavailable, and the leak report says so rather than leaving
you to wonder:

```
  leak #1  4 bytes  age=0ms  (origin not recorded)
  this platform cannot capture backtraces, so origins are unavailable
  however --mem.backtrace is set
```

### Windows

Supported through **MSVC and CMake**. The suite builds warning-free at `/W4 /WX`
and passes in CI on every commit (MSVC 19.51, Windows Server 2025).

```powershell
cmake -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Three things are missing there, and it is better to know before you rely on it:

| | |
|---|---|
| **No sanitizers** | ASan/UBSan/TSan are run on Linux only. |
| **No `make`** | The GNU Makefile and `ci/run.sh` are POSIX shell. CMake is the only build path. |
| **Backtraces have no symbols** | `--mem.backtrace` gives addresses; nothing resolves them to names yet. Leak detection itself is unaffected. |

The CLI regression (`tests/cli_test.sh`) is a shell script, so it runs through
the bash that ships with Git for Windows. CMake finds it automatically and
warns if it cannot.

**MinGW-w64 has not been tried.** **WSL** works and follows the Debian or
Fedora row above.

## Where the headers go

testrixa is one directory, `testrixa/`, containing 8 public headers and a
`detail/` subdirectory. Wherever you put it, the parent of `testrixa/` is what
goes on your include path, because every include is written
`#include <testrixa/…>`.

| Destination | When |
|---|---|
| `/usr/local/include/testrixa/` | System-wide, one machine, you have `sudo`. The default for `make install`. |
| `$HOME/.local/include/testrixa/` | Your account only, no `sudo`. Add `-I$HOME/.local/include`. |
| `third_party/testrixa/` inside your repo | You want the version pinned and vendored with your source. Nothing to install. |
| A CMake build directory | `FetchContent` — CMake downloads and places it; you never see the path. |

There is no "correct" answer. Vendoring is the least surprising for a small
project; a system install is the least repetitive if you use it everywhere.

⚠️ **Do not put the headers on the include path of a production build.**
testrixa replaces global `operator new`/`delete` and, where you include
`<testrixa/malloc_shim.h>`, redefines `malloc`. That is how the memory checks
work, and it is why the library belongs in test binaries only. See
[SECURITY.md](../../SECURITY.md).

## Five ways to install

### 1. Copy the headers

The whole thing, with no build system involved:

```sh
git clone https://github.com/htcom-code/testrixa
cp -r testrixa/include/testrixa /usr/local/include/
```

or vendored into your own repository:

```sh
mkdir -p third_party
cp -r testrixa/include/testrixa third_party/
# then build with -Ithird_party
```

### 2. `make install`

Copies the headers and nothing else — no libraries, no CMake package files.

```sh
git clone https://github.com/htcom-code/testrixa
cd testrixa
make install                      # into /usr/local (may need sudo)
make install PREFIX=$HOME/.local  # into your account
```

Then build against it:

```sh
c++ -std=c++17 -I$HOME/.local/include -o mytest.out mytest.cpp
```

`DESTDIR` is honoured if you are staging for a package:

```sh
make install DESTDIR=/tmp/stage PREFIX=/usr
```

### 3. CMake install + `find_package`

This is the route that gives your consumers a proper imported target.

```sh
git clone https://github.com/htcom-code/testrixa
cd testrixa
cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build
cmake --install build
```

In your project:

```cmake
find_package(testrixa 0.1 REQUIRED)

add_executable(mytest mytest.cpp)
target_link_libraries(mytest PRIVATE testrixa::testrixa)
```

and configure with `-DCMAKE_PREFIX_PATH=$HOME/.local` if you installed
somewhere CMake does not search by default.

The imported target carries the include path and `cxx_std_17`, so you do not
set either yourself.

### 4. `add_subdirectory`

For a vendored copy, when your project already uses CMake:

```sh
git submodule add https://github.com/htcom-code/testrixa third_party/testrixa
```

```cmake
add_subdirectory(third_party/testrixa)

add_executable(mytest mytest.cpp)
target_link_libraries(mytest PRIVATE testrixa::testrixa)
```

testrixa builds its own tests and examples **only when it is the top-level
project**, so pulling it in this way does not add targets to your build.

### 5. `FetchContent`

No submodule, no vendoring — CMake fetches it at configure time:

```cmake
include(FetchContent)
FetchContent_Declare(
    testrixa
    GIT_REPOSITORY https://github.com/htcom-code/testrixa
    GIT_TAG        main            # pin a tag once releases exist
)
FetchContent_MakeAvailable(testrixa)

add_executable(mytest mytest.cpp)
target_link_libraries(mytest PRIVATE testrixa::testrixa)
```

Pin `GIT_TAG` to a tag or commit rather than `main` for anything you intend to
keep building.

## Verifying the install

Do not take the copy's word for it. Compile something.

```sh
cat > /tmp/smoke.cpp <<'EOF'
#define TEST_RUN_TERM
#include <testrixa/testrixa.h>

TESTCASE_BEGIN(smoke)

TESTCASE_BASIC(smoke) {
    TESTCASE_GROUP_START(it_works) {
        CHECK(true);
        CHECK_SAME(1 + 1, 2);
    } TESTCASE_GROUP_END(it_works)

    TESTCASE_RETURN
}

TESTCASE_MEASURE(smoke) { TESTCASE_RETURN }

TESTCASE_END(smoke)
EOF

c++ -std=c++17 -I$HOME/.local/include -o /tmp/smoke.out /tmp/smoke.cpp && /tmp/smoke.out
```

Drop the `-I` if you installed into `/usr/local`. You should see:

```
[smoke::BASIC]
----------------------------------------------------------------------------------
| Group Name                                     | Lvl|   Total| Success|    Fail|
----------------------------------------------------------------------------------
| it_works                                       |   0|       2|       2|       0|
|--------------------------------------------------------------------------------|
| Total Test case                                     |       2|       2|       0|
----------------------------------------------------------------------------------
...
ALL test case finised total[1] success[1] fail[0]
```

and `echo $?` should print `0`. If the exit code is not zero, the run failed
even if the table looks fine — that is the point of it.

Check what actually landed:

```sh
ls $HOME/.local/include/testrixa/
# malloc_shim.h  memory.h  mock.h  options.h  stress.h  testrixa.h  thread.h
# traits.h  detail/
```

Eight public headers plus `detail/`. If `mock.h` or `thread.h` are missing you
have an older copy.

## Uninstalling

```sh
make uninstall                      # removes $PREFIX/include/testrixa
make uninstall PREFIX=$HOME/.local
```

or, for a CMake install, delete the directory and the package files:

```sh
rm -rf $HOME/.local/include/testrixa $HOME/.local/lib/cmake/testrixa
```

Nothing else is written anywhere.

## Next

- [02. Getting Started](02-getting-started.md) — from an empty directory to a
  passing test
- [09. Build Integration](09-build-integration.md) — wiring testrixa into an
  existing project's build

## Related

- [README](../../README.md) — what testrixa is
- [Guide index](README.md)

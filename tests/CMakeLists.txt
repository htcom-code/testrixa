# testrixa's own test suite.

add_executable(testrixa_tests
    testMain.cpp
    testTEST.cpp
    testOptions.cpp
    testMemory.cpp
    testLeak.cpp
    testCompare.cpp
    testShim.cpp
    testQuarantine.cpp
    testReporter.cpp
    testBacktrace.cpp
    testFixture.cpp
    testStress.cpp
    testThread.cpp
    testMock.cpp)

find_package(Threads REQUIRED)
target_link_libraries(testrixa_tests PRIVATE testrixa::testrixa Threads::Threads)
target_compile_definitions(testrixa_tests PRIVATE TEST_TERM)
target_compile_options(testrixa_tests PRIVATE ${TESTRIXA_WARNINGS})

add_test(NAME testrixa_self_test COMMAND testrixa_tests)

# The memory and stress phases are opt-in, so a plain run does not touch them.
# ctest saw only the first line above until now, which meant the memory suite --
# global operator new replacement, aligned allocation, backtrace capture, the
# most platform-sensitive code here -- was never exercised through CMake at all.
# On Linux `make check` covered it; on Windows, where CMake is the only build
# path, nothing did.
#
# The stress phase had it worse: no CI on any platform ran it. A phase that
# never runs is indistinguishable from one that does not exist.
add_test(NAME testrixa_memory COMMAND testrixa_tests --mem)
add_test(NAME testrixa_stress COMMAND testrixa_tests --stress --stress.scale=10)

# Pins the terminal runner's command line. The prototype's parser shipped with
# no tests, so this is the only thing the option registry can be checked against.
#
# It is a POSIX shell script, which Windows cannot execute directly -- ctest
# reports "inappropriate file type or format". Dropping it there would leave
# argument handling unchecked on the one platform whose path syntax differs
# most, so it runs through bash instead: Git for Windows ships one, and the
# GitHub runners have it on PATH.
if(WIN32)
    find_program(TESTRIXA_BASH NAMES bash)
    if(TESTRIXA_BASH)
        add_test(NAME testrixa_cli_test
                 COMMAND ${TESTRIXA_BASH} ${CMAKE_CURRENT_SOURCE_DIR}/cli_test.sh
                         $<TARGET_FILE:testrixa_tests>)
    else()
        # Say so loudly. A CLI regression that quietly does not run is worse
        # than one that is absent, because the report looks the same as green.
        message(WARNING
            "testrixa: no bash found -- the CLI regression (98 assertions) will "
            "NOT run in this configuration. Install Git for Windows or run it "
            "by hand: bash tests/cli_test.sh <path-to-runner>")
    endif()
else()
    add_test(NAME testrixa_cli_test
             COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/cli_test.sh $<TARGET_FILE:testrixa_tests>)
endif()

# Contract check for the macro naming scheme: compiled, never linked, so it
# cannot add a test case to the runner's output. Building it IS the assertion --
# checkPrefix.cpp fails with #error if the short aliases survive
# TESTRIXA_NO_SHORT_MACROS.
add_library(testrixa_check_prefix OBJECT checkPrefix.cpp)
target_link_libraries(testrixa_check_prefix PRIVATE testrixa::testrixa)
target_compile_definitions(testrixa_check_prefix PRIVATE TEST_TERM)
target_compile_options(testrixa_check_prefix PRIVATE ${TESTRIXA_WARNINGS})

# Including testrixa must not drop anything into the consumer's global
# namespace -- core.hpp used to carry a global `using namespace std;`, so a
# consumer with their own `string` or `count` failed to compile.
add_library(testrixa_check_namespace OBJECT checkNamespace.cpp)
target_link_libraries(testrixa_check_namespace PRIVATE testrixa::testrixa)
target_compile_definitions(testrixa_check_namespace PRIVATE TEST_TERM)
target_compile_options(testrixa_check_namespace PRIVATE ${TESTRIXA_WARNINGS})

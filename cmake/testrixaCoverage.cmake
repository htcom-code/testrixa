# testrixa_add_coverage(<target> [NAME <name>] [EXCLUDE <regex>...])
#
# Turns on coverage instrumentation for <target> and adds a custom target that
# runs it and reports **your** coverage -- not testrixa's, and not the standard
# library's.
#
#     testrixa_add_coverage(my_tests)
#     cmake --build build --target coverage
#
# Why this exists
# ---------------
# Measuring coverage is the compiler's job and this does not try to do it. What
# it does is make the resulting number mean the code you wrote.
#
# testrixa is header-only, so every one of its headers is compiled into your
# test binary and the instrumentation counts them. Whether that reaches your
# report depends on where testrixa lives:
#
#   installed, or included from outside the project   gcovr's root filter
#                                                     already drops it
#   vendored (third_party/testrixa), or FetchContent  it does not
#
# Measured on a 14-line example whose own coverage is 100%: vendored and
# unfiltered, gcovr reported 36% over 973 lines. The missing 959 were testrixa's
# headers. Which of those layouts a consumer picked is not something they should
# have to think about at report time, so the exclusion is applied either way.
#
# Requires gcovr (`pip install gcovr`, or `apt install gcovr`). MSVC is not
# supported -- gcov has no MSVC equivalent; see docs/guide/12-coverage.md.

function(testrixa_add_coverage target)
    cmake_parse_arguments(TRXCOV "" "NAME" "EXCLUDE" ${ARGN})

    if(NOT TARGET ${target})
        message(FATAL_ERROR "testrixa_add_coverage: no such target '${target}'")
    endif()

    set(name "${TRXCOV_NAME}")
    if(NOT name)
        set(name coverage)
    endif()

    # MSVC has no gcov. Say so once and do nothing -- adding a target that
    # cannot work would be worse than not having one.
    if(MSVC)
        message(STATUS
            "testrixa: coverage is not available with MSVC (no gcov). "
            "The '${name}' target was not created.")
        return()
    endif()

    if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
        message(STATUS
            "testrixa: coverage needs gcc or clang; compiler is "
            "'${CMAKE_CXX_COMPILER_ID}'. The '${name}' target was not created.")
        return()
    endif()

    # Instrumentation has to reach both stages. Putting --coverage only on the
    # compile line links without the runtime and fails with undefined
    # references to __gcov_* -- the same shape as the sanitizer flags.
    target_compile_options(${target} PRIVATE --coverage -O0 -g)
    target_link_options(${target} PRIVATE --coverage)

    find_program(TESTRIXA_GCOVR NAMES gcovr)
    if(NOT TESTRIXA_GCOVR)
        # Loudly. A coverage target that silently reports nothing looks the
        # same as one that reported good news.
        message(WARNING
            "testrixa: gcovr not found -- the '${name}' target will not be "
            "created. Install it with `pip install gcovr` or your package "
            "manager, then re-run CMake.")
        return()
    endif()

    # gcov and llvm-cov are not interchangeable: a clang-built binary needs
    # `llvm-cov gcov`. gcovr is told which one to use rather than guessing.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(gcov_tool "llvm-cov gcov")
    else()
        set(gcov_tool "gcov")
    endif()

    # `.*/testrixa/.*` catches the vendored and FetchContent layouts by the
    # directory name, which is the one thing every arrangement shares.
    set(excludes --exclude "${CMAKE_BINARY_DIR}/.*" --exclude ".*/testrixa/.*")
    foreach(pattern IN LISTS TRXCOV_EXCLUDE)
        list(APPEND excludes --exclude "${pattern}")
    endforeach()

    add_custom_target(${name}
        COMMAND $<TARGET_FILE:${target}>
        COMMAND ${TESTRIXA_GCOVR}
                --root "${CMAKE_SOURCE_DIR}"
                --gcov-executable "${gcov_tool}"
                ${excludes}
                --print-summary
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Running ${target} and reporting coverage of your sources"
        USES_TERMINAL
        VERBATIM)
endfunction()

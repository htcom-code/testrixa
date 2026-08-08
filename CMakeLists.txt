cmake_minimum_required(VERSION 3.14)

project(testrixa
        VERSION 0.1.0
        DESCRIPTION "C/C++ testing platform"
        LANGUAGES CXX)

# GNUInstallDirs must come before the target is described: CMAKE_INSTALL_INCLUDEDIR
# is used inside $<INSTALL_INTERFACE:> below, and an unset variable there expands
# to nothing -- the exported target then carries no include directory at all and
# every find_package() consumer fails with "testrixa/testrixa.h not found".
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ---------------------------------------------------------------------------
# The library
#
# Header-only, so there is nothing to compile: an INTERFACE target carries the
# include path and the language requirement to whoever links it.
# ---------------------------------------------------------------------------
add_library(testrixa INTERFACE)
add_library(testrixa::testrixa ALIAS testrixa)

target_include_directories(testrixa INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(testrixa INTERFACE cxx_std_17)

# testrixa_add_coverage(). Included here so add_subdirectory() and FetchContent
# consumers get it too -- find_package() consumers get it from the generated
# package config, which includes the same file.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/testrixaCoverage.cmake)

# ---------------------------------------------------------------------------
# Options
#
# Tests and examples default ON only when testrixa is the top-level project.
# A consumer pulling us in via add_subdirectory()/FetchContent gets the library
# and nothing else. (PROJECT_IS_TOP_LEVEL would say this in one line but needs
# CMake 3.21; the floor here is 3.14.)
# ---------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(TESTRIXA_IS_TOP_LEVEL ON)
else()
    set(TESTRIXA_IS_TOP_LEVEL OFF)
endif()

# Warning flags for our own targets only -- never on the INTERFACE library, or
# every consumer would inherit them. The build is warning-free on gcc 12 and
# clang 14, and -Werror is what keeps it that way.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(TESTRIXA_WARNINGS -Wall -Wextra -Werror)
elseif(MSVC)
    set(TESTRIXA_WARNINGS /W4 /WX)
endif()

option(TESTRIXA_BUILD_TESTS    "Build testrixa's own test suite" ${TESTRIXA_IS_TOP_LEVEL})
option(TESTRIXA_BUILD_EXAMPLES "Build testrixa examples"         ${TESTRIXA_IS_TOP_LEVEL})

if(TESTRIXA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(TESTRIXA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ---------------------------------------------------------------------------
# Install / find_package support
# ---------------------------------------------------------------------------
install(TARGETS testrixa EXPORT testrixaTargets)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT testrixaTargets
        FILE testrixaTargets.cmake
        NAMESPACE testrixa::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/testrixa)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/testrixaConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/testrixaConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/testrixa)

# 0.x releases are allowed to break on a minor bump, so an exact-minor match is
# the honest compatibility claim here.
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/testrixaConfigVersion.cmake
    COMPATIBILITY SameMinorVersion)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/testrixaConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/testrixaConfigVersion.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/testrixaCoverage.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/testrixa)

# A stand-in for somebody else's project.
#
# Built against an *installed* testrixa, not the source tree, so it exercises
# the export/install path that add_subdirectory never touches. That path broke
# once already: include(GNUInstallDirs) sat below target_include_directories,
# so $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> expanded to nothing and
# the exported target carried no include directory at all. Everything in-tree
# kept working; only a real consumer noticed.
#
#   cmake --install <build> --prefix /tmp/prefix
#   cmake -S tests/consumer -B /tmp/consumer -DCMAKE_PREFIX_PATH=/tmp/prefix
#   cmake --build /tmp/consumer && /tmp/consumer/consumer_smoke

cmake_minimum_required(VERSION 3.14)
project(testrixa_consumer LANGUAGES CXX)

find_package(testrixa REQUIRED)

add_executable(consumer_smoke main.cpp)
target_link_libraries(consumer_smoke PRIVATE testrixa::testrixa)

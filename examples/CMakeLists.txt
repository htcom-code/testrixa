# Examples are built, not run as tests: compiling them is what keeps them from
# rotting when the public headers change.

add_executable(testrixa_example testExample.cpp)
target_link_libraries(testrixa_example PRIVATE testrixa::testrixa)
target_compile_options(testrixa_example PRIVATE ${TESTRIXA_WARNINGS})

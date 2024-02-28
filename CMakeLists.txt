cmake_minimum_required (VERSION 3.6)
project (qgcjson C)

set(C_STANDARD 99)

# add_compile_definitions(QGCJSON_DEBUG)

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-std=c99)
endif()

add_library(qgcjson qgcjson.c)
add_executable(qgcjson_test test.c)
target_link_libraries(qgcjson_test qgcjson)
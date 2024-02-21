cmake_minimum_required (VERSION 3.6)
project (qgcjson C)

add_compile_options(-std=c99)

# add_compile_definitions(QGCJSON_DEBUG)

add_library(qgcjson qgcjson.c)
add_executable(qgcjson_test test.c)
target_link_libraries(qgcjson_test qgcjson)
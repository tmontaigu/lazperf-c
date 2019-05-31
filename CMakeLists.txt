cmake_minimum_required(VERSION 3.10)
project(lazperf-c)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD  11)

include_directories(laz-perf)
include_directories(.)
add_library(lazperf-c lazperf_c.cpp lazperf_c.h stream_utils.h)

add_executable(test-decompression tests/test_decompression.c)
set_property(TARGET test-decompression PROPERTY C_STANDARD 11)
target_link_libraries(test-decompression lazperf-c)
cmake_minimum_required(VERSION 3.10)
project(lazperf-c)

set(CMAKE_CXX_STANDARD 11)

include_directories(.)
add_library(lazperf-c lazperf_c.cpp lazperf_c.h)

add_executable(test-decompression tests/test_decompression.c)
target_link_libraries(test-decompression lazperf-c)
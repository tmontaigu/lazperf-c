cmake_minimum_required(VERSION 3.10)
project(lazperf-c)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD  11)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

include_directories(laz-perf)
include_directories(.)
add_library(lazperf-c lazperf_c.cpp lazperf_c.h stream_utils.h)

add_executable(test-simple tests/test_simple.c)
set_property(TARGET test-simple PROPERTY C_STANDARD 11)
target_link_libraries(test-simple lazperf-c)
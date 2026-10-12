cmake_minimum_required(VERSION 3.19)
project(memlrhythm_tests CXX)

# Host test for RhythmCore: no Max, no SDK, runs anywhere including Linux.
#   cmake -S source/tests -B build-tests && cmake --build build-tests && ./build-tests/rhythm_test
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif ()

add_executable(rhythm_test rhythm_test.cpp)
target_include_directories(rhythm_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../common")

enable_testing()
add_test(NAME rhythm_test COMMAND rhythm_test)

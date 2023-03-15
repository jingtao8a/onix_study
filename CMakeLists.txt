cmake_minimum_required(VERSION 3.18)

project("tests")

add_executable(testtypes tests/testtypes.c)
target_include_directories(testtypes PUBLIC src/include/)

add_executable(testhello tests/testhello.c)

set(CMAKE_C_FLAGS "-g")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

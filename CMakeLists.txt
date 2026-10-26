cmake_minimum_required(VERSION 3.28)
project(bodgewire LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BW_BUILD_TESTS "Build unit tests" ON)

add_library(bw_warnings INTERFACE)
target_compile_options(bw_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Werror)

add_subdirectory(src/sim)
add_subdirectory(src/parts)
add_subdirectory(src/netlist)
add_subdirectory(src/cli)
add_subdirectory(bench)

if(BW_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

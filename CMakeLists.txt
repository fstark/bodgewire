cmake_minimum_required(VERSION 3.28)
project(bodgewire LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# No C++ modules: without this CMake scans every file with clang-scan-deps on Linux clang.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(BW_BUILD_TESTS "Build unit tests" ON)
option(BW_BUILD_UI "Build bw_ui" ON)

add_library(bw_warnings INTERFACE)
target_compile_options(bw_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Werror)
# Newer clangs (Emscripten's) put this in -Wextra; designated initializers here leave fields out
# on purpose, to their defaults.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wmissing-designated-field-initializers BW_HAS_MISSING_DESIGNATED)
if(BW_HAS_MISSING_DESIGNATED)
  target_compile_options(bw_warnings INTERFACE -Wno-missing-designated-field-initializers)
endif()
# Clang 23 calls __COUNTER__ a C2y extension under -Wpedantic; Catch2's TEST_CASE and the Z80
# core's resumable cycles both use it.
check_cxx_compiler_flag(-Wc2y-extensions BW_HAS_C2Y_EXTENSIONS)
if(BW_HAS_C2Y_EXTENSIONS)
  target_compile_options(bw_warnings INTERFACE -Wno-c2y-extensions)
endif()

add_subdirectory(src/sim)
add_subdirectory(src/devices)
add_subdirectory(src/runner)
add_subdirectory(src/instruments)
add_subdirectory(src/parts)
add_subdirectory(src/asm)
add_subdirectory(src/netlist)
add_subdirectory(src/manual)
add_subdirectory(src/board)
add_subdirectory(src/view)
add_subdirectory(src/machine)
add_subdirectory(src/session)
add_subdirectory(src/cli)
if(BW_BUILD_UI)
  add_subdirectory(src/ui)
endif()
add_subdirectory(bench)

if(BW_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

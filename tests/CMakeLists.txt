include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(bw_tests
  board/kicad_netlist_test.cpp
  board/sexpr_test.cpp
  machines/test_pattern_test.cpp
  netlist/builder_test.cpp
  netlist/diagnostic_test.cpp
  netlist/parser_test.cpp
  netlist/sample_test.cpp
  netlist/units_test.cpp
  parts/catalogue_test.cpp
  parts/gate_package_test.cpp
  parts/ls161_test.cpp
  parts/ls74_test.cpp
  parts/oscillator_test.cpp
  sim/check_test.cpp
  sim/circuit_test.cpp
  sim/event_queue_test.cpp
  sim/logic_test.cpp
  sim/primitives_test.cpp
  sim/recorder_test.cpp
  sim/rng_test.cpp
  sim/simulator_test.cpp
  sim/time_test.cpp
  sim/vcd_test.cpp
)
target_link_libraries(bw_tests PRIVATE bw_board bw_warnings Catch2::Catch2WithMain)
target_compile_definitions(bw_tests PRIVATE BW_NETLIST_DIR="${PROJECT_SOURCE_DIR}/netlists")

include(Catch)
catch_discover_tests(bw_tests)

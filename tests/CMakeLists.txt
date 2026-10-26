include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(bw_tests
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
target_link_libraries(bw_tests PRIVATE bw_sim bw_warnings Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(bw_tests)

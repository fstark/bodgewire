include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(bw_tests
  sim/check_test.cpp
  sim/rng_test.cpp
  sim/time_test.cpp
)
target_link_libraries(bw_tests PRIVATE bw_sim bw_warnings Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(bw_tests)

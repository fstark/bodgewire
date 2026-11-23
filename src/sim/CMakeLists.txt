add_library(bw_sim STATIC
  check.cpp
  circuit.cpp
  component.cpp
  event_queue.cpp
  logic.cpp
  recorder.cpp
  simulator.cpp
  time.cpp
  vcd.cpp
)
target_include_directories(bw_sim PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(bw_sim PUBLIC bw_text PRIVATE bw_warnings)

# Components for tests and benchmarks only (Gate, DFlipFlop, ClockSource, Stimulus...): not
# linked by bw or bw_ui.
add_library(bw_sim_fixtures STATIC fixtures.cpp)
target_link_libraries(bw_sim_fixtures PUBLIC bw_sim PRIVATE bw_warnings)

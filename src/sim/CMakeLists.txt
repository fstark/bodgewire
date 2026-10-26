add_library(bw_sim STATIC
  check.cpp
  circuit.cpp
  component.cpp
  event_queue.cpp
  logic.cpp
  primitives.cpp
  recorder.cpp
  simulator.cpp
  vcd.cpp
)
target_include_directories(bw_sim PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(bw_sim PRIVATE bw_warnings)

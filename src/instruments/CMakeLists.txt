add_library(bw_instruments STATIC
  logic_analyzer.cpp
  logic_probe.cpp
  multimeter.cpp
  oscilloscope.cpp
)
target_link_libraries(bw_instruments PUBLIC bw_sim PRIVATE bw_warnings)

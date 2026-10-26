add_library(bw_netlist STATIC
  diagnostic.cpp
  parser.cpp
  units.cpp
)
target_link_libraries(bw_netlist PUBLIC bw_parts PRIVATE bw_warnings)

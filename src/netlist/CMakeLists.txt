add_library(bw_netlist STATIC
  builder.cpp
  diagnostic.cpp
  net_pins.cpp
  parser.cpp
  units.cpp
)
target_link_libraries(bw_netlist PUBLIC bw_parts PRIVATE bw_warnings)

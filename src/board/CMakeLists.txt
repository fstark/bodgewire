add_library(bw_board STATIC
  board_check.cpp
  copper.cpp
  footprint.cpp
  geometry.cpp
  kicad_board.cpp
  kicad_netlist.cpp
  kicad_pcb_writer.cpp
  place_rules.cpp
  placer.cpp
  sexpr.cpp
)
target_link_libraries(bw_board PUBLIC bw_netlist PRIVATE bw_warnings)

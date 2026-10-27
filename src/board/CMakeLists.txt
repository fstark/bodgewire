add_library(bw_board STATIC
  copper.cpp
  kicad_board.cpp
  kicad_netlist.cpp
  sexpr.cpp
)
target_link_libraries(bw_board PUBLIC bw_netlist PRIVATE bw_warnings)

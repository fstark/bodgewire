add_library(bw_board STATIC
  sexpr.cpp
)
target_link_libraries(bw_board PUBLIC bw_netlist PRIVATE bw_warnings)

add_library(bw_manual STATIC markdown.cpp manual.cpp)
target_link_libraries(bw_manual PUBLIC bw_netlist PRIVATE bw_warnings)

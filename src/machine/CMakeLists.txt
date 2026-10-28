add_library(bw_machine STATIC
  crack.cpp
  puzzle.cpp
)
target_link_libraries(bw_machine PUBLIC bw_board PRIVATE bw_warnings)

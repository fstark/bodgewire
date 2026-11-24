add_library(bw_machine STATIC
  crack.cpp
  electrical.cpp
  input_edges.cpp
  knob_setting.cpp
  machine.cpp
  machine_list.cpp
  press.cpp
  puzzle.cpp
  repair.cpp
  typing.cpp
)
target_link_libraries(bw_machine PUBLIC bw_board PRIVATE bw_manual bw_warnings)

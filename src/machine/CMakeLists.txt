add_library(bw_machine STATIC
  crack.cpp
  electrical.cpp
  knob_setting.cpp
  machine.cpp
  press.cpp
  puzzle.cpp
  repair.cpp
)
target_link_libraries(bw_machine PUBLIC bw_board PRIVATE bw_warnings)

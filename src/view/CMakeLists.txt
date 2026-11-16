add_library(bw_view STATIC
  board_scene.cpp
  digit_shape.cpp
  focus.cpp
  keyboard_layout.cpp
  matrix_keyboard_layout.cpp
  time_text.cpp
  time_view.cpp
  viewport.cpp
)
target_link_libraries(bw_view PUBLIC bw_board bw_instruments PRIVATE bw_warnings)

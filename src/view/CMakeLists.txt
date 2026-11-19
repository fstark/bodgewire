add_library(bw_view STATIC
  bench_places.cpp
  binder.cpp
  board_marks.cpp
  board_scene.cpp
  digit_shape.cpp
  focus.cpp
  machine_slot.cpp
  keyboard_layout.cpp
  matrix_keyboard_layout.cpp
  pencil_marks.cpp
  time_text.cpp
  time_view.cpp
  tv_controls.cpp
  viewport.cpp
)
target_link_libraries(bw_view PUBLIC bw_board bw_devices bw_instruments PRIVATE bw_warnings)

add_library(bw_view STATIC
  bench_places.cpp
  contents_defaults.cpp
  binder.cpp
  board_marks.cpp
  board_scene.cpp
  chip_body.cpp
  digit_shape.cpp
  flip_corner.cpp
  focus.cpp
  launcher_lanes.cpp
  machine_slot.cpp
  memory_contents.cpp
  output_frame.cpp
  pace.cpp
  keyboard_layout.cpp
  matrix_keyboard_layout.cpp
  pencil_marks.cpp
  power_cycle.cpp
  time_text.cpp
  time_view.cpp
  tv_controls.cpp
  viewport.cpp
)
target_link_libraries(bw_view PUBLIC bw_asm bw_board bw_devices bw_instruments PRIVATE bw_warnings)

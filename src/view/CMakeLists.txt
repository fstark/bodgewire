add_library(bw_view STATIC
  board_scene.cpp
  viewport.cpp
)
target_link_libraries(bw_view PUBLIC bw_board PRIVATE bw_warnings)

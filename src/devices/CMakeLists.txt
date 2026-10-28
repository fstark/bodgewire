add_library(bw_devices STATIC
  bmp.cpp
  tv.cpp
)
target_link_libraries(bw_devices PUBLIC bw_sim PRIVATE bw_warnings)

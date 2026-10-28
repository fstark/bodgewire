add_library(bw_runner STATIC
  dispatcher.cpp
  tv_consumer.cpp
)
target_link_libraries(bw_runner PUBLIC bw_devices PRIVATE bw_warnings)

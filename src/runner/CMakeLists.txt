add_library(bw_runner STATIC
  analyzer_consumer.cpp
  dispatcher.cpp
  probe_consumer.cpp
  runner.cpp
  tv_consumer.cpp
)
target_link_libraries(bw_runner PUBLIC bw_devices bw_instruments PRIVATE bw_warnings)

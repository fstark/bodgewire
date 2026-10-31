add_library(bw_runner STATIC
  analyzer_consumer.cpp
  dispatcher.cpp
  meter_consumer.cpp
  probe_consumer.cpp
  runner.cpp
  scope_consumer.cpp
  tv_consumer.cpp
)
target_link_libraries(bw_runner PUBLIC bw_devices bw_instruments PRIVATE bw_warnings)

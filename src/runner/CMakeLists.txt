add_library(bw_runner STATIC
  analyzer_consumer.cpp
  channel_nets.cpp
  dispatcher.cpp
  led_consumer.cpp
  meter_consumer.cpp
  probe_consumer.cpp
  runner.cpp
  scope_consumer.cpp
  single_net_consumer.cpp
  tv_consumer.cpp
)
target_link_libraries(bw_runner PUBLIC bw_devices bw_instruments PRIVATE bw_warnings)

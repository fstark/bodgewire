add_library(bw_session STATIC analyzer_setup.cpp input_devices.cpp leads.cpp scope_setup.cpp session.cpp)
target_link_libraries(bw_session PUBLIC bw_machine bw_runner PRIVATE bw_warnings)

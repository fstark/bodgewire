add_executable(bw main.cpp)
target_link_libraries(bw PRIVATE bw_board bw_devices bw_instruments bw_machine bw_warnings)

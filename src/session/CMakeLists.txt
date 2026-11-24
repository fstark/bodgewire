add_library(bw_session STATIC analyzer_setup.cpp input_devices.cpp leads.cpp meter_setup.cpp probe_setup.cpp procedural_puzzle.cpp puzzle_maker.cpp scope_setup.cpp session.cpp vetting.cpp)
target_link_libraries(bw_session PUBLIC bw_machine bw_runner PRIVATE bw_warnings)

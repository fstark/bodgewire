add_executable(bw main.cpp)
target_link_libraries(bw PRIVATE bw_asm bw_board bw_devices bw_instruments bw_machine bw_warnings)
target_compile_definitions(bw PRIVATE BW_FOOTPRINT_DIR="${PROJECT_SOURCE_DIR}/footprints")

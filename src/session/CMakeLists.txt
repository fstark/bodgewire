add_library(bw_session STATIC session.cpp)
target_link_libraries(bw_session PUBLIC bw_machine bw_runner PRIVATE bw_warnings)

add_library(bw_text STATIC text.cpp)
target_include_directories(bw_text PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(bw_text PRIVATE bw_warnings)

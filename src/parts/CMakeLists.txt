add_library(bw_parts STATIC
  catalogue.cpp
  gate_package.cpp
  ls161.cpp
  ls74.cpp
  oscillator.cpp
)
target_link_libraries(bw_parts PUBLIC bw_sim PRIVATE bw_warnings)

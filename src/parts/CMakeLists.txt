add_library(bw_parts STATIC
  button.cpp
  catalogue.cpp
  connector.cpp
  digit.cpp
  gate_package.cpp
  led.cpp
  ls107.cpp
  ls157.cpp
  ls161.cpp
  ls164.cpp
  ls175.cpp
  ls47.cpp
  ls48.cpp
  ls74.cpp
  ls90.cpp
  ls93.cpp
  oscillator.cpp
  paddle.cpp
  rom.cpp
  rom_contents.cpp
)
target_link_libraries(bw_parts PUBLIC bw_sim PRIVATE bw_warnings)

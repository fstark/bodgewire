add_library(bw_asm STATIC
  asm6502.cpp
  assembler.cpp
  asmz80.cpp
  mos6502_disassembler.cpp
  z80_disassembler.cpp
)
target_link_libraries(bw_asm PUBLIC bw_parts PRIVATE bw_warnings)

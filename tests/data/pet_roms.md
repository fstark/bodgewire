# The PET 2001's ROMs

The BW-2001 runs Commodore's own ROMs: BASIC 1, MAME's `pet2001` set (BIOS `basic1o`), 2048 bytes each. They are committed in `machines/130-bw2001/roms/` (decided 2026-11-22; until then they were a reference only, kept out of git in `external/roms/pet/`). Checked on 2026-11-15 and again when committed:

| File | Socket | Address | SHA1 |
|---|---|---|---|
| `901447-01.bin` | UH1 | `$C000` | `831db40324113ee996c434d38b4add3fd1f820bd` |
| `901447-02.bin` | UH5 | `$C800` | `70c0f4fa67a70995b168668c957c3fcf2c8641bd` |
| `901447-03.bin` | UH2 | `$D000` | `4bf2c20c51a63d213886957485ebef336bb803d0` |
| `901447-04.bin` | UH6 | `$D800` | `d293972d529023d8fd1f493149e4777b5c253a69` |
| `901447-05.bin` | UH3 | `$E000` | `f02f5fb492ba93dbbd390f24c10f7a832dec432a` |
| `901447-06.bin` | UH4 | `$F000` | `960717282878e7de893d87242ddf9d1512be162e` |
| `901447-07.bin` | UH7 | `$F800` | `d440f2510bc52e20c3d6bc8b9ded9cea7f462a9c` |

There is no ROM at `$E800`–`$EFFF`: that range is the I/O (PIA1 `$E810`, PIA2 `$E820`, VIA `$E840`).

The character ROM 901447-08 (UA2, SHA1 `3e067cc621e4beafca2b90cb8f6dba975df2855b`) is `machines/130-bw2001/roms/901447-08.bin` (decided 2026-11-15).

The tests reach them through `tests/netlist/pet_roms.hpp`: `tests/netlist/pet_roms_test.cpp`, the BASIC 1 boot on the bench (`tests/netlist/bw2001_bench_test.cpp`), and every BW-2001 Machine and Puzzle case that does not need our firmware.

## Our firmware, the debug ROMs

P5p's firmware (Tiny BASIC, the editor, the monitor, the self-test, and the harness signal the typist waits on) is in `machines/130-bw2001/debug-roms/`. `--roms machines/130-bw2001/debug-roms` (`bw run`, `bw_ui`) runs it: its `sockets.txt` says which image fills which socket. The tests that drive the firmware load it the same way (`Bw2001DebugCircuit`, `tests/machines/bw2001_rig.hpp`).

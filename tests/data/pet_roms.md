# Real PET ROMs for tests

The BW-2001 runs our own firmware. The PET 2001's own ROMs are a reference only. The user owns them, and they never go in git. Tests that use them are skipped when the files are missing (`tests/netlist/external_rom.hpp`).

Put the files in `external/roms/pet/`, which is git-ignored, either as copies or through a symlink to wherever they are kept. These are BASIC 1, MAME's `pet2001` set (BIOS `basic1o`), 2048 bytes each, checked on 2026-11-15:

| File | Socket | Address | SHA1 |
|---|---|---|---|
| `rom-1-c000.901447-01.bin` | UH1 | `$C000` | `831db40324113ee996c434d38b4add3fd1f820bd` |
| `rom-1-c800.901447-02.bin` | UH5 | `$C800` | `70c0f4fa67a70995b168668c957c3fcf2c8641bd` |
| `rom-1-d000.901447-03.bin` | UH2 | `$D000` | `4bf2c20c51a63d213886957485ebef336bb803d0` |
| `rom-1-d800.901447-04.bin` | UH6 | `$D800` | `d293972d529023d8fd1f493149e4777b5c253a69` |
| `rom-1-e000.901447-05.bin` | UH3 | `$E000` | `f02f5fb492ba93dbbd390f24c10f7a832dec432a` |
| `rom-1-f000.901447-06.bin` | UH4 | `$F000` | `960717282878e7de893d87242ddf9d1512be162e` |
| `rom-1-f800.901447-07.bin` | UH7 | `$F800` | `d440f2510bc52e20c3d6bc8b9ded9cea7f462a9c` |

There is no ROM at `$E800`–`$EFFF`: that range is the I/O (PIA1 `$E810`, PIA2 `$E820`, VIA `$E840`).

The character ROM is the exception: 901447-08 (UA2, SHA1 `3e067cc621e4beafca2b90cb8f6dba975df2855b`) ships with the BW-2001 as `roms/901447-08.bin` (decided 2026-11-15), since every screen of the Machine goes through it.

They are used by `tests/netlist/external_rom_test.cpp` and by the BASIC 1 boot on the bench (`tests/netlist/bw2001_bench_test.cpp`).

## Running the BW-2001 on them (P5q2)

`--roms DIR` (`bw run`, `bw_ui`) takes a directory whose files are named after the sockets. Make one of links to the files above:

```
cd external/roms && mkdir pet-basic1
ln -s ../pet/rom-1-c000.901447-01.bin pet-basic1/UH1.bin
ln -s ../pet/rom-1-c800.901447-02.bin pet-basic1/UH5.bin
ln -s ../pet/rom-1-d000.901447-03.bin pet-basic1/UH2.bin
ln -s ../pet/rom-1-d800.901447-04.bin pet-basic1/UH6.bin
ln -s ../pet/rom-1-e000.901447-05.bin pet-basic1/UH3.bin
ln -s ../pet/rom-1-f000.901447-06.bin pet-basic1/UH4.bin
ln -s ../pet/rom-1-f800.901447-07.bin pet-basic1/UH7.bin
```

then `bw_ui netlists/bw2001.bwn --roms external/roms/pet-basic1`: Commodore's PET 2001 on the BW-2001's Board. The tests make the same links in a temporary directory.

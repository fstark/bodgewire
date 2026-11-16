# zx80_ic2.bin — the Sinclair ZX80 ROM

The BW-80's ROM: Sinclair's 4K ZX80 ROM, IC2 on the ZX80's board (a 2332 mask ROM, "8332" on its parts list), as the user dumped it. 4096 bytes, SHA1 `b6769a3197c77009e0933e038c15b43cf4c98c7a`.

It ships in git (the user, 2026-11-16: the ZX80 ROM is distributed with emulators), so the BW-80 runs Sinclair's own code and P5w (our own ROM) is skipped. © Sinclair Research; the BW-80's netlist loads it as `part IC2 2332 select=LL contents=../roms/zx80_ic2.bin`.

- `$0000`–`$0DFF`: the program (4K BASIC, the editor, the display driver: `MASK-INT` at `$0038`, `DISP-1`/`KEYBOARD` at `$013C`).
- `$0E00`–`$0FFF`: the character set, 64 glyphs of 8 rows, top row first, bit 7 the leftmost dot: code 0 space, `$30` `K`. The display reads it with `I` = `$0E` in each refresh.

Reference for its code: Geoff Wearmouth's assembly listing of the ZX80 ROM (`zx80 rom source.txt`, k1.spdns.de/Vintage/Sinclair/80/Sinclair ZX80/ROMs/). It is not committed (its annotations are not Sinclair's): keep a copy at `external/roms/zx80/zx80_rom_source.txt` (git-ignored).

`roms/bw80_test.asm` takes its character set from this file (`.incbin`).

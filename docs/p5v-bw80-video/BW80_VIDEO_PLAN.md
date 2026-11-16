# P5v BW-80 video spike — Implementation Plan

Design: [BW80_VIDEO_DESIGN.md](BW80_VIDEO_DESIGN.md)
Spec: [BW80_VIDEO.md](BW80_VIDEO.md)

Implemented 2026-11-16, in the worktree `../bodgewire-p5v`, branch `p5v-bw80-video`.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `7e198fb` |
| 1 | Memory timing from the address (2114, ROMs) | `b38ad37` |
| 2 | `C` Part: units, builder, Component, Board, Net panel, datasheet | `dc8c185` |
| 3 | `.incbin`; `roms/zx80_ic2.bin`, its note; the test ROM | `cfc5720` |
| 4–5 | Z80 halted fetches at the HALT; `netlists/bw80.bwn`; the test ROM's page, the 450 ns mutation, the ZX80 ROM's `K` cursor | `3d74437` |
| 6 | Release check, recipes, roadmap | this commit |

## Notes

- Review (before code), fixed in the docs: the halted Z80 fetched the byte after its `HALT`, and the ZX80's load gate ignores `HALT_N`, so a short row would repeat the next row's first character (the step's finding: the Z80 now re-reads the `HALT`, PC still past it for the interrupt's push); the `K` case cannot check the whole frame (the TV draws the sync periods unblanked); only IC4 (bit 6) decides the NOP; the tests build the netlist without a Board (`LoadNetlistMachine` needs one); phase 2's builder change needs the full suite; `.incbin`'s tests live in `asm6502_test.cpp`; the model's Z80 has no setup time (IC4 at 450 ns misses by about 10 ns, not 45); the glyph-load margin is about 20 ns; the TV's sync Net is `SYNC`; the ARCHITECTURE P5 row.
- Phase 1–3 code was written while the review ran. Phase 1's fence: the full debug suite, 1299/1299 (865 s). Phases 2–5 were tested together: the full debug suite, 1316/1316 (930 s; it ran the picture cases in debug before they became release-only), then committed one by one, `tests/CMakeLists.txt` staged per phase.
- Bring-up: the netlist loaded without warnings and the test ROM's page showed on the first run, both seeds; the schematic reading (Searle's PDF, rendered with PDFKit: poppler draws its labels as boxes) needed no fix. Gate inputs follow the drawing's pin numbers (IC11, IC16 units 2 and 4 reordered before the first run).
- The test ROM's code at `$0000` overran `$0038` (`$0038 written twice`): a jump at `$0000` to the setup at `$0050`.
- The ZX80 ROM: the `K` cursor is on the bottom row (the edit line), found at TV row 245. A dashed line under it is the bottom margin's first line, where the ROM's code between the page and the margin runs (opcodes with bit 6 clear load glyphs): outside the page, and what the circuit as drawn does.
- Debug times (M5 Max): the test ROM case about 93 s a seed, the 450 ns case 135 s, the `K` case 75 s: all three release-only (`#ifndef NDEBUG` → `SKIP`), 6 s together in release.
- Release, headless, `bw run --time 2s`, three runs alternated: the ZX80 ROM 0.245–0.249× (388.6 M applied events), the test ROM 0.217–0.220× (426.9 M), `bus_load_z80` 0.424–0.427× (234.6 M, as P5s). `bw_ui` screenshots of both viewed: the page, normal and inverse rows, in the ZX80's character set; the `K` cursor screen.

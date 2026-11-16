# P5v BW-80 video spike — Implementation Plan

Design: [BW80_VIDEO_DESIGN.md](BW80_VIDEO_DESIGN.md)
Spec: [BW80_VIDEO.md](BW80_VIDEO.md)

In the worktree `../bodgewire-p5v`, branch `p5v-bw80-video`. The worktree needs `external/` linked from the main tree for the PET tests, as P5u did: `ln -s ../bodgewire/external external`, after checking that `external` does not already exist there (the main tree has a stray `external/external` link from such a run).

Every commit message ends with the tests its fence ran.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | |
| 1 | Memory timing from the address (2114, ROMs) | |
| 2 | `C` Part: units, builder, Component, Board, Net panel, datasheet | |
| 3 | `.incbin`; `roms/zx80_ic2.bin`, its note; the test ROM | |
| 4 | Z80 halted fetches at the HALT; `netlists/bw80.bwn`, the page helpers moved, the test ROM's cases | |
| 5 | The ZX80 ROM's `K` cursor case | |
| 6 | Release check, recipes, roadmap | |

## Phase 1 — memory timing

Design §3. `ram2114.{hpp,cpp}`, `rom.{hpp,cpp}`, their tests.

Fence: `ctest --preset debug -j 12 -R "2114|rom|ram"` and then the **full debug suite** (shared Parts). A Machine case that fails: stop and report to the user with the margin it lost (Design §3.3). `clang-format`. Commit: `parts: 2114 and ROMs time their data from the address (P5v)`.

## Phase 2 — `C`

Design §2. `units`, `catalogue.{hpp,cpp}`, `builder.cpp`, `src/parts/capacitor.{hpp,cpp}` (and `src/parts/CMakeLists.txt`), `kicad_netlist.cpp` and `boards/footprints/C_Disc_D3.0mm_W1.6mm_P2.50mm.kicad_mod`, `electrical.cpp`, `net_pins.{hpp,cpp}`, `datasheets/C.md`, tests (`units`, `capacitor`, `electrical`, `net_pins`, the catalogue and footprint tests). The catalogue, footprint and manual tests build every Part: a `nullptr` create as `R`'s.

Also `tests/CMakeLists.txt` (`capacitor_test.cpp`) and `tests/data/capacitor_bench.bwn` (the electrical and Net panel tests' netlist).

Fence: the **full debug suite** (the builder and the Board are shared). Commit: `parts: the capacitor, coupling an edge into a pulled line (P5v)`.

## Phase 3 — `.incbin`, the ROMs

Design §4, §6. `assembler.cpp` (+ its header's syntax comment), `tests/asm/asm6502_test.cpp`; `roms/zx80_ic2.bin` (copied, SHA1 checked), `roms/zx80_ic2.md`; `roms/bw80_test.asm`, assembled with the release `bw asm` into `roms/bw80_test.rom`.

Fence: `ctest --preset debug -j 12 -R "asm"`. Commit: `asm: .incbin; the ZX80 ROM and a BW-80 test ROM (P5v)`.

## Phase 4 — the netlist and the test ROM's picture

Design §3.3, §5, §7. `z80_core.{hpp,cpp}`, `tests/parts/z80_core_test.cpp`, `datasheets/Z80.md`; `netlists/bw80.bwn`; `tests/machines/tv_page.{hpp,cpp}` (moved from `bus_load_test.cpp`); `tests/machines/bw80_test.cpp`; `tests/CMakeLists.txt`.

Bring-up order: load without warnings; run 50 ms and check `SYNC`'s line period (63.7 µs) and the VSYNC; then the page. A failure that a reading of the drawing explains is fixed here (say so in the notes). One that the circuit as drawn produces on the model goes to the user (the roadmap row).

Fence: the **full debug suite** (the Z80 Part is shared). A case over about a minute in debug becomes release-only (Design §7); the fence then also runs `ctest --preset release -R bw80`. Commit: `bw80: the ZX80's netlist shows the test ROM's page (P5v)`.

## Phase 5 — the ZX80 ROM

Design §7, the `K` cursor case.

Fence: `ctest --preset debug -j 12 -R "bw80"` (and in release when its cases are release-only). Commit: `bw80: the ZX80 ROM shows its K cursor (P5v)`.

## Phase 6 — release check

Design §8. The full debug suite, the release ratios and screenshots (viewed), recipes, GLOSSARY, ARCHITECTURE rows, this plan cut to its table and notes.

Commit: `docs: P5v done, the BW-80 shows the ZX80's picture`.

## Notes

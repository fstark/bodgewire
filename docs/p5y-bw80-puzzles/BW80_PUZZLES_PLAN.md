# P5y BW-80 Puzzles — Implementation Plan

Spec: [BW80_PUZZLES.md](BW80_PUZZLES.md). Design: [BW80_PUZZLES_DESIGN.md](BW80_PUZZLES_DESIGN.md). Machine: [BW80.md](../machines/140-bw80/BW80.md), [BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md).

Every phase ends with its fence: `cmake --build --preset debug` and `--preset release` clean, `clang-format` on the C++ files touched, `ctest --preset debug -j 12 -R "bw80|manuals|puzzle"` and `ctest --preset release -j 12 -R "bw80"` green, each new Puzzle's symptom seen with `bw run --tv` or `bw_ui --screenshot`, one commit of this step's own paths naming the tests run.

## Phases

### Phase 1 — Rig

Design §1: `bw80_rig.{hpp,cpp}` out of `bw80_test.cpp` (the TV's Nets by connector pin), `Bw80Built`, the mapped page, `tests/CMakeLists.txt`. No Puzzle test file yet: with no case it would not build.

Fence: the Machine cases unchanged (release `bw80` green). Commit: `tests: bw80_rig, the BW-80 Machine helpers shared with its Puzzles (P5y)`.

### Phase 2 — Picture Puzzles (1–7)

Design §2 rows 1–7: `bw80_puzzles_test.cpp` (`LoadPuzzle`, `kPuzzles`, `KScreen`, `PageOf`, `Cell`, `Toggles`), the seven files, the two debug cases over them, `BottomHalvesCrack` in `test_points`, seven release cases; seeds 2 and 3 tried with `bw run --tv`. Commit: `bw80: picture Puzzles, Q everywhere to Blank page (P5y)`.

### Phase 3 — Keyboard and sync Puzzles (8–12)

Design §2 rows 8–12: five files, `EightKeysCrack`, `NoPrintCrack`, `RollingScreenCrack` in `test_points`, five release cases; seeds 2 and 3 tried; the user manual's and BW80.md's keys (PRINT on O, LET on K). Commit: `bw80: keyboard and sync Puzzles (P5y)`.

### Phase 4 — Dead Puzzles (13–15)

Design §2 rows 13–15: three files, `VerticalStripesCrack`, three release cases on seeds 1–3. Commit: `bw80: dead Puzzles, Still starting to White screen (P5y)`.

### Phase 5 — Finish

Design §3: BW80.md and BW80_DESIGN.md Puzzle sections, ADD_PUZZLE; `bw_ui` screenshots of every Puzzle viewed; the full debug suite; the ARCHITECTURE row done with the date and links; this plan cut down to the phase table and notes. Commit: `docs: P5y done, BW-80 Puzzles`.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `3fffc75` |
| 1 | Rig | `49ad5f3` |
| 2 | Picture Puzzles (1–7) | `e5bac3b` |
| 3 | Keyboard and sync Puzzles (8–12) | this commit |
| 4 | Dead Puzzles (13–15) | |
| 5 | Finish | |

## Implementation notes

- Candidates tried before the docs (headless, `bw run --tv`): the spec's "Found while trying candidates" lists what was dropped and why.
- Docs review (before the docs commit), fixed: LET is on K, not on `A13`'s row; the user manual and BW80.md had PRINT on P and LET on O (the ROM and the key table: O and K), fixed in phase 3; phase 1 is the rig alone (a test file with no case does not build); the typed Puzzles' screenshots need `--after 25s`; `BottomHalvesCrack` in phase 2; the TV's Nets by connector pin; `KBD_N`'s junction is a via; the Still starting check spelled out; seeds 2–3 for every Puzzle.
- Phase 1: the rig as designed; `Bw80TestRomDirectory` no longer removes the shared directory (two release cases run in parallel use it: the 450 ns case failed under `ctest -j` once the Puzzle cases joined). Release `bw80` 17/17, debug fence green.
- Phase 2: all seven as designed. The page a Puzzle's picture should show is built from the display file's codes through the fault's glyph set, but the cells past a row's end stay blank whatever the chain does to a space: no code is loaded there (the first page for Quotes for spaces drew them as quotes and failed); only Q everywhere fills them, with the HALT's glyph. The cursor under Quotes for spaces is an inverse `L`, as the mapping says (seen at 10×). Seeds 2 and 3 give the same pictures. Release 6–28 s a case; debug `every Puzzle builds` 4 s for seven. Release `bw80` 26/26; debug `bw80|manuals|puzzle` 38/38 in 122 s.
- Phase 3: all five as designed. No PRINT's edit line after `PRINT 2+2` NEW LINE reads `2+s2l` (the syntax error marker before the last 2, then the `L` cursor). `PinNet` on a resistor's pad gives nothing useful, so Eight keys checks the column at the connector reads high (its pull-up kept) instead of naming `R15`. The user manual's and BW80.md's keys corrected (PRINT on O, LET on K). Seeds 2 and 3 as seed 1. `VerticalStripesCrack` declared here with the other three (phase 4 uses it). Release `bw80` 31/31 in 35 s; debug `bw80|manuals|puzzle` 43/43 in 129 s.

# P5r BW-2001 Puzzles — Implementation Plan

Spec: [BW2001_PUZZLES.md](BW2001_PUZZLES.md). Design: [BW2001_PUZZLES_DESIGN.md](BW2001_PUZZLES_DESIGN.md). Machine: [BW2001.md](../machines/130-bw2001/BW2001.md), [BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md).

Every phase ends with its fence: `cmake --build --preset debug` and `--preset release` clean, `clang-format` on the C++ files touched, `ctest --preset debug -j 12 -R "bw2001|manuals|puzzle"` and `ctest --preset release -j 12 -R "bw2001"` green, each new Puzzle's symptom seen with `bw run --tv` or `bw_ui --screenshot`, one commit of this step's own paths naming the tests run.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Rig | this commit |
| 2 | Picture Puzzles (1–5) | |
| 3 | Computer Puzzles (6–10) | |
| 4 | Self-test and instrument Puzzles (11–15) | |
| 5 | Finish | |

## Phase 1 — Rig

- `tests/machines/bw2001_rig.{hpp,cpp}` (design §1): the helpers moved out of `bw2001_test.cpp`, `Running` from a `Machine`, `SnowWatch` by pin.
- `tests/machines/bw2001_puzzles_test.cpp`: `LoadPuzzle`, `SelfTest`, `Banner`; no case yet: the two debug cases come with Phase 2's first Puzzles (an empty C array does not compile).
- `tests/CMakeLists.txt`: the two new sources.

Fence: the existing `bw2001 machine:` cases unchanged and green in both builds.
Commit: `tests: bw2001_rig, the BW-2001 Machine helpers shared with its Puzzles (P5r)`

## Phase 2 — Picture Puzzles

`bw2001: Puzzles have a title and a brief`, `bw2001: every Puzzle builds` (design §1). `odd_e`, `striped_screen`, `rolling_text`, `stutter`, `negative_screen` (design §2, rows 1–5): for each, the Fault tried and measured, the `.bwp`, its case, the crack helpers (`Cd3Crack`, `VertDriveCrack`), the lists. `odd_e`'s dot chosen on a `bw_ui` screenshot.

Commit: `bw2001: five picture Puzzles (P5r)`

## Phase 3 — Computer Puzzles

`short_memory`, `missing_keys`, `frozen_cursor`, `random_screen`, `snow` (rows 6–10); `Kin0Crack`. `random_screen` on seeds 1–3. The service manual note on `J5`'s columns and `R18`–`R25` (design §3).

Commit: `bw2001: five computer Puzzles, snow among them (P5r)`

## Phase 4 — Self-test and instrument Puzzles

`bad_zero_page`, `wrong_chip`, `ghost_letter`, `no_clock`, `joined_bits` (rows 11–15); `Osc8Crack`. The dead ones and `no_clock` on seeds 1–3; `wrong_chip`'s failed `UI8` swap and its allowed build warning; the debug time of `every Puzzle builds` measured.

Commit: `bw2001: self-test and instrument Puzzles (P5r)`

## Phase 5 — Finish

- Machine docs (design §3); `bw_ui` screenshots of all fifteen viewed against their briefs (design §4).
- Full debug suite; the release `bw2001` cases.
- Recipes: ADD_PUZZLE with what this step found (a `stuck` Fault on a port pin drives its Net; a floating NMOS port input reads high; a program-ROM cell can break one code path precisely, e.g. a wait; the self-test's ROM row as a second clue).
- ARCHITECTURE row P5r done with the date and links; this plan cut down to the phase table and notes.

Commit: `docs: P5r done, BW-2001 Puzzles`

## Implementation notes

- Docs review (before the docs commit), fixed: a video RAM column garbles the self-test's own report, so `garbled_text` became `ghost_letter`, one stuck cell; `wrong_chip`'s dry Joint gives an expected build warning; `SnowWatch`'s pins (`UE5.4Y`, `UC6.3Y`, `UC5.2Q`); `short_memory` on `IO1` (`RAM BAD 0C00 01`); the dead Puzzles' pictures measured on seeds 1–3, `no_clock` too; the cursor cell left out of the stripe check; the service manual note `missing_keys` needs; no empty Puzzle list; timings.
- Phase 1: `bw2001_rig.{hpp,cpp}` as designed (names prefixed `Bw2001`: they leave the anonymous namespace). The release case `a scrolling screen without snow` failed before any change (1 access): at power-on, before reset ends, the 6502's random address selects the screen for a moment on seed 3 (the draws changed when P5q2 fitted `UH2`, `UH6`); the case now counts from the banner, as it means to. Debug `bw2001|manuals|puzzle` 57/57 in 205 s; release `bw2001` 46/46 in 197 s.

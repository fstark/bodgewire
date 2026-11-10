# P4k Board Placement — Implementation Plan

Design: [BOARD_PLACEMENT_DESIGN.md](BOARD_PLACEMENT_DESIGN.md)
Spec: [BOARD_PLACEMENT.md](BOARD_PLACEMENT.md)

Each phase ends with its fence: the checks listed, `cmake --build --preset debug` and `ctest --preset debug -j 12` green, `clang-format` on the C++ files touched, then one commit with the message given (plus the attribution line). Only this step's paths are committed. The status table and the notes are updated in the phase's commit.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Vendored footprints, `LoadFootprints` | |
| 2 | Rules file parser | |
| 3 | Placer: panel rules, grid, annealing | |
| 4 | Writer, `bw place`, 070 rules, DRC ctest | |
| 5 | 070 Board regenerated (user routing), Puzzles checked | |
| 6 | Release check, recipe, docs | |

## Phases

### 1. Footprints

Design §2. Copy the nine `.kicad_mod` files and write `boards/footprints/README.md`; `src/board/footprint.{hpp,cpp}`; `BW_FOOTPRINT_DIR` on `bw_tests` (and on `bw` in phase 4); `tests/board/footprint_test.cpp`.

Fence: `footprint` tests green. Commit `board: phase 1 — vendored footprints`.

### 2. Rules

Design §3. `src/board/place_rules.{hpp,cpp}`, `tests/board/place_rules_test.cpp`.

Fence: `place rules` tests green. Commit `board: phase 2 — placement rules`.

### 3. Placer

Design §4. `src/board/placer.{hpp,cpp}`, `tests/board/placer_test.cpp` including `[slow]` `placer: tennis` (written against a draft `tennis.place` in the test until phase 4 commits the file; the timing of the case noted).

Fence: `placer` tests green; `placer: tennis` wire length (seed 1) noted against the prototype's 10.5–11.4 m. Commit `board: phase 3 — placer`.

### 4. Writer and `bw place`

Design §5, §6, §7 (rules file only), §8. `KicadUuid` and `KicadQuote` into `kicad_netlist.hpp`; `src/board/kicad_pcb_writer.{hpp,cpp}`; `bw place`; `boards/tennis/tennis.place`; `tests/board/kicad_pcb_writer_test.cpp`; `tests/place_drc.cmake` and the `place_drc_tennis` ctest. Check by hand that KiCad's DRC loads a project written by `WriteKicadProject` (generate into the scratchpad with a `.kicad_pro`, run `kicad-cli pcb drc`), and that a second run gives the same file.

Fence: writer tests green, `kicad pcb writer: tennis` included; `place_drc_tennis` green; `bw place` refuses `boards/tennis/tennis.kicad_pcb` without `--force`. Commit `board: phase 4 — bw place`.

### 5. 070 Board

Design §7. `bw place netlists/tennis.bwn boards/tennis/tennis.place -o boards/tennis/tennis.kicad_pcb --force` (working copy only). **Stop: the user routes it in KiCad** (open `boards/tennis/tennis.kicad_pro`; KICAD_GUIDE steps 7–8; 2 layers, no zones, DRC 0 errors; the Invisible ball constraint: route `SERVING` `U41` → `U43` → `U38`, with `U38` at an end). Then `bw check-board`; read the `SERVING` route and every crack Puzzle's Net (ADD_PUZZLE.md), moving cracks if needed; TENNIS_DESIGN.md § Board updated.

Fence: `bw check-board` `ok`; `drc_tennis`, `boards`, `electrical`, `board scene`, `tennis` tests green. Commit `tennis: Board from bw place`.

### 6. Release check, recipe, docs

Release build; `bw_ui puzzles/invisible_ball.bwp --screenshot … --after 1s` with every instrument, ratio and UI ms noted, PNG viewed. ADD_MACHINE.md § 3 rewritten for `bw place` (the agent writes `<m>.place` from the Machine design and runs it; the user routes, KICAD_GUIDE steps 7–8). ARCHITECTURE row P4k done with the date and links; this plan cut to the table and notes.

Fence: release check done; full debug suite green. Commit `docs: P4k implemented, user check pending`.

## Implementation notes


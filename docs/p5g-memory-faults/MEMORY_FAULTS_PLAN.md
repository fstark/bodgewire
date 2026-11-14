# P5g Memory Faults — Implementation Plan

Design: [MEMORY_FAULTS_DESIGN.md](MEMORY_FAULTS_DESIGN.md)
Spec: [MEMORY_FAULTS.md](MEMORY_FAULTS.md)

Each phase ends with its fence: the checks listed, `cmake --build --preset debug`, the ctest subset named, `clang-format` on the C++ files touched, then one commit with the message given (plus the attribution line). The commit message names the subset run. Only this step's paths are committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | `MemoryCells`: stuck bits in the 2114 and the ROMs | |
| 2 | `fault cell`, `fault column`: syntax, builder, Machine, swap | |
| 3 | Machine tests: 110 LED computer, 080 message display | |
| 4 | Release check, recipes, glossary, game design, roadmap | |

## Phase 1 — Memory cells in the Parts

Design §1–§3.

- Before any change: release build, `bw run netlists/led_computer.bwn --time 2s` and `bw run netlists/bus_load.bwn --time 20ms` headless ratios noted (median of 3).
- `src/parts/memory_cells.hpp` (new); `ram2114.{hpp,cpp}` (`store`, masks, `powerOn`, `stick`, `dataBit`, `wordCount`); `rom.{hpp,cpp}` (`stick`, `dataBit`, `wordCount`); `src/parts/CMakeLists.txt` if headers are listed.
- Tests: `ram2114_test` and `rom_test` cases of design §8.
- The same two ratios after the change: within noise.

Fence: full debug `ctest --preset debug -j 12` (the 2114 and the ROMs are used by `bus load`, `ram bench`, `led_computer`, `message`, `typewriter`).
Commit: `parts: phase 1 — stuck memory cells in the 2114 and the ROMs`

## Phase 2 — Puzzle lines, builder, Machine

Design §4–§6.

- `builder.{hpp,cpp}`: `StuckCell`, the fourth `BuildCircuit` argument, `cells_`, the sticking loop.
- `puzzle.{hpp,cpp}`: `CellDecl`, `cells`, `fault cell`, `fault column`, the catch-all message.
- `machine.{hpp,cpp}`: `cells_`, loading checks, `swap`, `build`.
- Tests: `builder_test`, `puzzle_test` (new cases; the catch-all strings updated), `machine_test` (design §8).
- Check by hand: an uncommitted `puzzles/p5g_check.bwp` with `machine led_computer` and `fault cell U4.IO4 $1FF 0`: `./build/debug/src/cli/bw run puzzles/p5g_check.bwp --time 200ms --probe U7.1Q,U7.2Q` shows `1Q` HIGH, `2Q` LOW; with `--repair swap:U4`, `1Q` LOW, `2Q` HIGH (the second step at about 145 ms). File deleted.

Fence: full debug `ctest --preset debug -j 12` (builder: shared code).
Commit: `faults: phase 2 — fault cell and fault column`

## Phase 3 — Machine tests

Design §8 (110 and 080 cases).

- `test_points.{hpp,cpp}`: `InlinePuzzle`.
- `led_computer_test.cpp`: `Run` from a built circuit; the stack cell and the address line cases.
- `message_test.cpp`: `PageFault` font override; the column and cell cases.
- `docs/machines/110-led-computer/LED_COMPUTER.md` and `docs/machines/080-message/MESSAGE.md` Puzzle notes: the memory candidates (tested, no Puzzle file yet).

Fence: `ctest --preset debug -j 12 -R "led_computer|message"` passes.
Commit: `machines: phase 3 — memory faults on the 110 and the 080`

## Phase 4 — Release check, recipes, docs

- Release build. `bw_ui` screenshot of `puzzles/p5g_check.bwp` (message, the stuck cell), viewed, file deleted; speed ratio noted.
- `ADD_PUZZLE.md`, `GLOSSARY.md`, `GAME_DESIGN.md`, `ARCHITECTURE.md` (P5g row done with the date and links; P5h row; new P5n row) as design §7, staged from `HEAD` plus these changes only.
- This plan cut down to the status table and notes ("user check pending").

Fence: full debug `ctest --preset debug -j 12` green; release builds.
Commit: `docs: P5g implemented, user check pending`

## Notes

(filled while implementing)

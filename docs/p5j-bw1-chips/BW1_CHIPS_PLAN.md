# P5j BW-1 chips — Implementation Plan

Design: [BW1_CHIPS_DESIGN.md](BW1_CHIPS_DESIGN.md)
Spec: [BW1_CHIPS.md](BW1_CHIPS.md)

Each phase ends with its fence: the checks listed, `cmake --build --preset debug`, the ctest subset named, `clang-format` on the C++ files touched, then one commit with the message given (plus the attribution line). The commit message names the subset run. Only this step's paths are committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | ROM and RAM blocks, memory names in cell Faults | |
| 2 | Kernel: pull-up Pins | |
| 3 | 6102 and 74125 | |
| 4 | 6530 | |
| 5 | Bench and its program | |
| 6 | Release check, recipes, glossary, roadmap | |

## Phase 1 — Memory blocks and names

Design §1, §2.

- Before any change (taken at `f785ac8`, before any code): release build, `bw run netlists/led_computer.bwn --time 2s`, `netlists/bus_load.bwn --time 20ms`, `netlists/message.bwn --time 1s` headless ratios (median of 3), noted.
- `src/parts/memory_block.{hpp,cpp}`, `memory_cells.hpp`, `rom.{hpp,cpp}`, `ram2114.{hpp,cpp}`, `CMakeLists.txt`.
- `puzzle.{hpp,cpp}`, `builder.{hpp,cpp}`, `machine.{hpp,cpp}` (`ResolveCell`).
- Tests: `memory_block_test.cpp` (new), `ram2114_test`, `rom_test` (the new `stick` signature), `builder_test`, `puzzle_test` (new forms and strings), `machine_test` (the 110 `U3 ram` error; the 6530 cases come in phase 4); `tests/CMakeLists.txt`.

Fence: full debug `ctest --preset debug -j 12` (the 2114 and ROMs are in six Machines; builder, Machine).
Commit: `parts: phase 1 — ROM and RAM blocks, memory names in cell Faults`

## Phase 2 — Pull-up Pins

Design §3.

- `logic.hpp`, `simulator.cpp`, `builder.cpp` (stuck tie).
- Tests: `logic_test`, `simulator_test` (a test Component with a `PullUp` Pin). The builder's stuck `PullUp` case waits for the 6530 (phase 4).
- The three ratios after: within noise of phase 1's.

Fence: full debug `ctest --preset debug -j 12` (kernel).
Commit: `sim: phase 2 — pull-up pins`

## Phase 3 — 6102 and 74125

Design §4, §5.

- `src/parts/ram6102.{hpp,cpp}`, `ttl125.{hpp,cpp}`, `delays.hpp`, `CMakeLists.txt`, `catalogue.cpp` rows (sorted by name: `6102` after `2716`, `74125` before `74145`; phase 4 puts `6530` after `6502`), `datasheets/6102.md`, `datasheets/74125.md`.
- Tests: `ram6102_test.cpp`, `ttl125_test.cpp`, `catalogue_test` (`kMosSignals` `6102`, `kOffCornerPower` `6102`).

Fence: `ctest --preset debug -j 12 -R "6102|125|catalogue|manuals|footprint"`.
Commit: `parts: phase 3 — 6102 static RAM and 74125 buffer`

## Phase 4 — 6530

Design §6, §2.3 (6530 cases), §3.3 (stuck `PullUp` test).

- `src/parts/mos6530.{hpp,cpp}`, `delays.hpp`, `CMakeLists.txt`, `catalogue.cpp` (`CreateMos6530`, row), `manual.cpp` (`PartPinNames`), `datasheets/6530.md`.
- Tests: `mos6530_test.cpp`, `catalogue_test` (params, `kMosSignals`, `kOffCornerPower`), `manual_test` (40 names), `machine_test` (`ResolveCell` on a 6530), `builder_test` (stuck `PullUp`).

Fence: `ctest --preset debug -j 12 -R "6530|catalogue|manual|footprint|machine:|builder|puzzle"`.
Commit: `parts: phase 4 — 6530 ROM, RAM, I/O and timer`

## Phase 5 — Bench

Design §7, §9 (bench).

- `roms/bw1_bench.asm`, `bw asm` → `roms/bw1_bench.rom`; `tests/data/bw1_bench.bwn`; `tests/netlist/bw1_bench_test.cpp`; `tests/CMakeLists.txt`.
- Check by hand: `./build/debug/src/cli/bw run tests/data/bw1_bench.bwn --time 30ms --probe U2.PB7` shows `PB7` pulsing low every ~800 µs after ~20 ms.
- Then the full debug suite (the roadmap step's code is complete).

Fence: full debug `ctest --preset debug -j 12`.
Commit: `parts: phase 5 — BW-1 bench: 6502, 6530, 6102s and 74125s on the KIM's bus`

## Phase 6 — Release check, docs

Design §8, §10.

- Release: the three ratios against phase 1's; the bench's ratio; `bw_ui` screenshot of the 110 puzzle, viewed.
- `ADD_PART.md`, `ADD_PUZZLE.md`, `GLOSSARY.md`, ARCHITECTURE P5j row done with the date and links; this plan cut to the table and notes ("user check pending").

Fence: docs only, no ctest.
Commit: `docs: P5j implemented, user check pending`

## Notes

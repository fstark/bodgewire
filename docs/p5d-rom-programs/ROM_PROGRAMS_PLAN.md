# P5d — ROM programs + first computer — Implementation Plan

Spec: [ROM_PROGRAMS.md](ROM_PROGRAMS.md). Design: [ROM_PROGRAMS_DESIGN.md](ROM_PROGRAMS_DESIGN.md). Machine: [LED_COMPUTER.md](../machines/110-led-computer/LED_COMPUTER.md), [LED_COMPUTER_DESIGN.md](../machines/110-led-computer/LED_COMPUTER_DESIGN.md).

Every phase ends with its fence: `cmake --build --preset debug` clean (reconfigure first when files are added), `clang-format` on the C++ files touched, the tests the phase names green in debug, one commit of this step's own paths only. The full debug suite runs at phases 2 (catalogue rows: shared code) and 6.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Decode and assembler | |
| 2 | 74LS138, 74LS273, POR | |
| 3 | Program, netlist, manuals, fault-free test | |
| 4 | Board (user routes) | |
| 5 | Session test, release check | |
| 6 | Recipes, docs, full suite | |

## Phase 1 — Decode and assembler

- `Mos6502Decode` (design, Decode); core decode cases in `mos6502_core_test.cpp`.
- `src/asm/` (`bw_asm`), `Assemble6502`, `Assemble6502File`; root `CMakeLists.txt`; `bw asm` in `main.cpp`.
- `tests/asm/asm6502_test.cpp` (spec, Tests: Assembler), `BW_ROM_DIR`; the ROMs case passes with no `.asm` yet.
- `tests/asm_cli.cmake` and the `asm_<name>` glob (no test until phase 3 adds an `.asm`).
- Fence: `ctest --preset debug -j 12 -R "6502|asm|heartbeat"` (the core is in a Part Heartbeat uses; the CLI change is a new command only). Commit: `asm: bw asm, a 6502 assembler writing .rom files`.

## Phase 2 — 74LS138, 74LS273, POR

- The three Parts, delays, catalogue rows, `CreatePowerOnReset`, datasheets (ADD_PART, Files); `catalogue_test.cpp` and `PartPinNames` (`manual.cpp`) pass `time=1ms` to `POR` (design, Catalogue tests).
- `tests/parts/ls138_test.cpp`, `ls273_test.cpp`, `power_on_reset_test.cpp` (spec, Tests).
- Fence: full debug suite (catalogue is shared). Commit: `parts: 74LS138, 74LS273, power-on reset`.

## Phase 3 — Program, netlist, manuals, fault-free test

- `roms/led_computer.asm`, then `bw asm roms/led_computer.asm -o roms/led_computer.rom` (release build); check the listing's addresses and vectors by eye.
- `netlists/led_computer.bwn`, manuals, `tests/machines/led_computer_test.cpp` built from the netlist (`LoadNetlistFile`) until the Board exists; `manuals_test.cpp` list.
- VCD of the first 50 µs after the reset release (`bw run --record-all --vcd`): the vector fetch, the first `STA $8000` and its `LATCH_CLK` (about 31 µs after the release). One step measured against 124.78 ms from a longer run (`bw run --time 400ms --record LATCH_CLK`).
- Debug time of the slow bounce case measured and noted (estimate 20–40 s).
- Release headless `bw run netlists/led_computer.bwn --time 2s`: ratio noted.
- Fence: `ctest --preset debug -R "led_computer|asm|manuals"`. Commit: `led computer: program, netlist, manuals, fault-free test`.

## Phase 4 — Board

- `boards/led_computer/led_computer.place`; `bw place` (release); the wire length and grid noted; `bw export-kicad netlists/led_computer.bwn -o /tmp/led_computer.net` if the user wants it.
- Stop for the user: open `boards/led_computer/led_computer.kicad_pro`, route (constraints in LED_COMPUTER_DESIGN, Board), save.
- After routing: `bw check-board` `ok`; per-Machine loops (`boards_test`, `electrical_test`, `board_scene_test`); the Machine test built from the Board (`LoadNetlistMachine`), the warnings case from the netlist.
- Fence: `ctest --preset debug -j 12 -R "led_computer|boards|electrical|view:|drc"`. Commit: `led computer: Board`.

## Phase 5 — Session test, release check

- `session: led computer with every instrument` (Machine design, Per-Machine checks).
- Release `bw_ui netlists/led_computer.bwn` with every instrument (`--probe`, `--clip`, `--hook`, `--meter`), screenshot at 1 s viewed; ratio 1.00×, UI ms noted.
- Fence: `ctest --preset debug -R "session: led computer"`. Commit with phase 6.

## Phase 6 — Recipes, docs, full suite

- ADD_MACHINE (ROM programs, reference commits), ADD_PART (three kinds, reference commits, a required parameter needs `PartPinNames` too), any recipe step found wrong on the way.
- ARCHITECTURE: P5d row done with the date, links; GAME_DESIGN ladder: a 110 row.
- This plan cut to the status table and implementation notes, "user check pending".
- Fence: full debug suite. Commit: `docs: P5d implemented, user check pending`.

## Implementation notes

- Docs review (before the docs commit), fixed: `DurationFromText`, not `ParseDuration` (`bw_netlist` is above `bw_parts`); the phase 4 fence named no board-scene test (`view:`); `PartPinNames` also builds every Part and needs `time=` for `POR`; the bounce test reads `L0`–`L7` after each `LATCH_CLK` rise and only after the reset (the 74LS02's random power-on output can make one early rise); the POR's release at exactly `time`; the VCD window 50 µs; the service manual one bullet a reference with `J1`; the phase 1 fence takes Heartbeat; debug cost measured in phase 3; `asm` added to NAMING; the `.rom` header names the filename; `~` in the spec; `LATCH_CLK` 80 ns after `PHI0` falls; the 2114's data 120 ns after `CS_N`; POR's NC pins TTL; the Session check on `D3` at 300 ms.
- Written before the docs commit (drafted while the review ran): phase 1 code (decode, assembler, `bw asm`) and phase 2 Parts. POR simplified from a wake to one drive delayed by `time`.

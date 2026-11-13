# P5c — 6502 Part — Implementation Plan

Spec: [MOS6502.md](MOS6502.md). Design: [MOS6502_DESIGN.md](MOS6502_DESIGN.md). Machine: [HEARTBEAT.md](../machines/100-heartbeat/HEARTBEAT.md), [HEARTBEAT_DESIGN.md](../machines/100-heartbeat/HEARTBEAT_DESIGN.md).

Every phase ends with its fence: `cmake --build --preset debug` clean (`-Werror`), `clang-format` on the C++ files touched, the tests the phase names green in debug, then one commit with the message given (the tests run named in it). Only this step's own paths are committed.

## Phase 1 — Test data

`tests/data/6502/`: `convert_harte.py`, `fetch.sh`, `README.md`, `LICENSE-harte`; `.gitignore` `external/`. `fetch.sh --subset` writes the full set into `external/6502/` and the committed subset into `tests/data/6502/harte/` from each JSON before deleting it.

Fence: the subset has 256 files of 100 cases (`grep -c '^I '`); `a9.txt` checked by eye against the JSON. Commit `tests: 6502 test data, Harte subset and fetch script`.

## Phase 2 — Instruction core

`src/parts/mos6502_core.{hpp,cpp}` with all 256 opcodes (design, Instruction core), `tests/parts/mos6502_cases.{hpp,cpp}` (the runner clearing only the cells a case touched), `tests/parts/mos6502_conformance_test.cpp`, `tests/parts/mos6502_core_test.cpp` (every hand case of the design, interrupts and reset included). Full Harte timed in debug; hidden tag if over 2 minutes (design, Tests); run once in release either way.

Fence: `ctest --preset debug -j 12 -R "6502"`, with `external/6502/` present (Dormann and, by name if hidden, full Harte green). Commit `parts: 6502 instruction core, all 256 opcodes, Harte and Dormann tests`.

## Phase 3 — The 6502 Part

`src/parts/mos6502.{hpp,cpp}`, `kMos6502…` delays, catalogue row, `PowerPinVolts` and `PowerPinRail` (`GND2`), the builder's two power-pin spots, `datasheets/6502.md`, catalogue test lists and power-pin names, `builder_test` case, `tests/parts/mos6502_test.cpp` with `BenchMemory` (design, Pins and Tests).

Fence: the builder is shared code, so the full `ctest --preset debug -j 12`. Commit `parts: 6502, the core behind its Pins; GND2`.

## Phase 4 — 100 Heartbeat: netlist, manuals, fault-free test

`netlists/heartbeat.bwn`, `manuals/heartbeat/`, `tests/machines/heartbeat_test.cpp` (netlist-built), the `manuals_test.cpp` list. By hand first: `bw run netlists/heartbeat.bwn --time 300ms` and a VCD of `PHI0,SYNC,RW,A0,A1,A15,RES_N` over 20 µs, read against HEARTBEAT.md, When it works. Release `bw run … --time 2s` ratio noted.

Fence: `ctest --preset debug -j 12 -R "heartbeat|6502|manuals"`. Commit `heartbeat: netlist, manuals, fault-free test`.

## Phase 5 — Board

`boards/heartbeat/heartbeat.place`, `bw place` (release), `.kicad_pro` written; the `.net` exported for the user. **Stop: the user routes the Board** (HEARTBEAT_DESIGN, Board). Then `bw check-board` `ok`, the per-Machine loops (`boards_test`, `electrical_test`, `board_scene_test`), `heartbeat_test.cpp` built from the Board for all but the warnings case.

Fence: `ctest --preset debug -j 12 -R "heartbeat|board|electrical|scene|drc"`. Commit `heartbeat: Board`.

## Phase 6 — Session, release check, docs

`session: heartbeat with every instrument`. Release: `bw_ui netlists/heartbeat.bwn --screenshot … --after 1s` with every instrument (`--probe`, `--clip`, `--hook`, `--meter`), ratio and UI ms noted, PNG viewed. ADD_PART (a CPU: core apart from the Pins, conformance data, `GND2` and `PowerPinRail`; reference commits), ADD_MACHINE (reference commits). ARCHITECTURE: P5c row done with the date and links. This plan cut down to the phase table and notes, "user check pending".

Fence: full `ctest --preset debug -j 12`. Commit `docs: P5c implemented, user check pending`.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Test data | `d7aec85` |
| 2 | Instruction core | `9517870` |
| 3 | The 6502 Part | |
| 4 | Heartbeat netlist, manuals, fault-free test | |
| 5 | Board | |
| 6 | Session, release check, docs | |

## Implementation notes

- Docs review (before phase 1), fixed: `GND2` refused by the builder's power-pin check and missing from `addImplicitPower` (now `PowerPinRail`, phase 3, full-suite fence); LED resistors overlapping behind a row of LEDs (now in the grid) and `SW1` against the row (row from the left); `LA15`'s high time 65.536 ms; the reset fetch at the 6th fall; the subset written before each JSON is deleted; the taken-branch poll latched at the fetch; `NC1`–`NC3` MOS; how "wakes only on PHI0" is measured; the full-Harte debug time; Dormann's success line is a macro expansion; watched Nets 43 (one slot per Net); data Nets `DB0`–`DB7`; test names `"6502: …"`.
- Written before the docs commit (the core was drafted while the review ran): the subset and full set fetched (Harte `2f6980a`, Dormann `7954e2d`, success `$3469`; `external/6502/` 418 MB, the subset 4.8 MB); the core passes all 25,600 subset cases (17 s debug) and Dormann (3.7 s debug) at the first run; a mutation (ANE's constant) is caught. Phases 2 and 3 of the first plan (documented, then undocumented opcodes) became one: the table was written whole.
- Phase 2: every core case green; full Harte (2.56 M cases) green in 86 s debug with the runner reusing one array (17 s → 0.9 s for the subset), so it stays a registered ctest, not hidden; Dormann traps at `$3469` (success) in 3.7 s debug. Mutations caught: ANE's constant (subset), the taken-branch poll (IRQ case). Harte's PLP and RTI: B cleared and bit 5 set in the pulled P.
- Phase 3: first Part test run caught three test mistakes (an address sampled before the stalled cycle drove it, an X tie the Circuit refuses, power-on evaluations counted); the Part itself unchanged. Full debug suite: 838 tests green in 557 s.

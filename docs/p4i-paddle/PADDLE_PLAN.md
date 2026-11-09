# P4i Paddle Controller — Implementation Plan

Design: [PADDLE_DESIGN.md](PADDLE_DESIGN.md)
Spec: [PADDLE.md](PADDLE.md); Machine: [KNOB_METER.md](../machines/045-knob-meter/KNOB_METER.md), [KNOB_METER_DESIGN.md](../machines/045-knob-meter/KNOB_METER_DESIGN.md)

Each phase ends with its fence: the checks listed, `cmake --build --preset debug` and `ctest --preset debug -j 12` green, `clang-format` on the C++ files touched, then one commit with the message given (plus the attribution line). Only this step's paths are committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | 74LS175 Part, Datasheet, tests | `2bb1a10` |
| 2 | PADDLE Part, powered through its Connector | `8bfbcc6` |
| 3 | Knob: parse, Session, `bw run`, `bw_ui` side panel and keys | `da76381` |
| 4 | Knob meter netlist, manuals, headless fault-free tests | `d24f490` |
| 5 | Knob meter Board (user layout), per-Machine loops | `73063c0` |
| 6 | Knob meter Puzzles + Symptom/Repair tests | |
| 7 | Session test, release check, recipes, docs | |

## Phase 1 — 74LS175

Design §2. ADD_PART.md (files, rules).

- `src/parts/ls175.{hpp,cpp}`, `delays.hpp`, catalogue row, CMake.
- `datasheets/74LS175.md`.
- `tests/parts/ls175_test.cpp` (design §11.1), `tests/CMakeLists.txt`.

Fence: `[parts]` pass; `catalogue: chip pinouts are consistent` and the Datasheet check cover the new row.
Commit: `parts: phase 1 — 74LS175`

## Phase 2 — PADDLE Part

Design §3, §4.

- `src/parts/paddle.{hpp,cpp}` (`DurationFromText`, `Paddle`); `units.cpp` wraps it.
- `connector.{hpp,cpp}`: `ConnectorPins("PADDLE")`, `ConnectorPowersDevice`.
- Catalogue `CreatePaddle` and row; `kFootprints` KK-4; `PaddleOf` in `builder.{hpp,cpp}`.
- `FindPower` in `electrical.cpp`.
- `PartPinNames` in `src/manual/manual.cpp` (`PADDLE` parameters); comments in `electrical.hpp`, `builder.hpp` (Powered covers the controller).
- `datasheets/PADDLE.md`.
- `tests/parts/paddle_test.cpp` (design §11.1); `manual_test.cpp`: the PADDLE has 4 pin names.
- Check by hand: `bw run` on a scratch netlist (OSC 1 kHz on `TRIG_N`, `PADDLE min=30us max=9.93ms`) with `--record PULSE --vcd` shows 30 µs pulses every 1 ms.

Fence: `[parts]`, `[netlist]`, `[machine]` pass (units, electrical, every existing Machine's power unchanged).
Commit: `parts: phase 2 — PADDLE controller`

## Phase 3 — Knob

Design §5, §6, §7.

- `src/machine/knob_setting.{hpp,cpp}`, CMake.
- `session.{hpp,cpp}`: `knobs`, `setKnob`, `knob`, `knobs_`, reapplied after a rebuild.
- `src/cli/main.cpp`: `--knob`.
- `src/ui/main.cpp`: `--knob`; `side_panel.cpp`: Knob section and arrow keys.
- Tests: `knob_setting_test.cpp`; `session: knob kept across power and rebuild` (netlist-only part; the rebuild part comes in phase 6).
- Release screenshot of the scratch netlist with `--knob J2:42%`: the Knob section at 42 %, viewed.

Fence: `[machine]`, `[session]` pass; release builds; the screenshot viewed.
Commit: `ui: phase 3 — knob through the Session`

## Phase 4 — Knob meter netlist

KNOB_METER_DESIGN.md (Netlist, Manuals). ADD_MACHINE.md step 2.

- `netlists/knob.bwn`, `manuals/knob/user.md`, `service.md`; `"knob"` in `manuals_test.cpp`.
- `tests/machines/knob_test.cpp` on `LoadNetlistFile`: netlist without warnings, reads the knob, follows a turn, one measuring cycle (design §11.3).
- Release: `bw run netlists/knob.bwn --time 2s --knob J2:50%` timed (expected well above 1×: the stopwatch's load).
- `bw export-kicad netlists/knob.bwn -o boards/knob/knob.net`.

Fence: `knob:` cases and `manuals` pass.
Commit: `knob: phase 4 — netlist, manuals, fault-free tests`

Then **stop for the user's layout**, with the Board constraints of KNOB_METER_DESIGN.md (Board).

## Phase 5 — Board

ADD_MACHINE.md step 3.

- The user's `boards/knob/knob.kicad_pro` and `knob.kicad_pcb`; `bw check-board` prints `ok`; `drc_knob` appears (reconfigure).
- Read the routing of `VCC` at `J2` pin 3 and of `C3`; if a constraint is missed, ask the user before phase 6.
- `"knob"` in `boards_test.cpp`, `electrical_test.cpp`, `board_scene_test.cpp`; `knob_test.cpp` fault-free cases on `LoadNetlistMachine`.

Fence: `[board]`, `[machine]`, `[view]`, `knob:` pass; `drc_knob` passes.
Commit: `knob: phase 5 — Board`

## Phase 6 — Puzzles

KNOB_METER_DESIGN.md (Puzzles). ADD_PUZZLE.md.

- `puzzles/dead_knob.bwp`, `runaway.bwp`, `single_figures.bwp`, `odd_tens.bwp`, `four_to_seven.bwp`, the crack percentages read on the routed Board.
- `test_points.{hpp,cpp}`: `DeadKnobCrack`, `SingleFiguresCrack`.
- `knob_test.cpp`: titles and briefs; one case per Puzzle; Dead knob checks `J2` in `power().unpowered` and `PULSE` floating. `session_test.cpp`: the knob kept across a Repair and power-on.
- `KNOB_METER_DESIGN.md`: the crack lines filled in.
- `bw run puzzles/<each>.bwp --time 0.4s --knob J2:<n>%` by hand.

Fence: `knob:` and `[session]` cases pass.
Commit: `puzzles: phase 6 — knob meter`

## Phase 7 — Release check and docs

Design §8.8, §8.9, §11.4. AGENTS.md step 5.

- `session: knob meter with every instrument`.
- Release `bw_ui` on each Puzzle and on the netlist with `--knob` and every instrument attached: 1.00×, UI under ~1 ms; screenshots viewed.
- `docs/recipes/ADD_CONNECTOR_DEVICE.md` (new), `AGENTS.md` recipe list, ADD_BOARD_DEVICE pointer, ADD_PART 74LS175, ADD_MACHINE reference commits; `GLOSSARY.md` (Input Device, Knob, Powered); `ARCHITECTURE.md` Power paragraph and row P4i done with the date and links; this plan cut down to the status table and notes.

Fence: full suite green; the screenshots viewed.
Commit: `docs: P4i implemented, user check pending`

## Implementation notes

- Docs review (before phase 1): the circuit and every predicted reading held. Fixed: the Datasheet check builds Parts without parameters (`PartPinNames` gives `PADDLE` its own); `--knob` checked in `bw_ui`'s `main()` with the other options; debug runs at 0.12× here, the long cases are `[slow]` and step the knob within one power-on; comments and docs saying only chips are Powered; the count formula's 0.2 µs (half way sits on a boundary); Dead knob compares segment masks; instrument Clips on pads.
- Phase 4: the netlist worked as first written, no warnings; every reading exact (00, 20, 50, 79, 99; 30 → 59 after a turn). Release headless: 14× real time. `knob: reads the knob` (three seeds, one case) takes 19 s in debug.
- Phase 5: the user's layout keeps both Puzzle constraints. `VCC`: `J2` pin 3 is a leaf, a 23 mm straight run on B.Cu to `U6` pin 14 (crack `J2.3 U6.14`). `C3`: `U10` pin 13 → `U8` pin 11 → `U7` pin 14 on F.Cu, with a long straight stretch on the last leg (crack `U8.11 U7.14`). `J2` ended up on the right edge, which no doc depends on.
- Phase 6: every Puzzle as predicted, first time, on seeds 1–3 where tested: Dead knob holds the same segments at 20 % and 80 %; Runaway adds 30 each cycle; 47 % → 07; 0 % → 10, 25 % → 35; 0 % → 04, 9 % → `0` and the shape of 13. On a damaged Board the Nets are named from the Copper, so the bridge test compares `U12` `C` and `LT_N` pin Nets rather than `L2` and `VCC`. `knob:` cases take 55 s in debug.

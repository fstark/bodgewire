# P5i Switches on the Board — Implementation Plan

Design: [SWITCHES_DESIGN.md](SWITCHES_DESIGN.md)
Spec: [SWITCHES.md](SWITCHES.md)

Each phase ends with its fence: the checks listed, `cmake --build --preset debug`, the ctest subset named, `clang-format` on the C++ files touched, then one commit with the message given (plus the attribution line). The commit message names the subset run. Only this step's paths are committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Kernel: switched Pin, external drives, waking | `bce6852` |
| 2 | `Switch`, `SwitchNetwork`, `BUTTON` and `KEY`, builder, electrical, session | `7248627` |
| 3 | 74145 and the switch bench | `e1516b6` |
| 4 | `fault switch`: syntax, Machine, swap, dice | |
| 5 | `bw_ui`: keycaps on the Board, PC keys through Typing | |
| 6 | Release check, recipes, glossary, roadmap | |

## Phase 1 — Kernel

Design §1.

- Before any change: release build, `bw run netlists/bus_load.bwn --time 20ms`, `netlists/tennis.bwn --time 2s` and `netlists/led_computer.bwn --time 2s` headless ratios (median of 3).
- `logic.{hpp,cpp}` (`Switched`, `DriveOf`, `Resolver::merge`, `==`, `strength`), `circuit.{hpp,cpp}` (`switchPin`, the Check), `simulator.{hpp,cpp}` (`switched`, `external_`, `externalDrives`, `resolveNet`, `powerOn`, `runUntil`), `component.hpp` (`external`, `externalChanged`, `wake(other, delay)`).
- `tests/sim/switched_test.cpp` with a minimal test-only joining Component (two or three `Switched` Pins, one group, written in the test) for the kernel cases of design §13: `Resolver::merge` against `add`, Supply drive, the wake on an external change, no wake on its own drive, power-on Z. The full `SwitchNetwork` cases come in phase 2.
- The same three ratios after: within noise.

Fence: full debug `ctest --preset debug -j 12` (kernel).
Commit: `sim: phase 1 — switched pins: external drives and waking`

## Phase 2 — Switch Parts

Design §2–§6, §9 (session and CLI, not the UI).

- `src/parts/switch.{hpp,cpp}` (new; `button.*` removed), `CMakeLists.txt`, `catalogue.{hpp,cpp}` (`PartKind::Switch`, `BUTTON`, `KEY`), `boards/footprints/SW_Cherry_MX_1.00u_PCB.kicad_mod`, `kicad_netlist.cpp`.
- `builder.{hpp,cpp}` (`StuckSwitch`, fifth argument, `wireSwitches`, `SwitchOf`), `net_pins.cpp`, `electrical.cpp`, `machine.cpp` (`PartKind` renames only), `session.{hpp,cpp}` (`switches`, `switchKeys`, wake), `cli/main.cpp`, `ui/main.cpp` (renames: `buttons()` → `switches()`), `chip_panel.cpp` and `manual.cpp` if they name the kind.
- `datasheets/BUTTON.md` updated, `datasheets/KEY.md` new.
- Tests: `switched_test.cpp` network cases, `switch_test.cpp` (replaces `button_test.cpp`), `builder_test`, `electrical_test`, `session_test`, `runner_test`; `tests/CMakeLists.txt`. Renames only (`Button`, `ButtonOf`, `parts/button.hpp` → `Switch`, `SwitchOf`, `parts/switch.hpp`; the `wake(id())` calls stay): `tests/machines/{dice,tennis,typewriter,stopwatch,led_computer,heartbeat}_test.cpp`, `tests/netlist/ram_bench_test.cpp`, `tests/machine/test_points.{hpp,cpp}`; `tests/parts/power_on_reset_test.cpp` rewritten (its hand-built button used `attach`).
- The six button Machines' tests pass unchanged, including the 110's Blackout seed (same random draws).

Fence: full debug `ctest --preset debug -j 12` (a Part six Machines use, the builder).
Commit: `parts: phase 2 — one switch Part for buttons and keys, joined by a switch network`

## Phase 3 — 74145 and the bench

Design §7, §11.

- `src/parts/ttl145.{hpp,cpp}`, `delays.hpp`, `CMakeLists.txt`, catalogue row, `datasheets/74145.md`, `tests/parts/ttl145_test.cpp`.
- `tests/data/switch_bench.bwn`, `tests/netlist/switch_bench_test.cpp` (design §11, §13); `switchKeys()` on the bench in `session_test`.
- Check by hand: `./build/debug/src/cli/bw run tests/data/switch_bench.bwn --time 20us --press SW6:0:20us --probe U3.2Y` (key 5: row 1, column 1, read by `U3.2A`) shows `KEY1` pulsing high while row 1 is scanned.

Fence: `ctest --preset debug -j 12 -R "145|switch|catalogue|manuals|session"`.
Commit: `parts: phase 3 — 74145 and the switch bench`

## Phase 4 — Switch Faults

Design §8.

- `puzzle.{hpp,cpp}`, `machine.{hpp,cpp}`; `puzzle_test` (new case, catch-all strings), `machine_test` (errors, swap), `dice_test` (`InlinePuzzle` closed and open).

Fence: full debug `ctest --preset debug -j 12` (`Machine::build`, `BuildCircuit` callers: every Machine).
Commit: `faults: phase 4 — fault switch closed|open`

## Phase 5 — UI

Design §10.

- `src/ui/keycap.{hpp,cpp}` (moved code), `keyboard_window.cpp`, `board_view.{hpp,cpp}`, `board_keys.{hpp,cpp}`, `main.cpp`, `toolbar.cpp` if its text needs it, `src/ui/CMakeLists.txt`.
- Release build. Screenshots, viewed:
  - the 090 typewriter with its keyboard window (`bw_ui puzzles/…typewriter….bwp`), compared with one taken before the move: the same caps;
  - the 030 dice with `--press SW1:0:2s`: the button as before;
  - a scratch keypad Machine in the scratchpad (not committed), laid out as `<root>/netlists/keypad.bwn` and `<root>/boards/keypad/keypad.kicad_pcb`: `POWER` and eight `KEY`s (one-, two- and four-character legends), each leg on a Net of its own (no copper needed), placed by `bw place` with a `.place` file giving the `KK-2` connector and the `MX-1U` keys rules (`edge`, `row`/`next`: they are not grid packages); `bw_ui` with `--press SW2:0:2s`: keycaps with legends, one down.
- Check by hand in a live `bw_ui` (not possible headless): noted as user check pending.

Fence: full debug `ctest --preset debug -j 12` (UI); release builds.
Commit: `ui: phase 5 — keycaps on the Board, PC keys through Typing`

## Phase 6 — Release check, docs

- Release: `bw run` headless ratios of `bus_load`, 070 and the 110 against phase 1's; the 070 `bw_ui` ratio with every instrument at 1.00×.
- Recipes, glossary and roadmap as design §12; the 110 or 030 Machine docs if they describe the button as a Driver.
- This plan cut down to the status table and notes ("user check pending").

Fence: full debug `ctest --preset debug -j 12` green; release builds.
Commit: `docs: P5i implemented, user check pending`

## Notes

- Phase 1 speed (release, headless, median of 3): before 0.392× `bus_load`, 1.974× 070, 5.83× 110 in the morning; the same build again just before the change 0.363×, 1.726×, 5.03×; after 0.374×, 1.86×, 5.22×. The machine drifts more than the change: within noise.

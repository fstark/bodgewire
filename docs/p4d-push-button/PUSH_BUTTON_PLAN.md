# P4d Push Button — Implementation Plan

Design: [PUSH_BUTTON_DESIGN.md](PUSH_BUTTON_DESIGN.md)
Spec: [PUSH_BUTTON.md](PUSH_BUTTON.md); Machine: [DICE.md](../machines/030-dice/DICE.md), [DICE_DESIGN.md](../machines/030-dice/DICE_DESIGN.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | `Simulator::wake`; Button Part: `Button`, `PartKind::Button`, catalogue, footprint, Datasheet | done 28ecaea |
| 2 | Builder `wireButtons`, `ButtonOf` | done e5983f3 |
| 3 | `ParsePress`, `Runner::wake`, `bw run --press` | done c2958a2 |
| 4 | Dice netlist, manuals, headless fault-free test | done b23b235 |
| 5 | Courtyards from lines, dice Board (**user layout**), per-Machine loops, `bodyAt` | 5.1 done 23e790e; `bodyAt` code e86fa8c; waiting for the layout |
| 6 | Buttons on damaged Boards, dice Puzzles + Symptom/Repair tests | 6.2 code done 175929e; rest needs the Board |
| 7 | Session `buttons` / `setPressed` / `pressed` | done 469ce1d (LED brightness test needs the Board) |
| 8 | Board view pressing and drawing, `bw_ui --press`, screenshots | code done e86fa8c; screenshots need the Board |
| 9 | Release check, user check, docs | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate: `ctest --preset debug -j 12`

All C++ clang-formatted (repo `.clang-format`; never format CMakeLists.txt).

---

## Phase 1 — Button Part

The Part and the one kernel call it needs, tested on hand-built Circuits. No netlist uses it yet.

### 1.1 — `Simulator::wake`

[src/sim/simulator.hpp](../../src/sim/simulator.hpp#L28), after `runUntil`: `void wake(ComponentId component);  // evaluate at now(), in the next runUntil`. In simulator.cpp: `Check(ToIndex(component) < componentDirty_.size(), "simulator: unknown component")`, then `queue_.push({now_, ToIndex(component), MakeStamp(0, true, LogicValue::Zero)})`.

### 1.2 — `Button`

`src/parts/button.hpp`, `button.cpp` as design §2: `kButtonDelay = 1`, Pins `{"1", Output, Ttl}`, `{"2", Output, Ttl}`, `attach`, `signal()`, `setPressed`/`pressed` (relaxed atomic), `powerOn` (`setOutput` Z on both Pins), `evaluate` (`ctx.drive(signal_, pressed() ? rail_ : LogicValue::Z, kButtonDelay)`). Add `button.cpp` to `src/parts/CMakeLists.txt`.

### 1.3 — Catalogue, footprint, Datasheet

- [catalogue.hpp](../../src/parts/catalogue.hpp#L15): `Button` appended to `PartKind`.
- [catalogue.cpp](../../src/parts/catalogue.cpp): `CreateButton` beside `CreateLed` (no parameters: `"BUTTON takes no parameters"`); entry `PartInfo{"BUTTON", "push button", "TACT-6x3.5", Button, &CreateButton}` between `74LS86` and `LED`.
- [kicad_netlist.cpp](../../src/board/kicad_netlist.cpp#L15): array size 7, `{"TACT-6x3.5", "Button_Switch_THT:SW_PUSH_1P1T_6x3.5mm_H5.0_APEM_MJTP1250"}`.
- `datasheets/BUTTON.md` (design §2).

### 1.4 — Tests

- `tests/sim/simulator_test.cpp`: `simulator: wake evaluates at now` (counting Component; `runUntil(5)`, `wake`, `runUntil(10)` → one more evaluation, at time 5).
- `tests/parts/button_test.cpp` `[parts]`: design §12 row, on a hand-built Circuit (`GND` tie Supply, signal Net tied Weak 1, `attach(0, Zero)`, connect Pin 1 only); TTL contention via a second Component driving 1. Add to `tests/CMakeLists.txt`.
- `kicad_netlist_test`: `KicadFootprint("TACT-6x3.5")` if that test lists packages.

### Fence

- [ ] `Simulator::wake`, `Button`, `PartKind::Button`, catalogue entry, footprint, `BUTTON.md` exist
- [ ] `ctest --preset debug -j 12` passes (incl. `catalogue_test`, `manuals_test` Datasheet checks)
- [ ] Commit: `"buttons: phase 1 — Button Part and Simulator::wake"`

---

## Phase 2 — Netlist Builder

### 2.1 — `wireButtons`

[builder.cpp](../../src/netlist/builder.cpp): member `std::set<std::pair<std::string, PinIndex>> notConnected_;`; `wireButtons()` called after `tieResistors()` (design §5.1, error text `"<ref>: a button needs one pin on GND or VCC and the other on a signal net"`); in the connect loop, `else if (notConnected_.contains(pin)) {}` before `else if (part.component != nullptr)`. The Connection is still pushed.

### 2.2 — `ButtonOf`

[builder.hpp](../../src/netlist/builder.hpp): declaration as design §3.3; definition next to `PinNet`: find `reference` in `loaded.parts` with `info->kind == PartKind::Button`, then in `loaded.components`; `static_cast<Button*>`. Include `parts/button.hpp` in builder.hpp.

### 2.3 — Tests

`tests/netlist/builder_test.cpp`: design §12 row (netlist texts with `part J1 POWER`, `part R1 R value=4k7`, `part SW1 BUTTON`, `part U1 74LS04`). Pressing through `ButtonOf(...)->setPressed(true)` + `sim.wake` drives the input Net of `U1.1A` to 0 and `U1.1Y` to 1.

### Fence

- [ ] Builder wires Buttons; refusal for rail–rail and signal–signal
- [ ] `ButtonOf` works; tests pass
- [ ] Commit: `"buttons: phase 2 — builder wiring and ButtonOf"`

---

## Phase 3 — Presses Headless

### 3.1 — `ParsePress`

`src/machine/press.hpp`, `press.cpp` as design §4; add to `src/machine/CMakeLists.txt`. `tests/machine/press_test.cpp` (design §12 row); add to tests CMake.

### 3.2 — `Runner::wake`

[runner.hpp](../../src/runner/runner.hpp#L61): `CommandKind::Wake`, `Command::component`, `void wake(ComponentId component);` (queues). `apply`: wake only when not Off. `tests/runner/runner_test.cpp`: design §12 row (Circuit: Button on `GND` + pulled-up Net, watched by a recording Consumer as the existing Runner tests do).

### 3.3 — `bw run --press`

[cli/main.cpp](../../src/cli/main.cpp): `--press` in the option list and usage (`[--press REF:DOWN:UP,...]`); `Options::press` filled by `SplitCommas` + `ParsePress` (error → usage error). After loading: `ButtonOf` per reference, else `UsageError("'" + ref + "' is not a button")`. Edges: a vector of `{Time at, Button*, bool down}` sorted by time. The stepped loop condition becomes `tv || !probes.empty() || !edges.empty()`; step end `min(end, now + 1_ms, next edge)`; after `runUntil`, apply every edge with `at == sim.now()`: `setPressed(down)`, `sim.wake(id)`.

### Fence

- [ ] `press_test`, `runner_test` pass
- [ ] `bw run` of a test netlist with a Button and `--press SW1:1ms:2ms --record BTN_N --vcd /tmp/b.vcd`: the VCD shows `BTN_N` low from 1 ms + 1 ps to 2 ms + 1 ps (checked by hand on a small netlist in `/tmp`: `J1`, `R1` pull-up, `SW1`, a 74LS04 input on `BTN_N`)
- [ ] Commit: `"buttons: phase 3 — press parsing, Runner::wake, bw run --press"`

---

## Phase 4 — Dice Netlist and Manuals

### 4.1 — Netlist

`netlists/dice.bwn` exactly as [DICE_DESIGN.md](../machines/030-dice/DICE_DESIGN.md#netlist), with a header comment in the style of `chaser.bwn` (what the Machine does, where its spec is).

### 4.2 — Manuals

`manuals/dice/user.md` (`# Dice`, `## What it does`, one paragraph) and `manuals/dice/service.md` (`# Dice — Service Manual`, `## Components`, one line per reference) per DICE_DESIGN.md, Manuals. Add `"dice"` to the loop in [manuals_test.cpp](../../tests/machines/manuals_test.cpp#L41) if it needs only the netlist; otherwise in phase 5.

### 4.3 — Fault-free test

`tests/machines/dice_test.cpp` `[machines]`, helpers in the shape of [chaser_test.cpp](../../tests/machines/chaser_test.cpp#L26-L100): `LoadNetlistFile(BW_NETLIST_DIR "/dice.bwn")` (no Board yet), `warnings.empty()`. `Roll(Time held)`: fresh build, `powerOn(1)`, run to 1 ms, press, run `held`, release, run 100 µs; return the face from `N0`–`N2` and the seven LED states (cathode Net value 0 = lit, via `PinNet(Dn, "K")`). Test: `held` = 100 + k µs, k = 0…5 → faces form {1…6}; each LED pattern matches its face (DICE.md, Circuit table). Add to tests CMake.

### Fence

- [ ] `dice.bwn` loads with no warnings; `dice_test` passes
- [ ] `bw run netlists/dice.bwn --press SW1:1ms:1100us --time 2ms --record N0,N1,N2` runs
- [ ] Commit: `"dice: phase 4 — netlist, manuals, fault-free test"`

---

## Phase 5 — Dice Board

### 5.1 — Courtyards from lines

[kicad_board.cpp](../../src/board/kicad_board.cpp#L192): design §5.3. `tests/board/kicad_board_test.cpp`: a minimal footprint with four `fp_line`s on `F.CrtYd` (at 0° and 90°) → 4 corners; with an `fp_rect` too → the rect.

### 5.2 — Layout (**user**)

1. `bw export-kicad netlists/dice.bwn -o boards/dice/dice.net`.
2. **User** places and routes `boards/dice/dice.kicad_pcb` in KiCad 10 per DICE_DESIGN.md, Board (die-face LEDs, `SW1` at the front edge, straight segments for the two cracks).
3. `bw check-board netlists/dice.bwn boards/dice/dice.kicad_pcb` → `ok`; `drc_dice` passes after re-running CMake (glob).

Stop and wait for the user here.

### 5.3 — Per-Machine loops and `bodyAt`

- Add `"dice"` to [boards_test.cpp](../../tests/board/boards_test.cpp#L81) (plus `boards: dice matches its netlist` like the chaser's at L48), [electrical_test.cpp](../../tests/machine/electrical_test.cpp#L73), [board_scene_test.cpp](../../tests/view/board_scene_test.cpp#L146) and [L425](../../tests/view/board_scene_test.cpp#L425), and `manuals_test` if not done in 4.2.
- `BoardScene::bodyAt` (design §3.7): first loop as `componentAt`'s courtyard loop, skipping a Component if any of its pads `Covers(at)`. Test in `board_scene_test.cpp` on the dice (design §12 row); `SW1`'s courtyard must come from its lines (not `PadBox`).
- `dice_test` fault-free now also through `LoadNetlistMachine` (Board build): same six faces.

### Fence

- [ ] `boards/dice/dice.kicad_pcb` committed; `check-board` ok; `drc_dice` passes
- [ ] All per-Machine loops include the dice; `bodyAt` test passes
- [ ] Commit: `"dice: phase 5 — Board, courtyards from lines, bodyAt"`

---

## Phase 6 — Damaged Boards and Puzzles

### 6.1 — Crack points

On the user's layout, pick (as in P4a/P4b, midpoint of a long straight segment):

- `kBtnNCrack`: `BTN_N` segment from `SW1` pad 1 to the rest of the Net;
- `kN2Crack`: `N2` segment on the branch to `U2` pin 3 only;
- `kSw1GndCracks`: a `GND` segment that cuts `SW1` pad 2 off the rail (test only).

Add them to [test_points.hpp](../../tests/machine/test_points.hpp) with a comment each.

### 6.2 — Buttons in `ElectricalNetlist`

[electrical.cpp](../../src/machine/electrical.cpp#L122): design §5.2 (`IsButton` next to `IsResistor`; a `buttons` map; left out unless one end's island is a rail and the other's a named signal). `electrical_test.cpp`: design §12 row, with `kSw1GndCracks` and `kBtnNCrack`.

### 6.3 — Puzzles

`puzzles/dead_button.bwp`, `corners.bwp`, `seven.bwp`, `odd_dice.bwp` per DICE_DESIGN.md, Puzzles (comment line, `machine dice`, `title`, `brief`, `fault`).

### 6.4 — Tests

`dice_test.cpp`: Puzzle files load with title, brief and `machine dice`; per Puzzle, Symptom then Repair (design §12 row). Rolls reuse `Roll` on the Puzzle's `Machine::build()`; Repairs through `Machine::mend` / `swap`.

### Fence

- [ ] Four Puzzles; each Symptom and Repair test passes
- [ ] Button left out when its rail end is cut off; kept on the dead_button crack
- [ ] Commit: `"dice: phase 6 — buttons on damaged Boards, Puzzles"`

---

## Phase 7 — Session

[session.hpp](../../src/session/session.hpp#L126) / session.cpp: `buttons()`, `setPressed`, `pressed`, member `pressed_`, `powerOff` release (design §6). `tests/session/session_test.cpp`: design §12 row (`waitIdle()` after `powerOff` before checking).

### Fence

- [ ] Session tests pass; TSan clean on `session_test` and `runner_test` (`/tmp/bw-tsan` build as before)
- [ ] Commit: `"buttons: phase 7 — Session presses"`

---

## Phase 8 — Board View and `bw_ui`

### 8.1 — Board view

[board_view.hpp](../../src/ui/board_view.hpp#L105): `ButtonTag`, `setButtons`, `held`, `takeLatchToggle`, members (design §7). [board_view.cpp](../../src/ui/board_view.cpp): input (design §5.4; release check before `if (!hoveredPanel) return;`), drawing after the LEDs (design §5.6, constants beside `kLedDark`).

### 8.2 — Frame loop and option

[ui/main.cpp](../../src/ui/main.cpp): `--press` (usage, `Options::press`, `SplitCommas` + `ParsePress`, failure → usage); `std::set<std::string> latched`; design §5.5 after the LED tags.

### 8.3 — Screenshots (release build)

- `bw_ui netlists/dice.bwn --press SW1:200ms:700ms --screenshot /tmp/dice_rolling.png --after 500ms`: all LEDs glowing, `SW1` drawn pressed.
- Same with `--after 1s`: one face, `SW1` released.
- View both PNGs.

### Fence

- [ ] Build clean; all tests pass
- [ ] Screenshots as above
- [ ] Commit: `"buttons: phase 8 — pressing on the Board, bw_ui --press"`

---

## Phase 9 — Release, User Check, Docs

1. Release build: all tests pass; `bw_ui` on the dice and each Puzzle with 8 Clips, 2 Hooks, probe and meter: status line 1.00×, ui time noted.
2. **User check**: hold to roll, right-click latch, the four Puzzles.
3. Docs: ARCHITECTURE P4d row → "done" with links to DICE.md, PUSH_BUTTON.md, PUSH_BUTTON_PLAN.md; this plan pruned to Implemented + notes (crack points, test times).

### Fence

- [ ] User confirms
- [ ] Commit: `"docs: P4d done"` (own paths only; the user edits shared docs in parallel)

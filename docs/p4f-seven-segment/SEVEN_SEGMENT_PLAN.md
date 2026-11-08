# P4f Seven-Segment Digits — Implementation Plan

Design: [SEVEN_SEGMENT_DESIGN.md](SEVEN_SEGMENT_DESIGN.md)
Spec: [SEVEN_SEGMENT.md](SEVEN_SEGMENT.md); Machine: [STOPWATCH.md](../machines/040-stopwatch/STOPWATCH.md), [STOPWATCH_DESIGN.md](../machines/040-stopwatch/STOPWATCH_DESIGN.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | `EvalContext::fell`; 74LS90 Part, Datasheet, tests | done 5b5d52e |
| 2 | 74LS47 Part, Datasheet, tests | done f8a69d7 |
| 3 | Lamps in the catalogue, DIGIT Part; `LedGlow` anodes, `LedConsumer`, Session lamps; TTL-only warning; `kMaxWatched` 48 | done 1419047 |
| 4 | Digit geometry (`digit_shape`) and drawing in the Board view | done a2deab7 |
| 5 | Stopwatch netlist, manuals, headless fault-free test | done 8a18984 |
| 6 | Stopwatch Board (**user layout**), per-Machine loops | done 8d3644a |
| 7 | Stopwatch Puzzles + Symptom/Repair tests | done |
| 8 | Session test, release check, screenshots, recipes, docs | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate: `ctest --preset debug -j 12`

All C++ clang-formatted (repo `.clang-format`; never format CMakeLists.txt). Each phase ends with its fence and one commit; commit only this milestone's paths.

---

## Implementation notes

- Phase 3: `probe consumer: touching many Nets in turn` went round the test pattern's Nets once, which is fewer than 48: it now loops until it passes `kMaxWatched`.
- Phase 5: every test passed as first written; the stopwatch tests take ~14 s in debug (the `[slow]` seconds case most of it).
- Phase 6: the first screenshot read 0.7x with every digit upright and `DS1`'s point lit; no geometry change.
- Phase 7: the routed `T0` runs `U9` pin 7 → `U6` pin 1 → `U6` pin 12, so no crack cuts `U6` pin 1 off alone. Instead of a reroute, Nineteen cracks `U6.1 U6.12`: `U6` `CKB` and `U9` `A` float together, the watch reads 0.10 after Reset and runs 0.10–0.19 round again (STOPWATCH.md updated). The four cracks are on long straight stretches; every Puzzle test passed as first written.

---

## Phase 1 — Falling Edges and the 74LS90

### 1.1 — `fell`

Design §8.1. Test in `tests/sim/simulator_test.cpp`: `simulator: fell only at a 1 to 0 change` (a probe Component recording `fell` on each evaluation; Stimulus 0 → 1 → 0).

### 1.2 — `Ls90`

[ADD_PART.md](../recipes/ADD_PART.md), chip with state; design §2 for pins, state, `kNextFive`, evaluate order, delays, catalogue row, Datasheet. Copy the shape of `src/parts/ls161.{hpp,cpp}`.

### 1.3 — Tests

`tests/parts/ls90_test.cpp` `[parts]`: design §12 row, Bench as `tests/parts/ls164_test.cpp`. The `QA` → `CKB` link is a Net joining Pins 12 and 1 in the Bench Circuit. Add to `tests/CMakeLists.txt`.

`catalogue_test` pinout check: one `VCC` and one `GND`, corner positions except `kOffCornerPower` (design §2); note in ADD_PART.md.

### Fence

- [ ] `fell`, `Ls90`, catalogue row, `datasheets/74LS90.md` exist
- [ ] `ctest --preset debug -j 12` passes (incl. `catalogue_test`, `manuals: every catalogue Part has a Datasheet`)
- [ ] Commit: `"parts: phase 1 — 74LS90 and EvalContext::fell"`

---

## Phase 2 — 74LS47

### 2.1 — `Ls47`

ADD_PART.md; design §3: pins, `Ls47Segments` (exposed for tests), X enumeration without allocation (`std::array`, bit loops), open-collector outputs, `kLs47`, catalogue row, Datasheet.

### 2.2 — Tests

`tests/parts/ls47_test.cpp` `[parts]`: design §12 row. Each output Net in the Bench gets a weak pull-up (`circuit.tie(net, One, DriveStrength::Weak)`) so a released output reads 1, not floating.

### Fence

- [ ] `Ls47`, catalogue row, `datasheets/74LS47.md` exist; tests pass
- [ ] Commit: `"parts: phase 2 — 74LS47"`

---

## Phase 3 — Lamps and the DIGIT Part

### 3.1 — Catalogue lamps

Design §4.1: `LampShape`, `Lamp`, `PartInfo::lamps`, `kLedLamps`, `kDigitLamps`; the LED row gets its lamps. `catalogue_test`: every lamp's Pins exist (create the Part in a scratch `Circuit`, look its Pin names up).

### 3.2 — `Digit`

Design §4.2: `src/parts/digit.{hpp,cpp}` (as `led.{hpp,cpp}`), `CreateDigit`, row `DIGIT`, footprint `DIGIT-0.56` (array size 8), `datasheets/DIGIT.md`. `tests/parts/digit_test.cpp`.

### 3.3 — Glow, consumer, Session, UI tag

- `LedGlow` second anode (design §5); `led_glow_test` cases.
- `LedNets` with a lamp index and two anodes; `LedConsumer` (design §5); update `led_consumer_test` and any other `LedNets{…}` construction (grep).
- Session `makeLive` and `leds()` from lamps, connected Pins only (`loaded.connections`), `LedLight::shape` (design §6).
- `LedTag::shape`, frame loop passes it; the Board view still draws only `Round` tags (digits come in phase 4: skip other shapes for now).

### 3.4 — Builder warning, watch budget

Design §8.2–8.3. `builder_test`: an unconnected `DIGIT` `DP` gives no warning, an unconnected 74LS04 input still does.

### Fence

- [ ] `grep -rn "LedNets{\|LedLight{" src tests` all updated; counter, chaser, dice Session tests unchanged and passing
- [ ] `ctest --preset debug -j 12` passes
- [ ] Commit: `"leds: phase 3 — lamps, DIGIT Part, watch budget 48"`

---

## Phase 4 — Drawing Digits

### 4.1 — `digit_shape`

Design §7.1, `src/view/digit_shape.{hpp,cpp}` (add to `src/view/CMakeLists.txt`). `tests/view/digit_shape_test.cpp`: design §12 row. Build the LTS courtyard corners by hand (local `(-2.16, -1.47)…(17.4, 11.63)` mm, pad 1 at (0, 0)), rotated about the origin by 0/90/180/270.

### 4.2 — Board view

Design §7.2 in [board_view.cpp](../../src/ui/board_view.cpp#L395): body once per digit Component, segments with `AddConvexPolyFilled`, colours `kDigitBody`, `kSegmentDim`.

### Fence

- [ ] `digit_shape_test` passes; `bw_ui` builds (release too: `cmake --build --preset release`)
- [ ] Screenshot deferred to phase 6 (no Board with a digit yet)
- [ ] Commit: `"view: phase 4 — 7-segment digits drawn on the Board"`

---

## Phase 5 — Stopwatch Netlist and Manuals

### 5.1 — Netlist, manuals

[ADD_MACHINE.md](../recipes/ADD_MACHINE.md) step 2; `netlists/stopwatch.bwn` exactly as [STOPWATCH_DESIGN.md](../machines/040-stopwatch/STOPWATCH_DESIGN.md#netlist) with every `...` written out; manuals per STOPWATCH_DESIGN.md, Manuals (Service Manual: one bullet per reference, each starting with it). Add `"stopwatch"` to `manuals_test`.

### 5.2 — Fault-free test

`tests/machines/stopwatch_test.cpp` `[machines]` with `LoadNetlistFile` (no Board yet):

- `Display(loaded, sim)`: for `DS1`–`DS3`, the seven segment Nets `PinNet(DSn, "A".."G")` → lit mask → `'0'`–`'9'` by the 74LS47 table (`Ls47Segments(v, true, true, true)` for v = 0..9), `' '` when no segment is lit, `'?'` otherwise; result `"d.dd"`.
- `Press(sim, loaded, "SWn", down, up)` helper: `ButtonOf` + `setPressed` + `sim.wake` at exact times (as the dice's `Roll`).
- Fault-free, seed 1: Stop 0.5–1 ms and Reset 1–1.5 ms first (the run flip-flop and counters start random) → `"0.00"`; Start 2–2.5 ms, Stop at 125.4 ms → `"0.12"`; Start again for 50 ms → `"0.17"`. `[slow]`: Start, read at 1.055 s → `"1.05"`. Read only mid-count (design §12).
- Check also two other seeds give the same results (seed-independent).

### Fence

- [ ] `stopwatch.bwn` loads with no warnings; manuals pass their checks; `stopwatch_test` passes
- [ ] `bw run netlists/stopwatch.bwn --press SW2:1ms:2ms,SW3:2ms:3ms,SW1:4ms:5ms --time 40ms --record H0,H1,H2,H3` runs
- [ ] Commit: `"stopwatch: phase 5 — netlist, manuals, fault-free test"`

---

## Phase 6 — Stopwatch Board

### 6.1 — Layout (**user**)

1. `./build/debug/src/cli/bw export-kicad netlists/stopwatch.bwn -o boards/stopwatch/stopwatch.net`.
2. **User** places and routes `boards/stopwatch/stopwatch.kicad_pcb` in KiCad 10 per STOPWATCH_DESIGN.md, Board (digits upright with the pad 1 row at the bottom, buttons at the front, the three Puzzle Traces).
3. `bw check-board netlists/stopwatch.bwn boards/stopwatch/stopwatch.kicad_pcb` → `ok`; `drc_stopwatch` passes after re-running CMake.

Stop and wait for the user here.

### 6.2 — Per-Machine loops

ADD_MACHINE.md step 3: `boards_test` (+ `stopwatch matches its netlist`), `electrical_test`, `board_scene_test` loops; `stopwatch_test` switches to `LoadNetlistMachine` + `build()`, same results.

### 6.3 — First look

Release: `bw_ui netlists/stopwatch.bwn --press SW2:50ms:100ms,SW3:100ms:150ms,SW1:200ms:250ms --screenshot /tmp/sw.png --after 1s`: three upright digits reading about 0.80, `DS1` decimal point lit, `DS3` blurred. Fix the geometry constants if it looks wrong (record in the notes).

### Fence

- [ ] `stopwatch.kicad_pro`, `stopwatch.kicad_pcb` committed (not the `.net`); `check-board` ok; `drc_stopwatch` passes
- [ ] Screenshot viewed, digits upright and legible
- [ ] Commit: `"stopwatch: phase 6 — Board, per-Machine loops"`

---

## Phase 7 — Puzzles

### 7.1 — Crack positions

For `T0` (cut `U6` pin 1 off), `DS1_E` (`U8.9 DS1.1`), `RUN` (`U11.5 U12.2`) and `VCC` (cut `U5` pin 5 off): read the routed Copper, pick the pad pair and a `%` on a straight stretch ([ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md)); check with `bw run puzzles/<name>.bwp --time 300ms --press … --probe …` that the right pad is cut off. If the routing makes one impossible (e.g. `U6` pin 1 in the middle of `T0`), stop and ask the user to reroute that Net. Write the chosen lines into STOPWATCH_DESIGN.md (Board, Cracks; Puzzles table).

### 7.2 — Puzzle files and tests

Six `puzzles/*.bwp` per STOPWATCH_DESIGN.md, Puzzles (comment line, `machine stopwatch`, title, brief, fault). Tests per design §12 row: title-and-brief case listing all six, one `TEST_CASE` each; crack helpers `NineteenCrack()`, `BrokenBarCrack()`, `NonStopCrack()`, `DarkSecondsCrack()` in `tests/machine/test_points.{hpp,cpp}`. Every Puzzle test starts with Stop, Reset (and Start where needed) as in phase 5.

### Fence

- [ ] Six Puzzles load; each Symptom and Repair test passes; three seeds checked by hand for the crack Puzzles
- [ ] Commit: `"stopwatch: phase 7 — Puzzles"`

---

## Phase 8 — Session, Release Check, Docs

### 8.1 — Session test

`session: stopwatch with every instrument` `[session][slow]`, shaped like `session: chaser with every instrument`: no presses; `leds()` lists 24 lamps (`DS1` `A`…`G`, `DP`, then `DS2`, `DS3`); 8 Clips on `U1`–`U4` outputs and `H0`–`H3`, 2 Hooks, probe, meter lead on a segment pad; `powerOn(1, 1_s)`, `waitIdle`; `DS1`'s `DP` lamp ≈ 1; no abort.

### 8.2 — Release check

ADD_MACHINE.md step 5: every Puzzle once in `bw_ui` with `--probe`, `--clip`, `--hook`, `--meter` attached and `--press` Stop, Reset, Start: 1.00×, UI under ~1 ms; view the PNGs (eights shows `DS2` as 8 after Reset).

### 8.3 — Docs

- Recipes (design §8.5), with reference commits.
- `docs/ARCHITECTURE.md` P4f row → done + date + summary, links; GAME_DESIGN unchanged.
- This plan pruned to Implemented + notes; user check pending.

### Fence

- [ ] Debug and release tests pass
- [ ] Commit: `"docs: P4f implemented, user check pending"`

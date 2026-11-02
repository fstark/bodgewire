# P4e Crack Placement — Implementation Plan

Design: [CRACK_PLACEMENT_DESIGN.md](CRACK_PLACEMENT_DESIGN.md)
Spec: [CRACK_PLACEMENT.md](CRACK_PLACEMENT.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Copper graph: `BuildCopperGraph`, `PadItemAt`, `Touches`; `FindIslands` on it | |
| 2 | `Crack` (placed) split from `CrackDecl`; `CrackBoard`, Machine, UI on `Crack` | |
| 3 | `PlaceCrack`, `Machine::placeCrack`, `Machine::cracks()`, crack tests | |
| 4 | Conversion table for the seven Puzzles and two test-only cracks | |
| 5 | New `fault crack` syntax, Puzzles rewritten, tests off hard-coded points | |
| 6 | Docs, release check | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate: `ctest --preset debug -j 12`

---

## Phase 1 — Copper graph

The contact graph that `PlaceCrack` walks, built with FindIslands' own shapes so "touching" means one thing.

### 1.1 — `src/board/copper.hpp`

Add `CopperGraph`, `BuildCopperGraph`, `PadItemAt`, `Touches` exactly as in design §2.3.

### 1.2 — `src/board/copper.cpp`

- `BuildCopperGraph`: the shape building and pair loop of today's `FindIslands`, but each touching pair `(a.item, b.item)` (items of arc chords are one Trace item; skip `a.item == b.item`) goes into `touching[a]` and `touching[b]`; Bodge Wires add `PadItemAt(wire.a)`↔`PadItemAt(wire.b)` (keep the `Check`). Sort and `unique` each list. Set `pads`, `vias`.
- `FindIslands`: `const CopperGraph graph = BuildCopperGraph(board);` then union every `i`–`touching[i][k]`; the rest (island numbering, `padIsland`/`viaIsland`/`traceIsland`) unchanged.
- `PadItemAt`: move out of the anonymous namespace, unchanged.
- `Touches(board, item, piece)`: a `ShapeBuilder` with the item's shape (pad: walk components to the item; via; Trace with `addTrace`), a second with `addTrace(0, piece)`; true if any pair shares a layer and `HullDistance <= a.radius + b.radius`.

### 1.3 — Tests (`tests/board/copper_test.cpp`)

- `"copper: graph of two pads and a Trace"`: pad items 0 and 1 each touch item 2; `pads == 2`, `vias == 0`.
- `"copper: a Bodge Wire joins its pads in the graph"`.
- `"copper: Touches"`: a short piece in the middle of the Trace touches the Trace item but neither pad; a piece at a pad touches it.
- Existing island tests and every per-Machine Board test pass unchanged.

### Fence

- [ ] Full build clean; all tests pass
- [ ] Commit: `"copper: phase 1 — copper graph"`

---

## Phase 2 — `Crack`

Separate the placed crack from the Puzzle line, so phase 5 can change the line alone.

### 2.1 — `src/machine/crack.hpp/.cpp`

Add `struct Crack` (design §2.2). `CrackBoard(const Board&, std::span<const Crack>)`. Drop `#include "machine/puzzle.hpp"`; include `machine/puzzle.hpp` in `crack.cpp` only if `LayerName` needs it (it does: keep it in the `.cpp`).

### 2.2 — Machine and UI

- `machine.hpp`: `std::vector<Crack> cracks_, cuts_;` `std::span<const Crack> cuts() const`; include `machine/crack.hpp`.
- `machine.cpp`: `LoadMachine` still reads the old `CrackDecl {line, layer, at}`; it builds `const Crack crack{decl.layer, decl.at}` and checks/pushes that. `mend`'s lambda, `cut` (`cuts_.push_back({layer, at})`), `rebuildBoard` take `Crack`.
- `src/ui/main.cpp:528`: `const Crack& cut`.

### 2.3 — Tests

`CrackDecl{n, layer, at}` used as a placed crack becomes `Crack{layer, at}`: [crack_test.cpp](../../tests/machine/crack_test.cpp), [electrical_test.cpp](../../tests/machine/electrical_test.cpp) (`crack` locals; `PowerMachine` keeps the Puzzle `CrackDecl`), [test_points.hpp](../../tests/machine/test_points.hpp) (`kR1VccCracks`, `kSw1GndCracks` become `Crack` arrays), [board_scene_test.cpp](../../tests/view/board_scene_test.cpp) (`Scene(…, std::span<const Crack>)`). Puzzle tests (`p.cracks`, `puzzle.cracks = …`) are untouched.

### Fence

- [ ] `grep CrackDecl src` shows only `puzzle.hpp`, `puzzle.cpp`, `machine.cpp`
- [ ] Full build clean; all tests pass
- [ ] Commit: `"machine: phase 2 — Crack split from CrackDecl"`

---

## Phase 3 — Placing a crack

### 3.1 — `PlaceCrack` (`src/machine/crack.cpp`)

Design §5.2–5.4: local `Path` (BFS, ascending neighbours), `Stretch`, `kCrackStep = 10'000`, the walk with `cursor`, the target walk, the separation check on `BuildCopperGraph(CrackBoard(board, {&crack, 1}))`. `Check(ia && ib, "PlaceCrack: not a pad centre")`. Error strings exactly as in design §2.2.

### 3.2 — `Machine::placeCrack`, `Machine::cracks()`

Design §2.4 and §3 point 3. `placeCrack` uses `padOf` (returns `"crack A B: unknown pad"` if nullopt; LoadMachine checks first, so only direct callers see it), `"two different pads"` when both `padOf` give the same pin, else `PlaceCrack(pristine_, *padPosition(a…), *padPosition(b…), percent)` with the prefix.

### 3.3 — Tests

- `tests/machine/crack_test.cpp`: every case of design §9 crack_test. Helpers next to `Joined()`: `Path2(Point a, Point corner, Point b)`, a via Board, a T Board, a loop Board.
- `tests/machine/machine_test.cpp`: `"machine: placeCrack"` on the fault-free test pattern: `placeCrack({"U1","QA"}, {"U13","2B"}, 50)` lands on `F.Cu` and cracking it separates `U1.14` from `U13.6` (`ElectricalNetlist` as in electrical_test); same pad → `"crack U1.QA U1.14: two different pads"`; `U1.QA`–`U5.A` (other Net) → `"…: pads not joined by Copper"`.

### Fence

- [ ] Full build clean; all tests pass
- [ ] Commit: `"machine: phase 3 — PlaceCrack"`

---

## Phase 4 — Conversion table

Work out each new line from today's point. Temporary code only, not committed.

### 4.1 — Temporary test `[.convert]` in `tests/machine/machine_test.cpp`

For each old crack (the seven Puzzles, `kR1VccCracks`, `kSw1GndCracks`):

1. Load the fault-free Machine; `T = FindCrackTrace(pristine, layer, at)`.
2. In `BuildCopperGraph(pristine)`, BFS from the items touching `T` at its start end, never entering `T`: the first pad item reached is pad A; same from the end end: pad B. Name them `REF.NUMBER` (walk components to the item).
3. For `p` in 0, 0.1, …, 100: `placeCrack(A, B, p)`; keep the `p` nearest the old point. Prefer the nearest whole percent if within 0.2 mm.
4. Print `name, old point, A, B, p, distance, same Trace index?`.

Hints from the Puzzle comments: `broken_trace` U1.QA–U13.2B; `dark_led` U5.QD–D1.K; `gap` R6.2–D5.A; `dark_chaser` U6.8–U5.1; `dead_button` SW1.1–R9.2; `loaded_dice` U2.3–U4.6; `unpowered_chip` U6.14–?.

### 4.2 — Record

Paste the table into this plan's Implementation notes; use pin names where the Puzzle comment uses them. Every row: distance ≤ 0.5 mm and the same Trace. If not, pick other pads by hand and note why.

### Fence

- [ ] Table in Implementation notes; temporary test deleted (`git diff` touches only this plan)
- [ ] Commit: `"docs: P4e crack conversion table"`

---

## Phase 5 — New syntax

One commit: the Puzzle files, the parser and the tests must change together.

### 5.1 — Puzzle line

- `puzzle.hpp`: `CrackDecl {line, PinRef a, b; double percent}`.
- `puzzle.cpp`: `Parser::crack`: 5 tokens, two pins with the `bridge` pin lambda (lift it to an anonymous-namespace `ParsePin`), `ParsePercent` (design §5.1). Errors: `expected: fault crack REF.PIN REF.PIN N%`, `bad position 'X'`.

### 5.2 — `LoadMachine`

Lift the bridge `known` lambda above the crack loop. Crack loop: `known(a) && known(b)`, then `placeCrack`; error on `decl.line`; push the `Crack`. Remove the `islands` count.

### 5.3 — Puzzles

Rewrite the seven `fault crack` lines from the phase 4 table; keep the `#` comments' wording.

### 5.4 — Test helpers

- New `tests/machine/test_points.cpp` (add to `tests/CMakeLists.txt`): `Crack PuzzleCrack(std::string_view puzzle)`, `Crack BoardCrack(std::string_view machine, const PinRef& a, const PinRef& b, double percent)` (design §9), `REQUIRE` on load.
- `test_points.hpp`: declarations; remove `kH0Crack`, `kC19Crack`, `kLedA5Crack`, `kSerCrack`, `kBtnNCrack`, `kN2Crack`, `kR1VccCracks`, `kSw1GndCracks`. Comments on the helpers say which Puzzle or which pads.
- Every use: a local at the top of the test, e.g. `const Crack h0 = PuzzleCrack("broken_trace");` then `h0.at` / `h0.layer` (`machine_test`, `electrical_test`, `board_repair_test`, `bodge_wire_test`, `scope_meter_test`, `test_pattern_faults_test`, `session_test`, `board_scene_test`, `counter_test`, `chaser_test`, `dice_test`). `kR1VccCracks` / `kSw1GndCracks` → one-element arrays from `BoardCrack` with the phase 4 pads. `PowerMachine` takes `std::vector<CrackDecl>` in the new form (`unpowered_chip`'s pads).

### 5.5 — Tests of the syntax and errors

- `puzzle_test`: design §9 cases; the old `F.Cu X Y` form now gives the `expected:` error.
- `machine_test` `"machine: load errors"`: crack line 6 becomes `U99.1 U1.QA 50%` → `"6: unknown reference 'U99'"`; `"machine: a crack must cut the copper"` becomes `"machine: crack errors"` (same pad; not joined).
- All Machine Symptom and Repair tests pass unchanged.

### Fence

- [ ] `grep -r "Crack{" tests` uses no literal coordinates except hand-made Boards
- [ ] `grep -E "fault crack [FB]\.Cu" puzzles` empty
- [ ] Full build clean; all tests pass
- [ ] Commit: `"machine: phase 5 — cracks placed by pads and position"`

---

## Phase 6 — Docs, release

1. Machine design docs: Puzzle tables in [COUNTER_DESIGN.md](../machines/010-counter/COUNTER_DESIGN.md), [LED_CHASER_DESIGN.md](../machines/020-led-chaser/LED_CHASER_DESIGN.md), [DICE_DESIGN.md](../machines/030-dice/DICE_DESIGN.md) show the new lines; the "segment long enough to crack" notes say placement now skips short stubs.
2. Release build: all tests pass; `bw_ui puzzles/broken_trace.bwp --screenshot` shows the crack where it was.
3. ARCHITECTURE P4e row → "done" with links; this plan pruned to Implemented + notes.

### Fence

- [ ] Commit: `"docs: P4e done"`

## Implementation notes

### Conversion table (phase 4)

All on the old crack's Trace and Layer, whole percents.

| Crack | New line | Old point | New point | Distance |
|---|---|---|---|---|
| `broken_trace` | `fault crack U1.QA U13.2B 79%` | F.Cu 137.59 99.17 | 137.676 99.169 | 0.086 mm |
| `unpowered_chip` | `fault crack U6.VCC U10.VCC 24%` | F.Cu 164.85 51.05 | 164.880 51.047 | 0.030 mm |
| `dark_led` | `fault crack U5.QD D1.1 51%` | F.Cu 192.52 122 | 192.513 122.000 | 0.007 mm |
| `gap` | `fault crack R6.2 D5.2 51%` | F.Cu 73.46 101.05 | 73.460 101.041 | 0.009 mm |
| `dark_chaser` | `fault crack U6.8 U5.1 50%` | B.Cu 70.17 73.69 | 70.280 73.690 | 0.110 mm |
| `dead_button` | `fault crack SW1.1 R9.2 48%` | F.Cu 100.03 87.00 | 100.010 87.000 | 0.020 mm |
| `loaded_dice` | `fault crack U2.3 U4.6 52%` | B.Cu 58.86 71.17 | 58.858 71.206 | 0.036 mm |
| `kR1VccCracks` (counter) | `R1.1 U1.16 58%` | B.Cu 128.31 106.30 | 128.310 106.304 | 0.004 mm |
| `kSw1GndCracks` (dice) | `SW1.2 J1.2 7%` | F.Cu 109.23 89.77 | 109.120 89.880 | 0.156 mm |

`Machine::padOf` only knows chips, so `placeCrack` resolves pads with `padPosition` (chip pins by name or number, passives by pad number) and compares positions for "two different pads". Fixed in phase 5.

Phase 3: `placeCrack(U1.QA, U13.2B, 50)` lands on B.Cu (the path goes through a via), so the machine_test checks separation, not the Layer.

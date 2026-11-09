# P4h Pin Faults — Detailed Design

Implements [PIN_FAULTS.md](PIN_FAULTS.md): stuck inputs, dry Joints, the leg/pad rule, the Resolder repair and two Puzzles. No new Machine: the Puzzles go on the 060 bouncing ball ([BOUNCING_BALL_DESIGN.md](../machines/060-bouncing-ball/BOUNCING_BALL_DESIGN.md)) and the 040 stopwatch ([STOPWATCH_DESIGN.md](../machines/040-stopwatch/STOPWATCH_DESIGN.md)).

Recipe followed: [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md) (both Puzzles); this step also extends it (§9). All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

Pins are written `REF.PIN` by number or name, as in every Puzzle line; the Machine stores pin numbers (`Machine::padOf`).

---

## 1. Puzzle syntax — `src/machine/puzzle.{hpp,cpp}`

```cpp
struct JointDecl {
    std::uint32_t line;
    PinRef pin;  // by number or name: U15.13, U15.2D
};
// Puzzle:
    std::vector<JointDecl> joints;  // dry Joints
```

- `Parser::line`: `fault joint` → `joint(number, tokens)`: exactly 3 tokens and `ParsePin(tokens[2])`, else `"expected: fault joint REF.PIN"`.
- The catch-all becomes `"expected: fault stuck|crack|bridge|joint ..."`.
- `fault stuck` syntax is unchanged.

## 2. Loading — `LoadMachine` in `src/machine/machine.cpp`

Stuck lines: the `IsDriver` check becomes

```cpp
const bool chip = kindOf(decl.reference) == PartKind::Chip;  // lambda over loaded.parts
if (!IsDriver(spec.kind) && !(chip && spec.kind == PinKind::Input)) {
    error(decl.line, ref + "." + pin + " (" + spec.name + ") is not an output or a chip input");
```

A connector input (`J2.VIDEO`) stays refused: the TV reads the connector's listed Net, so a stuck pin there would change nothing. The existing kind lookup in the bridge loop becomes a local lambda `kindOf(reference)` shared by stuck, bridge and joint.

Joint lines, after the bridges, with `known()`:

- `written = "joint REF.PIN"` (as given);
- unknown reference / no pin: `known()`'s messages;
- not a chip, or a pin of kind `Power` or `NoConnect`: `written + ": chip signal pins only"`;
- stored as `padOf(decl.pin)` (pin number) in `dry_`, once (a repeated line is not an error and adds nothing).

## 3. Machine — `src/machine/machine.{hpp,cpp}`

New public members:

```cpp
// Resolders a chip pin's Joint (number or name): removes its dry Joint if any. True also on a
// good Joint (the tool must not tell); false: unknown, or not a chip signal pin.
bool resolder(const PinRef& pin);
std::span<const PinRef> dryJoints() const { return dry_; }  // pin numbers
```

New private: `std::vector<PinRef> dry_;` and

```cpp
// The dry pin whose pad covers `at`, read as its leg: only with F.Cu active.
std::optional<PinRef> dryLegAt(Layer layer, Point at) const;
```

- `swap` unchanged: it erases every `StuckPin` of the chip, inputs included, and keeps `dry_`.
- `build()`: `ElectricalNetlist(design_, *designCircuit_, board_, dry_)`.
- `resolder`: `padOf(pin)`, chip and signal pin as the loader's joint check (one private helper `chipSignalPin(const PinRef&) -> std::optional<PinRef>` used by both), then `std::erase_if(dry_, SamePin)`; true. `board_` is untouched: nothing to rebuild.
- `dryLegAt`: nullopt unless `layer == Layer::Front`; else, for each `dry_` entry, its `Pad` in `board_.components` (by reference and number) and `Covers(pad, at)` (as `islandAt`). An F.Cu Trace contact inside a dry pad also reads the leg; pad snapping wins in practice.
- `probeNet`: first `if (const auto leg = dryLegAt(layer, at)) return PinNet(built, leg->reference, leg->pin);` — the leg's own Net (§5). Then the island path as today.
- `connected`: `const auto legA = dryLegAt(layerA, a), legB = dryLegAt(layerB, b);` if either is set, return `legA && legB && SamePin(*legA, *legB)`; else the island path. A dry leg is joined to nothing, not even its own pad.

The leg rule is written for dry pins only: for every other chip pin the leg and the pad are on one Net, so reading the island is the same. This also keeps power pins on the pad: an Unpowered chip's `VCC` pin is listed on `VCC` by the builder whatever its island, and its pad must still read dark (P3f `unpowered_chip`).

## 4. Electrical netlist — `src/machine/electrical.{hpp,cpp}`

```cpp
Netlist ElectricalNetlist(const Netlist& design, const LoadedCircuit& designCircuit,
                          const Board& board, std::span<const PinRef> dry = {});  // pin numbers
```

In the connection loop, a Connection whose `{reference, pin + 1}` is in `dry` is skipped (after the resistor/button and power checks, which never see a dry pin: §2 refuses them). Naming is unchanged: the island keeps the name its pins give it, the dry pin included, so `IslandNetNames` (used by `probeNet`) needs no `dry`. A dry pin on an island no other listed pin names leaves that island without an electrical Net: the pad then reads nothing, as bare Copper, and later NetIds shift (as with cracks; nothing relies on them, the "NetIds match the design's" comment gains "without dry Joints"). Neither Puzzle hits it.

The dry pin is on no Net: `BuildCircuit`'s `finalize()` gives it a private Net (`Circuit::finalize`, "U15.2D"), and `PinNet` falls back to it (no Connection). An input there floats (TTL reads high); an output drives only that Net.

## 5. Builder — `src/netlist/builder.cpp`

`StuckPin`'s comment becomes: "Stuck-at Fault: an output holds its Net at `value` (Strong); an input reads `value`, its Net unchanged."

- The `Check` accepts `IsDriver(kind) || kind == PinKind::Input`.
- Main loop: `stuckAt` keeps its branch for drivers (tie the listed Net, do not connect). A stuck input is neither tied nor connected there; its Connection is recorded as for any pin, so `PinNet`, Clips, Hooks and the probe on the leg read the listed Net.
- After the loop, for each stuck input of a placed, powered Component:

```cpp
const NetId own = circuit.addNet(reference + "." + pinName + " stuck");  // not in loaded.nets
circuit.tie(own, s.value, DriveStrength::Strong);
circuit.connect(component, s.pin, own);
```

  This also covers a stuck input that is unlisted or dry (the electrical netlist leaves it off every Net): it reads the stuck value, as the chip's inside does whatever the Joint. An Unpowered chip's stuck input is skipped like its other pins.

`builder_test`: the existing "stuck pin ties its net" stays; new "builder: stuck input reads its value, its Net unchanged".

## 6. Repairs — `src/machine/repair.{hpp,cpp}`, `src/session/session.cpp`, CLIs

- `struct ResolderRepair { PinRef pin; };` added last to `RepairAction`.
- `ParseRepair("resolder:U15.13")`: one field, `ParsePinRef`, else "bad repair". `FormatRepair`: `"resolder U15.13"` (pin as given).
- `Session::repair`: `[&](const ResolderRepair& r) { return machine_->resolder(r.pin); }`. No blob (a Mend's blob marks Copper; the Joint is under the pin). Refused while powered, like every Repair.
- `bw run` (`src/cli/main.cpp`): `Repair()` is a `get_if` chain ending in `std::get<UnbridgeRepair>`: a `ResolderRepair` branch must go before that final `else` (it would throw `bad_variant_access`, not fail the build); it uses the `done` path ("cannot apply '...'"); the usage line and the `Repair()` comment list `resolder:REF.PIN`.
- `bw_ui` (`src/ui/main.cpp`): `kUsage` lists `resolder:REF.PIN`; the `--repair` loop already goes through `Session::repair`.

## 7. UI — `src/ui/`, `src/view/board_scene.{hpp,cpp}`

- `Tool::Resolder` after `Wire` in `board_view.hpp`; key `R` in `BoardView::keys()` (free: L F P S M K W C O V U are taken).
- `toolbar.cpp`: button `"Resolder (R)"` after `Wire (W)`; hint `blocked = powered`, `"Resolder: power off first"` / `"Resolder: click a chip pin"`. The Probe hint becomes `"Probe: click a pin (F.Cu: leg, B.Cu: pad) or trace"`.
- `main.cpp` click switch, `case Tool::Resolder`: powered → `Refused`; not a pad pick → `toolbar.refused("Resolder: chip pins only")`; else `session.repair(ResolderRepair{{part.reference, pad.number}})`, `Unknown` → the same refusal (a resistor or LED pad).
- `BoardScene`: `bool chip(std::size_t component) const` (`info_[c].part` non-null and a `PartKind::Chip`). New free function next to `LeadLabel`:

```cpp
// "leg" (F.Cu) or "pad" (B.Cu) for a chip pad, "" for any other Hit.
std::string_view ContactSide(const BoardScene& scene, const Hit& hit, Layer layer);
```

- `LeadLabel(scene, hit, layer)`: a chip pad's label gains `" leg"` / `" pad"` ("U1.14 QA leg"); the one caller (`main.cpp`, Meter) passes `pick->layer`. Continuity labels too: the side decides the reading. `bw_ui --meter REF.PIN` builds its label from `Session::padLead` and places the lead on F.Cu: it appends `" leg"` for a chip pin (`main.cpp`).
- `side_panel.cpp`, Probe: after `hoverText`, `" · " + ContactSide(...)` when not empty ("U15 pin 13 2D · V2 · leg").
- Nothing on the Board shows a Fault (PIN_FAULTS.md).

## 8. Puzzles and Machine docs

| File | Fault line | Replaces |
|---|---|---|
| `puzzles/solid_net.bwp` | `fault joint U15.13` | `wide_net.bwp` (deleted) |
| `puzzles/missing_eight.bwp` | `fault stuck U8.D 0` | — |

```
# Solid net: U15 pin 13 (2D, V2) is a dry Joint; the gate's input floats high, so NET_N is decoded on every line.
machine ball
title "Solid net"
brief "The net down the middle is a solid line; it used to be dashed."
fault joint U15.13
```

```
# Missing eight: U8's input D (S3) is stuck at 0 inside the chip; the seconds digit shows 0 for 8 and 1 for 9.
machine stopwatch
title "Missing eight"
brief "The seconds never show 8 or 9: after 7 the digit shows 0 and 1, and then 0 again."
fault stuck U8.D 0
```

Electrical Nets:

- Solid net: `U15` pin 13 on a private Net, no Driver: floating, reads high. `NET_N` = NAND(`NET_A`, `H5`, `H4`, 1): low at the net's 2 dots on every line, a solid line. `V2` still reaches `U3`, `U7` and `U8` on the Copper; the pad, the Trace and a B.Cu touch on pin 13 pulse, the F.Cu touch on pin 13 shows nothing. A Clip or Hook on pin 13 reads the leg (floating). Swapping `U15` does not fix it.
- Missing eight: `U8`'s `D` input reads 0 through its own Net; `S3` keeps `U5.QD` as Driver and `U8.D` as Load. 8 (`DCBA` 1000) decodes as 0 and 9 (1001) as 1; seconds 0–7 are unchanged. The probe shows `S3` high on `U8` pin 6 (leg and pad) while `DS1` shows 0 or 1. Resoldering `U8` pin 6 does not fix it.

Machine docs:

- `BOUNCING_BALL.md`: the Puzzles table row Wide net → Solid net (Fault "dry Joint at `U15` pin 13 (`V2`)", Symptom "the net is a solid line instead of dashes; the ball is fine", Path "`V2` pulses at `U3` pin 12 and on `U15` pin 13's pad (B.Cu), but the leg (F.Cu) shows nothing: resolder `U15` pin 13"); the stand-in paragraph removed; the "teaches" sentence starts "a dry Joint that only changes the picture", the rest in order.
- `BOUNCING_BALL_DESIGN.md`: Files line; the `NET_A` crack site in the Board-constraints list removed (no Puzzle uses it); Puzzles table, briefs, Electrical Nets (the Wide net bullets become Solid net; "Wide net and Jumping ball: a floating Net…" becomes "Solid net and Jumping ball"); the Board-constraint note on `V2` keeps its history and ends "restored as a dry Joint in P4h".
- `STOPWATCH.md`: a Missing eight row; `STOPWATCH_DESIGN.md`: Files line, Puzzles table, brief, Electrical Nets bullet.

## 9. Recipes and glossary

- `ADD_PUZZLE.md`: the Faults list gains "`fault stuck U8.D 0` on a chip input: the chip reads 0, the Net is unchanged; swap" and "`fault joint U15.13`: dry Joint on a chip signal pin, its leg off the pad; resolder"; the repair list gains `machine.resolder({"U15", "13"})`; "Each Puzzle … should teach something different" lists stuck input and dry Joint; a design rule: pin Faults are invisible, the leg/pad touch is the clue (F.Cu leg, B.Cu pad).
- `GLOSSARY.md`: **Dry Joint** ("A Joint that does not connect: the Pin is off its pad's Net. Repaired by resoldering.") and **Leg** ("A chip Pin's lead above the Board; a touch on a chip pad with F.Cu active reads it, with B.Cu the pad."). `GAME_DESIGN.md` is the user's: not edited.

## 10. Tests

| File | Cases |
|---|---|
| `tests/machine/puzzle_test.cpp` | `fault joint U15.13` and `U15.2D` parsed with line; `fault joint`, `fault joint U15` and an extra token → "expected: fault joint REF.PIN"; the catch-all message updated where expected (the cases around lines 70 and 84) |
| `tests/machine/machine_test.cpp` | load errors: `U3.A` stuck now loads; `J1.1` (Power) and `J2.VIDEO` (connector input) → "is not an output or a chip input"; joint on `U99.1`, `U3.XX`, `J1.1`, `U3.16` (VCC, test_pattern `U3` 74LS161) with their messages; `resolder` true on a good pin, false on `U99.1` and `J1.1`; dry Joint removed by `resolder`, kept by `swap`; a stuck input removed by `swap` |
| `tests/netlist/builder_test.cpp` | stuck input: a gate input stuck at 0 makes its output 1 while the Net, driven 1 by another gate, reads 1 at a second Load |
| `tests/machine/electrical_test.cpp` | `ElectricalNetlist` with `dry = {{"U1","14"}}` on test_pattern: `U1.14` on no Net; every other pin as the design |
| `tests/machine/repair_test.cpp` | `resolder:U15.13` → `ResolderRepair`, `"resolder U15.13"`; `resolder:`, `resolder:U15`, `resolder:U15.1:U15.2` bad |
| `tests/machines/pin_faults_test.cpp` (new, test_pattern; its own `Frame`/`FaultFree` helpers copied from `shorts_power_test.cpp`) | a dry Joint on an input with a pulsing Net (`U13.2B`, `H0`): `probeNet` F.Cu → `PinNet` (floating, ≠ `H0`), B.Cu → `H0`; on a good pin both → the same Net; continuity leg→`U1.QA` open, pad→`U1.QA` closed, leg→leg closed; the Session refuses `resolder` powered, logs `resolder U13.2B` off, rebuilt frame fault-free; swap `U13` leaves the frame wrong. A stuck input (`U13.2B` at 1): `PinNet(U13.2B) == H0`, frame differs, swap fixes it |
| `tests/view/board_scene_test.cpp` | `ContactSide` on a chip pad (F/B), a resistor pad, a Trace; `LeadLabel` with the side: "leads: labels" (`== "U1.14 QA"`) becomes `"U1.14 QA leg"` with `Layer::Front` |
| `tests/machines/ball_test.cpp` | `wide_net` → `solid_net` in the title/brief list; "ball: solid_net": `netRows.size() == 224` (fault-free 112), probing `U15.13` (the leg: floating) and `U3.QC` (pulsing); `probeNet(Layer::Back, U15 pin 13) == PinNet(U3.QC)` and `probeNet(Layer::Front, …) == PinNet(U15.13)`; after `swap("U15")`, `PinNet(U15.2D) != PinNet(U3.QC)` (no frame run); `resolder({"U15","13"})` brings back 112 rows of 2 dots |
| `tests/machines/stopwatch_test.cpp` | `missing_eight` in the title/brief list; "stopwatch: missing_eight": after a 10 µs settle (the decoder outputs start random; well before `T3` can ripple), the first seed in 1–64 whose count has `S3` high and `S2`, `S1` low (8 or 9), `REQUIRE`d found (about 1 seed in 16: `RESET` also starts random); `PinNet(U8.D) == S3` reads 1 and `DS1` shows `'0' + S0`; `resolder({"U8","6"})` changes nothing; after `swap("U8")` the same seed shows `'8' + S0` |
| `tests/machine/test_points.{hpp,cpp}` | `WideNetCrack` removed |

Power-on states are random per seed, so the stopwatch case reads the 8/9 decode at power-on instead of simulating 8 s.

## 11. Size

About 250 lines of code: puzzle 20, loader 40, Machine 50, electrical 10, builder 20, repair 15, Session/CLIs 15, UI 60, scene 20; plus tests.

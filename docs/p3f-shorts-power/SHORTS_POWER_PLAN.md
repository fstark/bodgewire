# Solder Bridges and Real Power — Implementation Plan (P3f)

Design: [SHORTS_POWER_DESIGN.md](SHORTS_POWER_DESIGN.md)
Spec: [SHORTS_POWER.md](SHORTS_POWER.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Solder Bridges in Puzzle and Machine: parse, validate, join islands, remove | |
| 2 | `unbridge:` Repair through Session, `bw run` and `bw_ui --repair` | |
| 3 | Real power: `PowerPinVolts`, `FindPower`, unpowered chips left out of the Circuit | |
| 4 | Dead Rail LED state + four Puzzles with headless Symptom/Repair tests | |
| 5 | UI: bridge drawing, Cut on a bridge, `LIMIT` LED, screenshots | |
| 6 | Release check, user check, docs | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug -j 12`

Design correction found while planning: `bw run`'s `Repair()` ([src/cli/main.cpp](../../src/cli/main.cpp#L156)) dispatches with `std::get<UnwireRepair>` as its last branch, so it needs a real `UnbridgeRepair` branch, not only a usage string (Phase 2).

---

## Phase 1 — Solder Bridges in Puzzle and Machine

Bridges exist headlessly: a Puzzle declares them, the Machine validates them, `FindIslands` joins them, `removeBridge` takes them off. Nothing draws them yet. Design §2.2, §2.6, §3 points 2, 6, 7, §5.2, §5.3.

### 1.1 — Puzzle syntax

[src/machine/puzzle.hpp](../../src/machine/puzzle.hpp): add, and include `netlist/parser.hpp` for `PinRef`:

```cpp
struct BridgeDecl {
    std::uint32_t line;
    PinRef a, b;  // pin by number or name: U1.7, U1.ENP
};
// Puzzle:
std::vector<BridgeDecl> bridges;
```

[src/machine/puzzle.cpp](../../src/machine/puzzle.cpp) `Parser::line()`: branch `tokens[1] == "bridge"` → `bridge(number, tokens)`; catch-all message `expected: fault stuck|crack|bridge ...`. `bridge()`: exactly 4 tokens, tokens 2 and 3 split at the first `.` with both sides non-empty (as `stuck()`), else `expected: fault bridge REF.PIN REF.PIN`.

### 1.2 — Machine storage and validation

[src/machine/machine.hpp](../../src/machine/machine.hpp):

```cpp
bool removeBridge(const PinRef& a, const PinRef& b);  // either order; false: no such bridge
std::span<const WireEnds> bridges() const { return bridges_; }
// private:
std::vector<WireEnds> bridges_;  // Solder Bridges: same shape as a Bodge Wire
```

[src/machine/machine.cpp](../../src/machine/machine.cpp):
- `constexpr std::int64_t kBridgeReach = 2'600'000;  // nm: neighbouring DIP pins are 2.54 mm apart`
- `padPosition()` iterates `pristine_.components` instead of `board_` (pads identical; lets `LoadMachine` validate before the first `rebuildBoard()`).
- `LoadMachine`, after the cracks loop, for each `BridgeDecl` (errors at its line, design §5.2 order):
  1. unknown reference → `unknown reference 'U99'`; unknown pin → `U1 (74LS161) has no pin X` (same text as `fault stuck`; find the Part name as that loop does);
  2. `pa->reference != pb->reference` or same pin number → `bridge A B: pins of one chip only`;
  3. Part kind not `PartKind::Chip` → same message;
  4. `std::hypot` of the two `padPosition`s > `kBridgeReach` → `bridge A B: pins are not neighbours`.
  
  `A`/`B` are the pins as written (`U1.7`). Valid ones: `bridges_.push_back({*pa, *pb, {*atA, *atB}})`.
- File-local `FindEnds(std::vector<WireEnds>& list, const PinRef& a, const PinRef& b)` returning the iterator (either order); `removeWire` uses it; `removeBridge` = `padOf` both, `FindEnds(bridges_, ...)`, erase, `rebuildBoard()`.
- `rebuildBoard()`: after the wires loop, `for (const WireEnds& b : bridges_) board_.bodgeWires.push_back(b.wire);`.

### 1.3 — Board comment

[src/board/board.hpp](../../src/board/board.hpp#L72): `std::vector<BodgeWire> bodgeWires;  // pad-to-pad joins: Bodge Wires and Solder Bridges; empty on a loaded Board`.

### 1.4 — Tests

- `tests/machine/puzzle_test.cpp`: `fault bridge U1.7 U1.ENP` → one `BridgeDecl` with line, refs, pins; `fault bridge U1.7` and `fault bridge U17 U1.8` → `expected: fault bridge REF.PIN REF.PIN` at that line; `fault frob` → new catch-all text.
- `tests/machine/machine_test.cpp` (test-pattern Machine, inline `Puzzle`):
  - `U99.1 U1.8` → unknown reference; `U1.99 U1.8` → has no pin; `U1.7 U2.8` and `J1.1 J1.2` → one chip only; `U1.7 U1.9` → not neighbours; errors carry the line.
  - `U1.7 U1.ENP` → one chip only (ENP is pin 7: the same pad).
  - `U1.7 U1.8` loads: `bridges().size() == 1`, `connected(F.Cu, pad7, F.Cu, pad8)` true; `removeBridge(U1.GND, U1.7)` (names, reverse order) true, then not connected; second `removeBridge` false.
  - fault-free Machine: U1 pads 7 and 8 not connected.

### Fence

- [ ] `BridgeDecl`, `Puzzle::bridges`, `Machine::bridges()/removeBridge()` exist
- [ ] `padPosition` reads `pristine_`; existing tests still pass
- [ ] New tests pass: `ctest --preset debug -R "puzzle|machine"`
- [ ] Full build clean, full test gate passes
- [ ] Commit: `"P3f: phase 1 — Solder Bridges in Puzzle and Machine"`

---

## Phase 2 — `unbridge:` Repair

The player-side removal, headless: parse/format, Session, `bw run`, `bw_ui --repair`. Design §2.3, §3 points 3, 8, 10 (usage only), 12.

### 2.1 — Repair variant

[src/machine/repair.hpp](../../src/machine/repair.hpp):

```cpp
struct UnbridgeRepair {
    PinRef a, b;
};
using RepairAction =
    std::variant<SwapRepair, MendRepair, CutRepair, WireRepair, UnwireRepair, UnbridgeRepair>;
```

Update the two comments with `"unbridge:U1.7:U1.8"` / `"unbridge U1.7 U1.8"`.

[src/machine/repair.cpp](../../src/machine/repair.cpp): add `"unbridge:"` to the `{"wire:", "unwire:"}` prefix loop, building `UnbridgeRepair`; `FormatRepair` arm `"unbridge " + FormatPin(a) + " " + FormatPin(b)`.

### 2.2 — Session and CLIs

- [src/session/session.cpp](../../src/session/session.cpp) `repair()`: arm `[&](const UnbridgeRepair& r) { return machine_->removeBridge(r.a, r.b); }`.
- [src/cli/main.cpp](../../src/cli/main.cpp) `Repair()`: make the last branch `else if (const auto* unwire = std::get_if<UnwireRepair>(...))`, add `else { const auto& b = std::get<UnbridgeRepair>(*parsed); done = machine.removeBridge(b.a, b.b); }`; usage line (L42) and the comment (L155) add `unbridge:REF.PIN:REF.PIN`.
- [src/ui/main.cpp](../../src/ui/main.cpp#L47): usage string adds `,unbridge:REF.PIN:REF.PIN`. (Its `--repair` path goes through `Session::repair`; no other change.)

### 2.3 — Tests

- `tests/machine/repair_test.cpp`: `unbridge:U1.7:U1.8` → `UnbridgeRepair`, formats `unbridge U1.7 U1.8`; `unbridge:U1.7`, `unbridge:U1:U1.8` → `bad repair`.
- `tests/machines/bodge_wire_test.cpp` (it already builds Sessions on the test pattern): a Session on an inline Puzzle with `bridge U1.7 U1.8`: `repair(UnbridgeRepair)` refused while powered, Done when off, logged `unbridge U1.7 U1.8`, second time `Unknown`; continuity across U1.7–U1.8 beeps before, silent after, without power-on.

### Fence

- [ ] `UnbridgeRepair` parsed, formatted, applied by Session and `bw run`
- [ ] `build/debug/src/cli/bw run puzzles/... --repair unbridge:U1.7:U1.8` accepted (any bridge Puzzle from a temp file, or skip until Phase 4)
- [ ] Full build clean, test gate passes
- [ ] Commit: `"P3f: phase 2 — unbridge Repair"`

---

## Phase 3 — Real power

Chips run only when Powered. Design §2.1, §2.4, §2.5, §3 points 1, 4, 5, 6, §5.1.

### 3.1 — `PowerPinVolts`

[src/parts/catalogue.hpp](../../src/parts/catalogue.hpp) / `.cpp`:

```cpp
// Voltage a chip's power Pin expects: "VCC" 5, "GND" 0; nullopt for other names.
std::optional<double> PowerPinVolts(std::string_view pin);
```

Table of `{name, volts}` pairs, linear search.

### 3.2 — Builder leaves Unpowered chips out

[src/netlist/builder.hpp](../../src/netlist/builder.hpp): `BuildCircuit(const Netlist&, std::span<const StuckPin> stuck = {}, std::span<const std::string> unpowered = {})`; comment `// unpowered: References of chips left out of the Circuit's Nets (outputs Z, inputs ignored)`.

[src/netlist/builder.cpp](../../src/netlist/builder.cpp): `Builder` stores `unpowered_`; `bool isUnpowered(std::string_view reference) const` (linear scan). In the `pinNet_` loop, first branch `if (isUnpowered(pin.first)) {}` before the stuck tie and the connect (design §3 point 4); the `connections.push_back` stays for every Pin.

### 3.3 — `FindPower`

[src/machine/electrical.hpp](../../src/machine/electrical.hpp): `BoardPower` and `FindPower` exactly as design §2.4. [electrical.cpp](../../src/machine/electrical.cpp): algorithm of design §5.1 with `Namer(board, islands)` for `island()`; `Check(want.has_value(), "electrical: power Pin without a voltage")`; compare the island's set to `std::set<double>{*want}`. Includes `parts/catalogue.hpp`, `parts/connector.hpp`.

### 3.4 — Machine uses it

[src/machine/machine.hpp](../../src/machine/machine.hpp): `const BoardPower& power() const { return power_; }`, member `BoardPower power_;  // of board_, see rebuildBoard()`. [machine.cpp](../../src/machine/machine.cpp): `rebuildBoard()` ends with `power_ = FindPower(*designCircuit_, board_, FindIslands(board_));`; `build()` passes `power_.unpowered`.

### 3.5 — Tests

- `tests/netlist/builder_test.cpp`: test-pattern netlist with `unpowered = {"U1"}`: after a short run, U1.QA's Net has no Driver (Sample/net floating), `PinNet(loaded, "U1", "QA")` still returns the `H0` Net; a stuck Pin on U1 is ignored when U1 is Unpowered.
- `tests/machine/electrical_test.cpp` (test-pattern Machine): fault-free → `unpowered` empty, `deadRail` false; inline crack `F.Cu 164.85 51.05` → `{"U6"}`, no Dead Rail; `bridge U1.7 U1.8` → `deadRail`, `unpowered` = all 15 chips (X1, U1–U14), J1/J2 absent; `addWire(U9.7, U9.14)` → `deadRail`; `removeBridge` → back to none.
- Existing `[machines]` tests (fault-free frame, three P2 Puzzles, P3e wires) must pass unchanged: they prove every chip is Powered on an intact Board.

### Fence

- [ ] `PowerPinVolts`, `BoardPower`, `FindPower`, `Machine::power()` exist; `build()` passes `unpowered`
- [ ] New tests pass; all existing tests pass unchanged
- [ ] Full build clean, test gate passes
- [ ] Commit: `"P3f: phase 3 — real power"`

---

## Phase 4 — Dead Rail state and the four Puzzles

Session exposes the current limit; the Puzzles ship with tests proving Symptom and Repair. Design §2.7, §9, §10.

### 4.1 — `Session::currentLimited`

[src/session/session.hpp](../../src/session/session.hpp): inline `bool currentLimited() const { return powered_ && machine_ && machine_->power().deadRail; }` with the design's comment.

### 4.2 — Puzzle files

In `puzzles/`, each `machine test_pattern`, a leading `#` comment naming the Fault (as [broken_trace.bwp](../../puzzles/broken_trace.bwp)), title and Brief describing the Symptom only:

| File | Fault line |
|---|---|
| `sync_bridge.bwp` | `fault bridge U10.1 U10.2` |
| `line_bridge.bwp` | `fault bridge U9.6 U9.7` |
| `unpowered_chip.bwp` | `fault crack F.Cu 164.85 51.05` |
| `dead_rail.bwp` | `fault bridge U1.7 U1.8` |

Write Briefs after 4.3 shows the real Symptom (render each with `bw run x.bwp --tv out.bmp` and look at it).

### 4.3 — Tests

New `tests/machines/shorts_power_test.cpp`, tag `[machines]`, added to [tests/CMakeLists.txt](../../tests/CMakeLists.txt); reuse the `Frame()` / `FaultFree()` pattern of `bodge_wire_test.cpp` (copy the two helpers into an anonymous namespace):
- each Puzzle loads, `Frame != FaultFree`; after its Repair (`unbridge:U10.1:U10.2`, `unbridge:U9.6:U9.7`, `mend:F.Cu:164.85:51.05`, `unbridge:U1.7:U1.8`) `Frame == FaultFree`;
- `line_bridge`: `swap:U9` alone leaves `Frame != FaultFree`;
- `unpowered_chip`: a wire U6.14 to U14.14 (VCC) also restores `FaultFree`;
- `dead_rail`: `currentLimited()` false off, true powered; meter Volts with red lead on U1 pad 16 reads 0 V; after power-off, `unbridge`, power-on → false.

### Fence

- [ ] Four `.bwp` files load and have Symptom-only Briefs
- [ ] `ctest --preset debug -R machines` passes
- [ ] Full build clean, test gate passes
- [ ] Commit: `"P3f: phase 4 — Dead Rail state and four Puzzles"`

---

## Phase 5 — UI

Bridges visible and cuttable; the `LIMIT` LED. Design §3 points 9, 10, 11.

### 5.1 — Board view

[src/ui/board_view.hpp](../../src/ui/board_view.hpp): `void setBridges(std::span<const BodgeWire> bridges)`, `std::optional<std::size_t> bridgeAt(Point at) const;  // nearest within reach().copper; for the Cut tool`, member `std::vector<BodgeWire> bridges_;`.

[board_view.cpp](../../src/ui/board_view.cpp): `constexpr std::int64_t kBridgeWidth = 600'000;  // nm`. `bridgeAt` = `wireAt` over `bridges_`. Drawing right after the pads loop: `list.AddLine(map(b.a), map(b.b), WithAlpha(kPadTin, active_ == Layer::Back ? 1.0f : kInactiveAlpha), std::max(1.0f, map.pixels(kBridgeWidth)))`.

### 5.2 — Main loop

[src/ui/main.cpp](../../src/ui/main.cpp): in `Tool::Cut`, after the `wireAt` branch, `else if (const auto bridge = boardView.bridgeAt(click->at)) { const WireEnds& b = machine->bridges()[*bridge]; result = session.repair(UnbridgeRepair{b.a, b.b}); }`; next to `setBodgeWires`, collect `machine->bridges()` wires into `boardView.setBridges(...)`.

### 5.3 — `LIMIT` LED

[src/ui/toolbar.cpp](../../src/ui/toolbar.cpp#L126): after the Power button, `ImGui::SameLine()`, a lamp drawn like `DrawLamps` in side_panel.cpp (circle radius 0.4 × text height, `Dummy`, `SameLine`, text `LIMIT`): red `IM_COL32(230, 40, 40, 255)` when `session.currentLimited()`, else `IM_COL32(70, 70, 70, 255)`; `ImGui::SetItemTooltip("Bench supply in current limit")` on the text.

### 5.4 — Screenshots (agent reads the PNGs)

Release build: `build/release/src/ui/bw_ui puzzles/dead_rail.bwp --screenshot /tmp/dead_rail.png --after 1s` (LED lit, black TV); `puzzles/line_bridge.bwp` zoomed on U9 with B.Cu active if `bw_ui` has flags for it, otherwise `--repair` free shot plus a note for the user check; `puzzles/dead_rail.bwp --repair unbridge:U1.7:U1.8` (LED dark, picture back).

### Fence

- [ ] Bridges drawn, Cut removes them in the UI, `LIMIT` lit on `dead_rail`
- [ ] Screenshots checked
- [ ] Full build clean, test gate passes
- [ ] Commit: `"P3f: phase 5 — bridges and LIMIT LED in the UI"`

---

## Phase 6 — Release check, user check, docs

### 6.1 — Release

`cmake --preset release && cmake --build --preset release && ctest --preset release -j 12`; each new Puzzle in `bw_ui` runs at 1.00× (`--screenshot` prints the ratio); `[.perf]` rebuild loop still under 100 ms.

### 6.2 — User check

The user plays the four Puzzles to the end (spec, Done when). Record feedback here.

### 6.3 — Docs

- [ARCHITECTURE.md](../ARCHITECTURE.md) P3f row: `done <date>` with the plan link, as P3e.
- Prune this plan to the table, a result line and implementation notes, as P3e.

### Fence

- [ ] Release tests pass, 1.00×
- [ ] User confirmed
- [ ] Commit: `"docs: P3f done"`

# Bodge Wires and Trace Cuts — Implementation Plan (P3e)

Design: [BODGE_WIRES_DESIGN.md](BODGE_WIRES_DESIGN.md)
Spec: [BODGE_WIRES.md](BODGE_WIRES.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | `BodgeWire` in `Board`; `FindIslands` joins wired pads | |
| 2 | Machine: `cut`, `addWire`, `removeWire`, `mend` removes Cuts | |
| 3 | Repairs `cut:`/`wire:`/`unwire:` through Session and `bw run`; TV-frame tests | |
| 4 | UI: `Cut (K)`, `Wire (W)`, drawing, clicks, `--repair` errors, screenshots | |
| 5 | Release check, user check, docs | |

Build gate: `cmake --preset debug && cmake --build --preset debug` (and `release` in phase 5)
Test gate:  `ctest --preset debug -j 12`

Commit messages: `"bodge wires: phase N — …"`. All C++ clang-format clean.

---

## Phase 1 — Board and islands

The data a wire needs and its effect on connectivity (design §2.1, §3.1). Nothing creates wires yet.

### 1.1 — Board

`src/board/board.hpp`: add before `Board`

```cpp
// Player-soldered wire between two pads (both Layers: through-hole pads only).
struct BodgeWire {
    Point a, b;  // pad centres, nm
};
```

and `std::vector<BodgeWire> bodgeWires;  // empty on a loaded Board` as the last `Board` field.

### 1.2 — `FindIslands`

`src/board/copper.cpp`: in the anonymous namespace, `std::optional<std::uint32_t> PadItemAt(const Board& board, Point at)` — walk pads in item order (components, then pads; item index = running count) and return the first whose `Covers(pad, at)`. In `FindIslands`, after the shape-union loop and before numbering:

```cpp
for (const BodgeWire& wire : board.bodgeWires) {
    const auto a = PadItemAt(board, wire.a);
    const auto b = PadItemAt(board, wire.b);
    Check(a && b, "copper: Bodge Wire end off a pad");
    parent[Root(parent, *a)] = Root(parent, *b);
}
```

Include `sim/check.hpp` (`bw_board` reaches `bw_sim` through `bw_netlist`).

### 1.3 — Tests

`tests/board/copper_test.cpp`: `copper: bodge wire joins two pads` — the existing `TwoPads()` board gives 2 islands; with `bodgeWires = {{pad0.position, pad1.position}}` gives 1, and both `padIsland` entries are equal.

### Fence

- [ ] `BodgeWire`, `Board::bodgeWires`, wire unions in `FindIslands`
- [ ] Test passes; full build and ctest clean
- [ ] Commit: `"bodge wires: phase 1 — bodge wires join copper islands"`

---

## Phase 2 — Machine

Cuts and wires on the Machine's Board (design §2.3, §3.2, §4).

### 2.1 — State and rebuild

`src/machine/machine.hpp`: include `netlist/parser.hpp` (for `PinRef`); add

```cpp
// A Bodge Wire with its ends as pin numbers.
struct WireEnds {
    PinRef a, b;
    BodgeWire wire;  // pad positions
};
```

Private: `std::vector<CrackDecl> cuts_; std::vector<WireEnds> wires_; void rebuildBoard(); std::optional<PinRef> padOf(const PinRef& pin) const;`.

`rebuildBoard()`: `std::vector<CrackDecl> all = cracks_; all.insert(all.end(), cuts_.begin(), cuts_.end()); board_ = CrackBoard(pristine_, all);` then `board_.bodgeWires` = each `wires_[i].wire`. `LoadMachine` calls `machine.rebuildBoard()` instead of the `CrackBoard` line; `mend` calls it instead of its own.

`padOf(pin)`: `designCircuit_->components.find(pin.reference)`, `FindPin`, return `PinRef{reference, std::to_string(index + 1)}`; nullopt if unknown or `padPosition` fails.

### 2.2 — Operations

Public, per design §2.3:

- `bool cut(Layer layer, Point at)`: `FindCrackTrace(board_, layer, at)` fails → false; else `cuts_.push_back({0, layer, at})`, `rebuildBoard()`, true.
- `mend`: `erase_if` on both `cracks_` and `cuts_` with the existing 1 mm lambda; false when the total removed is 0; else `rebuildBoard()`.
- `bool addWire(const PinRef& a, const PinRef& b)`: `padOf` both, false if either fails or they are equal; positions from `padPosition`; push `{*pa, *pb, {*atA, *atB}}`; `rebuildBoard()`.
- `bool removeWire(const PinRef& a, const PinRef& b)`: `padOf` both; erase the first `wires_` entry with (a, b) or (b, a) (compare reference and pin); false if none; `rebuildBoard()`.
- `cuts()`, `wires()` spans.

`PinRef` has no `operator==`: compare fields in a local lambda.

### 2.3 — Tests

`tests/machine/machine_test.cpp` (`[machine]`), with `kH0Crack` from `test_points.hpp`:

- `machine: bodge wire across the crack` — `broken_trace`; pads `U1.14`, `U13.6` (via `padPosition`) not `connected`; `addWire({"U1","14"},{"U13","6"})` → true, connected; `wires().size() == 1`, `wires()[0].a.pin == "14"`; `removeWire({"U13","6"},{"U1","QA"})` → true, open again; `removeWire` again → false.
- `machine: bad wires` — unknown reference, unknown pin, `U1.14` to `U1.QA` (same pad) → false; `wires()` empty.
- `machine: cut and mend` — fault-free `LoadNetlistMachine(test_pattern.bwn)`: `cut(Front, kH0Crack)` → true, the two pads open, `cuts().size() == 1`; `cut(Front, kH0Crack)` again → false (inside the gap); `cut(Front, {0, 0})` → false; `mend(Front, kH0Crack)` → true, connected, `cuts()` empty.
- `machine: build with a wire` — `broken_trace` + wire: `build()` succeeds and `PinNet(U1,14) == PinNet(U13,6)`.

### Fence

- [ ] Machine API as design §2.3; tests pass; full ctest clean
- [ ] Commit: `"bodge wires: phase 2 — cuts and wires on the machine"`

---

## Phase 3 — Repairs

The three new Repairs through text, Session and `bw run` (design §2.2, §3.3, §3.4, §5.1).

### 3.1 — Repair types and text

`src/machine/repair.hpp`: include `netlist/parser.hpp`; add `CutRepair {Layer layer; Point at;}`, `WireRepair {PinRef a, b;}`, `UnwireRepair {PinRef a, b;}`; extend `RepairAction`. Update the comments with the new forms.

`src/machine/repair.cpp`:
- `Fields(text, prefixLength)` helper (the split loop of `mend:`), used by `mend:` and `cut:`.
- `ParsePinRef(std::string_view) → std::optional<PinRef>`: split at the first `.`, both sides non-empty.
- `cut:` → 3 fields, as `mend:` → `CutRepair`.
- `wire:` / `unwire:` → 2 fields, each `ParsePinRef`; textually equal → bad.
- `FormatRepair` via `std::visit`: `cut F.Cu 52.30 41.00`, `wire U1.14 U13.6`, `unwire U1.14 U13.6` (`reference + "." + pin`).

### 3.2 — Session

`src/session/session.cpp` `repair()`: replace the `if`/`else` with the `std::visit` of design §3.3 (`Overloaded` helper in the anonymous namespace); Mend keeps its blob; a `false` → `RepairResult::Unknown` before logging.

### 3.3 — `bw run`

`src/cli/main.cpp` `Repair()`: keep Swap (unknown reference message) and Mend (warning when nothing mended); for `CutRepair`, `WireRepair`, `UnwireRepair` call the Machine; `false` → `"cannot apply '" + std::string(repair) + "'"`. Update its comment.

### 3.4 — Tests

- `tests/machine/repair_test.cpp`: parse + format round trips `cut:F.Cu:52.30:41.00`, `cut:B.Cu:1:2`, `wire:U1.14:U13.6`, `unwire:U1.QA:U13.6`; errors `cut:F.Cu:1`, `cut:X.Cu:1:2`, `wire:U1.14`, `wire:U1:U2.3`, `wire:.14:U2.3`, `wire:U1.14:U1.14`.
- `tests/session/session_test.cpp`:
  - `session: cut and wire only with the power off` — powered: `CutRepair`, `WireRepair`, `UnwireRepair` → `Refused`; off: Cut at `kH0Crack` on `broken_trace` → `Unknown` (already cracked: inside the gap); wire `U1.14`–`U13.6` → `Done`; log ends with `wire U1.14 U13.6`; unwire → `Done`; unwire again → `Unknown`.
  - `session: continuity through a bodge wire` — Continuity mode, Leads on `U1.14` and `U13.6`: false; wire → true, with the power off; `drawnBoard()->bodgeWires` empty.
  - `session: continuity across a cut` — fault-free test pattern, Continuity mode, Leads on `U1.14` and `U13.6`: true; `CutRepair{Front, kH0Crack}` → `Done`, false; `drawnBoard()->traces.size()` unchanged; Mend → true.
- `tests/machines/bodge_wire_test.cpp` (new, in `tests/CMakeLists.txt`), helpers copied from `board_repair_test.cpp` (`MakeSession`, `Frame`, `FaultFree`):
  - `bodge wire: broken trace repaired by a wire` — `broken_trace`, power on 10 ms, off, `WireRepair{{"U1","14"},{"U13","6"}}` → frame == `FaultFree()`.
  - `bodge wire: cut and mend on the test pattern` — fault-free test pattern Session: `CutRepair{Front, kH0Crack}` → frame != fault-free; `MendRepair{Front, kH0Crack}` → frame == fault-free.
- `bw run puzzles/broken_trace.bwp --repair wire:U1.14:U13.6 ...` by hand once (existing `bw run` options) to see it accepted; `--repair wire:U1.99:U13.6` → exit 2 with `cannot apply`.

### Fence

- [ ] Five Repair kinds parse, format, apply; tests pass; full ctest clean
- [ ] Commit: `"bodge wires: phase 3 — cut, wire and unwire repairs"`

---

## Phase 4 — UI

Tools, drawing and clicks in `bw_ui` (design §2.4, §3.5–§3.7, §5.2).

### 4.1 — Board view

`src/ui/board_view.hpp/.cpp`:
- `Tool` gains `Cut`, `Wire` after `Mend` (switches over `Tool` in `main.cpp` and `toolbar.cpp` must handle them in this phase).
- `struct CutMark { Layer layer; Point at; };`, members `cuts_`, `bodgeWires_` (`std::vector<BodgeWire>`), `wireStart_` (`std::optional<Point>`); setters `setCuts`, `setBodgeWires`, `setWireStart`.
- `keys()`: `K` → `setTool(Tool::Cut)`, `W` → `setTool(Tool::Wire)`; update the `keys()` comment.
- `wireAt(Point at) const`: nearest wire with segment distance ≤ `reach().copper` (point–segment distance as in `crack.cpp`'s `NearestT`; a local copy in the anonymous namespace).
- Drawing in `draw()`: Cut marks after the solder blobs (an `X` of two strokes, ±0.3 mm, `kCutMark = IM_COL32(20, 20, 20, 255)`, 2 px, inactive-Layer alpha as blobs); Bodge Wires after pin names and before the probe tip (`kWireInsulation = IM_COL32(120, 170, 255, 255)`, thickness `max(2, map.pixels(300'000))`, `kSolder` circles of `max(kBlobMinPx, map.pixels(kBlobRadius / 2))` at both ends); the started wire from `wireStart_` to the mouse position in the same colour when the mouse is over the panel.

### 4.2 — Toolbar

`src/ui/toolbar.hpp/.cpp`: public `bool wireStarted = false;`; buttons `Cut (K)`, `Wire (W)` after `Mend (M)`; action texts per design §3.6 (powered → amber `Cut: power off first` / `Wire: power off first`).

### 4.3 — `main.cpp`

- `RunWindow` state `std::optional<PinRef> wireStart; Point wireStartAt{};`.
- Click switch: `Tool::Cut` and `Tool::Wire` cases per design §3.7 (powered → `result = RepairResult::Refused`). Cut: `boardView.wireAt(click->at)` first → `UnwireRepair` with that wire's `a`, `b`; Trace hit → arc check (`boardView.scene()->board().traces[pick->hit.index].mid` → `toolbar.refused("Cut: straight traces only")`) → `CutRepair{pick->layer, pick->contact}`; any `Unknown` result → `toolbar.refused("Cut: click a trace or a wire")`. Wire: pad hit → `PinRef{part.reference, pad.number}`; first → `wireStart`, `wireStartAt = pad.position`; same pad → drop; else `session.repair(WireRepair{*wireStart, end})`, drop.
- `Esc` with `Tool::Wire` drops `wireStart`; a `takePicked()` of any tool drops it (before the pairing flags).
- Each frame: `setCuts` from `machine->cuts()` (as `CutMark{layer, at}`), `setBodgeWires` from `machine->wires()[i].wire`, `setWireStart`, `toolbar.wireStarted`.
- `--repair` error: replace the `std::get<SwapRepair>` message with `"cannot apply '" + text + "'"` for any `Unknown`; usage text mentions `cut:`/`wire:`/`unwire:`.

### 4.4 — Check (spec S5)

Release build, then read both PNGs:
- `bw_ui puzzles/broken_trace.bwp --repair wire:U1.14:U13.6 --screenshot /tmp/wire.png --after 1s`: wire with blobs, log line, grid back on the TV.
- `bw_ui netlists/test_pattern.bwn --repair cut:F.Cu:137.59:99.17 --screenshot /tmp/cut.png --after 1s`: Cut mark (zoom may hide it at fit: then also `--probe U1.14` to frame nearby, or accept the log line), grid broken.

### Fence

- [ ] K/W tools work; refusals show their reasons; screenshots read
- [ ] Build, ctest clean
- [ ] Commit: `"bodge wires: phase 4 — cut and wire tools"`

---

## Phase 5 — Release check, user check, docs

### 5.1 — Release

`cmake --preset release && cmake --build --preset release && ctest --preset release -j 12`. `bw_tests "[.perf]"` rebuild time with `WireRepair{U1.14, U13.6}` before the loop and, in the loop, `MendRepair{Front, kH0Crack}` then `CutRepair{Front, kH0Crack}` (each `Done`: the Mend removes the crack or the previous Cut) instead of the `away` Mend: mean < 100 ms. `bw_ui puzzles/broken_trace.bwp --repair wire:U1.14:U13.6 --screenshot /tmp/p3e.png --after 5s`: 1.00×.

### 5.2 — User check

The user fixes `broken_trace` with a Bodge Wire instead of a Mend, and breaks then restores the picture with a Cut and a Mend. Record feedback under "Later".

### 5.3 — Docs

ARCHITECTURE roadmap P3e row → done; this plan: statuses, implementation notes, prune; repo memory.

### Fence

- [ ] Debug and release tests pass; release 1.00×; rebuild < 100 ms
- [ ] User confirmed
- [ ] Commit: `"docs: P3e done"`

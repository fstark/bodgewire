# Bodge Wires and Trace Cuts — Detailed Design (P3e)

Implements the specification in [BODGE_WIRES.md](BODGE_WIRES.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

No new files in `src/`:

```
src/board/board.hpp / copper.cpp   BodgeWire in Board; FindIslands joins the two pads' islands
src/machine/repair.hpp / .cpp      CutRepair, WireRepair, UnwireRepair; parse and format
src/machine/machine.hpp / .cpp     cuts_, wires_: cut(), addWire(), removeWire(); mend() also
                                   removes Cuts; board_ rebuilt with both
src/session/session.cpp            repair() dispatches the new Repairs
src/cli/main.cpp                   Repair() through std::visit
src/ui/board_view.hpp / .cpp       Tool::Cut (K), Tool::Wire (W); Cut marks, Bodge Wires,
                                   the started wire
src/ui/toolbar.hpp / .cpp          wireStarted; Cut/Wire buttons and action texts
src/ui/main.cpp                    Cut and Wire clicks, Esc, tags, --repair errors
tests/machines/bodge_wire_test.cpp NEW: Puzzles and the test pattern with wires and Cuts
```

A Bodge Wire is Copper the player adds, so it lives in the `Board` next to Traces and vias; everything that reads islands (`ElectricalNetlist`, `Machine::probeNet`, `Machine::connected`) then sees it with no change. A Cut is a crack the player makes, so it reuses `CrackDecl` and `CrackBoard`.

## 2. Public Interface

### 2.1 Board (`src/board/board.hpp`)

```cpp
// Player-soldered wire between two pads (both Layers: through-hole pads only).
struct BodgeWire {
    Point a, b;  // pad centres, nm
};

struct Board {
    std::vector<BoardComponent> components;
    std::vector<Trace> traces;
    std::vector<Via> vias;
    std::vector<OutlineEdge> outline;
    std::vector<BodgeWire> bodgeWires;  // empty on a loaded Board
};
```

### 2.2 Repairs (`src/machine/repair.hpp`)

```cpp
struct CutRepair {
    Layer layer;
    Point at;  // nm
};

struct WireRepair {  // also names the wire an UnwireRepair removes
    PinRef a, b;     // pin by number or name
};

struct UnwireRepair {
    PinRef a, b;
};

using RepairAction = std::variant<SwapRepair, MendRepair, CutRepair, WireRepair, UnwireRepair>;

// + "cut:F.Cu:52.30:41.00", "wire:U1.14:U13.6", "unwire:U1.QA:U13.6".
std::expected<RepairAction, std::string> ParseRepair(std::string_view text);
// + "cut F.Cu 52.30 41.00", "wire U1.14 U13.6", "unwire U1.14 U13.6".
std::string FormatRepair(const RepairAction& repair);
```

`PinRef` is the netlist's (`netlist/parser.hpp`). `ParsePinRef("U1.14")`: split at the first `.`, both sides non-empty; a wire whose two ends are textually equal is a parse error.

### 2.3 Machine (`src/machine/machine.hpp`)

```cpp
// A Bodge Wire with its ends as pin numbers.
struct WireEnds {
    PinRef a, b;
    BodgeWire wire;  // pad positions
};

// Machine, public:
    bool cut(Layer layer, Point at);  // false: no straight Trace there in the current Copper
    bool addWire(const PinRef& a, const PinRef& b);     // false: unknown pad, or the same pad
    bool removeWire(const PinRef& a, const PinRef& b);  // either order; false: no such wire
    std::span<const CrackDecl> cuts() const { return cuts_; }
    std::span<const WireEnds> wires() const { return wires_; }
    // mend(): now removes Cuts within 1 mm too.

// Machine, private:
    void rebuildBoard();  // board_ = pristine_ + cracks_ + cuts_ + wires_
    std::optional<PinRef> padOf(const PinRef& pin) const;  // pin as a number; nullopt unknown
    std::vector<CrackDecl> cuts_;  // line 0
    std::vector<WireEnds> wires_;
```

### 2.4 Session and UI

`Session::repair()` keeps its signature and result: `Done` (logged), `Refused` (powered), `Unknown` (Swap of an unknown reference, Cut off a straight Trace, unknown pin, unwire of no wire). Mend still adds a blob to `blobs()`; a Cut adds none (its mark comes from `Machine::cuts()`), and wires draw their own solder.

```cpp
// src/ui/board_view.hpp
enum class Tool : std::uint8_t { Probe, Swap, Mend, Cut, Wire, Clip, Hook, Meter };

struct CutMark {
    Layer layer;
    Point at;  // nm
};

// BoardView, public:
    void setCuts(std::span<const CutMark> cuts);
    void setBodgeWires(std::span<const BodgeWire> wires);
    void setWireStart(std::optional<Point> start);  // rubber band to the cursor
    // Index of the nearest wire within reach().copper of `at`; for the Cut tool.
    std::optional<std::size_t> wireAt(Point at) const;
```

## 3. Integration Points

### 3.1 `FindIslands` (`src/board/copper.cpp`)

After the shape unions, before numbering islands:

```cpp
for (const BodgeWire& wire : board.bodgeWires) {
    const auto a = PadItemAt(board, wire.a);  // item index of the pad covering the point
    const auto b = PadItemAt(board, wire.b);
    Check(a && b, "copper: Bodge Wire end off a pad");
    parent[Root(parent, *a)] = Root(parent, *b);
}
```

`PadItemAt` walks pads in item order with `Covers(pad, at)`. Cost: O(pads) per wire, nothing without wires. `Islands` gains no field: a wire is not an item.

### 3.2 Machine (`src/machine/machine.cpp`)

- `LoadMachine`: `machine.rebuildBoard()` replaces the `CrackBoard` line.
- `rebuildBoard()`: concatenate `cracks_` and `cuts_` into one vector, `board_ = CrackBoard(pristine_, all)`, then `board_.bodgeWires` from `wires_`.
- `cut(layer, at)`: `FindCrackTrace(board_, layer, at)` must succeed (on the current Copper, so a point inside an earlier gap is refused and `CrackBoard` never `Check`s); push `{0, layer, at}`; `rebuildBoard()`.
- `mend(layer, at)`: `erase_if` on `cracks_` and on `cuts_` with the same 1 mm test; `false` if neither removed anything.
- `addWire(a, b)`: `padOf` both (via `FindPin` on `designCircuit_`, as `padPosition`), refuse equal; `padPosition` both; push; `rebuildBoard()`.
- `removeWire(a, b)`: `padOf` both; erase the first wire with `{a, b}` or `{b, a}`.

`probeNet`, `connected`, `build()` read `board_`, so Cuts and wires reach instruments, continuity and the next build unchanged (spec S3).

### 3.3 Session (`src/session/session.cpp`)

`repair()` after the power check:

```cpp
const bool done = std::visit(Overloaded{
    [&](const SwapRepair& r) { return machine_->swap(r.reference); },
    [&](const MendRepair& r) { machine_->mend(r.layer, r.at); blobs_.push_back({r.layer, r.at});
                               return true; },
    [&](const CutRepair& r) { return machine_->cut(r.layer, r.at); },
    [&](const WireRepair& r) { return machine_->addWire(r.a, r.b); },
    [&](const UnwireRepair& r) { return machine_->removeWire(r.a, r.b); }}, repair);
if (!done) return RepairResult::Unknown;
```

then log, `dirty_ = true`, `recomputeContinuity()` as today. `Overloaded` is a two-line helper in the anonymous namespace.

### 3.4 `bw run` (`src/cli/main.cpp`)

`Repair()`: Swap and Mend as today; for the others `std::visit` to the Machine call; `false` → `"cannot apply '" + text + "'"` (usage error, exit 2, as an unknown Swap).

### 3.5 Board view (`src/ui/board_view.cpp`)

- `keys()`: `K` → `setTool(Tool::Cut)`, `W` → `setTool(Tool::Wire)`.
- `draw()`, after the solder blobs and before courtyards: Cut marks, an `X` of two 0.6 mm strokes in `kCutMark` (dark), inactive-Layer alpha as blobs. After pin names, before the probe tip: each Bodge Wire as a line of `max(2 px, 0.3 mm)` in `kWireInsulation` (light blue) with a `kSolder` blob of `kBlobRadius / 2` at each end; the started wire as the same line from its pad to the mouse.
- `wireAt(at)`: nearest wire whose segment is within `reach().copper` of `at` (same reach as Traces).

### 3.6 Toolbar (`src/ui/toolbar.cpp`)

Buttons `Cut (K)` and `Wire (W)` after `Mend (M)`. Public member `bool wireStarted = false;`, set by `main` each frame. Action texts: powered (amber) `Cut: power off first` / `Wire: power off first`; else `Cut: click a trace or a wire`, `Wire: click the first pin` or, when `wireStarted`, `Wire: click the second pin`.

### 3.7 `main.cpp`

State in `RunWindow`: `std::optional<PinRef> wireStart; Point wireStartAt;`.

```cpp
case Tool::Cut:
    if (powered) { result = Refused; break; }  // the usual amber flash
    if (const auto wire = boardView.wireAt(click->at)) {
        const WireEnds& w = machine->wires()[*wire];
        result = session.repair(UnwireRepair{w.a, w.b});
    } else if (!pick || pick->hit.kind != HitKind::Trace) {
        toolbar.refused("Cut: click a trace or a wire");
    } else if (boardView.scene()->board().traces[pick->hit.index].mid) {
        toolbar.refused("Cut: straight traces only");
    } else {
        result = session.repair(CutRepair{pick->layer, pick->contact});
    }
    if (result == Unknown) { toolbar.refused("Cut: click a trace or a wire"); }  // e.g. in a gap
    break;
case Tool::Wire:
    if (powered) { result = Refused; break; }
    if (!pick || pick->hit.kind != HitKind::Pad) { toolbar.refused("Wire: solder to pins only"); break; }
    // pad → PinRef{reference, pad number}; first click: wireStart; same pad: drop it;
    // second: session.repair(WireRepair{*wireStart, end}), drop wireStart
    break;
```

`Esc` with `Tool::Wire` drops `wireStart`; changing tool drops it too (`takePicked`). Each frame: `setCuts` from `machine->cuts()` (as `CutMark{layer, at}`), `setBodgeWires` from `machine->wires()[i].wire`, `setWireStart(wireStart ? wireStartAt : nullopt)`, `toolbar.wireStarted = wireStart.has_value()`.

`--repair` errors: `RepairResult::Unknown` prints `cannot apply '<text>'` (the Swap-only `std::get<SwapRepair>` goes).

The drawn Board (`Session::drawnBoard()`) is copied at load and never gets wires or Cuts, so `BoardScene` and Net highlighting stay on the design Netlist (spec Decisions).

## 4. Internal State

- `Machine::cuts_`: `CrackDecl` with `line = 0`, in Cut order; replayed after `cracks_` by `CrackBoard`. Every Cut was valid on the Copper it was made on, and removing a gap (Mend) only adds Copper back, so replaying stays valid.
- `Machine::wires_`: in solder order; duplicates allowed (two wires between the same pads; unwire removes one).
- No per-event state: Cuts and wires only change the Board between runs.

## 5. Key Algorithms

### 5.1 Repair parsing

```
"cut:"   → fields after the prefix split on ':': exactly 3, Layer F.Cu|B.Cu, two millimetre values
"wire:" / "unwire:" → fields: exactly 2, each ParsePinRef; equal → bad
```

`ParseRepair` gets a small `Fields(text, prefixLength)` helper shared with `mend:`.

### 5.2 Picking a wire to remove

```
wireAt(at):
    best = none
    for each wire w: d = distance(at, segment w.a–w.b)
        if d <= reach().copper and d < best distance: best = w
```

The Cut case checks wires before the pick, so a wire over a Trace is snipped, never the Trace (spec Decisions).

## 6. Reused Infrastructure

- `CrackDecl`, `FindCrackTrace`, `CrackBoard`, `FormatPoint`, `ParseMillimetres` for Cuts; the 1 mm `mend` rule.
- `FindIslands`/`Covers` for wires; `IslandNetNames`/`ElectricalNetlist` name merged islands as today (first Pin's design Net; POWER pads preset VCC/GND).
- `PinRef`, `FindPin`, `Machine::padPosition` for wire ends.
- Session rebuild, repair log, `recomputeContinuity()` (P3d); `Toolbar::refused(reason)` (P3d); `BoardView::reach()`.

## 7. Build Integration

```cmake
# tests/CMakeLists.txt: + machines/bodge_wire_test.cpp
```

No new libraries or source files.

## 8. Dependency Diagram

```
bw_ui (board_view, toolbar, main) ─> bw_session ─> bw_machine (cut, addWire, repair) ─> bw_board (BodgeWire, FindIslands)
bw (cli) ─> bw_machine
```

## 9. Testing

Catch2.

- `tests/machine/repair_test.cpp`: parse/format `cut:F.Cu:52.30:41.00`, `wire:U1.14:U13.6`, `unwire:U1.QA:U13.6`; errors `cut:F.Cu:1`, `cut:X.Cu:1:2`, `wire:U1.14`, `wire:U1:U2.3`, `wire:U1.14:U1.14`.
- `tests/board/copper_test.cpp`: two unconnected pads → 2 islands; with a `BodgeWire` between them → 1.
- `tests/machine/machine_test.cpp` (`broken_trace`):
  - `addWire(U1.14, U13.6)` → `connected` across the crack; `removeWire(U13.6, U1.QA)` → open again;
  - `addWire` unknown pin / same pad → false;
  - `cut` on the fault-free test pattern at `kH0Crack` → the two pads open; second `cut` at the same point → false; `mend` → connected; `cut` off a Trace → false.
- `tests/machines/bodge_wire_test.cpp` (TV frames as `board_repair_test.cpp`):
  - `broken_trace` + `WireRepair{U1.14, U13.6}` == fault-free frame;
  - fault-free test pattern + `CutRepair{Front, kH0Crack}` != fault-free; + Mend there == fault-free.
- `tests/session/session_test.cpp`: Cut, Wire, Unwire `Refused` while powered; log lines; continuity across the crack beeps after a wire, and goes silent after a Cut, with the power off; `drawnBoard()` keeps its Traces after a Cut and `bodgeWires` empty after a wire. The `BoardScene` and Net highlighting are built from the drawn Board, so that covers the design Net highlight (spec S5).
- UI (by the agent, spec S5): `bw_ui puzzles/broken_trace.bwp --repair wire:U1.14:U13.6 --screenshot /tmp/wire.png --after 1s`; `bw_ui netlists/test_pattern.bwn --repair cut:F.Cu:137.59:99.17 --screenshot /tmp/cut.png --after 1s`.

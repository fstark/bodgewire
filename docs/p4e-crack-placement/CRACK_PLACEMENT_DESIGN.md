# P4e Crack Placement — Detailed Design

Implements the specification in [CRACK_PLACEMENT.md](CRACK_PLACEMENT.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

No new library files. A placed crack becomes `Crack` (layer + point); `CrackDecl` becomes the Puzzle line (two pads + percent).

```
src/board/copper.{hpp,cpp}     CopperGraph, BuildCopperGraph, PadItemAt, Touches; FindIslands on the graph
src/machine/puzzle.{hpp,cpp}   CrackDecl {line, a, b, percent}; new `fault crack` syntax
src/machine/crack.{hpp,cpp}    Crack; PlaceCrack; CrackBoard/FindCrackTrace take Crack
src/machine/machine.{hpp,cpp}  cracks_/cuts_ are Crack; placeCrack(), cracks()
src/ui/main.cpp                CrackDecl → Crack (Cut marks)
puzzles/*.bwp                  seven crack lines rewritten
tests/machine/test_points.{hpp,cpp}  PuzzleCrack(), BoardCrack() replace hard-coded points
```

## 2. Public Interface

### 2.1 Puzzle line (`machine/puzzle.hpp`)

```cpp
struct CrackDecl {
    std::uint32_t line;
    PinRef a, b;     // as written: pin by number or name
    double percent;  // 0–100
};
```

### 2.2 Placed crack (`machine/crack.hpp`)

```cpp
struct Crack {
    Layer layer;
    Point at;  // nm, KiCad coordinates
    bool operator==(const Crack&) const = default;
};

// Crack `percent` (0–100) along the crackable Copper from the pad under `a` to the pad under `b`
// (pad centres). Errors: "pads not joined by Copper", "no crackable Trace between the pads",
// "does not separate the pads".
std::expected<Crack, std::string> PlaceCrack(const Board& board, Point a, Point b, double percent);

std::expected<std::size_t, std::string> FindCrackTrace(const Board& board, Layer layer, Point at);
Board CrackBoard(const Board& board, std::span<const Crack> cracks);
```

`crack.hpp` no longer includes `puzzle.hpp`.

### 2.3 Copper graph (`board/copper.hpp`)

```cpp
// Copper items numbered as FindIslands does: every pad (component by component), then vias, then
// Traces. Trace t is item pads + vias + t.
struct CopperGraph {
    std::uint32_t pads = 0, vias = 0;
    std::vector<std::vector<std::uint32_t>> touching;  // [item] → touching items, ascending
};

CopperGraph BuildCopperGraph(const Board& board);  // Bodge Wires join their two pads
std::optional<std::uint32_t> PadItemAt(const Board& board, Point at);  // first pad covering `at`
// `piece` (a Trace not on the Board) touches item `item`'s copper, with FindIslands' shapes.
bool Touches(const Board& board, std::uint32_t item, const Trace& piece);
```

`PadItemAt` moves out of the anonymous namespace unchanged.

### 2.4 Machine (`machine/machine.hpp`)

```cpp
std::span<const Crack> cracks() const { return cracks_; }  // the Puzzle's Broken Traces, placed
// A crack on the fault-free Board; errors say "crack A B: …" with the pins as given.
std::expected<Crack, std::string> placeCrack(const PinRef& a, const PinRef& b,
                                             double percent) const;

std::span<const Crack> cuts() const { return cuts_; }
std::vector<Crack> cracks_;
std::vector<Crack> cuts_;
```

`cracks()` is for tests; the UI keeps reading only `board()` and `cuts()`, so nothing leaks.

## 3. Integration Points

1. **`ParsePuzzle`** ([puzzle.cpp](../../src/machine/puzzle.cpp) `Parser::crack`): parses `fault crack REF.PIN REF.PIN N%` (§5.1). Errors: `expected: fault crack REF.PIN REF.PIN N%`, `bad position 'X'`.
2. **`LoadMachine`** ([machine.cpp](../../src/machine/machine.cpp#L114-L127)): the loop over `puzzle.cracks` becomes: check both pins with the `known` lambda (moved above the crack loop, shared with bridges), then `placeCrack(decl.a, decl.b, decl.percent)`; an error goes on `decl.line`. The `islands` count and the "does not cut the copper" check go: `PlaceCrack` checks separation.
3. **`Machine::placeCrack`**: `padOf` both pins (callers checked them), same pad → `"two different pads"`, else `PlaceCrack(pristine_, padPosition(a), padPosition(b), percent)`; every error prefixed `"crack " + a.reference + "." + a.pin + " " + b.reference + "." + b.pin + ": "`.
4. **`Machine::mend`, `cut`, `rebuildBoard`**: `CrackDecl` → `Crack`; `cut` pushes `{layer, at}`.
5. **`FindIslands`** ([copper.cpp](../../src/board/copper.cpp#L246)): builds `BuildCopperGraph(board)`, then unions every `touching` pair. Same result; one adjacency list per item more (≈ 1000 items on the test pattern: well under the 0.8 ms rebuild).
6. **`bw_ui`** ([main.cpp](../../src/ui/main.cpp#L528)): `const Crack& cut`.

## 4. Internal State

Nothing persistent beyond `Machine::cracks_`. `PlaceCrack` works on locals:

```cpp
struct Stretch {
    std::size_t trace;  // index into board.traces
    bool reversed;      // the path enters at Trace::end
    double from, to;    // nm along the Trace from its entry end; from <= to
};
```

## 5. Key Algorithms

### 5.1 Position parsing

```
"N%" or "N.D%" or "N.DD%": digits only around one optional '.', at most 2 decimals, then '%'
value = integer part + decimals / 10^count; refuse > 100
```

### 5.2 Path

```
Path(graph, from, to) → optional<vector<item>>:
    BFS from `from`, neighbours in ascending order (deterministic), stop at `to`
    nullopt if `to` not reached; else items from → to
```

Fewest pieces, as the spec says for loops. Unique on tree-shaped Copper.

### 5.3 Crackable stretches

```
kCrackStep = 10'000 nm
cursor = a
for item in path:
    if item is not a Trace: continue
    T = trace; w = T.width; gap = w/2 + kCrackMargin      // CrackBoard's half gap
    reversed = |T.end − cursor| < |T.start − cursor|
    entry, exit = reversed ? (T.end, T.start) : (T.start, T.end)
    cursor = exit
    if T.mid: continue                                     // arcs are never crackable
    L = |exit − entry|
    run = none
    for s = gap; s <= L − gap; s += kCrackStep:
        piece = Trace{T.layer, along(s − gap), along(s + gap), nullopt, w}
        clear = no item in graph.touching[T] Touches(board, item, piece)
        clear: extend run to s (open one at s if none)
        else:  close run into a Stretch
    close run
```

A clear piece touches nothing but `T`: the crack lands away from pads, vias, corners and junctions (P4b lesson: a short stub at a pad can't be cracked).

### 5.4 Placing

```
PlaceCrack(board, a, b, percent):
    ia, ib = PadItemAt(board, a), PadItemAt(board, b)       // Check: callers pass pad centres
    graph = BuildCopperGraph(board)
    path = Path(graph, ia, ib) else "pads not joined by Copper"
    stretches = §5.3 else (empty) "no crackable Trace between the pads"
    target = percent / 100 × Σ (to − from)
    for each stretch in order:
        if target <= to − from or it is the last: s = from + min(target, to − from); break
        target −= to − from
    crack = {T.layer, along(s) from T's entry end, rounded to nm}
    cracked = BuildCopperGraph(CrackBoard(board, {crack}))  // pads and vias keep their items
    if Path(cracked, ia, ib): "does not separate the pads"
    return crack
```

Cost: two graph builds plus ≈ 100 `Touches` per mm of path, at load only.

## 6. Reused Infrastructure

- FindIslands' shapes and contact test (`ShapeBuilder`, `HullDistance`): `BuildCopperGraph` and `Touches` use them, so "touching" means exactly what islands mean.
- `PadItemAt` (Bodge Wire ends) finds the pad items.
- `CrackBoard`, `kCrackMargin`, `FindCrackTrace`: unchanged gap; a placed crack is a Broken Trace as before (mend, Cut, drawing, probing unchanged).
- `PinRef`, `Machine::padOf`, `padPosition`, the bridge `known` checks and their messages.

## 7. Build Integration

```cmake
# tests/CMakeLists.txt: add machine/test_points.cpp
```

## 8. Dependency Diagram

```
bw_board (copper: graph, Touches) ← bw_machine (puzzle, crack: PlaceCrack, machine) ← session, ui, cli
```

## 9. Testing

- **copper_test**: `BuildCopperGraph` on two pads + one Trace: pad items touch the Trace; a Bodge Wire joins its pads; `FindIslands` results unchanged on every Machine Board (existing tests).
- **puzzle_test**: `fault crack U1.QA U13.14 30%` → `{line, {U1, QA}, {U13, 14}, 30}`; `37.25%`; errors: old form, missing `%`, `101%`, three decimals, no dot in a pin.
- **crack_test** (hand-made Boards, 0.25 mm Traces, 1.6 mm round pads):
  - one 10 mm Trace: `0%`/`100%` sit `0.8 + 0.175` mm from the pads; `50%` at the middle; swapping the pads mirrors `30%` to `70%`;
  - an L-shaped two-Trace path: neither end of the corner is crackable; 50 % of the clear length;
  - a via on the path (front → back): the crack's Layer follows the Trace it lands on;
  - a T-junction on the path: the junction is not crackable;
  - a pad stub shorter than the pad radius plus the gap: no stretch on it;
  - an arc on the path: skipped; all arcs → `"no crackable Trace between the pads"`;
  - a loop (two parallel Traces): `"does not separate the pads"`;
  - pads on separate Copper: `"pads not joined by Copper"`.
- **machine_test**: load errors from Puzzle lines (unknown reference, no such pin, same pad, not joined); `cracks()` returns the placed crack.
- **test_points**: `PuzzleCrack(name)` = `LoadPuzzleFile(BW_PUZZLE_DIR/name.bwp)->cracks()[0]`; `BoardCrack(machine, a, b, percent)` = `LoadNetlistMachine(...)->placeCrack(...)`. Every `kXxxCrack` use becomes one of them.
- **Machine tests**: all existing Puzzle Symptom and Repair tests pass unchanged.

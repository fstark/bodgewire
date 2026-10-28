# Board View — Implementation Plan (P2f)

Design: [BOARD_VIEW_DESIGN.md](BOARD_VIEW_DESIGN.md)
Spec: [BOARD_VIEW.md](BOARD_VIEW.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Board: courtyards, per-item islands, `Covers`, `ArcPolyline` | |
| 2 | `bw_view`: `BoardScene` and `Viewport` with tests | |
| 3 | Machine: `.bwn` with a Board as a Machine, view accessors | |
| 4 | `bw_ui`: loading via Machine, optional TV, layout, `--screenshot` | |
| 5 | `BoardView`: drawing, zoom, pan, Active Layer, Fit | |
| 6 | Hover, highlight, performance, Done-when check | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug`
Last phase also: `cmake --preset release && cmake --build --preset release && ctest --preset release`

All C++ is `clang-format` clean (never run it on CMakeLists.txt).

---

## Phase 1 — Board: courtyards, per-item islands, point-on-copper

Headless geometry the view needs, in `bw_board`, before anything uses it (design 3.1, 3.2, 4.1).

### 1.1 — Courtyards

- [src/board/board.hpp](../../src/board/board.hpp): `BoardComponent` gains `std::vector<Point> courtyard;  // F.CrtYd/B.CrtYd fp_rect or fp_poly, absolute; empty if none`.
- [src/board/kicad_board.cpp](../../src/board/kicad_board.cpp) `readFootprint`: replace the inline pad transform with a lambda `place(double x, double y) -> Point` (`position + (x·cos + y·sin, −x·sin + y·cos)`, `std::llround`), pass it to `readPad`. In the child loop: `head == "fp_rect" || head == "fp_poly"` with `LayerOf(child).ends_with(".CrtYd")` and `component.courtyard.empty()` → `readCourtyard(child, place)`:
  - `fp_rect`: `start`, `end` → 4 corners (design 4.1) through `place`;
  - `fp_poly`: every `(xy x y)` in `(pts)` through `place`.

### 1.2 — Per-item islands, `Covers`, `ArcPolyline`

[src/board/copper.hpp](../../src/board/copper.hpp) / [copper.cpp](../../src/board/copper.cpp):

```cpp
struct Islands {
    std::vector<std::vector<std::uint32_t>> padIsland;  // [component][pad] → island
    std::vector<std::uint32_t> viaIsland;               // [via] → island
    std::vector<std::uint32_t> traceIsland;             // [trace] → island
    std::vector<std::uint32_t> islandSize;              // copper items per island
};
bool Covers(const Pad& pad, Point at);    // point on one item's copper, FindIslands' shapes
bool Covers(const Via& via, Point at);
bool Covers(const Trace& trace, Point at);
std::vector<Point> ArcPolyline(Point start, Point mid, Point end);  // FindIslands' chords
```

- `FindIslands`: after pads, `viaIsland.push_back(islandOf[next++])` per via, then `traceIsland` per Trace.
- `ArcPoints` (private static) → free function in the anonymous namespace, wrapped by `ArcPolyline` (Vec ↔ Point with `std::llround`).
- `Covers`: a `ShapeBuilder` with that one item (`addPad` / via `add` / `addTrace`, which may add several chord shapes); true if any shape has `Inside(p, shape)` or distance from p to a hull edge (`PointSegmentDistance`; for 1-point hulls, distance to the point) `<= radius`.

### 1.3 — Tests

- [tests/board/kicad_board_test.cpp](../../tests/board/kicad_board_test.cpp): `"kicad board: courtyard rect is placed like pads"` (footprint at (10,20) rotated 90°, `fp_rect` on `F.CrtYd` → 4 expected corners); `"kicad board: courtyard poly"`; `"kicad board: no courtyard"` → empty.
- [tests/board/copper_test.cpp](../../tests/board/copper_test.cpp): `"copper: Covers trace caps and width"`, `"copper: Covers rotated rectangular pad"`, `"copper: Covers via"`, `"copper: via and trace islands"` (a Trace from a pad to a via: all three the same island), `"copper: arc polyline ends"`.
- [tests/board/boards_test.cpp](../../tests/board/boards_test.cpp): `"boards: every Component has a courtyard around its pads"` for both committed Boards (point-in-polygon written in the test).

### Fence

- [ ] `BoardComponent::courtyard`, `Islands::viaIsland/traceIsland`, `Covers` ×3, `ArcPolyline` exist
- [ ] New tests pass; all existing tests pass
- [ ] Full build clean
- [ ] Commit: `"board view: phase 1 — courtyards, item islands, point on copper"`

---

## Phase 2 — `bw_view`: `BoardScene` and `Viewport`

The headless model of the Board panel (design 2, 4, 5.1–5.5).

### 2.1 — Module

- `src/view/CMakeLists.txt`: `add_library(bw_view STATIC board_scene.cpp viewport.cpp)`, `target_link_libraries(bw_view PUBLIC bw_board PRIVATE bw_warnings)`.
- Root [CMakeLists.txt](../../CMakeLists.txt): `add_subdirectory(src/view)` after `src/board`.
- [tests/CMakeLists.txt](../../tests/CMakeLists.txt): add `view/board_scene_test.cpp`, `view/viewport_test.cpp`; link `bw_view`.

### 2.2 — `BoardScene`

`src/view/board_scene.hpp/.cpp`: `Bounds`, `HitKind`, `Hit`, `BoardScene` exactly as design 2 and 4.

- Constructor: outline (5.2), courtyards with fallback (4.2), Nets per item (5.1), `netNames_` from `design.nets` (index = `std::to_underlying(NetId)`), `info_` from `design.parts` (`value`) and `design.components` (`pins()[number − 1].name`; missing reference → empty names).
- `hitTest` (5.3) with a local `Contains(std::span<const Point> polygon, Point)` (crossing number); `hoverText` (5.4) with `std::format`.

### 2.3 — `Viewport`

`src/view/viewport.hpp/.cpp`: `ScreenPoint`, `Viewport` as design 2, 4, 5.5. Scale stored as px per nm; `kMaxPixelsPerMm / 1e6` is the upper clamp; zero-size bounds or panel → `fit` keeps the previous scale.

### 2.4 — Tests

- `tests/view/board_scene_test.cpp` (`[view]`, Machines via `LoadPuzzleFile` / `LoadMachine` with `BW_PUZZLE_DIR`, `#include "../machine/test_points.hpp"` for `kH0Crack`): the cases of design 9 `board_scene_test`. Pick test points from the loaded Board (a pad's position, a via's position, a Trace midpoint on F.Cu that is not under a pad or via, a courtyard point away from copper: e.g. a courtyard corner moved 0.2 mm inwards), not hard-coded coordinates.
- `tests/view/viewport_test.cpp` (`[view]`): the cases of design 9 `viewport_test`.

### Fence

- [ ] `bw_view` builds; `BoardScene`, `Viewport` as in the design
- [ ] `ctest --preset debug -R "view:"` passes; all tests pass
- [ ] Full build clean
- [ ] Commit: `"board view: phase 2 — BoardScene and Viewport"`

---

## Phase 3 — Machine: `.bwn` with a Board, view accessors

(design 3.3)

### 3.1 — Accessors

[src/machine/machine.hpp](../../src/machine/machine.hpp): `const Board& pristineBoard() const` (comment: fault-free Copper, for the view's highlight), `const LoadedCircuit& design() const`.

### 3.2 — `HasBoard`, `LoadNetlistMachine`

machine.hpp/.cpp:

```cpp
// root/netlists/<stem>.bwn whose root/boards/<stem>/<stem>.kicad_pcb exists.
bool HasBoard(const std::filesystem::path& netlist);
// The fault-free Machine of such a netlist (S1).
std::expected<Machine, std::vector<Diagnostic>> LoadNetlistMachine(
    const std::filesystem::path& netlist);
```

- `HasBoard`: `p = absolute(netlist)`; `p.parent_path().filename() == "netlists"` and `exists(root / "boards" / stem / (stem + ".kicad_pcb"))` with `root = p.parent_path().parent_path()`.
- `LoadNetlistMachine`: `LoadMachine(Puzzle{.file = filename, .machine = stem}, root)`.

### 3.3 — Tests

[tests/machine/machine_test.cpp](../../tests/machine/machine_test.cpp):

- `"machine: HasBoard"`: true for `BW_NETLIST_DIR/test_pattern.bwn` and `counter.bwn`; false for a copy of `test_pattern.bwn` in `std::filesystem::temp_directory_path() / "bw_has_board"` (removed after).
- `"machine: netlist with a Board loads fault-free"`: `LoadNetlistMachine(counter.bwn)` ok, `name() == "counter"`, `board().traces.size() == pristineBoard().traces.size()`.
- `"machine: Machine without VIDEO runs"`: counter `build()`, `Runner runner(loaded->circuit, {})`, `pauseAt(1 ms)`, `powerOn(1)`, `waitIdle()`, `status().simTime >= 1 ms`. Link is already there (`bw_runner`).
- `"machine: crack adds a Trace to the drawn Board"`: `broken_trace` → `board().traces.size() == pristineBoard().traces.size() + 1`.

### Fence

- [ ] Accessors, `HasBoard`, `LoadNetlistMachine` exist
- [ ] New tests pass; all tests pass
- [ ] Full build clean
- [ ] Commit: `"board view: phase 3 — fault-free Machine from a netlist"`

---

## Phase 4 — `bw_ui`: loading, optional TV, layout, `--screenshot`

The window shape and the screenshot first, so the agent can see every later phase (design 3.4, 5.7, 5.8).

### 4.1 — Loading and optional TV

[src/ui/main.cpp](../../src/ui/main.cpp):

- `struct Loaded { std::optional<Machine> machine; std::unique_ptr<LoadedCircuit> circuit; std::string name; };`
- `Load()`: `.bwp` → `LoadPuzzleFile` then `build()`; `.bwn` with `HasBoard` → `LoadNetlistMachine` then `build()`; else `LoadNetlistFile`. Diagnostics printed as today.
- Drop the "no VIDEO connector" error. `std::optional<TvConsumer> tv` constructed only when both `ConnectorNet`s exist; `Runner runner(circuit, tv ? std::vector<Consumer*>{&*tv} : std::vector<Consumer*>{})`.

### 4.2 — Arguments

`struct Options { std::filesystem::path file; std::optional<std::filesystem::path> screenshot; std::chrono::nanoseconds after{std::chrono::seconds(1)}; };` and `std::optional<Options> ParseOptions(int argc, char** argv)` (file first, then `--screenshot PATH`, `--after DURATION` via `ParseDuration`, ps → ns). Nullopt → `usage: bw_ui <file.bwn|file.bwp> [--screenshot OUT.png [--after 1s]]`, exit 2. `--after` without `--screenshot` is a usage error.

### 4.3 — Layout and status

- `SDL_CreateWindow(..., 1280, 800, ...)`.
- `src/ui/board_view.hpp/.cpp` with the design-2 interface; this phase: `draw()` fills the child with the board-panel background and the centred text `no board` when there is no scene (else nothing yet); `controls()` draws the Active Layer radio (F.Cu | B.Cu, `L` toggles) and a `Fit` button. Add `board_view.cpp` to `bw_ui`, link `bw_view`.
- Layout of design 5.7: left child ⅔ width (`boardView.draw`), right child: TV (`TvView::draw({w, w * 0.75f})` or a black box with `no video`), `boardView.controls()`, P2e power row, status line with `no video` when there is no TV and ` ui {:.1f} ms` (steady_clock from the start of the frame to just before `SDL_RenderPresent`, previous frame's value).
- `BoardScene` built from the Machine when there is one: `BoardScene(machine->board(), machine->pristineBoard(), machine->design())`.

### 4.4 — Screenshot

`RunWindow` returns `int`; `--screenshot` as design 5.8. `main` returns `RunWindow`'s code after shutdown.

### 4.5 — Check

- Build gate and test gate (no new unit tests: UI only).
- `build/debug/src/ui/bw_ui netlists/test_pattern.bwn --screenshot /tmp/bw-tp.png` → exit 0; same with `netlists/counter.bwn` → exit 0 (no VIDEO); `--after` alone → exit 2; `--screenshot /nonexistent/x.png` → exit 1. View both PNGs: right column with TV (grid) / `no video`, controls, status line.

### Fence

- [ ] `bw_ui` opens `.bwn` with a Board as a Machine; counter opens with `no video`
- [ ] `--screenshot` works; exit codes as S6
- [ ] All tests pass; full build clean
- [ ] Commit: `"board view: phase 4 — bw_ui loading, layout, screenshot"`

---

## Phase 5 — `BoardView`: drawing and navigation

(design 5.6 drawing and input except hover, S3, S4)

### 5.1 — Drawing

[src/ui/board_view.cpp](../../src/ui/board_view.cpp): the draw order of design 5.6 (steps 1–3, 5, 6; no highlight yet). Local helpers in the anonymous namespace: `DrawTrace(ImDrawList&, const Viewport&, ImVec2 origin, const Trace&, ImU32)` (line + two filled circles; arcs via `ArcPolyline`), `DrawPad(...)` per `PadShape`, `DrawVia(...)`. Named colour constants (`kBoardGreen`, `kFrontCopper`, `kBackCopper`, `kPadTin`, `kHole`, `kCourtyard`, `kInactiveAlpha`). Bounding-box culling per item.

### 5.2 — Navigation and Active Layer

Input of design 5.6 (wheel, right/middle drag, `L`, `F`, resize), guarded by `ImGui::IsWindowHovered()` for mouse and `!ImGui::GetIO().WantTextInput` for keys. `needFit_` fits on the first frame with a non-zero size.

### 5.3 — Check

- Build and test gates.
- Screenshots of `netlists/test_pattern.bwn` and `netlists/counter.bwn`: whole Board fitted, both Layers, pads, vias, courtyards, references visible. Zoom is checked by the user in phase 6.

### Fence

- [ ] Board drawn as S3; zoom/pan/Fit/`L` work
- [ ] All tests pass; full build clean
- [ ] Commit: `"board view: phase 5 — draw the Board, zoom and pan"`

---

## Phase 6 — Hover, highlight, performance, Done-when

(design 5.6 hover and highlight, S5, S7)

### 6.1 — Hover and highlight

`hover_ = scene.hitTest(...)` each frame while hovered; `ImGui::SetTooltip("%s", text.c_str())` when `hoverText` is non-empty; highlighted Net = `netOf(*hover_)`; draw step 4 and the amber pads and vias of step 5.

### 6.2 — Performance

Run `bw_ui netlists/test_pattern.bwn` (release): status `ui` < 16 ms fitted and at 100 px/mm, ratio ≈ 1.00× at `1×`. If `ui` is too high, cull harder (skip end caps under 1 px, skip text under 6 px) before anything else.

### 6.3 — Release and Done-when

- Release build and `ctest --preset release`.
- Screenshot of `puzzles/broken_trace.bwp` (agent views it).
- User check (spec Done-when): open `bw_ui puzzles/broken_trace.bwp`, hover H0, zoom along the highlight and find the crack; look good enough to build P2g on. Record tuning wishes in this plan's results.

### Fence

- [ ] Hover text and highlight as S5
- [ ] S7 numbers met (status line)
- [ ] Debug and release tests pass
- [ ] User confirms the Done-when
- [ ] Commit: `"board view: phase 6 — hover and highlight"`

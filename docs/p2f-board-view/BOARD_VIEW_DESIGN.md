# Board View — Detailed Design

Implements the specification in [BOARD_VIEW.md](BOARD_VIEW.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/view/                    new module bw_view: what views show, headless (no SDL/ImGui)
  board_scene.hpp/.cpp       BoardScene: drawn Board + design Nets per item, hit test, hover text
  viewport.hpp/.cpp          Viewport: board nm ↔ panel pixels, fit, zoom, pan
src/board/                   (changed) courtyards, per-item islands, point-on-copper tests
src/machine/                 (changed) fault-free Machine from a .bwn, accessors for the view
src/ui/
  board_view.hpp/.cpp        BoardView: ImGui drawing and input over a BoardScene + Viewport
  main.cpp                   (changed) loading via Machine, optional TV, layout, --screenshot
tests/view/                  board_scene_test.cpp, viewport_test.cpp
```

Everything the spec checks headlessly (S8) lives in `bw_view`, `bw_board` and `bw_machine`; `src/ui` only draws and routes input. A later schematic view goes in `src/view` too.

## 2. Public Interface

```cpp
// src/view/board_scene.hpp
struct Bounds {
    Point min, max;  // nm
};

enum class HitKind : std::uint8_t { Pad, Via, Trace, Component };

struct Hit {
    HitKind kind;
    std::size_t component = 0;  // Pad, Component: index in board().components
    std::size_t index = 0;      // Pad: pad within the Component; Via, Trace: index in board()
    bool operator==(const Hit&) const = default;
};

// The drawn Board (with cracks) labelled from the design Netlist and the fault-free Copper:
// nothing here depends on Faults (S5, P2f Decisions).
class BoardScene {
public:
    BoardScene(Board drawn, const Board& pristine, const LoadedCircuit& design);

    const Board& board() const { return board_; }
    std::span<const Point> outline() const { return outline_; }  // closed, arcs flattened
    std::span<const Point> courtyard(std::size_t component) const;  // never empty (4.2)
    Bounds bounds() const { return bounds_; }                       // of the outline

    std::optional<NetId> padNet(std::size_t component, std::size_t pad) const;
    std::optional<NetId> viaNet(std::size_t via) const;
    std::optional<NetId> traceNet(std::size_t trace) const;
    std::optional<NetId> netOf(const Hit& hit) const;  // Component → nullopt
    std::string_view netName(NetId net) const;

    std::optional<Hit> hitTest(Point at, Layer active) const;  // S5 priority
    std::string hoverText(const Hit& hit) const;  // "U1 pin 14 QA · H0", "H0", "U5 74LS161"
};
```

```cpp
// src/view/viewport.hpp — panel coordinates: pixels from the panel's top-left, y down
struct ScreenPoint {
    double x, y;
};

class Viewport {
public:
    static constexpr double kMaxPixelsPerMm = 100;  // S4; first guess
    static constexpr double kZoomStep = 1.2;        // per wheel notch
    static constexpr double kFitMargin = 0.95;      // share of the panel the fitted Board fills

    void fit(Bounds board, ScreenPoint panel);  // whole outline, centred; sets the minimum zoom
    void resize(ScreenPoint panel);             // refits if fitted, else keeps the centre point
    void zoomAt(ScreenPoint cursor, double notches);  // the board point under the cursor stays
    void pan(ScreenPoint delta);

    ScreenPoint toScreen(Point p) const;
    Point toBoard(ScreenPoint s) const;
    double toPixels(std::int64_t nm) const;
    double pixelsPerMm() const;
};
```

```cpp
// src/board/board.hpp — BoardComponent gains:
std::vector<Point> courtyard;  // F.CrtYd/B.CrtYd fp_rect or fp_poly, absolute; empty if none

// src/board/copper.hpp
struct Islands {
    std::vector<std::vector<std::uint32_t>> padIsland;  // [component][pad] → island
    std::vector<std::uint32_t> viaIsland;               // [via] → island
    std::vector<std::uint32_t> traceIsland;             // [trace] → island
    std::vector<std::uint32_t> islandSize;              // copper items per island
};
// Point on one item's copper, with the shapes FindIslands uses.
bool Covers(const Pad& pad, Point at);
bool Covers(const Via& via, Point at);
bool Covers(const Trace& trace, Point at);
std::vector<Point> ArcPolyline(Point start, Point mid, Point end);  // FindIslands' chords

// src/machine/machine.hpp
class Machine {
    // ...
    const Board& pristineBoard() const { return pristine_; }  // fault-free Copper (highlight)
    const LoadedCircuit& designCircuit() const { return *designCircuit_; }
};
// root/netlists/<stem>.bwn whose root/boards/<stem>/<stem>.kicad_pcb exists.
bool HasBoard(const std::filesystem::path& netlist);
// The fault-free Machine of such a netlist (S1).
std::expected<Machine, std::vector<Diagnostic>> LoadNetlistMachine(
    const std::filesystem::path& netlist);
```

```cpp
// src/ui/board_view.hpp
class BoardView {
public:
    explicit BoardView(std::optional<BoardScene> scene);  // nullopt → "no board"
    void draw(ImVec2 size);  // child region: input, Board, tooltip
    void controls();         // Active Layer (F.Cu | B.Cu) and Fit, one row
};
```

## 3. Integration Points

1. **`BoardReader::readFootprint`** — [src/board/kicad_board.cpp](../../src/board/kicad_board.cpp): in the child loop, `fp_rect` / `fp_poly` whose layer ends with `.CrtYd` → `readCourtyard()` (4.1). Other courtyard shapes are skipped (the view falls back, 4.2). The pad placement maths (`cos`, `sin`, rotate + translate) moves to a local `place(x, y)` lambda shared by pads and courtyards.
2. **`FindIslands`** — [src/board/copper.cpp](../../src/board/copper.cpp): after `islandOf` is filled, also copy the via and Trace entries (`islandOf[next++]` in item order: pads, vias, Traces) into `viaIsland`, `traceIsland`. `ArcPoints` becomes the public `ArcPolyline` (Point in/out, same chords). `Covers` builds the item's `CopperShape` with `ShapeBuilder` and tests `Inside(p) || min edge distance <= radius`. No cost change for existing callers.
3. **`Machine`** — [src/machine/machine.hpp](../../src/machine/machine.hpp): the two accessors; `HasBoard` and `LoadNetlistMachine` build a `Puzzle{file = filename, machine = stem}` and call `LoadMachine(puzzle, netlist.parent_path().parent_path())`. `LoadMachine` is unchanged.
4. **`bw_ui` `main.cpp`** — [src/ui/main.cpp](../../src/ui/main.cpp):
   - `Load()`: `.bwp` → `LoadPuzzleFile`; `.bwn` with `HasBoard` → `LoadNetlistMachine`; else `LoadNetlistFile` as today. `Loaded` gains `std::optional<Machine> machine`.
   - The VIDEO check is no longer an error: `TvConsumer` exists only when both connector Nets do; `Runner` gets `{}` otherwise (S1).
   - `RunWindow` builds `BoardView` from `BoardScene(machine.board(), machine.pristineBoard(), machine.designCircuit())`, lays out the window (4.5), and returns an exit code for `--screenshot` (5.4).
   - Window 1280 × 800 (S2). Arguments: `bw_ui FILE [--screenshot OUT.png] [--after DURATION]`.
5. **CMake** — root: `add_subdirectory(src/view)` after `src/board`; `src/ui`: `board_view.cpp`, link `bw_view`; tests: link `bw_view`, add `tests/view/*`.

`Simulator`, `Runner` and the run loop are untouched: S7's ≈ 1.00× depends only on the UI thread not starving the simulation thread, which runs on its own core.

## 4. Internal State

```cpp
class BoardScene {
    Board board_;                       // drawn: with cracks
    std::vector<Point> outline_;
    Bounds bounds_;
    std::vector<std::vector<Point>> courtyards_;  // [component], fallback filled in
    std::vector<std::vector<std::optional<NetId>>> padNets_;  // [component][pad]
    std::vector<std::optional<NetId>> viaNets_, traceNets_;
    std::vector<std::string> netNames_;           // [NetId]
    struct ComponentInfo {
        std::string value;                        // LoadedPart::value: "74LS161", "4k7"
        std::vector<std::string> pinNames;        // [pad], "" when unknown (resistors)
    };
    std::vector<ComponentInfo> info_;             // [component]
};

class Viewport {
    double nmToPixel_ = 1e-5;   // px per nm
    double minNmToPixel_ = 0;   // fitted scale
    ScreenPoint offset_{};      // panel position of board (0, 0)
    Bounds board_{};
    ScreenPoint panel_{};
    bool fitted_ = true;        // still the fitted view: resize refits
};

class BoardView {
    std::optional<BoardScene> scene_;
    Viewport viewport_;
    Layer active_ = Layer::Front;
    bool needFit_ = true;               // first draw knows the panel size
    std::optional<Hit> hover_;
};
```

### 4.1 Courtyard reading

`fp_rect (start x0 y0) (end x1 y1)`: corners `(x0,y0) (x1,y0) (x1,y1) (x0,y1)`, each through `place()`. `fp_poly (pts (xy …)…)`: each point through `place()`. Coordinates are footprint-local like pads (KiCad 9+). The first courtyard shape found wins.

### 4.2 Courtyard fallback

Empty `courtyard` → the pads' axis-aligned bounding box (pad position ± half the larger pad size) grown by 0.5 mm. Both committed Boards have a courtyard on every footprint (S8 checks it).

## 5. Key Algorithms

### 5.1 Design Nets per item (S5)

```
pin net: for c in design.connections: net[(c.reference, c.pin + 1 as text)] = design.nets.at(c.net)
pads:   padNets_[k][p] = net[(component.reference, pad.number)]   or nullopt (unconnected Pin)
islands = FindIslands(pristine)
islandNet[i] = net of any pristine pad in island i that has one   (check-board: they agree)
vias:   viaNets_[v] = islandNet[islands.viaIsland[v]]             (drawn vias = pristine vias)
traces: for each drawn Trace t:
          m = midpoint of t (for an arc: its mid)
          s = first pristine Trace on t.layer with Covers(s, m)
          traceNets_[t] = islandNet[islands.traceIsland[s]]
```

Cracks remove a Trace and append its pieces (`CrackBoard`), so drawn Trace indices don't match pristine ones; a piece's midpoint always lies on its parent, and Traces of different Nets never overlap (DRC). The same rule will label Traces after P2h's mends. Cost: ~500 × 500 `Covers` with a bounding-box test first, once per scene.

### 5.2 Outline polygon

```
edges = board.outline, each arc flattened by ArcPolyline
chain: start at edges[0]; repeatedly take the unused edge with an end equal to the current point
       (reversed if needed) until back at the start
not closed or edges left over → the bounding box of all edge points (4 corners)
```

### 5.3 Hit test (S5)

```
hitTest(at, active):
  any pad with Covers(pad, at)         → Pad      (both Layers: through-hole)
  any via with Covers(via, at)         → Via
  any Trace on `active` with Covers    → Trace
  any courtyard containing at          → Component   (crossing-number point-in-polygon)
  else nullopt
```

Linear scans: < 1000 items, once per UI frame. The priority is a first guess (spec Decisions).

### 5.4 Hover text

```
Pad:       "{ref} pin {number}{ ' ' + pinName if any } · {net name or '—'}"
Via/Trace: net name, or "" if none
Component: "{ref} {value}"
```

### 5.5 Viewport

```
fit(board, panel):   s = kFitMargin * min(panel.x / width, panel.y / height); min = s
                     offset = panel/2 − centre(board) * s; fitted = true
resize(panel):       fitted → fit; else keep the board point at the panel centre
zoomAt(c, n):        b = toBoard(c); s = clamp(s * kZoomStep^n, min, kMaxPixelsPerMm / 1e6)
                     offset = c − b * s; fitted = (s == min)
pan(d):              offset += d; fitted = false
toScreen(p) = p * s + offset;   toBoard(q) = round((q − offset) / s)
```

### 5.6 Drawing (BoardView::draw, S3)

One ImGui child region; one `ImDrawList`, clipped to it, coordinates `origin + viewport.toScreen(p)`. Order, back to front:

```
1 board fill: AddConcavePolyFilled(outline), dark green
2 Traces of the inactive Layer, its colour at 35% alpha     (thick line + round end caps);
  highlighted ones (traceNet == hovered Net) amber at 60%
3 Traces of the active Layer, full colour; highlighted ones amber (above all inactive copper)
4 pads (Circle: circle; Oval: capsule; Rectangle: rotated quad;
        RoundedRectangle: rotated path with PathArcTo corners), then drill holes
  vias: ring of `diameter`, hole of `drill`
  highlighted pads and vias in amber
5 courtyards: 1 px light-grey polyline; reference centred, font size clamp(1.5 mm, 10 px, 28 px);
  pin-1 mark: dot at the courtyard corner nearest pad "1"
```

Items whose bounding box is off the panel are skipped. Arcs are drawn as `ArcPolyline` segments. A crack needs no code: its gap is in `board()`. Colours, alphas and sizes are named constants in `board_view.cpp`, all first guesses.

Input (only while the child is hovered, and not while ImGui wants text input for the Seed field):

```
wheel        → viewport.zoomAt(mouse − origin, io.MouseWheel)
right/middle drag → viewport.pan(io.MouseDelta)
L            → toggle active_          F → viewport.fit
hover        → hover_ = scene.hitTest(viewport.toBoard(mouse − origin), active_);
               tooltip = hoverText (if not empty); highlighted Net = netOf(hover_)
size change  → viewport.resize
```

No scene → the child shows `no board` centred.

### 5.7 Window layout (S2)

```
one full-window ImGui window, no decoration
left child  "board": width ⅔, full height → boardView.draw
right child "side":
  TV: tvView.draw({width, width * 3/4}), or a black box with "no video"
  boardView.controls()                     Active Layer radio F.Cu | B.Cu, Fit button
  power controls (P2e row, wrapped as needed)
  status: "t 12.345 s  1.00×  locked  running  ring wait 3%  ui 2.1 ms"   (TV part: "no video")
```

`ui` is the UI thread's own work per frame (from frame start to just before present), for S7.

### 5.8 Screenshot (S6)

```
options: --screenshot OUT.png, --after D (ParseDuration, default 1 s; wall clock)
         --after without --screenshot, bad duration, unknown option → usage, exit 2
start = steady_clock::now() right after runner.powerOn(seed)
each frame, after RenderDrawData, before SDL_RenderPresent:
  if screenshot and now − start >= after:
      SDL_SetRenderScale(renderer, 1, 1)                 (read the whole framebuffer)
      surface = SDL_RenderReadPixels(renderer, nullptr)
      ok = surface && SDL_SavePNG(surface, OUT)          (SDL 3.4)
      SDL_DestroySurface(surface)
      ok ? exit code 0 : (print "bw_ui: " + SDL_GetError() to stderr, exit code 1)
      leave the loop
```

The PNG is in framebuffer pixels: window size × display scale (2× on a Retina screen).

## 6. Reused Infrastructure

- `ReadKicadBoardFile` and its footprint placement: courtyards are read in the same pass, with the same transform as pads.
- `FindIslands`, `ShapeBuilder`, `CopperShape`: the view's hit tests and design-Net labelling use exactly the geometry that decides connectivity, so what highlights is what conducts in the fault-free Board.
- `LoadMachine`, `Machine::board()`, `CrackBoard`: the drawn Board, cracks included; `LoadedCircuit::connections`, `nets`, `components` (`PinSpec::name`), `parts` (`LoadedPart::value`): labels.
- `ParseDuration` (netlist/units): `--after`, same syntax as `bw run`.
- P2e `Runner`, `TvConsumer`, `TvView`, controls and status code: unchanged apart from layout.

## 7. Build Integration

```cmake
# src/view/CMakeLists.txt
add_library(bw_view STATIC board_scene.cpp viewport.cpp)
target_link_libraries(bw_view PUBLIC bw_board PRIVATE bw_warnings)

# CMakeLists.txt: add_subdirectory(src/view) after src/board
# src/ui/CMakeLists.txt
add_executable(bw_ui main.cpp board_view.cpp tv_view.cpp)
target_link_libraries(bw_ui PRIVATE bw_machine bw_runner bw_view bw_imgui bw_warnings)
# tests/CMakeLists.txt: view/board_scene_test.cpp, view/viewport_test.cpp; link bw_view
```

## 8. Dependency Diagram

```
ui ──► view ──► board ──► netlist ──► parts ──► sim
 ├───► machine ──► board
 ├───► runner ──► devices ──► sim
 └───► bw_imgui ──► SDL3
```

`view` does not depend on `machine`: `BoardScene` takes a `Board`, a `Board` and a `LoadedCircuit`.

## 9. Testing

- **kicad_board_test**: an `fp_rect` courtyard on a footprint rotated 90° gives the 4 rotated, translated corners; `fp_poly` gives its points; no courtyard → empty.
- **copper_test**: `Covers` true at a Trace's end cap and false just beyond its half width; true inside a rotated rectangular pad's corner, false outside; via ring; `viaIsland`/`traceIsland` agree with `padIsland` for an item touching a pad; `ArcPolyline` starts and ends at the arc's ends and passes near `mid`.
- **boards_test**: every Component of both committed Boards has a non-empty courtyard containing all its pads.
- **board_scene_test** (`[view]`, test-pattern Machine via `LoadPuzzleFile`):
  - outline of both Boards is a closed 4-point rectangle; bounds match;
  - hit test: a point on a pad, a via, an F.Cu Trace (active F.Cu), the same Trace point with B.Cu active (not a Trace hit), inside a courtyard away from copper, and off the Board each give the expected `Hit` (or nullopt); a pad wins over a Trace ending on it;
  - `broken_trace`: the Traces on both sides of `kH0Crack` both have design Net H0; the crack point itself is not on a Trace;
  - every drawn Trace and via of the fault-free test pattern has a design Net;
  - hover text: `U1 pin 14 QA · H0`-style for a pad, Net name for a Trace, `U5 74LS161`-style for a Component; an unconnected pad ends in `· —`; counter `R1` pad has no pin name.
- **viewport_test**: fit centres the bounds with the margin; `toBoard(toScreen(p)) == p` (±1 nm); zoomAt keeps the board point under the cursor; zoom clamps at fit and at 100 px/mm; resize while fitted refits, after a pan keeps the centre.
- **machine_test**: `HasBoard` true for `netlists/test_pattern.bwn`, false for a copy in a temp directory; `LoadNetlistMachine(counter.bwn)` loads; its `build()` runs 1 ms through a `Runner` with no consumers (`pauseAt` + `waitIdle`); `board()` of `broken_trace` has one Trace more than `pristineBoard()` (the crack's two pieces).
- **Agent, screenshot**: `bw_ui netlists/test_pattern.bwn --screenshot /tmp/bw-tp.png` and the same for `counter.bwn` → exit 0, PNG of the window's pixel size; the agent views both.
- **Manual**: Done-when of BOARD_VIEW.md; status line `ui` < 16 ms fitted and at 100 px/mm; ≈ 1.00× at `1×`.

# Board Layout via KiCad — Detailed Design (P2a)

Implements the specification in [BOARD_LAYOUT.md](BOARD_LAYOUT.md) (sections S1–S5).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md). Terminology: [GLOSSARY.md](../GLOSSARY.md). Builds on the netlist loader ([PARTS_DESIGN.md](../p1b-parts/PARTS_DESIGN.md)).

## 1. Directory Layout

```
src/board/                 new module, library bw_board (namespace bw)
  board.hpp                Board model: Components, pads, Traces, vias, outline (nm)
  sexpr.hpp/.cpp           S-expression reader (KiCad files)
  kicad_netlist.hpp/.cpp   S1: WriteKicadNetlist()
  kicad_board.hpp/.cpp     S3: ReadKicadBoard()
  copper.hpp/.cpp          S4: FindIslands() — connectivity from geometry
  board_check.hpp/.cpp     S4: CheckBoard() — islands vs netlist
src/netlist/builder.*      LoadedCircuit gains parts + connections (§3)
src/cli/main.cpp           subcommands export-kicad, check-board (§3)
tests/board/*_test.cpp     one per source file
boards/<name>/             KiCad projects (S2), laid out by the user
docs/p2a-board-layout/KICAD_GUIDE.md   S5
```

`board` is separate from `netlist`: the Board model is the future copper graph of the game (Faults cut it), not a netlist detail. Only `board_check` and `kicad_netlist` need the netlist.

## 2. Public Interface

```cpp
// board.hpp — all lengths in nanometres (KiCad's internal unit), angles in degrees CCW.
enum class Layer : std::uint8_t { Front, Back };  // F.Cu, B.Cu
enum class PadShape : std::uint8_t { Circle, Oval, Rectangle, RoundedRectangle };

struct Point { std::int64_t x; std::int64_t y; };  // y grows downwards (KiCad)

struct Pad {
    std::string number;     // "1".."16"; = Pin index + 1
    Point position;         // absolute
    double angle;           // absolute orientation
    Point size;             // width, height before rotation
    std::int64_t cornerRadius;  // RoundedRectangle only
    std::int64_t drill;
    PadShape shape;
};

struct BoardComponent {
    std::uint32_t line;     // in the .kicad_pcb, for diagnostics
    std::string reference;
    std::string footprint;  // "Package_DIP:DIP-14_W7.62mm"
    Point position;
    double angle;
    Layer side;
    std::vector<Pad> pads;  // through-hole only: on both Layers
};

struct Trace { Layer layer; Point start; Point end; std::optional<Point> mid; std::int64_t width; };  // mid: arc
struct Via { Point position; std::int64_t diameter; std::int64_t drill; };
struct OutlineEdge { Point start; Point end; std::optional<Point> mid; };  // Edge.Cuts

struct Board {
    std::vector<BoardComponent> components;
    std::vector<Trace> traces;
    std::vector<Via> vias;
    std::vector<OutlineEdge> outline;
};
```

```cpp
// kicad_netlist.hpp
std::string_view KicadFootprint(std::string_view package);  // S1 table; "" if unknown
void WriteKicadNetlist(std::ostream& out, const LoadedCircuit& loaded, std::string_view source);

// kicad_board.hpp
std::expected<Board, std::vector<Diagnostic>> ReadKicadBoard(std::string_view text, std::string file);
std::expected<Board, std::vector<Diagnostic>> ReadKicadBoardFile(const std::filesystem::path& path);

// copper.hpp
struct PadRef { std::size_t component; std::size_t pad; };  // indices into Board
struct Islands {
    std::vector<std::vector<std::uint32_t>> padIsland;  // [component][pad] → island
    std::vector<std::uint32_t> islandSize;              // copper items per island
};
Islands FindIslands(const Board& board);

// board_check.hpp
std::vector<std::string> CheckBoard(const LoadedCircuit& loaded, const Board& board);  // empty = ok
```

`Diagnostic` and `Format()` are reused from `netlist/diagnostic.hpp`.

## 3. Integration Points

1. **`src/netlist/builder.hpp`**, `LoadedCircuit`: two new fields, filled by `Builder::build()` after errors are checked. The builder already has everything in `placed_` and `pinNet_` (which includes implicit power after `addImplicitPower()`); it copies them out.

```cpp
struct LoadedPart {
    std::string reference;
    const PartInfo* info;
    std::string value;  // resistor "value" parameter, else the Part name
};
struct Connection {
    std::string reference;
    PinIndex pin;
    std::string net;
};
// in LoadedCircuit:
std::vector<LoadedPart> parts;        // netlist order
std::vector<Connection> connections;  // every connected Pin incl. implicit power; by reference, pin
```

   No cost to `bw run`: copied once at load.

2. **`src/cli/main.cpp`**: `main` dispatches on `argv[1]`: `run` (unchanged), `export-kicad <file.bwn> -o <file.net>`, `check-board <file.bwn> <file.kicad_pcb>`. Usage text lists all three. Exit codes: 0 ok, 1 netlist/board errors or check problems, 2 usage.

## 4. Internal State

```cpp
// sexpr.hpp
struct SExpr {
    std::uint32_t line;
    std::string atom;          // set when list is empty and isList is false
    std::vector<SExpr> list;
    bool isList;
    const SExpr* find(std::string_view head) const;  // first child list starting with atom `head`
    std::string_view head() const;                   // first atom of a list, "" otherwise
};
std::expected<SExpr, Diagnostic> ParseSExpr(std::string_view text, std::string_view file);
```

```cpp
// copper.cpp (internal)
struct CopperShape {          // convex hull (1, 2 or 4 points) grown by radius
    std::uint32_t item;       // copper item: pad, via or Trace; union-find node
    std::uint8_t layers;      // bit 0 Front, bit 1 Back
    std::array<Point, 4> hull;
    std::uint8_t hullSize;
    double radius;            // nm
    Point min, max;           // bounding box including radius
};
```

Shapes per item (one geometry for everything: a hull grown by a radius):

| Item | Hull | Radius | Layers |
|---|---|---|---|
| Circle pad, via | centre | diameter / 2 | both |
| Oval pad | segment along the long axis, rotated | short side / 2 | both |
| Rectangle pad | 4 corners, rotated | 0 | both |
| Rounded rectangle pad | 4 corners inset by the corner radius, rotated | corner radius | both |
| Straight Trace | start, end | width / 2 | its Layer |
| Arc Trace | 16 chords through start, mid, end (one shape each) | width / 2 | its Layer |

## 5. Key Algorithms

### 5.1 Numbers

KiCad writes millimetres with up to 6 decimals. `ParseMillimetres("12.7")` → 12'700'000 nm, by decimal digits (no `double`), so pad and Trace end coordinates compare exactly. Angles are `double`.

### 5.2 Footprint transform (S3)

Pad `(at x y a)` inside a footprint is relative to the footprint, in its unrotated frame; `a` is the pad's absolute angle. KiCad rotates with y down, positive angle counter-clockwise on screen:

```
θ = footprint angle in radians
absolute.x = fp.x + round( x·cos θ + y·sin θ)
absolute.y = fp.y + round(−x·sin θ + y·cos θ)
```

Back-side footprints are stored already mirrored, so the same transform applies. The copper check itself validates this on real boards: a wrong transform turns a DRC-clean board into opens.

### 5.3 Reading the board (S3)

```
root must be (kicad_pcb (version N) ...), N ≥ 20241229 (KiCad 9) else error
layers: any copper layer (name ends ".Cu") other than F.Cu/B.Cu → error
for each child:
  footprint: reference from (property "Reference" R) (KiCad ≥ 8); at, layer → side
      pad: type thru_hole → Pad; np_thru_hole → skip; smd/connect → error
           shape circle|oval|rect|roundrect else error; roundrect: cornerRadius = rratio·min(w,h)
      fp_* / zone on a copper layer → error
  segment / arc → Trace (layer F.Cu|B.Cu else error)
  via → Via (blind/micro → error)
  zone → error
  gr_* on Edge.Cuts → OutlineEdge (gr_rect → 4 edges; gr_circle → error)
  gr_* on a copper layer → error
  anything else → ignored
no outline → error
```

Errors carry the S-expression's line. All errors are collected, not just the first.

### 5.4 Islands (S4)

```
items = pads (component-major), then vias, then Traces
shapes = CopperShapes of every item (table in §4)
union-find over items
for each pair of shapes i < j:
    if layers share a bit and bounding boxes overlap and
       HullDistance(i, j) ≤ radius_i + radius_j: union(item_i, item_j)
islands numbered by first appearance; islandSize counts items
```

`HullDistance(A, B)`: 0 if a hull point of one lies inside the other (hulls of 4 points); else the minimum `SegmentDistance` between an edge of A and an edge of B (a 1-point hull is a zero-length edge). `SegmentDistance` is 0 when the segments cross, else the minimum of the four point-to-segment distances. Pairs are all-pairs with a bounding-box test: a test-pattern Board has ~1–2 k shapes.

### 5.5 Check (S4)

```
expected[(ref, pad number)] = net, from loaded.connections (pad number = pin + 1)
for each netlist part: missing on board → "U5: missing from board"
                       footprint ≠ KicadFootprint(info.package) → "U1: footprint F, expected G"
for each board component not in netlist → "U15: not in netlist"
for each connection without a pad on its Component → "U1.3: no pad 3 on footprint F"
islands = FindIslands(board)
for each island: distinct expected nets of its pads ≥ 2 → short:
    "short: CLK (U1.2), H0 (U1.14)"            (nets sorted, first pad of each)
for each pad with no expected net: islandSize > 1 → "U3.7: copper on an unconnected pad"
for each net with ≥ 2 pads: pads in ≥ 2 islands → open:
    "open VCC: {U1.16, U2.16} | {U3.16}"       (islands by first pad; pads by reference, number)
```

Messages are sorted: Components, then shorts, unconnected pads, opens, each by name.

### 5.6 Writing the netlist (S1)

```
(export (version "E")
  (design (source "counter.bwn") (tool "bodgewire"))
  (components
    (comp (ref "U1") (value "74LS161") (footprint "Package_DIP:DIP-16_W7.62mm")
      (libsource (lib "bodgewire") (part "74LS161") (description "synchronous 4-bit binary counter"))
      (sheetpath (names "/") (tstamps "/"))
      (tstamps "<uuid of U1>")))
  (nets
    (net (code "1") (name "C0")
      (node (ref "U1") (pin "14") (pinfunction "QA")))))
```

- Components in netlist order; nets sorted by name, codes from 1; nodes by reference, then pin number.
- UUID per reference, deterministic (FNV-1a 64 of the reference with two seeds, formatted 8-4-4-4-12): re-importing after a netlist change keeps KiCad's links. The guide still says to link by reference.
- `pinfunction` = datasheet Pin name (chips only).
- Strings quoted, `"` and `\` escaped.

## 6. Reused Infrastructure

- `LoadNetlistFile()`/`LoadedCircuit` for parsing, errors, implicit power; only `parts`/`connections` are added.
- `PartInfo::package`, `PartInfo::description` from the catalogue for footprints and descriptions.
- `Component::pins()` for `pinfunction`.
- `Diagnostic`/`Format()` for board errors.

## 7. Build Integration

```cmake
# src/board/CMakeLists.txt
add_library(bw_board STATIC board_check.cpp copper.cpp kicad_board.cpp kicad_netlist.cpp sexpr.cpp)
target_link_libraries(bw_board PUBLIC bw_netlist PRIVATE bw_warnings)
```

Root `CMakeLists.txt`: `add_subdirectory(src/board)` after `src/netlist`. `bw` and `bw_tests` link `bw_board`. `.gitignore`: `boards/**/*.net`, `boards/**/*.kicad_prl`, `boards/**/*-backups/`, `boards/**/fp-info-cache`.

## 8. Dependency Diagram

```
bw (cli) ──► bw_board ──► bw_netlist ──► bw_parts ──► bw_sim
```

Inside `bw_board`: `board_check → copper → board.hpp`; `kicad_board → sexpr, board.hpp`; `kicad_netlist → netlist`.

## 9. Testing

Catch2, tag `[board]`, one file per source in `tests/board/`.

- `sexpr_test`: atoms, quoted strings with escapes, nesting, line numbers, unbalanced parentheses error.
- `kicad_netlist_test`: `KicadFootprint` table; output for a netlist with a chip, a resistor, a NoConnect Pin (absent), implicit VCC/GND (present), sorted nets, stable UUIDs; parses back with `ParseSExpr`.
- `kicad_board_test`: snippets: footprint transform at 0/90/180°, pad shapes, roundrect radius, arc Trace, via, outline from `gr_rect`/`gr_line`; errors: zone, In1.Cu, smd pad, copper graphic, old version, no outline.
- `copper_test`: Boards built in code: Trace ending in a pad joins it; Trace on the other Layer from a via joins through it; T-junction; arc; Trace passing near a pad without touching does not join; touching pad edge joins; rotated rectangle pad.
- `board_check_test`: in-code Boards against small netlists: ok; open; short; copper on unconnected pad; missing/extra Component; wrong footprint; missing pad.
- `builder_test`: `parts` and `connections`, including implicit power and resistor pins.
- Integration (after the user's layouts): `boards/*/*.kicad_pcb` pass against their netlists; a fixture copy of the counter Board with one track removed reports one open.

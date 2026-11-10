# P4k Board Placement — Detailed Design

Implements [BOARD_PLACEMENT.md](BOARD_PLACEMENT.md): vendored footprints, the `.place` rules, the placer (panel parts by rule, chips by annealing), the `.kicad_pcb` writer, `bw place`, and the 070 Board regenerated. The algorithm is the prototype's ([prototype/place.py](prototype/place.py)), in C++.

Recipes touched: [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) § 3 Board (rewritten in phase 6). All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory layout

```
boards/footprints/*.kicad_mod, README.md   the nine footprints, copied from KiCad 10.0
src/board/footprint.hpp, .cpp              Footprint, LoadFootprints
src/board/place_rules.hpp, .cpp            PlaceRule, ParsePlaceRules
src/board/placer.hpp, .cpp                 Placement, PlaceBoard (rules + annealing)
src/board/kicad_pcb_writer.hpp, .cpp       WriteKicadPcb, WriteKicadProject
src/cli/main.cpp                           `bw place`
boards/tennis/tennis.place                 070 rules
tests/board/footprint_test.cpp, place_rules_test.cpp, placer_test.cpp, kicad_pcb_writer_test.cpp
tests/place_drc.cmake                      ctest script: bw place, then kicad-cli DRC
```

Everything goes into `bw_board` (`src/board/CMakeLists.txt`). Lengths are nanometres in `Point` (`board/board.hpp`), as the reader's.

## 2. Footprints

`boards/footprints/` holds the nine files named by `KicadFootprint` (`src/board/kicad_netlist.cpp`, `kFootprints`), copied unchanged from `/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints/<Lib>.pretty/<Name>.kicad_mod` (KiCad 10.0), flat: `boards/footprints/<Name>.kicad_mod`. `README.md` names the source, the KiCad version, and the licence (CC-BY-SA 4.0 with the KiCad libraries' exception for designs using them).

```cpp
// src/board/footprint.hpp
struct FootprintPad {
    std::string number;  // "1"; Pin index + 1
    Point position;      // relative to the footprint origin, at 0°
};

struct Footprint {
    std::string id;    // "Package_DIP:DIP-14_W7.62mm", as in the netlist
    std::string text;  // the .kicad_mod file, unchanged
    std::vector<FootprintPad> pads;
    Point courtyardMin, courtyardMax;  // bounding box of the F.CrtYd graphics and the pads
};

// By package ("DIP-14"); diagnostics name the file. Only the packages asked for are read.
using FootprintLibrary = std::map<std::string, Footprint, std::less<>>;
std::expected<FootprintLibrary, std::vector<Diagnostic>> LoadFootprints(
    const std::filesystem::path& directory, std::span<const std::string_view> packages);
```

- Parsed with `ParseSExpr` (`board/sexpr.hpp`). Pads: `(pad "N" … (at x y …) (size w h))`, `np_thru_hole` skipped. Courtyard: every `fp_rect`, `fp_poly`, `fp_line`, `fp_arc`, `fp_circle` on `F.CrtYd`, by their `start`/`end`/`mid`/`center`/`xy` points, widened by the pad boxes (as `ReadKicadBoard` does for line courtyards). `ParseMillimetres` (`kicad_board.hpp`) converts.
- A package with no `KicadFootprint`, a missing file or a file without pads or courtyard is a diagnostic.
- The default directory is `BW_FOOTPRINT_DIR`, a compile definition (`${PROJECT_SOURCE_DIR}/boards/footprints`) on `bw` and `bw_tests`, as `BW_NETLIST_DIR` is on the tests.

## 3. Rules

```cpp
// src/board/place_rules.hpp
enum class Edge : std::uint8_t { Left, Right, Back, Front };  // not "Side": a Layer (GLOSSARY)

// Along an edge, 0 = the left (or back) end, 1 = the right (or front) end.
struct EdgeRule { Edge edge; double at; std::vector<std::string> references; };  // edge, row
struct NextRule { std::string reference, other; Edge side; };  // side of `other`

struct PlaceRule {
    std::uint32_t line;
    std::variant<EdgeRule, NextRule> rule;
};

struct PlaceRules {
    std::vector<PlaceRule> rules;  // file order
    std::optional<std::pair<std::int32_t, std::int32_t>> grid;  // columns, rows
};

std::expected<PlaceRules, std::vector<Diagnostic>> ParsePlaceRules(std::string_view text,
                                                                   std::string file);
```

- Words split on blanks; `#` starts a comment; blank lines skipped.
- `edge REF SIDE AT` is an `EdgeRule` of one reference; `row SIDE AT REF…` (at least one) the same with several.
- `AT`: `middle` → 0.5; the edge's ends → 0 or 1 (`left`/`right` on `back`/`front`, `back`/`front` on `left`/`right`; the other pair is an error: "'left' is not an end of the left edge"); `N%` with 0 ≤ N ≤ 100 → N/100.
- `next REF OTHER SIDE`; `grid C R`, C, R ≥ 1, once.
- Errors, all collected, `file:line`: unknown rule word, wrong word count, bad side or position, a reference in two rules, `next` whose `OTHER` is not in an earlier rule, `grid` twice. References are checked against the netlist by the placer, not here.

## 4. Placer

```cpp
// src/board/placer.hpp
struct PlacedPart {
    std::string reference;
    const Footprint* footprint;
    Point origin;  // footprint (at), 0°
};

struct Placement {
    Point outlineMin, outlineMax;
    std::vector<PlacedPart> parts;       // netlist order
    std::int32_t columns = 0, rows = 0;
    std::vector<std::string> grid;       // row-major slot → reference, "" when empty
    std::int64_t wireLength = 0;         // nm, half-perimeter, VCC and GND left out
};

struct PlaceOptions {
    std::uint64_t seed = 1;
    std::int32_t tries = 4;  // seeds seed … seed + tries − 1, the shortest kept (the first on a tie)
};

std::expected<Placement, std::vector<Diagnostic>> PlaceBoard(
    const LoadedCircuit& loaded, const PlaceRules& rules, std::string_view rulesFile,
    const FootprintLibrary& footprints, const PlaceOptions& options);
```

Constants (mm, in `placer.cpp`): column pitch 16, row pitch 27 (prototype); edge margin 1 (courtyard to outline); band gap 4 (panel courtyard to grid courtyard); bare margin 5 (outline to grid on an edge without panel parts); part gap 2 (between panel courtyards in a `row` and beside a `next`); outline top-left corner at (15, 15). Footprint origins are snapped to 1.27 mm, the grid's origin too (rounded up, so the bands only grow), so pads sit on the routing grid.

Steps:

1. **Sort parts.** Every reference in a rule must be a netlist Part (`loaded.parts`), else "unknown reference". A part with a rule is a panel part. Every other part is a grid part if its package is `DIP-14`, `DIP-16` or `axial`; any other package is "`DS1` (DIGIT-0.56) needs a placement rule".
2. **Groups.** Each `EdgeRule` starts a group on its edge; its parts are laid out along the edge in order, `part gap` apart, the outer sides of their courtyards on one line (the one nearest the edge); each `NextRule` adds its part to its `OTHER`'s group, its courtyard `part gap` from `OTHER`'s on `SIDE`, centred on it across. Positions are local to the group so far. A part whose courtyard would lie beyond its group's edge (a `next` towards the edge) is "`R1` falls off the front edge". A group's across-edge extent includes its `next` parts: `next R2 D1 back` on a front group deepens the front band.
3. **Grid.** `n` grid parts. `n` = 0: no slots, the grid box is empty (zero size at the Board origin plus the left and back depths), and the outline is the bands alone. Without `grid`: rows `r` from 1 to `n`, columns `ceil(n / r)`, keep the `r` whose `columns × 16 / (r × 27)` is closest to 1.6 (first on a tie). With `grid`, fewer than `n` slots is an error. A slot's box is the union of the courtyards of the grid footprints in use (pin 1 at the slot origin); the grid box is the slots' boxes from the first to the last.
4. **Bands.** A side's band depth is the largest depth of its groups (edge margin + courtyard extent across the edge + band gap), or the bare margin. The outline is the grid box grown by the four depths, its top-left corner at the Board origin.
5. **Along the edges.** The back and front edges span the whole width less the edge margin; the left and right edges span only the grid box's height (so their bands never meet the back and front ones). The anchor's courtyard-box centre (the part's for `edge`, the whole row's for `row`) goes to `start + margin + at × (length − 2 × margin)` along the span, then the group is shifted to stay inside the span less the margin at each end. A group longer than that is "`D1` falls off the left edge". Every panel courtyard is then checked against the others: an overlap is "`J3` overlaps `SW1`".
6. **Annealing.** As `place.py`: grid parts start in a random slot assignment; a move picks a grid part and a random slot and swaps with its occupant (or moves into an empty slot); the cost is the sum over Nets with pads on two or more parts, `VCC` and `GND` left out, of the half-perimeter of their pads' bounding box (pads from `Footprint::pads`, panel parts fixed). Only the Nets touched by the move are recomputed; Metropolis acceptance; the temperature falls geometrically from 200 mm to 0.0005 mm over `10'000 × n` moves. Random numbers: `std::mt19937_64` with own reduction (`rng() % k`, `rng() >> 11` scaled for the [0, 1) draw), so the result does not depend on the standard library. One run per seed, the shortest kept.
7. **Result.** `parts` in netlist order, `grid` for printing, `wireLength` the kept run's cost.

Diagnostics name `rulesFile` and the rule's line (0 for problems not on a line, such as a digit without a rule).

## 5. Writer

```cpp
// src/board/kicad_pcb_writer.hpp
// KiCad 10 .kicad_pcb: header, the footprints placed with their Nets, the outline; no tracks.
void WriteKicadPcb(std::ostream& out, const LoadedCircuit& loaded, const Placement& placement);
// KiCad project with the default design rules every Board uses; `name` is the file stem.
void WriteKicadProject(std::ostream& out, std::string_view name);
```

- Header as the feasibility file of 2026-11-10 (which KiCad 10's DRC loaded: 0 violations, unrouted connections listed): `(version 20260206)`, `(generator "bodgewire")`, `(general (thickness 1.6) (legacy_teardrops no))`, `(paper "A4")` (`"A3"` when the outline's far corner, origin offset included, lies beyond A4's 297 × 210 mm), the two copper layers and the user layers of `boards/tennis/tennis.kicad_pcb`, `(setup (pad_to_mask_clearance 0))`.
- Each footprint from `Footprint::text`, line by line, indented one tab: the first line's name becomes the id (`(footprint "Package_DIP:DIP-14_W7.62mm"`); the `(version …)` and `(generator …)` lines dropped; `(at X Y)` and `(path "/<uuid>")` added after `(layer "F.Cu")`, the uuid being `KicadUuid(reference)`, the one `WriteKicadNetlist` writes (its `Uuid` moved into `kicad_netlist.hpp` as `KicadUuid`), so a later netlist import links them; every `(uuid …)` and `(generator_version …)` line dropped (the button and digit footprints carry uuids: two copies on one Board would share them); `"REF**"` replaced by the reference and the `Value` property's text by `LoadedPart::value`; in each pad block whose number is on a Net (`loaded.connections`, pin + 1), `(net "NAME")` and `(pinfunction "PIN")` (the Component's pin name, when there is a Component) inserted before the block's `(layers` line. Unconnected pads get nothing. The vendored files are KiCad-formatted (one tab per depth, `(pad` blocks at depth 1); a pad block without a `(layers` line is a `Check` failure.
- No uuids at all apart from `path`: KiCad assigns them when it saves.
- The outline: one `gr_rect` on `Edge.Cuts`, width 0.05, as on `tennis.kicad_pcb`.
- Project: `{"board": {"design_settings": {"rules": …}}, "meta": {"filename": "<name>.kicad_pro", "version": 3}}` with the `rules` object of `boards/tennis/tennis.kicad_pro` (identical on every Board). Phase 4 checks KiCad fills the rest and the DRC reads it.
- `Quote` from `kicad_netlist.cpp` moves to the header too (`KicadQuote`), for net names.

## 6. `bw place`

`bw place <file.bwn> <file.place> -o <file.kicad_pcb> [--seed N] [--tries N] [--footprints DIR] [--force]`, in `src/cli/main.cpp` beside `ExportKicad`, added to `kUsage` and the command list.

1. `Load` the netlist and `BoardNetlistProblems` as `ExportKicad` does.
2. `ParsePlaceRules` on the rules file; `LoadFootprints` for the packages of `loaded.parts`.
3. If the output exists and has a `(segment`, `(arc` or `(via` (text search), refuse unless `--force`: "boards/x/x.kicad_pcb has tracks; --force replaces it".
4. `PlaceBoard`; diagnostics printed with `Format`, exit 1.
5. Write the `.kicad_pcb`; write `<stem>.kicad_pro` beside it only if absent.
6. Print `wire_length_m 10.62` and the grid, one line per row, references right-aligned in 4 columns (`--` for an empty slot), as `place.py`.

## 7. 070

`boards/tennis/tennis.place`, from TENNIS_DESIGN.md § Board:

```
# 070 Paddle tennis: panel parts (docs/machines/070-tennis/TENNIS_DESIGN.md, Board)
edge J1 left middle    # power
edge J2 back right     # TV
edge J3 front left     # left player's controller
edge J4 front right    # right player's controller
edge SW1 front middle  # Start
next R1 SW1 right      # START_N pull-up
```

59 grid parts (58 chips and `X1`) → 5 rows of 12, as the prototype. The user routes the result (KICAD_GUIDE steps 7–8). Then, per ADD_MACHINE § 3 and ADD_PUZZLE: `bw check-board` `ok`; the Invisible ball constraint (`SERVING` routed with `U38` at an end, TENNIS_DESIGN § Board) read from the new routing, and the crack moved if needed; `boards_test`, `electrical_test`, `board_scene_test`, `drc_tennis`, `tennis_test` green. TENNIS_DESIGN § Board says the Board comes from `tennis.place`.

## 8. Tests

- `footprint_test.cpp`: every package of the catalogue loads (`AllParts`); DIP-14 pad 1 at (0, 0), pad 8 at (7.62, 15.24), courtyard (−1.06, −1.53)–(8.67, 16.77) widened by the pads; a missing file and an unknown package are diagnostics.
- `place_rules_test.cpp`: `edge`, `row`, `next`, `grid`, comments; each error with its line.
- `placer_test.cpp`, on netlists written in the test (`LoadNetlist`): an `edge` LED at `front 30%` touches the front margin with its centre at 30 % of the width; a `row` of three LEDs in order, 2 mm apart; `next R1 D1 back` behind the LED; `next R1 D1 front` on a front LED falls off; two `edge` parts at the same place overlap; a `DIGIT` without a rule is refused; an unknown reference is refused; `grid 1 1` with two chips is refused; with `grid 4 4`, six chips of which two share eight Nets: those two end in adjacent slots; the same options give the same `Placement`; no grid parts gives an outline of the bands alone; LEDs in a `row` longer than the left edge fall off. `[slow]` `placer: tennis`: the 070 netlist and rules, seed 1, one try: every courtyard inside the outline and apart, `J3` left of `J4` and both on the front, `R1` right of `SW1`, `J1` on the left, `J2` on the back, wire length under 12 m (the value noted in phase 3).
- `kicad_pcb_writer_test.cpp`: a small Board written and read back with `ReadKicadBoard`: every part at its origin, pads where the footprint puts them, the outline; with `ParseSExpr`, each pad's `net` is its netlist Net and unconnected pads have none; `REF**` gone; two `DIGIT`s give a file without `(uuid`; the project is valid JSON with the rules. `[slow]` `kicad pcb writer: tennis`: the 070 Board written from its `Placement` reads back with `ReadKicadBoard` without diagnostics, every netlist Part present once, and `ParseSExpr` finds every pad's `net` equal to its netlist Net.
- `tests/place_drc.cmake`, run by ctest `place_drc_tennis` when `kicad-cli` is found (next to the `drc_*` tests in `tests/CMakeLists.txt`), given `-DBW=$<TARGET_FILE:bw>`, `-DKICAD_CLI=…`, the netlist, rules and output paths: `bw place` 070 into the build directory, `kicad-cli pcb drc --severity-error` **without** `--exit-code-violations` (it exits 5 on unrouted pads), then the report must contain `Found 0 DRC violations` and `Found 0 Footprint errors` (unrouted pads are expected: about 500 on 070).

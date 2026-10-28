# UI Usability — Detailed Design

Implements the specification in [UI_USABILITY.md](UI_USABILITY.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

---

## 1. Directory Layout

```
manuals/
  counter/user.md, counter/service.md
  test_pattern/user.md, test_pattern/service.md
datasheets/
  74LS00.md 74LS02.md 74LS04.md 74LS08.md 74LS161.md 74LS20.md 74LS30.md
  74LS32.md 74LS74.md 74LS86.md OSC.md POWER.md R.md VIDEO.md      # one per catalogue Part
src/manual/                 # new library bw_manual: headless, no SDL/ImGui
  markdown.hpp/.cpp         # Markdown subset → blocks and spans
  manual.hpp/.cpp           # load User/Service Manuals and Datasheets, checks, lookups
src/machine/                # Puzzle title/brief, ReadPuzzleFile, MachineRoot
src/view/                   # BoardScene::pick (snapping, Layer fall-through), Viewport::frame
src/ui/
  board_view.hpp/.cpp       # + pin labels, selection, focus/flash, drag-pan, keys
  toolbar.hpp/.cpp          # new: state banner + toolbar
  side_panel.hpp/.cpp       # new: right column sections
  chip_panel.hpp/.cpp       # new: Chip section (pinout, Datasheet)
  manual_view.hpp/.cpp      # new: Markdown drawing with links; manual windows
  main.cpp                  # options, loading, frame loop only
tests/manual/markdown_test.cpp, manual_test.cpp
tests/machines/manuals_test.cpp   # the committed manuals and Datasheets
```

`bw_manual` is separate from `bw_view` because it reads files and knows the Parts catalogue; `bw_view` stays geometry and labelling. Everything testable (parsing, checks, lookups, picking, framing) is headless; `src/ui` only draws and routes input.

## 2. Public Interface — `src/manual/markdown.hpp`

```cpp
namespace bw {

enum class SpanStyle : std::uint8_t { Text, Code, Bold };

struct Span {
    SpanStyle style;
    std::string text;  // without the ` or ** markers
};

enum class BlockKind : std::uint8_t { Title, Heading, Paragraph, Bullet };  // #, ##, text, -

struct Block {
    BlockKind kind;
    std::uint32_t line;  // first source line, for diagnostics
    std::vector<Span> spans;
};

struct Markdown {
    std::vector<Block> blocks;
};

Markdown ParseMarkdown(std::string_view text);  // never fails: unknown syntax stays text

std::string PlainText(std::span<const Span> spans);
std::string_view Title(const Markdown& doc);  // first Title block's text, "" if none
// Blocks after "## heading" up to the next Title/Heading; empty if absent.
std::span<const Block> Section(const Markdown& doc, std::string_view heading);

}  // namespace bw
```

## 3. Public Interface — `src/manual/manual.hpp`

```cpp
namespace bw {

struct Manuals {
    std::optional<Markdown> user, service;
    std::map<std::string, Markdown, std::less<>> datasheets;  // by Part name, Parts on the Machine
};

struct LoadedManuals {
    Manuals manuals;
    std::vector<Diagnostic> problems;  // Severity::Warning: unreadable or failing a check
};

// root/manuals/<machine>/{user,service}.md and root/datasheets/<Part>.md for the design's Parts.
// Missing files are not problems (S2: "no user manual").
LoadedManuals LoadManuals(const std::filesystem::path& root, std::string_view machine,
                          const LoadedCircuit& design);

// S10 checks; each returns all problems found, empty if fine.
std::vector<Diagnostic> CheckUserManual(const Markdown& doc, std::string_view file);
std::vector<Diagnostic> CheckServiceManual(const Markdown& doc, std::string_view file,
                                           const LoadedCircuit& design);
std::vector<Diagnostic> CheckDatasheet(const Markdown& doc, std::string_view file,
                                       const PartInfo& part);

std::vector<std::string> PartPinNames(const PartInfo& part);  // empty for resistors

// "- `U6` end-of-frame decoder: detail" → {"end-of-frame decoder", "detail"}.
struct Role {
    std::string name, detail;
};
std::optional<Role> ComponentRole(const Markdown& service, std::string_view reference);

// Pin names a Datasheet `Pins` bullet covers: its code spans before the first ':', ranges
// `1A`–`1D` expanded (S3).
std::vector<std::string> BulletPins(const Block& bullet);
const Block* PinBullet(const Markdown& datasheet, std::string_view pin);  // nullptr if none

// A backticked Component reference, optionally followed by " pin N", starting at spans[i].
struct ManualLink {
    std::string reference;
    std::optional<std::string> pin;  // number
    std::size_t length;              // spans covered (1, or 2 when the pin is in the next Text)
};
std::optional<ManualLink> LinkAt(std::span<const Span> spans, std::size_t i,
                                 const LoadedCircuit& design);

}  // namespace bw
```

## 4. Integration Points

### 4.1 `src/machine/puzzle.hpp` / `puzzle.cpp` (S1)

```cpp
struct Puzzle {
    ...
    std::string title, brief;             // "" when absent
    std::uint32_t titleLine = 0, briefLine = 0;
};
```

`Parser::line` checks the first token before stripping `#` comments: `title` and `brief` take the raw rest of the line through `QuotedValue` (§6.1), so `#` inside quotes is text. Errors: `expected: title "TEXT"`, `title already given at line N` (same for `brief`).

### 4.2 `src/machine/machine.hpp` / `machine.cpp`

```cpp
// The directory holding netlists/, boards/, puzzles/, manuals/, datasheets/: the file's
// directory's parent.
std::filesystem::path MachineRoot(const std::filesystem::path& file);
// Read + ParsePuzzle; LoadPuzzleFile becomes ReadPuzzleFile + LoadMachine.
std::expected<Puzzle, std::vector<Diagnostic>> ReadPuzzleFile(const std::filesystem::path& path);
```

`LoadPuzzleFile`, `HasBoard` and `LoadNetlistMachine` use `MachineRoot` instead of their own `parent_path().parent_path()`. The Machine itself stays Puzzle-text free.

### 4.3 `src/view/board_scene.hpp` (S6, S7, S8)

`hitTest` is replaced by `pick`; callers are `BoardView::input` and `board_scene_test.cpp`.

```cpp
struct PickReach {
    std::int64_t pad = 0;     // nm: 12 px at the current zoom
    std::int64_t copper = 0;  // nm: 10 px at the current zoom
};

struct Pick {
    Hit hit;
    Layer layer;    // the picked Trace's Layer; the Active Layer for pads, vias, Components
    Point contact;  // pad/via centre, nearest centreline point of a Trace, else the cursor
};

class BoardScene {
    ...
    std::optional<Pick> pick(Point at, Layer active, PickReach reach) const;  // §6.4

    std::string_view partName(std::size_t component) const;  // "74LS20", "R", "" if unknown
    std::string_view pinName(std::size_t component, std::size_t pad) const;  // "" if unknown
    bool powerPin(std::size_t component, std::size_t pad) const;              // PinKind::Power
    std::optional<std::size_t> componentIndex(std::string_view reference) const;
    std::optional<std::size_t> padIndex(std::size_t component, std::string_view pin) const;  // number or name
private:
    struct ComponentInfo {
        std::string value;
        const PartInfo* part = nullptr;     // LoadedPart::info
        std::vector<std::string> pinNames;  // [pad]
        std::vector<bool> powerPins;        // [pad]
        std::int64_t halfPitch = 0;         // nm: half the smallest pad spacing, 0 for one pad
    };
```

`hoverText` is unchanged; the UI appends the role and Datasheet line (S6).

### 4.4 `src/view/viewport.hpp` (S8, S9)

```cpp
// Centres `box` and zooms so it fills `share` of the panel, within [fitted, kMaxPixelsPerMm].
void frame(Bounds box, double share = 0.5);
```

### 4.5 `src/ui/board_view.hpp`

```cpp
struct BoardTarget {
    std::size_t component;
    std::optional<std::size_t> pad;
};

struct BoardClick {
    Point at;                              // cursor, nm
    Layer active;
    std::optional<Pick> pick;              // nullopt: off everything
    std::optional<std::size_t> component;  // the pick's Component, else componentAt(at)
};

class BoardView {
public:
    void draw(ImVec2 size);
    Tool tool() const;
    void setTool(Tool tool);
    Layer activeLayer() const;
    void toggleLayer();
    void fit();
    std::optional<BoardClick> takeClick();          // released without dragging
    std::optional<std::size_t> chip() const;        // hovered Component, else selected (S6)
    void select(std::size_t component);
    void hoverFromOutside(std::optional<BoardTarget> target);  // this frame only
    void focus(BoardTarget target);                 // frame (§6.6) + flash 1 s
    const BoardScene* scene() const;
    ...
};
```

`controls()` disappears (moved to `Toolbar`). New input in `input()`: left-drag pan past ImGui's drag threshold (the click is then dropped), click on release, double-click on a Component → `focus`, `+`/`-` → `zoomAt(panel centre, ±1)`, `Esc` → `escape_ = true` (read by main via `takeEscape()`), `U`/`H` handled by `Toolbar`. The tooltip is built by the caller-supplied `TooltipText` callback so `BoardView` does not depend on `bw_manual`:

```cpp
using TooltipText = std::function<std::string(const BoardScene&, const Hit&)>;
void setTooltip(TooltipText text);
```

### 4.6 `src/ui/main.cpp`

Keeps `ParseOptions` (+ `--manual user|service`), `Load`, `SaveScreenshot`, SDL/ImGui set-up and the frame loop. `Load` returns:

```cpp
struct Loaded {
    std::optional<Session> session;
    std::string title;         // Puzzle title, else Machine name, else netlist stem
    std::string brief;
    LoadedManuals manuals;     // problems printed to stderr as warnings, never fatal
};
```

For `.bwp`: `ReadPuzzleFile` → `LoadMachine(puzzle, MachineRoot(file))`. Manuals load for any Machine with a design (`LoadManuals(MachineRoot(file), machine.name(), machine.designCircuit())`); a netlist without a Board loads them from its `LoadedCircuit`.

The frame loop becomes:

```
banner/toolbar row (Toolbar::draw)
BoardView::draw(left)  → route takeClick() (§6.7)
SidePanel::draw(right)
ManualWindows::draw()  (floating, toggled)
```

### 4.7 Click routing (§6.7) and the Probe tip

`Session::touch(ProbeTip{pick.contact, pick.layer})`: the contact point lies on the picked copper, so `Machine::probeNet` (which uses `Covers`) finds the same item even when the cursor was 10 px away. No change to `Session` or `Machine::probeNet`.

## 5. Internal State

```cpp
class Toolbar {  // src/ui/toolbar.hpp
public:
    void draw(Session& session, BoardView& board, Speed& speed, std::uint64_t seed);
    void refused();  // a click the tool could not act on: flash amber
    bool userManualOpen = false, serviceManualOpen = false;
private:
    std::chrono::steady_clock::time_point flashUntil_{};
};

class SidePanel {  // src/ui/side_panel.hpp
public:
    void draw(Session& session, TvView& tv, BoardView& board, const Loaded& loaded,
              DebugStats stats, std::uint64_t& seed);
private:
    ChipPanel chip_;
};

struct DebugStats {
    double simSeconds, speedRatio, ringWaitShare, uiMilliseconds;
    std::string_view lock, state;
};

class ChipPanel {  // src/ui/chip_panel.hpp
public:
    // Draws the Component's section; returns the pinout pin under the mouse, if any.
    std::optional<BoardTarget> draw(const BoardScene& scene, std::size_t component,
                                    const Manuals& manuals);
};

class ManualView {  // src/ui/manual_view.hpp; one per window, plus one for the Puzzle section
public:
    // Draws blocks; returns the link under the mouse; a clicked link sets `clicked`.
    std::optional<BoardTarget> draw(std::span<const Block> blocks, const BoardScene* scene,
                                    const LoadedCircuit& design, std::optional<BoardTarget>& clicked);
};
```

`BoardView` gains: `selected_`, `hovered_` (Component), `outside_` (`std::optional<BoardTarget>`, cleared after each draw), `flash_` (`BoardTarget` + deadline), `dragging_`, `escape_`, `tooltip_`.

## 6. Key Algorithms

### 6.1 Quoted values in `.bwp`

```
QuotedValue(rest):
  skip spaces; expect '"'; read to the next '"' (none → error); value may be empty
  after it: only spaces, or '#' then anything
```

### 6.2 Markdown subset

```
for each line (numbered):
  blank                → close the open block
  "# " / "## "         → Title / Heading block (one line)
  "- "                 → new Bullet; following lines indented ≥ 2 spaces continue it
  otherwise            → append to the open Paragraph (or start one), joined with a space
inline (per block text):
  `...`   → Code span; **...** → Bold span; an unmatched marker stays literal Text
```

### 6.3 Pin bullets and ranges (S3)

```
BulletPins(bullet):
  take spans up to the first Text span containing ':'
  names = Code spans
  between two Code spans a Text span that is exactly "–" or "-" makes a range:
    a, b differ in exactly one character position, both letters or both digits there
    → every name with that character from a[i] to b[i]; else both names as written
```

`CheckDatasheet`: `Title` starts with the Part name, `Function` and `Pins` present in that order, and every name in `PartPinNames(part)` is in some `BulletPins`. `PartPinNames` builds the Part in a scratch `Circuit` through `PartInfo::create` (with `freq=1MHz` for `OSC`) and reads `pins()`.

`CheckServiceManual`: `How it works`, `Components` in order, `Troubleshooting` optional and last; one `Components` bullet per `design.parts` reference (each bullet starts with a Code span naming it; none missing, none extra, no duplicates); in every block of both manuals each Code span that looks like a reference (`[A-Z]+[0-9]+`) must be in `design.parts`, and a following ` pin N` must name an existing pin number of that Component. `CheckUserManual`: `What it does` then `Connections`.

### 6.4 Picking (S8)

```
pick(at, active, reach):
  pads:  best = nearest pad centre over all Components with
         Covers(pad, at) or |at − centre| ≤ max(reach.pad, halfPitch[k])
         → Pick{Pad, active, centre}
  vias:  edge = max(0, |at − centre| − diameter/2) ≤ reach.copper, nearest
         → Pick{Via, active, centre}
  traces, for layer in (active, other):
         edge = max(0, distance(at, centreline) − width/2) ≤ reach.copper, nearest
         (arcs: their ArcPolyline chords, as Covers)
         first layer with a candidate wins → Pick{Trace, trace.layer, nearest centreline point}
  courtyards: Contains → Pick{Component, active, at}
  none → nullopt
```

`BoardView` computes `reach` each frame: `{12 px, 10 px} / pixelsPerMm × 1e6`. ~200 pads + ~500 Trace segments per frame: negligible next to drawing.

### 6.5 Labels (S7)

```
fitted or larger: reference at courtyard centre (as now); Part name under it at 0.6× size
                  when it fits the courtyard width
pin names, per pad, when min(pad.size) in px ≥ 14 and the pad is on the panel:
  d = courtyard centre − pad centre; axis = the larger of |d.x|, |d.y|
  text starts pad half-size + 3 px from the centre along that axis, towards the centre,
  vertically centred on the pad; font size clamp(0.9 mm in px, 10, 16)
```

### 6.6 Framing

```
focus(target):
  box = pad ? pad.position ± 4 mm : bounds of courtyard(component)
  viewport.frame(box, pad ? 0.3 : 0.5); flash_ = {target, now + 1 s}
draw: the flashed pad (or every pad of the Component) gets a pulsing white ring
```

### 6.7 Click routing (main)

```
click = board.takeClick(); if click:
  if click.component: board.select(*click.component)
  switch tool:
    Probe: if pick and pick.hit.kind != Component: touch(ProbeTip{contact, pick.layer})
           (empty board: nothing; Esc: touch(nullopt))
    Swap:  if component: r = session.repair(Swap{reference}); r == Refused → toolbar.refused()
    Mend:  layer, at = pick is Trace ? (pick.layer, contact) : (active, click.at)
           r = session.repair(Mend{layer, at}); r == Refused → toolbar.refused()
```

### 6.8 Banner text (S4)

```
power  = !powered ? "POWER OFF" : paused ? "POWER ON · paused" : "POWER ON"
action = Probe: "Probe: click a pin or trace"
         Swap:  powered ? "Swap: power off first" (amber) : "Swap: click a chip"
         Mend:  powered ? "Mend: power off first" (amber) : "Mend: click a crack"
whole banner background amber while now < flashUntil_ (0.4 s)
```

## 7. Reused Infrastructure

- `Diagnostic`/`Format` (netlist) for manual problems and `.bwp` errors.
- `PartInfo`, `AllParts`, `FindPart`, `PartInfo::create` (parts) for Datasheet checks and pin names; no pin tables duplicated.
- `LoadedCircuit::parts`/`components`/`connections` and `FindPin` for references, pin numbers and pinout Nets.
- `BoardScene` pad Nets, `netName`, `courtyard`, `hoverText`; `Covers`, `ArcPolyline` (board) for picking.
- `Viewport::zoomAt`/`pan`/`fit`; `Session::touch`/`repair` return values (`Refused`); `Machine::probeNet` unchanged.
- `--screenshot` path from P2f for visual checks.

## 8. Build Integration

```cmake
# src/manual/CMakeLists.txt
add_library(bw_manual STATIC markdown.cpp manual.cpp)
target_link_libraries(bw_manual PUBLIC bw_netlist PRIVATE bw_warnings)  # bw_parts via bw_netlist
```

- Top-level `CMakeLists.txt`: `add_subdirectory(src/manual)` before `src/ui`.
- `src/ui/CMakeLists.txt`: `bw_ui` sources `+ toolbar.cpp side_panel.cpp chip_panel.cpp manual_view.cpp`; link `bw_manual`.
- `tests/CMakeLists.txt`: `manual/markdown_test.cpp manual/manual_test.cpp machines/manuals_test.cpp`; link `bw_manual`; define `BW_ROOT_DIR="${PROJECT_SOURCE_DIR}"`.

## 9. Dependency Diagram

```
bw_ui ──► bw_session ──► bw_machine ──► bw_board ──► bw_netlist ──► bw_parts ──► bw_sim
  ├────► bw_view ───────────────────────► bw_board        ▲
  └────► bw_manual ───────────────────────────────────────┘
```

No cycles: `bw_manual` knows neither the Board nor the UI; `bw_view` knows no manuals.

## 10. Testing

| File | Covers |
|---|---|
| `tests/machine/puzzle_test.cpp` | `title`/`brief` parse; `#` inside quotes; empty value; duplicate; missing closing quote; text after the quote; absent → empty |
| `tests/machine/machine_test.cpp` | `MachineRoot`; `ReadPuzzleFile` + `LoadMachine` equals `LoadPuzzleFile` |
| `tests/manual/markdown_test.cpp` | Title/Heading/Paragraph/Bullet, continuation lines, code and bold spans, unmatched markers literal, line numbers, `Section` |
| `tests/manual/manual_test.cpp` | `BulletPins` ranges (`1A`–`1D`, `QA`–`QD`, `1A`–`4A`, non-range), `PinBullet`, `ComponentRole`, `LinkAt` (`` `U1` pin 15 ``, non-reference code), each check failing on a crafted bad document, `LoadManuals` with missing files |
| `tests/machines/manuals_test.cpp` `[machines]` | committed User/Service Manuals of `counter` and `test_pattern` and every catalogue Part's Datasheet pass their checks; the three Puzzles have title and brief |
| `tests/view/board_scene_test.cpp` | `pick` replaces `hitTest` cases; S10 snapping at fitted zoom and 100 px/mm; B.Cu-only spot with F.Cu active → B.Cu Trace with `layer == Back`; contact lies on the picked copper (`Covers`); `componentIndex`/`padIndex`/`pinName`/`powerPin` |
| `tests/view/viewport_test.cpp` | `frame`: box centred, zoom clamped to [fitted, max] |
| `tests/machines/board_probe_test.cpp` | probing at a pick's contact 8 px beside a Trace reads that Trace's Net |
| Manual | `--screenshot` of each Puzzle (fitted, `--probe`, `--manual user`, `--manual service`) viewed by the agent; release ≈ 1.00× at `1×`, `ui` < 16 ms fitted and at 100 px/mm |

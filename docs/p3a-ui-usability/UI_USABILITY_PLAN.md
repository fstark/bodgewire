# UI Usability — Implementation Plan (P3a)

Design: [UI_USABILITY_DESIGN.md](UI_USABILITY_DESIGN.md)
Spec: [UI_USABILITY.md](UI_USABILITY.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Puzzle `title`/`brief`, `ReadPuzzleFile`, `MachineRoot`; window title | |
| 2 | View: `BoardScene::pick` (snapping, Layer fall-through), accessors, `Viewport::frame` | |
| 3 | `BoardView`: pick-driven hover/click, drag-pan, double-click fit, keys, selection, focus, labels | |
| 4 | Layout: banner + toolbar, sectioned right column, Debug section | |
| 5 | User check A: usability half | |
| 6 | `bw_manual`: Markdown subset, checks, lookups, `LoadManuals` | |
| 7 | Content: Datasheets for every Part, User and Service Manuals for both Machines | |
| 8 | UI: Chip panel, manual windows, links, tooltips, `--manual` | |
| 9 | Release, Done-when | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug -j 10`
Last phase also: `cmake --preset release && cmake --build --preset release && ctest --preset release -j 10`

All C++ is `clang-format` clean (never run it on CMakeLists.txt). Visual checks: `build/<preset>/src/ui/bw_ui FILE [options] --screenshot /tmp/x.png --after 2s`, then view the PNG (release build for timing-sensitive views).

---

## Phase 1 — Puzzle title and brief

Smallest self-contained change; gives the Brief its data before any UI work.

### 1.1 — Parser (design §4.1, §6.1)

`src/machine/puzzle.hpp`: add to `Puzzle`

```cpp
std::string title, brief;  // "" when absent
std::uint32_t titleLine = 0, briefLine = 0;
```

`src/machine/puzzle.cpp`: in `Parser::line`, before stripping `#`, take the first token of the raw line; if it is `title` or `brief`, call a new file-local

```cpp
// `"TEXT"` then only spaces or a # comment; the error message otherwise.
std::expected<std::string, std::string> QuotedValue(std::string_view rest);
```

on the text after the keyword, and store it with its line. Errors: `expected: title "TEXT"` (resp. `brief`), `title already given at line N`.

### 1.2 — Machine helpers (design §4.2)

`src/machine/machine.hpp/.cpp`:

```cpp
std::filesystem::path MachineRoot(const std::filesystem::path& file);  // absolute(file).parent_path().parent_path()
std::expected<Puzzle, std::vector<Diagnostic>> ReadPuzzleFile(const std::filesystem::path& path);
```

`LoadPuzzleFile` = `ReadPuzzleFile` + `LoadMachine(puzzle, MachineRoot(path))`. `HasBoard`, `LoadNetlistMachine` use `MachineRoot`.

### 1.3 — Puzzles and `bw_ui` title

- `puzzles/*.bwp`: add `title` and `brief` from the Symptom, never the Fault (check each with `build/debug/src/cli/bw run puzzles/X.bwp --tv /tmp/X.bmp` if unsure). First drafts:
  - `broken_trace.bwp`: `title "Missing grid lines"`, `brief "The vertical lines of the test grid are gone; the horizontal ones are fine."`
  - `dead_counter.bwp`: `title "Rolling picture"`, `brief "The picture will not hold still and the bottom of the frame is never reached."`
  - `stuck_gate.bwp`: `title "White screen"`, `brief "The whole picture area is white; no grid at all."`
- `src/ui/main.cpp` `Load`: for `.bwp`, `ReadPuzzleFile` then `LoadMachine(puzzle, MachineRoot(file))`; `Loaded` gains `std::string title, brief` (title = Puzzle title, else Machine name, else netlist stem). Window title uses `title`.

### 1.4 — Tests

- `tests/machine/puzzle_test.cpp`: `"puzzle: title and brief"` (values, line numbers, `#` inside quotes, empty value), `"puzzle: bad title or brief"` (missing quote, no quote, text after the closing quote, duplicate), absent → empty.
- `tests/machine/machine_test.cpp`: `"machine: MachineRoot"`; `"machine: ReadPuzzleFile + LoadMachine"` loads `dead_counter.bwp` with the same name and Stuck-at as `LoadPuzzleFile`.
- `tests/machines/test_pattern_faults_test.cpp` (or a new case there): the three Puzzles have non-empty title and brief.

### Fence

- [ ] `.bwp` `title`/`brief` parsed; three Puzzles have them; `bw run` unchanged
- [ ] `bw_ui puzzles/dead_counter.bwp` window title `Bodgewire — Rolling picture`
- [ ] Debug build clean, tests pass
- [ ] Commit: `"ui usability: phase 1 — puzzle title and brief"`

---

## Phase 2 — Picking and framing (headless)

All the S8 rules in `bw_view`, tested before the UI uses them.

### 2.1 — `BoardScene` data (design §4.3)

- `ComponentInfo` gains `const PartInfo* part`, `std::vector<bool> powerPins`, `std::int64_t halfPitch` (half the smallest centre distance between two pads of the Component; 0 with one pad). Fill them in the constructor (`LoadedPart::info`; `pins()[n].kind == PinKind::Power`).
- Accessors: `partName`, `pinName`, `powerPin`, `componentIndex(reference)`, `padIndex(component, pin)` (pad number, else pin name via `info_.pinNames`).

### 2.2 — `pick` (design §6.4)

```cpp
struct PickReach { std::int64_t pad = 0, copper = 0; };  // nm
struct Pick { Hit hit; Layer layer; Point contact; };
std::optional<Pick> pick(Point at, Layer active, PickReach reach) const;
```

File-local helpers in `board_scene.cpp`: `NearestOnSegment(Point a, Point b, Point at) -> Point`, `Distance(Point, Point) -> double`, and `TraceEdgeDistance(const Trace&, Point at, Point& nearest)` → return a `struct { double edge; Point nearest; }` instead of an output parameter (arcs: over `ArcPolyline` chords). Remove `hitTest`; update its uses in `board_scene_test.cpp`. `BoardView::input` calls `pick(at, active_, {})` for now (same behaviour except Layer fall-through).

### 2.3 — `Viewport::frame` (design §4.4)

`void frame(Bounds box, double share, double minPixelsPerMm);` centres `box`, zoom = `max(share × min(panel.x / width, panel.y / height), minPixelsPerMm / 1e6)` clamped to `[minNmToPixel_, kMaxPixelsPerMm / 1e6]`, `fitted_ = false` unless equal to the fitted scale. Degenerate box (zero width/height): keep the zoom (still at least `minPixelsPerMm`), centre only.

### 2.4 — Tests

- `tests/view/board_scene_test.cpp`: rewrite `"view: hit test"` on `pick(..., {})`; the front-Trace midpoint with `Layer::Back` active now picks that Trace with `layer == Front`. New `"view: pick snapping"` at the fitted scale and at 100 px/mm (reach = px / pixelsPerMm × 1e6): 0.5 mm off a pad over a Trace → pad; midway-ish between two pads → the nearer; 8 px beside a 0.2 mm Trace on bare board → that Trace, `Covers(trace, contact)`; B.Cu-only spot with F.Cu active → B.Cu Trace, `layer == Back`; off the Board → nullopt. `"view: scene accessors"`: `componentIndex("U6")`, `padIndex(u1, "QA") == padIndex(u1, "14")`, `pinName`, `powerPin` on VCC/GND, `partName(R1) == "R"` on the counter.
- `tests/view/viewport_test.cpp`: `"viewport: frame"` centres a box, honours `minPixelsPerMm`, clamps to fitted and to 100 px/mm.

### Fence

- [ ] `hitTest` gone; `pick`, accessors, `frame` implemented and tested
- [ ] `bw_ui` behaves as before (plus fall-through)
- [ ] Debug build clean, tests pass
- [ ] Commit: `"ui usability: phase 2 — picking and framing"`

---

## Phase 3 — Board panel interaction

### 3.1 — Pick-driven hover and click (design §4.5, §6.7)

- `BoardView::input`: reach = `{12 px, 10 px}` converted with `viewport_.pixelsPerMm()`; hover highlight and tooltip from `pick`. Hovered Component = the pick's Component (Pad/Component hit) or `componentAt(at)`.
- `BoardClick` becomes `{Point at; Layer active; std::optional<Pick> pick; std::optional<std::size_t> component;}`; emitted on left **release** when the press did not turn into a drag.
- `src/ui/main.cpp` routes clicks per §6.7: select the Component; Probe touches `ProbeTip{contact, pick.layer}` only for copper picks; Swap/Mend as now, Mend on a Trace pick uses `(pick.layer, contact)`.

### 3.2 — Navigation

- Left-drag past `ImGui::GetIO().MouseDragThreshold` pans (all tools); right/middle drag unchanged.
- Double-click where `pick(...)` is nullopt → `fit()`.
- `+`/`=` and `-` (also keypad) → `viewport_.zoomAt(panel centre, ±1)`.
- `Esc` → `takeEscape()` true once; main calls `session.touch(std::nullopt)`.
- Public `setTool`, `activeLayer`, `toggleLayer`, `fit` (Phase 4's toolbar uses them; keys P/S/M/L/F stay in `input`).

### 3.3 — Selection, focus, outside hover

- Members `selected_`, `hovered_`, `outside_`, `flash_` (target + `steady_clock` deadline); `chip()`, `select()`, `focus()` (§6.6), `hoverFromOutside()` (highlights the target's pad Net, or the whole Component outline, at the next draw only: panels draw after the Board, one frame of lag), `route(TargetHits)` (hovered → `hoverFromOutside`, clicked → `focus`). `TargetHits` lives in `board_view.hpp`.
- Flash: a white ring on the pad (or every pad of the Component), alpha pulsing over 1 s.

### 3.4 — Labels (design §6.5)

- Part name under the reference at 0.6× when it fits the courtyard width.
- Pin names for `chip()` only, pad ≥ 10 px, on a dark plate (`IM_COL32(0, 0, 0, 200)`).

### 3.5 — Checks

- Screenshots (debug is fine): `bw_ui puzzles/broken_trace.bwp --probe U1.QA`; the probe tip sits on the pad. No headless additions beyond Phase 2 (drawing only).

### Fence

- [ ] Hover/click snap; drag never acts; double-click on free board fits; Esc lifts the probe
- [ ] Screenshots show part names; tip on pad
- [ ] Debug build clean, tests pass
- [ ] Commit: `"ui usability: phase 3 — board panel interaction"`

---

## Phase 4 — Layout

### 4.1 — `Toolbar` (design §5, §6.8)

`src/ui/toolbar.hpp/.cpp`: `class Toolbar` with `draw(Session&, BoardView&, Speed&, std::uint64_t seed)`, `refused()`, `userManualOpen`, `serviceManualOpen`. Row 1: banner (power, tool action, amber when blocked or flashing 0.4 s). Row 2: Power, Pause/Resume, `1×`/`Max`, tool buttons `Probe (P)` `Swap (S)` `Mend (M)` (large, selected one highlighted), `F.Cu`/`B.Cu (L)`, `Fit (F)`. Manual buttons are added in Phase 8. Main calls `toolbar.refused()` when `session.repair` returns `RepairResult::Refused`.

### 4.2 — `SidePanel`

`src/ui/side_panel.hpp/.cpp`: `class SidePanel` with `draw(Session&, TvView&, BoardView&, const Loaded&, DebugStats, std::uint64_t& seed)` — a seed edit field is ImGui's in-place idiom; keep `seed` owned by main. `Loaded` moves to a small header `src/ui/loaded.hpp` so both see it. Sections (`ImGui::CollapsingHeader`, open by default except Debug):
- `Puzzle`: title (larger), Brief wrapped.
- `TV`: as now (`no video` when absent).
- `Probe`: lamps + `U1 pin 13 QA · H0` (`BoardScene::hoverText` of the Pick at the tip; `not probing` when none).
- `Chip`: for `board.chip()`: `U6 · 74LS20 · dual 4-input NAND` (catalogue description); filled out in Phase 8.
- `Repairs`: log.
- `Debug` (closed): Seed, `t`, ratio, lock, state, ring wait, ui ms.

### 4.3 — `main.cpp`

Frame loop: `toolbar.draw` (full width), then `boardView.draw` (left, `kBoardShare`), then `sidePanel.draw` (right). Remove `DrawLamps`/status code moved into `SidePanel`. Add `side_panel.cpp toolbar.cpp` to `src/ui/CMakeLists.txt`.

### 4.4 — Checks

Screenshots of the three Puzzles and `netlists/counter.bwn` (no video), with `--probe U1.QA` and `--repair swap:U3` (banner shows power state). The agent reviews layout and text.

### Fence

- [ ] Banner, toolbar, sections, Debug collapsed; `power off first` amber with Swap/Mend while powered
- [ ] Screenshots reviewed
- [ ] Debug build clean, tests pass
- [ ] Commit: `"ui usability: phase 4 — layout"`

---

## Phase 5 — User check A

Release build. The user plays the three Puzzles with the new navigation, snapping and layout. Record tuning wishes here (reach in px, label size, banner wording) and fix small ones in this phase; larger ones become notes for Phase 9. Fence: user confirms; commit only if something changed: `"ui usability: phase 5 — tuning after user check"`.

---

## Phase 6 — `bw_manual` library

### 6.1 — Markdown (design §2, §6.2)

`src/manual/markdown.hpp/.cpp`: `SpanStyle`, `Span`, `BlockKind`, `Block`, `Markdown`, `ParseMarkdown`, `PlainText`, `Title`, `Section`.

### 6.2 — Manuals (design §3, §6.3)

`src/manual/manual.hpp/.cpp`: `Manuals`, `LoadedManuals`, `LoadManuals`, `CheckUserManual`, `CheckServiceManual`, `CheckDatasheet`, `PartPinNames` (scratch `Circuit`, `freq=1MHz` for `OSC`), `Role`/`ComponentRole`, `BulletPins`, `PinBullet`, `ManualLink`/`LinkAt`. Diagnostics: `Severity::Warning`, file `manuals/<machine>/user.md` etc., block line.

### 6.3 — Build

`src/manual/CMakeLists.txt` (design §8); top-level `add_subdirectory(src/manual)` after `src/netlist`; `tests/CMakeLists.txt` adds the two test files, links `bw_manual`, defines `BW_ROOT_DIR`.

### 6.4 — Tests

- `tests/manual/markdown_test.cpp`: `"markdown: blocks"`, `"markdown: bullet continuation"`, `"markdown: inline spans"`, `"markdown: unmatched markers"`, `"markdown: section"`.
- `tests/manual/manual_test.cpp`: `"manual: bullet pins and ranges"` (`1A`–`1D`, `QA`–`QD`, `1A`–`4A`, `-` too, `CLK`/`CLR_N` list, mismatched pair not expanded), `"manual: pin bullet"`, `"manual: component role"`, `"manual: links"` (on the counter design: `` `U1` pin 15 (RCO) `` → pin "15", `pinChars` = length of `" pin 15"`, rest kept as text; `` `QA` `` → none), `"manual: part pin names"` (74LS161 has `CLR_N`, R empty), one failing case per check message (including a bad reference in a User Manual), `"manual: load with missing files"` (temp dir: no problems, nothing loaded).

### Fence

- [ ] `bw_manual` built and tested; nothing in the UI yet
- [ ] Debug build clean, tests pass
- [ ] Commit: `"ui usability: phase 6 — manual library"`

---

## Phase 7 — Content

### 7.1 — Datasheets

`datasheets/<Part>.md` for every `AllParts()` entry (14): Title `# 74LS20 — dual 4-input NAND`, `## Function` (short; truth table as bullets or a counting rule), `## Pins` covering every pin name. Facts from the Part models in `src/parts` (behaviour as simulated, not from memory of TI sheets).

### 7.2 — Manuals

`manuals/counter/{user,service}.md`, `manuals/test_pattern/{user,service}.md`. Figures (frequencies, line/frame periods, which counter bit drives what) from the netlist comments, `docs/p1c-test-pattern/TEST_PATTERN.md`, and fault-free runs: `bw run netlists/test_pattern.bwn --time 40ms --probe U1.RCO` etc. Troubleshooting: 4–8 checks per Machine, from the clock outward, none naming a Puzzle's Fault.

### 7.3 — Tests

`tests/machines/manuals_test.cpp` `[machines]`: every catalogue Part has a Datasheet passing `CheckDatasheet`; both Machines' manuals pass `CheckUserManual`/`CheckServiceManual` against their design; `LoadManuals` on both reports no problems.

### Fence

- [ ] 14 Datasheets, 4 manuals; checks pass
- [ ] User reviews the content (wording, level of detail); fixes applied
- [ ] Debug build clean, tests pass
- [ ] Commit: `"ui usability: phase 7 — datasheets and manuals"`

---

## Phase 8 — Manuals in the UI

### 8.1 — `ManualView` (design §5)

`src/ui/manual_view.hpp/.cpp`: draws `std::span<const Block>`: Title/Heading as collapsing headers, Paragraph/Bullet wrapped; Code spans in a distinct colour; `LinkAt` spans underlined, hover → `TargetHits::hovered`, click → `TargetHits::clicked` (caller passes them to `BoardView::route`); signature as design §5.

`ManualWindows` (same file): windows `User Manual` / `Service Manual` (floating, 480 × 600 first use), toggled by toolbar buttons and `U`/`H`; `Troubleshooting` header collapsed, labelled `Troubleshooting (easy mode)`; `no user manual` / `no service manual` when absent.

### 8.2 — `ChipPanel` (design §5, S6)

`src/ui/chip_panel.hpp/.cpp`: returns `TargetHits` (SidePanel calls `board.route`); header line; role (`ComponentRole`); `Zoom to` button → `clicked {component}`; pinout: DIP (package starts with `DIP`) two columns, pins 1..n/2 down the left, n/2+1..n up the right, else one column; each row `number name net`, power greyed; hovered row → `PinBullet` line in a tooltip and `hovered`; clicked row → `clicked`; Datasheet `Function` (collapsing) and `Pins`.

### 8.3 — Wiring

- `Loaded` gains `LoadedManuals manuals` (problems printed as warnings).
- `SidePanel` `Puzzle` section adds the User Manual's `What it does`.
- Tooltip callback (`BoardView::setTooltip`): Component → `hoverText` + ` · ` + role name; Pad → `hoverText` + ` — ` + `PlainText` of `PinBullet` after its `:`.
- `--manual user|service` opens that window at start (usage string updated).
- `src/ui/CMakeLists.txt`: `chip_panel.cpp manual_view.cpp`, link `bw_manual`.

### 8.4 — Checks

Screenshots: `test_pattern` Puzzle with `--manual user`, `--manual service`, and `--probe U6.1Y` (Chip panel shows U6); `netlists/counter.bwn`.

### Fence

- [ ] Manual windows, links, Chip panel with pinout and Datasheet, enriched tooltips
- [ ] Screenshots reviewed
- [ ] Debug build clean, tests pass
- [ ] Commit: `"ui usability: phase 8 — manuals in the UI"`

---

## Phase 9 — Release, Done-when

- Release build and tests.
- `bw_ui puzzles/broken_trace.bwp --probe U1.CLK --screenshot /tmp/bw-clk.png --after 3s`: ≈ 1.00×; Debug `ui` < 16 ms fitted and at 100 px/mm (zoom with `+` before the screenshot is not scriptable: check by hand in the user play).
- User check (spec Done-when): play the three Puzzles; knows what the Board should do and what each chip is for, sees the state, hits pins and Traces without hunting. Record tuning wishes here.
- [ARCHITECTURE.md](../ARCHITECTURE.md) roadmap: P3a row done.

### Fence

- [ ] ≈ 1.00× at `1×`; `ui` < 16 ms
- [ ] Debug and release tests pass
- [ ] User confirms the Done-when
- [ ] Commit: `"ui usability: phase 9 — release check"`

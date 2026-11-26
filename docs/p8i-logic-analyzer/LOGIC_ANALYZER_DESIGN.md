# P8i — The Logic Analyzer redesigned — Design

How [LOGIC_ANALYZER.md](LOGIC_ANALYZER.md) is built. Plan: [LOGIC_ANALYZER_PLAN.md](LOGIC_ANALYZER_PLAN.md). Numbers in parentheses (§3) are the items of the spec's "What the player sees".

The sampling and the Trigger (`AnalyzerState`), the consumer and the runner, the Oscilloscope's behaviour and the Machines are untouched. The pieces of the scope's drawn case that both Equipment need move to shared files (`view/case_strip`, `ui/case_keys`), the scope rebuilt on them unchanged. New: the analyzer's case layout (`view/analyzer_layout`), its rows' order and its arming in `AnalyzerSetup`. The analyzer's window is rewritten as a drawn case with no child window; the helpers only its old window used go.

## Files

| File | Change |
|---|---|
| `src/view/case_strip.{hpp,cpp}` (new) | `Box` and `BandAt` (from `scope_layout`), the strip's sizes, `LayOutStepper`, `Badge`, `LayOutBadge`, `SizeKeyBox` |
| `src/view/scope_layout.{hpp,cpp}` | built on `case_strip`; `kScopeStrip`, `kScopeClose` become `kCaseStrip`, `kCaseClose` |
| `src/view/analyzer_layout.{hpp,cpp}` (new) | `AnalyzerCaseLayout`, `LayOutAnalyzerCase`, `AnalyzerRowHeight`, the sizes |
| `src/view/time_view.{hpp,cpp}` | `kAxisHeight` (from `instrument_marks.hpp`); `UnitRow`, `UnitRows` replace `TimebaseRow`, `TimebaseRows`; `StatusText`, `OffScreenTrigger` go |
| `src/view/bench_places.cpp` | the analyzer's minimal size 880 × 340 |
| `src/ui/case_keys.{hpp,cpp}` (new) | from `scope_case`: keys, LEDs, lamps, rockers; new: the frame, a group's panel and label, the badge, the bezel, `TimeGrid` |
| `src/ui/scope_case.{hpp,cpp}` | `DrawScopeCase` only, built on `case_keys` |
| `src/ui/scope_window.cpp` | `TimeGrid`, `DrawCursorTag`, the new `DrawRocker` and `DrawLamp`; `DrawAxisLabels` without `arrow` |
| `src/ui/instrument_marks.{hpp,cpp}` | `DrawCursorTag` (from the scope); `DrawAxisLabels` loses `arrow`; `BeginInstrument`, `StatusLine`, `DrawOffScreenTrigger`, `OffScreenText`, `OffScreenWidth` go |
| `src/ui/controls.{hpp,cpp}` | `ChoicePopup` becomes `ChoicesPopup` (the popup only, for a drawn key) |
| `src/ui/analyzer_window.{hpp,cpp}` | rewritten |
| `src/ui/board_view.hpp`, `board_tags.cpp`, `board_view.cpp` | `RowColour`; a Clip's disc numbered and coloured by its row |
| `src/ui/shelf.{hpp,cpp}`, `src/ui/main.cpp` | `Resume`; `ARM` while paused resumes the run |
| `src/session/analyzer_setup.{hpp,cpp}` | `AnalyzerRow`, `rows()`, `move()`, `label()`, `inCapture()`, `poweringOn()`; a new Clip arms; `disarm()` goes |
| `src/session/session.cpp` | `Session::powerOn` calls `analyzer_.poweringOn()` |
| `src/ui/CMakeLists.txt`, `src/view/CMakeLists.txt`, `tests/CMakeLists.txt` | the new files |
| tests | `tests/view/analyzer_layout_test.cpp` (new), `time_view_test.cpp`, `scope_layout_test.cpp`, `session_test.cpp` |
| `AGENTS.md` | the shared helpers' list: `ChoicesPopup`, `case_strip`, `case_keys` |

## 1. The shared case (§1, §2)

### `src/view/case_strip.hpp` (new)

```cpp
struct Box { ... };  // moved as is from scope_layout.hpp
// The box whose centre is nearest `y` (a drop's target); `boxes` not empty. Moved as is.
std::size_t BandAt(std::span<const Box> boxes, float y);

// A drawn case's strip of keys (P8h, P8i), px from the window's top left.
inline constexpr float kCaseStrip = 72;    // the strip's height
inline constexpr float kCaseClose = 40;    // CaseClose's corner, px square
inline constexpr float kStripMargin = 12;  // the first group from the left edge
inline constexpr float kStripPad = 6;      // a group's panel round its keys
inline constexpr float kStripGap = 8;      // between groups
inline constexpr float kStripKeyGap = 2;   // between a key and its LED
inline constexpr float kStripKey = 36, kStripKeyHigh = 44;  // kBig high, for a finger
inline constexpr float kStripKeysTop = 22;  // under the groups' labels
inline constexpr float kGroupTop = 6, kGroupBottom = 70;
inline constexpr float kSizeKey = 36;

float LayOutStepper(float x, float led, Box& less, Box& value, Box& more);  // ◀ value ▶; its end
struct Badge {  // the logo, 32 px high, and BODGE over WIRE beside it when `hasWords`
    Box logo, words;
    bool hasWords = false;
};
float LayOutBadge(float x, bool words, Badge& badge);  // from `x`; its end
Box SizeKeyBox(float width);  // the size key, left of CaseClose's corner
```

`scope_layout.cpp`'s anonymous constants and `Stepper` are these; `ScopeCaseLayout` keeps its fields (`logo`, `words`, `hasWords` filled from a `Badge`). The `scope layout:` cases pass with the renamed constants.

### `src/ui/case_keys.hpp` (new)

From `scope_case.hpp`, unchanged: `DrawKey`, `DrawArrowKey`, `DrawSizeKey`, `DrawLed`, `kLedFrame`, `kLedFrameHovered`, `kLedText`, `kScopeCase` (renamed `kCasePanel`). Changed or new:

```cpp
void DrawCaseFrame(ImDrawList& list, ImVec2 at, float width, float height);  // frame + panel
void DrawGroup(ImDrawList& list, ImVec2 at, const Box& group);               // tinted panel
void DrawGroupLabel(ImDrawList& list, ImVec2 at, float x0, float x1, float y, const char* text);
void DrawBadge(ImDrawList& list, ImVec2 at, const Badge& badge);
void DrawBezel(ImDrawList& list, ImVec2 at, const Box& bezel);
void DrawLamp(ImDrawList& list, ImVec2 at, const Box& lamp, bool lit, const char* name);
// A rocker of 2 or 3 halves side by side, the `lit` one raised.
void DrawRocker(ImDrawList& list, ImVec2 at, std::span<const Box> halves,
                std::span<const char* const> names, std::size_t lit);
// The grid of a drawn LED's values by unit (P8h's timebase grid), once ImGui::OpenPopup(id) was
// called; under `below`; the value clicked, once.
std::optional<Time> TimeGrid(const char* id, std::span<const Time> values, Time current,
                             ImVec2 below);
```

C++23 has no `std::span` from a braced list: the scope calls `DrawRocker(list, at, std::array{layout.autoHalf, layout.normHalf}, kModeNames, normal ? 1 : 0)` with `constexpr std::array<const char*, 2> kModeNames{"AUTO", "NORM"}`. Its lamp: `DrawLamp(…, "TRIG'D")`; its grid: `TimeGrid("##timebase grid", kTimebases, settings.timebase, …)`. `DrawScopeCase` stays in `scope_case`, made of these.

### `src/view/time_view.hpp`

```cpp
inline constexpr float kAxisHeight = 18.0f;  // moved from ui/instrument_marks.hpp: the layouts
// `values` (ps, shortest first) by unit, for a drawn LED's grid (P8h, P8i): the ns, µs, ms and
// s rows that have values, each its indices into `values`.
struct UnitRow {
    const char* unit;
    std::vector<std::size_t> indices;
};
std::vector<UnitRow> UnitRows(std::span<const Time> values);
```

`TimebaseRow` and `TimebaseRows` go; `time view: the scope's timebase rows` becomes `time view: unit rows` over `kTimebases` (ns 5–500, µs 1–500, ms 1–10, as now) and over the analyzer's periods shortest first (ns 10–500, µs 1–500, ms 1). `kAnalyzerPeriods` runs longest first: the analyzer passes a reversed copy (`static const std::array`).

### `src/ui/instrument_marks.hpp`

```cpp
// The cursor's tag (P8h, P8i): `text` on a light plate right of `x`, near `top`, or left of it
// when it would pass `right`.
void DrawCursorTag(ImDrawList& list, float x, float top, float right, const std::string& text);
void DrawAxisLabels(ImDrawList& list, const TimeView& view, const std::vector<GridLine>& grid,
                    const TimeToX& x, float y, float left, float right);  // `arrow` gone
```

`DrawCursorTag` is the scope's tag code. With the old analyzer window go its only users' helpers: `BeginInstrument`, `StatusLine`, `DrawOffScreenTrigger`, `OffScreenText`, `OffScreenWidth`, `DrawAxisLabels`' `arrow` (the scope passes `false`), `StatusText` and `OffScreenTrigger` in `time_view` and their `time view:` cases. `NextInstrumentSize` and `DrawSizeIcon` stay.

### `src/ui/controls.hpp`

```cpp
// The choices of a drawn key as a strip of kBig buttons, once ImGui::OpenPopup(id) was called
// (P8f's ChoicePopup without its button). True when `index` changed.
bool ChoicesPopup(const char* id, std::span<const char* const> labels, std::size_t& index,
                  float width);
```

`ChoicePopup`'s only user was the old window's Trigger column; AGENTS.md's list names `ChoicesPopup` instead.

## 2. The rows and the arming (§4, §6, §7, §8)

### `src/session/analyzer_setup.hpp`

```cpp
// One row of the analyzer's screen (P8i): a Channel Group, or a Clip in none.
struct AnalyzerRow {
    std::optional<std::size_t> group;  // index into groups()
    std::size_t channel = 0;           // a loose Clip's Channel; a group's first in the order
};

// The rows top to bottom: each clipped Channel in the order clipped (a Clip goes at the bottom,
// an unclip closes its gap), a group's row where its first member is; kept across rebuilds.
std::vector<AnalyzerRow> rows() const;
// The row at place `from` moved to place `to` (clamped), the others closing up; a group's
// members move together.
void move(std::size_t from, std::size_t to);
const std::string& label(std::size_t channel) const;  // the Clip's label; "" if free
// Whether `capture` recorded these Channels' present Clips: each one's label then the same.
bool inCapture(const Capture& capture, std::span<const std::size_t> channels) const;
// A power-on (Session::powerOn, before the runner's): arms, when something is clipped (§7).
void poweringOn();
```

`order_` (`std::vector<std::size_t>` of clipped Channels) beside `clips_`: `clip()` appends the Channel it takes, `unclip()` and `unclipGroup()` erase theirs, `clipGroup()` appends the members it clips (those already clipped keep their place), `group()` and `ungroup()` leave it. `rows()` walks `order_`, a grouped Channel giving its group's row the first time the group is met. `move()` takes the row's Channels out of `order_` as a block and puts them back before the Channels of the row now at `to`, or at the end.

**Arming** (the user, 2026-11-26): `clip()` and `clipGroup()` arm whenever they clip a pin (`++config_.arms; config_.armed = true;` before `configure()`), a new Clip after a Capture too. `poweringOn()` does the same when `order_` is not empty; `Session::powerOn` calls it after the rebuild and before `runner->powerOn`, so `AnalyzerConsumer::powerOn` adopts it (`analyzer_consumer.cpp`: a new `arms` re-arms at time 0) with or without a rebuild. `beforeRebuild()` stays. `disarm()` goes with its only caller, the old Stop.

### `src/ui/board_view.hpp`, `board_tags.cpp`, `board_view.cpp`

```cpp
// A row's colour on the analyzer and on the Board (P8i): by its place, eight colours repeating.
inline ImU32 RowColour(std::size_t place) { return kChannelColours[place % 8]; }
struct ClipTag { Point at; std::size_t place; ImU32 colour; };  // was `channel`
```

`ClipTagsOf` numbers each Clip by its row's place (`place + 1` on the disc; a group's members all the group's) and colours it `RowColour(place)`; `board_view.cpp` draws `tag.place + 1` and `tag.colour`. `ChannelColour` and `GroupColour` go once unused.

### Tests (`tests/session/session_test.cpp`, `[session]`)

- `session: the analyzer's rows in clip order`: three Clips, a group clipped from a pin's stem, an unclip from the middle, `move` up, down, clamped; the order kept across a repair (a rebuild).
- `session: a new Clip arms`: a Capture completed (`Captured`), another pin clipped: `Armed`.
- `session: a Channel reused is not in the Capture`: two Clips, a Capture; the second unclipped, another pin clipped on its Channel: `inCapture` false for it, true for the first (the bug of §8).
- `session: a power-on re-arms after a Capture` (replaces `a capture is not re-armed by a repair`): a Capture, power off, a repair, power on: `Armed`, then `Captured` again.
- `session: a power-on with no Clip does not arm`: `Idle`.

## 3. The case's layout (§1, §2, §3)

### `src/view/analyzer_layout.hpp` (new)

```cpp
// The Logic Analyzer's case (P8i), px from the window's top left: the strip (CAPTURE, SAMPLE,
// TRIGGER AT, VIEW / DIV), the screen in its bezel under it; the screen's corner above the plates
// holds PIN | BUS, GROUP and the count of Clips, the overview and the time axis are above the
// lanes, the rows under them.
struct AnalyzerCaseLayout {
    Badge badge;
    Box captureGroup, armKey;
    std::array<Box, 3> lamps;  // ARMED, TRIG'D, DONE
    Box sampleGroup, sampleLess, sampleValue, sampleMore;
    Box triggerAtGroup;
    std::array<Box, 3> triggerAt;  // EARLY, MID, LATE
    Box viewGroup, viewLess, viewValue, viewMore, fitKey;
    Box sizeKey;
    Box bezel, screen;
    Box pinHalf, busHalf, groupKey, count;  // the corner above the plates
    Box overview, axis;                     // above the lanes
    Box rows;                               // plates and lanes, to the screen's bottom
    float laneX0 = 0;                       // the lanes' left: screen.x0 + kAnalyzerPlate
};
inline constexpr float kAnalyzerMinWidth = 880, kAnalyzerMinHeight = 340;
inline constexpr float kAnalyzerPlate = 248;
inline constexpr float kRowMin = 28, kRowMax = 56, kRowGap = 2;
AnalyzerCaseLayout LayOutAnalyzerCase(float width, float height);
// A row's height for `count` rows in `room` px: the room shared, from kRowMin to kRowMax.
float AnalyzerRowHeight(float room, std::size_t count);
```

The strip from the left, with `case_strip`'s sizes: the badge; CAPTURE (pad, `ARM` 48 × 44, gap, three lamps 22 px every 40, pad); SAMPLE (a stepper, LED 64); TRIGGER AT (a rocker of three 44 px halves); VIEW / DIV (a stepper, LED 64, gap, `FIT` 44 × 44); the size key; `CaseClose`'s corner. The words when the strip fits with them (from 927 px, the scope's rule: the last group's end and a gap left of the size key; the normal 940, not the minimal 880, which needs about 869). The bezel from 80 px to 12 px above the bottom, 14 in from the sides; the screen 8 px inside. The corner, 34 px high: `PIN` and `BUS` halves 40 × 28 from 6 px in and 3 px down, `GROUP` 72 × 28 after 8 px (`UNGROUP` fits), the count right-aligned to `laneX0 - 8`. The overview 10 px high from 4 px down, over the lanes; the axis `kAxisHeight` under it, to 34 px; the rows from 34 px down to the screen's bottom.

Below 880 px (a bench narrower than that, the upright tablet's 820): the window is the bench's width (the constraint clamps), the strip runs on under its right edge, `FIT` and the size key out of reach. Not handled: the analyzer is used on the bench in landscape.

### Tests (`tests/view/analyzer_layout_test.cpp`, `analyzer layout: …`, `[view]`)

- `the strip at the minimal size`: at 880 × 340 no two of the strip's boxes overlap, all lie within `kCaseStrip`, the size key ends left of `width - kCaseClose`, no words.
- `the words at the normal size`: at 940 × 520 the words, the strip still clear of the size key.
- `the screen's corner and rows`: the corner's boxes at least 28 px high, left of `laneX0` and above `rows.y0`; `overview` and `axis` right of it; `rows` reaching `screen.y1`.
- `row heights`: one row in 300 px: 56; five in 300: 56 (capped); twenty in 300: 28 (scrolled).

`bench_places.cpp`: `MinimalSize(Analyzer)` becomes `{880, 340}`; the `bench places:` cases hold.

## 4. The window (§1–§8)

### `src/ui/analyzer_window.hpp`

```cpp
// The floating Logic Analyzer (P8i): a drawn case, its strip of keys; its screen's rows in the
// setup's order, each labelled on a plate over its left end (tick or swatch, name, value,
// Trigger key, ×); the shown Capture drawn at the view.
class AnalyzerWindow {
public:
    bool draw(bool& open, Session& session);  // true when clicked: it takes the focus
    void preset(std::optional<TimeView> view, std::optional<Time> marker,
                std::optional<Time> cursor);
    bool bus() const { return bus_; }
    // ARM was pressed while paused (P8i): bw_ui resumes the run (Resume), once.
    bool takeRunAsked() { return std::exchange(runAsked_, false); }
private:
    // As now: shown_, shownPeriod_, view_, marker_, canvas_, wheelRest_, presetView_,
    // presetMarker_, placedCursor_, bus_, expanded_, runs_, patternRefused_. `selected_` keeps
    // the ticked loose Channels, `tickedGroup_` a ticked group's name; `scroll_` the rows'
    // scroll in px; `dragging_` (P8h's Dragging: a top row's place, its press y, moving);
    // `runAsked_`. `labelScroll_` and `rowsScroll_` go.
};
```

There is no child window: the rows are drawn and scrolled by hand inside the screen, so the corner grip, submitted last, keeps its 44 px (a child would take its hover and draw over it), and no scrollbar narrows the lanes under a full-width overview and axis.

`draw`, in order:

1. **The case** as the scope's (`ScopeWindow::draw`'s opening, ADD_INSTRUMENT.md's drawn-case shape): `PlaceNext(Analyzer, NormalSize)`, the size constraints (880 × 340, clamped to the bench), a `NoTitleBar | NoScrollbar | NoScrollWithMouse | NoSavedSettings | NoCollapse` window, `InstrumentClicked(kCaseClose)`, `DrawCaseFrame`, the groups and labels, `DrawBadge`, `DrawBezel`.
2. **The strip's items**, InvisibleButtons over the layout's boxes, drawn pressed while held (the scope's `key`):
   - `ARM`: `session.analyzer().arm()`; when `session.powered()` and `session.status().state == RunState::Paused`, `runAsked_ = true`. Lamps: `ARMED` at `Armed`, `TRIG'D` at `Filling`, `DONE` with a Capture shown and the phase `Captured` or `Idle`.
   - SAMPLE: ◀ a longer period, ▶ a shorter (the old `Stepper`'s + was finer); the LED opens `TimeGrid` over the periods shortest first.
   - TRIGGER AT: the rocker's halves set `settings.position`; each half's tooltip the time before the Trigger (`FormatTime(PreTriggerPoints(p) * period)`).
   - VIEW / DIV: ◀ `zoom(centre, -1)`, ▶ `zoom(centre, 1)` about the lanes' visible centre; the LED `FormatTime(view_.perDivision)`, `-` with no Capture; `FIT`: `Fit(RecordOf(*shown_))`.
   - The size key: `NextInstrumentSize(Analyzer)`.
3. **The corner** (§3), drawn in the screen's look (outlined keys as the plates', the lit one filled; not the strip's light caps): `PIN | BUS` sets `bus_`; `GROUP` (step 9); the count `N of 64` in the hint grey.
4. **The overview and the axis** as now, from the layout's boxes: the view's box at least 6 px wide, the Trigger and the Marker as ticks; its item covers the overview and the axis (28 px high), a press or drag there centring the view, the wheel zooming. `DrawAxisLabels` from `laneX0`.
5. **The screen under the axis**, on the window's draw list clipped to `layout.rows`: the background and the grid over all of it (§3: to the bottom), the plates' column, then each visible row at `top - scroll_` (its height `AnalyzerRowHeight(rows.height(), count)`, `kRowGap` of black under it), then the Trigger, Marker and cursor lines over all of it, and the cursor's tag (`DrawCursorTag`, `CursorTagText`). Rows: `setup.rows()`, a group's members under it when expanded (as now: most significant first, indented, not draggable, not tickable).
6. **Items, in this order**, each inside `ImGui::PushClipRect(layout.rows)` so a row half scrolled off is not hovered over the axis:
   - the canvas: one `SetNextItemAllowOverlap` InvisibleButton over the lanes' part of `layout.rows`, to the screen's bottom (a press below the last row is the screen's, not the window's drag); `canvas_.read(…, vertical = the rows overflow)`; its `scrollY` scrolls `scroll_`;
   - per visible row, the plate's keys first, then the plate's own button (P8h's order: the first item submitted takes the hover): the tick (the swatch's cell, 28 px wide, the row's height, a click toggling it), the fold arrow (a group's), the Trigger key, the ×; then the plate (`SetNextItemAllowOverlap`, the dragging of step 10);
   - the wheel over the plates scrolls `scroll_` (on the lanes it zooms, as now); two fingers on the rows zoom and scroll (`PinchResize(rowsHovered)` as now).
   Every key is at least 28 × 28 (`kControl`, ADD_INSTRUMENT.md), the row's height at least: on a 28 px row the drawn Trigger key is 28 × 24, the × 24 × 24, the tick box 14 px, their hit boxes 28 high.
7. **A plate** (§4), 248 px: the swatch (`RowColour(place)`, a member its group's), or with two rows or more a tick box in that colour; the fold arrow on a group; the name; the value right-aligned before the Trigger key, read at the cursor, else the Marker, else the Trigger (`0`); the Trigger key; ×. The Trigger key (a bit's 28 wide, a bus's 44): dark with `·` on `any` (a bus's `xxxx`), lit orange with `0`, `1` or a triangle up or down (the font has no arrows); a click on a Channel's opens `ChoicesPopup` (`any 0 1 rise fall`); a group's is its pattern's `NumberField` (`PadKind::Pattern`), its `FieldLook` lit orange when the pattern is not all `x`. A member's key is drawn dark and takes no click (its group's pattern sets it). × unclips (a group's: all its members, no popup).
8. **The lane** as now (`RunsOf`, levels and slanted bus cells), with three changes: dimmed (alpha 90) while the phase is `Armed` or `Filling` (§8); empty, `not in this Capture` in faint text, when `!setup.inCapture(*shown_, channels)` (§8); the Marker and the cursor land on an edge for the mouse too (`landed` without its `FingerPointer()` test, as the scope since P8h).
9. **Grouping** (§4, the user, 2026-11-26): a tick on a loose Channel adds it to `selected_`, on a group sets `tickedGroup_` (the two exclusive: ticking one kind clears the other). `GROUP`, lit with two or more Channels ticked, groups them in row order (`group(picked)`, as the old button); with a group ticked it reads `UNGROUP`, lit, and ungroups it. Dark otherwise.
10. **Reordering** (§4): a top row's plate pressed and moved past the slop is dragged (P8h's `Dragging`), drawn last at the pointer, dropped on the top row whose box is nearest (`BandAt` over the top rows' own boxes, a group's without its members): `setup.move(from, to)` after the drawing, as the scope's `dropAt`. No scrolling while dragging (a long list: scroll, then drag).
11. Last: the settings (`setSettings` when changed), the deferred unclip, group, ungroup or move, `CaseClose`, `PopOnDoubleClick`, `CornerGrip`, `PinchResize(rowsHovered)`.

With no row, the screen shows `Clip a pin: the analyzer in hand, click a chip's pin.` faintly at its centre (the old empty hint, not a status line). The gesture hint and the status line go.

### `src/ui/shelf.hpp`, `src/ui/main.cpp`

```cpp
// The run switch back from `‖` (P8i: ARM while paused), as PowerOn does: to 1×, running.
void Resume(Session& session, RunSwitch& run);
```

`main.cpp`, after `instrument(Owner::Analyzer, analyzerWindow)`: `if (analyzerWindow.takeRunAsked()) Resume(session, run);`.

## 5. Docs

- GLOSSARY: Logic Analyzer (the drawn case, its strip, the plates, the corner, the arming), Clip (its disc numbered by its row), Channel Group (`GROUP` / `UNGROUP`), Marker (the analyzer's mouse snaps too, its tag on the screen), Equipment (no title bar left: the analyzer's size key and × on its case).
- ADD_INSTRUMENT.md: the Window row rewritten for the drawn case as the only shape (`BeginInstrument`, its title-bar size button and the status line gone); a drawn case's shared pieces (`case_strip`, `case_keys`, `TimeGrid`, `DrawCursorTag`, `ChoicesPopup`).
- AGENTS.md's shared helpers: `case_strip`, `case_keys`, `ChoicesPopup` for `ChoicePopup`.
- README: the `analyzer` shot retaken (`python3 -I tools/readme_shots.py analyzer`), viewed; README_SPEC's line checked.
- The manuals' analyzer setups ("Sample every 100 ns", "Arm, then press RESET") still read right: SAMPLE and ARM keep those words.
- ARCHITECTURE: P8i done, one line and the links.

## Checks

- Each phase's fence is the plan's.
- Debug (ASan, UBSan) after phase 2: `ctest --preset debug -j -R "^session: (clips|clip a|group|unclip|hand groups|groups survive|the analyzer|a new Clip|a Channel reused|a power-on|arm before|hooking the scope)"` (none of the BW-80's or BW-2001's cases).
- Phase 1, the scope unchanged: screenshots before and after (the counter with four Hooks; at `--delay 1.5us`; the timebase grid opened; the minimal size). The traces differ from run to run (about 19 000 px between two runs of the same shot): the case, the strip and the plates are compared pixel for pixel (`magick compare` on the crop above the traces), the rest by eye.
- Screenshots (release, viewed): the LED computer at 940 × 520 (`--group U1.A0:F800,U1.D0 --clip U1.SYNC:r,U1.PHI2,U1.RW --sample 100ns --view 1us:+3us --marker +1us --cursor +2.3us`); the counter at the minimal size; the bug's repro (two Clips, `--run pause`, × on one, a `--tap` on a pad: the new row empty, `not in this Capture`); `ARM` tapped while paused (the run switch at 1×, a new Capture); the SAMPLE grid; a Trigger key's choices; two plates ticked and `GROUP` tapped; a group ticked, `UNGROUP`; a plate dragged (`--drag`), the Board's discs renumbered; twenty Clips (the rows scrolled); `--window 1080:810`.
- Web: `cmake --build --preset web` builds (the drawing is ImDrawList only, as the scope's).
- Speed: the LED computer and the counter with the analyzer out, UI ms under 1.

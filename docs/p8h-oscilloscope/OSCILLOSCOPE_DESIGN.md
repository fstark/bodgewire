# P8h — The Oscilloscope redesigned — Design

How [OSCILLOSCOPE.md](OSCILLOSCOPE.md) is built. Plan: [OSCILLOSCOPE_PLAN.md](OSCILLOSCOPE_PLAN.md). Numbers in parentheses (§3) are the spec's items.

The kernel, the runner, the Sweep engine (`ScopeState`, `Oscilloscope`), the `ScopeConsumer`, the Logic Analyzer and the Machines are untouched. Pure maths is added to `time_view` and two new view files (`scope_reading`, `scope_layout`); `ScopeSetup` gains the bands' order; the scope's window is rewritten as a drawn case, its drawing in a new `scope_case` and `bw_logo`; `instrument_marks` gives an instrument with no title bar its size preset, its icon and its click; `NumberField` gains a look.

## Files

| File | Change |
|---|---|
| `src/view/time_view.{hpp,cpp}` | `ScopeView`, `WithView` take a lead of divisions; `CursorTagText`; `TimebaseRows` |
| `src/view/scope_reading.{hpp,cpp}` (new) | `BandReading` |
| `src/view/scope_layout.{hpp,cpp}` (new) | `Box`, `ScopeCaseLayout`, `LayOutScopeCase`, `ScopeBands`, `BandAt` |
| `src/view/CMakeLists.txt` | the two new files |
| `src/view/bench_places.cpp` | the scope's minimal size 620 × 400 |
| `src/session/scope_setup.{hpp,cpp}` | `order()`, `move()`; `hook` / `unhook` keep the order and the Trigger's source |
| `src/ui/board_view.hpp`, `board_view.cpp`, `board_tags.cpp` | `HookTag::place`; the disc's digit is the place |
| `src/ui/instrument_marks.{hpp,cpp}` | `NextInstrumentSize`, `DrawSizeIcon`; `InstrumentClicked(closeZone)`; `DrawAxisLabels(…, arrow)` |
| `src/ui/controls.{hpp,cpp}` | `FieldLook`, `NumberField(…, look)` |
| `src/ui/bw_logo.{hpp,cpp}` (new) | `DrawBwLogo` |
| `src/ui/scope_case.{hpp,cpp}` (new) | `DrawScopeCase`, `DrawKey`, `DrawLed`, `DrawLamp`, `DrawRocker`, `DrawBadge`, `DrawArrow` |
| `src/ui/scope_window.{hpp,cpp}` | rewritten: case, strip, screen, plates, reorder |
| `src/ui/CMakeLists.txt` | `bw_logo.cpp`, `scope_case.cpp` |
| `tests/view/scope_reading_test.cpp`, `tests/view/scope_layout_test.cpp` (new), `tests/CMakeLists.txt` | their cases |
| `tests/view/time_view_test.cpp`, `tests/session/session_test.cpp` | new cases |
| `machines/{040-stopwatch,050-test-pattern,060-bouncing-ball}/manuals/service.md` | the setups by pin (§8) |
| `docs/GLOSSARY.md`, `docs/recipes/ADD_INSTRUMENT.md`, `docs/ARCHITECTURE.md`, `docs/README_SPEC.md`, `README.md`, `docs/readme/scope.png` | §8 |

## 1. The view behind the plate (§2, §5, §7, §8)

The engine's Sweep runs from one division before the Trigger mark (`Sweep::start = Trigger + delay - timebase`, `oscilloscope.cpp`'s `startSweep`) for `kDivisions` divisions, and stays so. The screen shows the same `kDivisions` divisions over its own width, shifted left by the plate: `lead` divisions (`kPlate / width × kDivisions`, fractional) sit under the plate before the division left of the Trigger mark. Nothing was captured there; the plate is opaque. The last `lead` divisions of each Sweep are captured but not shown.

### `src/view/time_view.hpp`

```cpp
// The scope's view is its settings: left = delay - timebase - llround(lead * timebase). `lead`:
// divisions under the label plates, left of the division before the Trigger (P8h); 0: the
// Trigger at the first division.
TimeView ScopeView(const ScopeSettings& settings, double lead = 0);
// Back: delay = view.left + perDivision + llround(lead * perDivision), then the timebase within
// kTimebases, the delay rounded to a tenth of a division and clamped to 0 … kMaxDelayDivisions
// divisions, as now; `lead` as ScopeView's (the same llround, so a round trip is exact).
ScopeSettings WithView(ScopeSettings settings, const TimeView& view, double lead = 0);

// The cursor's tag (P8h): "+5 µs", "+5 µs   dt 3 µs" (no frequency; the font has no Greek).
std::string CursorTagText(Time cursor, std::optional<Time> marker);

// kTimebases by unit for the timebase grid (P8h): ns, µs, ms rows, each its indices into
// kTimebases, shortest first: {0…6}, {7…15}, {16…19}.
struct TimebaseRow {
    const char* unit;
    std::vector<std::size_t> indices;
};
std::vector<TimebaseRow> TimebaseRows();
```

A zoom (`ZoomAbout` on `ScopeView(settings, lead)`) keeps its anchor at the same fraction of the width and a pan moves by the pixels dragged, as now. The window recomputes `lead` each frame from the screen's width; resizing the case moves the Trigger's place, not the settings. The existing callers pass no lead. `CursorTagText` formats with `FormatTime` as `StatusText` does; `StatusText` stays for the Logic Analyzer.

### Tests (`tests/view/time_view_test.cpp`, `[view]`)

- `time view: the scope's view with a lead`: `ScopeView({.timebase = 1_us}, 2.5).left == -3'500'000`; `WithView` of it gives the settings back; a zoom in about +1 µs with a lead of 2.5 keeps +1 µs at the same fraction; a pan of −100 px over 1000 px at 1 µs/div from delay 0 gives a delay of 1 µs.
- `time view: the cursor's tag`: `"+5 µs"`, `"+5 µs   dt 3 µs"`, a cursor before the Marker (`dt -2 µs`), `dt 0`.
- `time view: the timebase rows`: three rows, units `ns µs ms`, sizes 7, 9, 4, together every index once, ascending.

## 2. The readings (§4)

### `src/view/scope_reading.hpp` (new)

```cpp
// A band's reading (P8h): what its Samples show between `from` and `to` (ps since power-on: the
// Sweep's start, and the screen's right edge, before the Sweep's end by the lead).
//   High: value One and not floating; a rising edge: a Sample high after one not high (Zero, X,
//   or floating whatever its value: a float driven high counts), the first Sample excluded (the
//   value at the start).
//   at least two rising edges: "1 MHz · 50 %": the frequency of their mean period (first rise to
//     last over the count less one), the share of that time high, to the percent;
//   else a whole pulse (a level held between two edges, the first one): "high 1 µs", "low 4 µs";
//   else the level at `to`: "H", "L", "Z 1.4 V", "X";
//   a rail (`rail`: its volts): "5.0 V"; no Samples: "".
std::string BandReading(std::span<const Sample> samples, Time from, Time to,
                        std::optional<double> rail);
```

`FormatFrequency(period)` and `FormatTime(width, false)` (`view/time_text.hpp`) format, trailing zeros dropped as everywhere (`1 MHz`, `4 µs`, `15.6 kHz`); `LogicVolts` (`instruments/volts.hpp`) gives `Z`'s volts. The window caches a band's reading by Sweep serial, `to` and powered (power off: `""`, the traces flat at 0 V); a 10 ms/div Sweep of a 1 MHz clock holds 20 000 Samples a Channel.

### Tests (`tests/view/scope_reading_test.cpp`, `scope reading: …`, `[view]`)

A 1 MHz square wave over 10 µs (`1 MHz · 50 %`), a 25 % duty cycle, edges past `to` left out, one high pulse (`high 1 µs`), one low pulse (`low 4 µs`), a single rise (`H`), a constant `L`, floating (`Z 1.4 V`), a float driven high counted as a rise, X, a rail (`5.0 V`), no Samples (`""`).

## 3. The bands' order (§2, §3, §5, §9)

### `src/session/scope_setup.hpp`

```cpp
    // The hooked Channels top to bottom on the screen (P8h): a Hook goes at the bottom; an
    // unhook closes its gap; kept across rebuilds, as the Hooks are.
    std::span<const std::size_t> order() const { return order_; }
    // The band at place `from` moved to place `to` (clamped), the others closing up.
    void move(std::size_t from, std::size_t to);
private:
    std::vector<std::size_t> order_;
```

- `hook` appends the new Channel to `order_` (a pin already hooked returns its Channel, as now). When the Trigger's source is not a hooked Channel (none yet, or its Hook removed with no band left), the source becomes the new Channel, in the same `configure()`: with one Hook its `T` is lit.
- `unhook(channel)` erases it; when the source was that Channel and a band is left, the source becomes `order_.front()`.
- `move` rotates `order_`; it does not reconfigure (the order is the screen's only).
- A Channel's colour stays `kHookColours[channel]`: it stays with the signal.

`--hook A,B,C` (`src/ui/bench_setup.cpp`) hooks in its order, so its bands are in that order; nothing changes there.

### The Board (`src/ui/board_view.hpp`, `board_tags.cpp`, `board_view.cpp`)

`HookTag` gains `std::size_t place; // 0: the top band`. `HookTagsOf` walks `order()`; `BoardView` draws `'1' + place` in the disc, its colour the Channel's.

### Tests (`tests/session/session_test.cpp`, `[session]`; orders compared with `std::ranges::equal`)

- `session: the scope's bands in hook order`: three pins: `{0, 1, 2}`; unhook the middle: `{0, 2}`; hook another: Channel 1, last: `{0, 2, 1}`.
- `session: the scope's source follows its Hook`: source on the middle band's Channel, unhook it: `order().front()`; unhook all, hook one: the source is its Channel.
- `session: the scope's bands moved`: `move(2, 0)`; past the end clamps; the order survives a Repair (a rebuild), as `session: hooks and leads survive a repair` does for the Hooks.

## 4. The layout (§1, §3, §6)

### `src/view/scope_layout.hpp` (new)

```cpp
struct Box {  // px
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    float width() const { return x1 - x0; }
    float height() const { return y1 - y0; }
    bool contains(float x, float y) const;
};

// The Oscilloscope's case (P8h) for a window `width` × `height` px, from its top left.
struct ScopeCaseLayout {
    Box timeGroup, triggerGroup;                       // the strip's tinted panels
    Box timeLess, timeValue, timeMore;                 // TIME / DIV ◀ value ▶
    Box positionLess, positionValue, positionMore;     // POSITION ◀ value ▶
    Box autoHalf, normHalf, lamp;                      // the rocker's halves, TRIG'D
    Box badge;                                         // the logo, and the words when `words`
    bool words = false;
    Box sizeKey;                                       // minimal / normal / full
    Box bezel, screen;
};
ScopeCaseLayout LayOutScopeCase(float width, float height);

inline constexpr float kPlate = 156;     // a band's label plate, px
inline constexpr float kBandGap = 6;     // black between bands
inline constexpr float kMinBand = 50;

// The bands' boxes in `screen` below its axis strip (`axis` px), `count` of them, the height
// shared, at least kMinBand each; and the band under `y` (the nearest, for a drop).
std::vector<Box> ScopeBands(const Box& screen, float axis, std::size_t count);
std::size_t BandAt(std::span<const Box> bands, float y);
```

The strip is 72 px high, from the left at 12 px: the time group (6 px padding; TIME / DIV: ◀ 36, LED 64, ▶ 36; 8 px; POSITION: ◀ 36, LED 80, ▶ 36), 8 px, the trigger group (rocker 96, 8, lamp 22, 6 px padding), 8 px, the badge (logo 51 × 32; with `words`, 110 px more for `BODGE·WIRE` / `OSCILLOSCOPE`), 8 px, the size key (36 × 36), then `CaseClose`'s 40 px corner: 609 px without words. Keys `kBig` (44 px) high, 36 wide. The time LED shows `1 µs` (no `/div`: the label says it); the position LED holds `FormatTimeStep` values up to `123.46 µs`. The words appear when the window is at least 719 px wide (the full preset on a laptop's bench; not at 680). The bezel from 80 px to 12 px above the bottom, 14 px in from the sides; the screen 8 px inside it, so its bottom right lies 22 px from the window's corner (the corner grip's 44 px overlap it: §5).

### Tests (`tests/view/scope_layout_test.cpp`, `scope layout: …`, `[view]`)

At 620 × 400: no two boxes of the strip overlap, the size key ends left of the 40 px close corner, no words; at 720: words, the badge clear of the size key; four bands at 620 × 400 each at least kMinBand; `BandAt` above, inside, in a gap, below.

## 5. The screen (§2, §3, §4, §5, §7, §8)

`ScopeWindow` keeps `shown_`, `marker_`, `placedCursor_`, `canvas_`, `panRest_`, `pressed_`, `delayRefused_` and the column cache; `stopped_` goes (§10). New: `readings_` (per Channel: the text and its cache key), `dragging_` (the place whose plate is dragged, its press y and whether it passed the slop), and the actions of this frame (`unhookChannel`, `dropAt`).

Items and hover. `IsItemHovered` tests an item's own rectangle only, unless the item allows overlap (imgui.cpp, `IsItemHovered`'s `ImGuiItemFlags_AllowOverlap` branch, through `HoveredIdPreviousFrame`). So the screen's `InvisibleButton` is submitted with `ImGui::SetNextItemAllowOverlap()`, first; every item submitted after it over its rectangle (the plates' `T`, ×, plate, and `CornerGrip` last) takes the hover from it, and `TimeCanvas::read` then gives no hover, zoom, fit or Marker there (one frame late, ImGui's previous-frame hover: unnoticeable).

Each frame, in this order:

1. The case and strip (§6), their items.
2. `LayOutScopeCase`, `ScopeBands` for the hooked Channels in `order()`; `width` the screen's; `lead = kPlate / width × kDivisions`.
3. The screen's `InvisibleButton` (allow overlap) and `TimeCanvas::read`, with `ScopeView(settings, lead)` and `WithView(…, lead)` wherever the window now calls them. The mouse's snap (§8): `landed(px)` drops its `FingerPointer()` condition; the hovered cursor snaps too, on the band under the pointer's y (`BandAt`), with the same `SnapToEdge` and `kEdgeReach` (12 px).
4. Per band, its plate's items: `T` (28 px, `kControl`), × (28 px), then the plate's `InvisibleButton` (its drag, §7). A click on `T`: on the source, `edge` flips; on another band, `source` becomes its Channel (both through `set()`, into the frame's `settings`). A click on ×: `unhookChannel = c` (applied at step 7).
5. Drawing, clipped to the screen:
   - black; per band its background (`kScreen`), its six volt lines (0 V and 5 V `kGrid`, 1–4 V `kGridMinor`);
   - the time lines from `LabelledGrid(view, width)` across all bands and gaps;
   - the traces from the column cache (as now: one rectangle per run of equal columns), each glowing: the same rectangles first in the Hook's colour at 25 % alpha grown by 2 px, then at full alpha;
   - the Trigger (`DrawTrigger`) when on screen;
   - the plates: filled `kPlateColour` (opaque), `kPlate` wide, the band's height; on each the `T` key (lit orange on the source, with a small triangle drawn up for rise, down for fall: no ▲ in the font), the name (`hooks()[c]->label`, the Hook's colour), the ×, the reading (`BandReading` over `[shown_->start, the screen's right edge]`, cached), then with a cursor or Marker the value under it (`valueAt`, as now); the texts clipped to the plate;
   - the 0 V arrow, a triangle in the Hook's colour pointing right, at the plate's right edge on the band's 0 V line;
   - clipped to the right of the plates: the Marker (`DrawMarker`), the cursor's line and tag (`CursorTagText`, a light tag 20 px high at the screen's top, flipped left of the line when it would pass the right edge).
   - the axis: `DrawAxisLabels(list, view, grid, x, axisY, plateRight, right, false)`: none over the plates, no room kept for an arrow; `DrawOffScreenTrigger` not called (§10).
6. No band: the hint "Hook, then click a chip pin." as now. Power off: each band's flat line at 0 V, as now.
7. After the drawing: `if (changed) setSettings(settings)` as now, then `unhook(unhookChannel)` (which may move the source: after the settings, so the frame's copy does not undo it), then `move(dragging_ place, dropAt)` (§7). The bands are next drawn from the new order on the next frame.

`StatusLine` and `kGestures` are no longer called from the scope (§10); both stay for the analyzer.

### `src/ui/instrument_marks.hpp`

```cpp
// Labels along an axis strip …; `arrow`: room kept for the off-screen Trigger's arrow (the
// analyzer's); the scope has none (P8h).
void DrawAxisLabels(ImDrawList& list, const TimeView& view, const std::vector<GridLine>& grid,
                    const TimeToX& x, float y, float left, float right, bool arrow = true);
```

## 6. The case and the strip (§1, §6, §7)

### `src/ui/scope_case.hpp` (new)

```cpp
// The Oscilloscope's case (P8h), HP-like: drawing only, on ScopeCaseLayout's boxes offset to the
// window's top left; the window submits the items.
void DrawScopeCase(ImDrawList& list, ImVec2 at, const ScopeCaseLayout& layout);  // frame, panel, groups, bezel
void DrawKey(ImDrawList& list, const Box& key, ImVec2 at, const char* label, bool pressed);
void DrawArrow(ImDrawList& list, const Box& key, ImVec2 at, bool right, bool pressed);  // ◀ ▶ keys
void DrawLed(ImDrawList& list, const Box& window, ImVec2 at, const char* text);  // red on dark red
void DrawLamp(ImDrawList& list, const Box& lamp, ImVec2 at, bool lit);           // TRIG'D, green
void DrawRocker(ImDrawList& list, const Box& autoHalf, const Box& normHalf, ImVec2 at, bool normal);
void DrawBadge(ImDrawList& list, const Box& badge, ImVec2 at, bool words);       // logo + words
```

Light grey key caps (pressed: lower, no shadow); the ◀ ▶ keys' arrows are triangles (`DrawArrow`: the font has no ◀ ▶, ADD_INSTRUMENT's gotcha). Labels in the UI font at its size; the badge's words in the default font.

### `src/ui/bw_logo.hpp` (new)

```cpp
// The Bodge Wire logo (P8h), a spoof of HP's: a blue rounded rectangle 1000 × 622 units, a
// white circle, `bw` in slanted straight strokes, the b's stem leaving through the top edge
// with a white gap each side. Drawn `height` px high at `min`.
void DrawBwLogo(ImDrawList& list, ImVec2 min, float height);
```

As `docs/p8h-oscilloscope/mockups/scope_mock.py`'s `letters("plain")` and `logo()`: the letters' outlines as polygons (a rounded corner 6 segments), skewed by −21° and scaled by 0.94 about the circle's centre, each filled with `AddConcavePolyFilled` (one outline, no holes: the b's outer outline, then its counter filled white over it, as it lies inside the white circle; the w is one outline); the stem's gap a white quadrilateral 18 units wider each side drawn first, inside a `PushClipRect` of the rectangle (the stem leaves through the straight top edge, clear of the rounded corners). Checked by eye in a crop of a 2× screenshot (64 device px high).

### `src/ui/controls.hpp`

```cpp
// A NumberField's own look (P8h: the scope's position as an LED window): its frame's colours,
// hovered and pressed included, its text's, its rounding and height (0: kControl). The pad it
// opens keeps the style's.
struct FieldLook {
    ImU32 frame, frameHovered, frameActive, text;
    float rounding = 0;
    float height = 0;
};
bool NumberField(const char* id, std::string& text, PadKind kind, float width, bool refused = false,
                 std::size_t limit = 16, const FieldLook* look = nullptr);
```

`NumberField` pushes the look around its field's button only, popped before the pad's popup begins.

### `src/ui/scope_window.cpp`: the window

- Begun as the TV is (`src/ui/tv_window.cpp`): `PlaceNext(BenchObject::Scope, normal)`, `SetNextWindowSizeConstraints` with a minimum of the minimal size clamped to the bench (P8f's shrink on a small bench is kept), no padding, `ImGuiWindowFlags_NoTitleBar | NoSavedSettings | NoScrollbar | NoScrollWithMouse | NoCollapse`, the case's colour as its background. Not `BeginInstrument`: it draws a title bar.
- Clicked: `InstrumentClicked(40)` (§ below): not in `CaseClose`'s 40 px corner.
- An `InvisibleButton` on each key's box, drawn pressed while `IsItemActive()`:
  - TIME / DIV ◀ ▶: one step longer / shorter (▶ always looks closer), zooming about the centre of the screen's visible part (right of the plates) with `ZoomAbout(…, ∓1, …)` through `WithView(…, lead)`, as the Stepper's − and + do now. Its value: `DrawLed` with `FormatTime(timebase, false)`; a click opens the grid (`ImGui::OpenPopup("##timebase")`): a popup under the value, per `TimebaseRows()` row its unit then one key per value (`kBig` high, 44 wide), the current one lit; a click sets the timebase (the delay clamped to `kMaxDelayDivisions` divisions, as now) and closes it.
  - POSITION ◀ ▶: the delay ∓ 5 divisions, clamped to 0 … `kMaxDelayDivisions` divisions. Its value: `NumberField` as now (`PadKind::Duration`, refused red) with a `FieldLook` (dark red frames, LED red text, rounding 3, height `kBig`).
  - The rocker: a click on a half sets `mode`. The lamp: lit when `shown_` is a triggered Sweep (`kind == SweepKind::Triggered`) and the power is on.
  - The size key: `NextInstrumentSize(BenchObject::Scope)`, drawn with `DrawSizeIcon`.
- `CaseClose()` puts it away; `PopOnDoubleClick(BenchObject::Scope)`, `CornerGrip()`, `PinchResize(input.hover.has_value())` last, as now.

### `src/ui/instrument_marks.hpp`

```cpp
// The minimal / normal (/ full, on the bench) preset after the window's size now (P6d, P8f), as
// BeginInstrument's title-bar button does; inside the window. For an instrument drawn as a case
// (the scope, P8h).
void NextInstrumentSize(BenchObject object);
// The size button's icon (two squares), `side` px at `min`; BeginInstrument's and the scope's.
void DrawSizeIcon(ImDrawList& list, ImVec2 min, float side, ImU32 colour);
// The left button went down in the window this frame, not in its top-right close zone
// (`closeZone` px square; the title bar's height by default).
bool InstrumentClicked(std::optional<float> closeZone = std::nullopt);
```

`BeginInstrument`'s button calls both; its behaviour is unchanged for the analyzer.

## 7. Reordering (§3)

The plate's `InvisibleButton` (§5, step 4): pressed, `dragging_ = {place, press y}`; while held and past the slop (`kTapSlop` for a finger, the mouse's 3 px `kClickSlop`, as `TimeCanvas` uses them), the plate is drawn at the pointer's y (clamped to the screen), the others where they are; at the release, `dropAt = BandAt(bands, y)`, applied at step 7. A release within the slop does nothing. The plate is an item, so a drag on it never moves the case.

## 8. Docs and the rest

- Manuals:
  - 050: "Hook `U14.1Q` on channel 1 and `U14.2Q` on channel 2. Trigger on channel 1, rising edge" becomes "Hook `U14.1Q`, then `U14.2Q`. Trigger on `U14.1Q`, rising"; its expectations' "Channel 1:", "Channel 2:" and "channel 2 stays high" name the pins.
  - 060: the same with `U11.1Q` and `U11.2Q`.
  - 040: "trigger on channel 1 rising" becomes "trigger on `U4.QD` rising".
- GLOSSARY: **Oscilloscope** (bands, the labels on its screen, the strip; no title bar), **Hook** (its band; the Board's number is the band's place), **Marker** (the mouse's lands on an edge too), **Bench** (the scope's presets on its case), **Stepper** (no longer the scope's timebase: the analyzer's sample period and the Contents page's width), a new **Band** (one Hook's strip of the Oscilloscope's screen, with its own 0–5 V scale).
- ADD_INSTRUMENT.md: the Window row: an instrument may be a drawn case (the scope): `NextInstrumentSize`, `DrawSizeIcon`, `InstrumentClicked(closeZone)`; a gotcha: `IsItemHovered` ignores items submitted over an item unless it allows overlap (`SetNextItemAllowOverlap`), so a canvas with controls over it allows overlap.
- README: the `scope` shot (050, `J2.VIDEO,J2.SYNC_N`, 10 µs) retaken by `python3 -I tools/readme_shots.py scope`; README_SPEC's scope line if its words no longer match.
- ARCHITECTURE: the P8h row done, one line and the links.
- The web build (`gpu_render_web.cpp` draws the same draw lists): built and its page looked at once (AGENTS.md).

## Checks

- Release: per phase the cases that reach it (the plan); the full suite after phases 3 and 4 (shared `instrument_marks`, `controls`) and at the end.
- Debug (ASan, UBSan), never the BW-80 or BW-2001 cases: `ctest --preset debug -j -R "^(session: (the scope|hooks)|scope (reading|layout): |time view: )"`.
- Screenshots (`build/release/src/ui/bw_ui machines/010-counter/puzzles/01-dark-led.bwp --hook … --focus scope --screenshot /tmp/x.png --after 1s`, window px for the scripted fingers read off a first shot, halved): listed per phase in the plan.
- Speed: `machines/140-bw80` with four Hooks: `bw_ui` prints the ratio (1.00×) and UI ms (under 1).
- Only the user can check: the mouse's snap (every scripted pointer is a finger: `--tap`, `--drag`, `--hold` set `--touch`), and the feel of the keys and the drag.

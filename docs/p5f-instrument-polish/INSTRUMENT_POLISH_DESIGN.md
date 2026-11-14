# P5f — Instrument polish — Design

Spec: [INSTRUMENT_POLISH.md](INSTRUMENT_POLISH.md). Plan: [INSTRUMENT_POLISH_PLAN.md](INSTRUMENT_POLISH_PLAN.md). Inventory: [INVENTORY.md](INVENTORY.md). Recipe: [ADD_INSTRUMENT.md](../recipes/ADD_INSTRUMENT.md): only its Window and CLI layers change (and its Gotchas on window input and drawing from runs still hold). No engine, consumer, Session or placement change.

## Files

| File | Change |
|---|---|
| `src/view/time_view.{hpp,cpp}` (new, bw_view) | the view maths shared by both windows (below) |
| `src/view/time_text.{hpp,cpp}` (moved from `src/ui`) | `FormatTime` as today; `FormatFrequency` new |
| `src/view/CMakeLists.txt` | `time_view.cpp`, `time_text.cpp`; `bw_view` links `bw_instruments` PUBLIC (`time_view.hpp` exposes `kDivisions`, `kTimebases`, `ScopeSettings`; `bw_instruments` needs only `bw_sim`: no cycle) |
| `src/ui/CMakeLists.txt` | `time_text.cpp` removed |
| `src/ui/analyzer_window.{hpp,cpp}` | view as a `TimeView`; header, overview, axis strip, grid, Trigger, runs open at the edges, marker, status line (below) |
| `src/ui/scope_window.{hpp,cpp}` | view from the settings; gestures; axis strip; bands; values; Stop; marker, status line (below) |
| `src/ui/instrument_marks.{hpp,cpp}` (new) | ImGui drawing shared by both windows: Trigger mark, off-screen arrow, marker tag, grid and labels, status line (below) |
| `src/ui/main.cpp` | `--sample` (replaces `--rate`), `--view`, `--marker`, `--cursor`; usage; the Board's `Esc` ignored while an instrument window has focus |
| `tests/view/time_view_test.cpp`, `time_text_test.cpp` (new), `tests/CMakeLists.txt` | spec, Tests |
| `docs/recipes/ADD_INSTRUMENT.md` | time view, flags, the screenshot gotcha (below) |
| `docs/GLOSSARY.md` | **Marker** (a time picked on an instrument's screen, `dt` measured from it) |
| `docs/ARCHITECTURE.md` | P5f row done |

## Time view (`src/view/time_view.hpp`)

Times are ps from the Trigger (negative before it). Nothing here knows ImGui; widths are pixels as `float`.

```cpp
struct TimeView {
    Time perDivision = 1'000'000;  // a 1-2-5 step
    Time left = 0;                 // ps from the Trigger at the left edge
    Time span() const { return kDivisions * perDivision; }
    bool operator==(const TimeView&) const = default;
};
struct TimeRange { Time from = 0, to = 0; };  // what a record holds, ps from the Trigger

Time StepAtLeast(Time t);           // smallest 1-2-5 step >= t (1 ps … )
Time StepUp(Time step);             // next 1-2-5 step: 1 µs → 2 µs
Time StepDown(Time step);           // 1 µs → 500 ns

// `notches` > 0 zooms in (smaller steps), one step per notch; `anchor` stays at the same fraction
// of the width; the step stays within [minStep, maxStep].
TimeView ZoomAbout(TimeView view, Time anchor, int notches, Time minStep, Time maxStep);
TimeView Pan(TimeView view, double pixels, float width);  // + moves the content right
TimeView Clamp(TimeView view, TimeRange record);  // inside when it fits, else centred
TimeView Fit(TimeRange record);                     // StepAtLeast(length / 10), centred
TimeView FirstView(TimeRange record, Time period);  // StepAtLeast(10 * period) centred on T, clamped
TimeView CentredOn(Time perDivision, Time centre);

struct GridLine { Time time; bool labelled; };
// Lines at whole multiples of perDivision in the view; labels every 1, 2, 5 or 10 divisions,
// the fewest whose spacing reaches `labelGap` px.
std::vector<GridLine> Grid(const TimeView& view, float width, float labelGap);

std::optional<Time> OffScreenTrigger(const TimeView& view);  // distance, < 0: T left of the view

// Where a run's value text starts, or none: [x0, x1] the run's true ends (may lie outside
// [left, right]); `slant` only at the ends inside the view; cut runs put the text against the
// cut edge (A12).
std::optional<float> RunTextX(float x0, float x1, float left, float right, float textWidth,
                              float slant);

// The scope: the view its settings give, and the settings a view gives (delay rounded to a tenth
// of a division, clamped to 0 … kMaxDelayDivisions; the timebase within kTimebases).
TimeView ScopeView(const ScopeSettings& settings);       // left = delay - timebase
ScopeSettings WithView(ScopeSettings settings, const TimeView& view);

std::string StatusText(std::optional<Time> cursor, std::optional<Time> marker);
```

- **Zoom**: `next = notches > 0 ? StepDown^n : StepUp^n`, clamped; `left' = anchor - (anchor - left) * next / perDivision` (as `double`, rounded). The scope calls it with `kTimebases.front()`/`back()` and passes the result through `WithView`, so a zoom near T at delay 0 stays at delay 0 (delay can't go below 0: the anchor moves instead; accepted).
- **Clamp**: when `span() <= to - from`: `left` into `[from, to - span()]`; else `left = from - (span() - (to - from)) / 2`.
- **Analyzer record** (`RecordOf(capture)`): `from = (first - trigger) * period`, `to = (points - trigger) * period`: from the first sampled point (`Capture::trigger` is always the pre-Trigger count; an early Trigger shows as `first > 0`).
- **Grid labels**: `labelGap` 60 px; labelled lines are whole multiples of `n` divisions from T, so `0` is always labelled when visible; label text `FormatTime(t, true)` with `0` for T, so `-3 µs`, `0`, `+6 µs` (`FormatTime` writes `-` as ASCII: the font lacks `−`).
- **`StatusText`**: `"cursor +3.7 µs"`, `"M +2 µs"`, joined by three spaces, then `"dt 1.7 µs (588 kHz)"` when both are set; `dt` = cursor − marker; no frequency when it is 0. Neither UI font (`AddFontDefaultVector`, `AddFontDefaultBitmap`) has `Δ`, `−` or `◀`: `dt`, ASCII `-`, arrows drawn as triangles.
- **`FormatFrequency(Time period)`**: three significant digits of `1e12 / |period|`, Hz/kHz/MHz/GHz, trailing zeros dropped (`588 kHz`, `1.67 MHz`, `10 MHz`, `50 Hz`).

## Shared drawing (`src/ui/instrument_marks.hpp`)

Small ImGui functions both windows call, so the marks look the same:

- `DrawGrid(list, grid, x(t), top, bottom, colour)`, `DrawAxisLabels(list, grid, x(t), y, colour)`.
- `DrawTrigger(list, x, top, bottom)`: orange (`kTriggerMark`, the scope's today) line 1.5 px and a triangle on the top edge.
- `DrawOffScreenTrigger(list, distance, axisLeft, axisRight, y)`: the arrow on that side of the axis strip, `T 800 ns` beside it.
- `DrawMarker(list, x, top, bottom)`: yellow line, a 14 × 13 px tag with `M` on the top edge.
- `StatusLine(cursor, marker, hint)`: `ImGui::TextUnformatted(StatusText(...))`, or the hint in grey when both are empty.
- Colours: one set (`kTriggerMark`, `kMarkerLine`, `kCursorLine`, `kGrid`) moved here from the two windows.

## Analyzer window

State: `TimeView view_`, `std::optional<Time> marker_` (ps from T), `std::optional<Time> shownPeriod_` (to keep the view on a new Capture of the same period), the preset from `bw_ui` (below). `from_`/`to_` in points go; a lane converts with `point = trigger + t / period`.

Layout, top to bottom:

1. **Header line 1**: `Clip (C)`, `Arm`/`Stop`, phase text (as today); `Sample every` + combo of `kAnalyzerPeriods` shown by `FormatTime`, then grey `· 6.55 ms captured` (`kDepth * period`); `Before T` + combo of the three positions shown as `FormatTime(PreTriggerPoints(p) * period)`; `32 of 32` clipped; `Group` when ≥ 2 selected (as today).
2. **Header line 2** (lane column): `-` button, time per division in a frame (`1 µs/div`), `+`, `Fit`, `T`. Tooltips: "zoom out (wheel)", "zoom in (wheel)", "whole Capture (double-click)", "centre on the Trigger". Disabled without a Capture.
3. **Overview** (lane column, 14 px): the record as a dark bar; the view as an outlined box at least 8 px wide; T as an orange tick, the marker as a yellow tick. Press or drag on it: `view_ = Clamp(CentredOn(view_.perDivision, timeAt(mouse)))`. The wheel is owned here too (zooms about the view's centre).
4. **Axis strip** (18 px): left part the headings `Name`, `Value`, `Trigger` at the columns' x (12, 190, 240 as today); lane part `DrawAxisLabels`, the off-screen Trigger arrow. Outside the `rows` child, so it stays when the rows scroll.
5. **Rows child** (as P5e: labels left, one canvas right), now with the grid behind the lanes (`DrawGrid`) and the area outside the record darker.
Order in a frame: the overview's `InvisibleButton` and the axis strip's `Dummy` reserve their rows (the overview takes its press there); the rows child runs the canvas input; only then are the overview and the axis drawn, into the window's draw list, from the view this frame settled (no frame of lag). Every strip uses one `laneX0` and `width` = available − label column − `ScrollbarSize`, so they line up whether or not the rows scroll.

6. **Status line** under the child: `StatusLine` with the hint `wheel: zoom   drag: pan   click: marker   right-click: clear   double-click: fit`.

Input (canvas, once per frame before drawing, ADD_INSTRUMENT Gotchas): wheel → `ZoomAbout(view_, timeAt(mouseX), WheelNotches(wheel, wheelRest_), StepAtLeast(period), Fit(record).perDivision)`; `MouseWheelH` → `Pan(view_, -wheelH * divisionPx / 2, width)` (both `ImGuiKey_MouseWheelY` and `X` owned); drag → `Pan(view_, MouseDelta.x)`; click without drag → `marker_ = timeAt(mouseX)`; right click → `marker_.reset()`; double-click → `Fit`; then `Clamp` once. `Esc` with the window focused (`ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)`) clears the marker. The Board reads its keys whatever window has focus (`BoardView::keys`, only text input stops them), and the tool keys must keep working from the instrument windows; so each window exposes `focused()` (from its last frame) and `main.cpp` drops `boardView.takeEscape()` while either is focused.

Wheel notches: ImGui gives fractional wheel values on a trackpad. `WheelNotches` accumulates them in `wheelRest_` and steps once per whole notch, so a trackpad zooms in steps and never jitters.

State added to P5e's: `TimeView view_`, `std::optional<Time> marker_` (ps from T), `shownPeriod_`, `wheelRest_`, `focused_`, the preset.

New Capture (`captures().update()`): `runs_` cleared, marker cleared; `view_ = Clamp(view_)` if `shownPeriod_ == capture.period`, else `FirstView`; then the preset (below) once.

Lanes:

- **Runs open at the edges (A12)**: `lane()` computes `x0 = x(start)`, `x1 = x(end)` unclamped; a bus shape gets its slant only at an end inside `[laneX0, laneX1]` (a cut end is a straight edge outside the clip rect); text at `RunTextX(x0, x1, laneX0, laneX1, textWidth, slant)`. The sub-pixel jump (P5e) is unchanged.
- Group row label: `std::format("{}[{}:0]", group.name, channels.size() - 1)`; the count after the name goes. Bit 0 is the group's first Channel (name index order for stems, `A0`; hand groups too: `group 1[3:0]`).
- `Trigger` button renamed `T` (centres on the Trigger keeping the step).
- `DrawTrigger` replaces the white Trigger line; `DrawMarker` the marker line; the cursor line stays white.

## Scope window

State: `std::optional<Time> marker_` (ps from T), `bool stopped_`, the preset, `wheelRest_`, `panRest_` (px), `std::optional<ScopeSettings> pressed_` (the settings at a drag's press), `focused_`. `shown_` as today; while `stopped_`, `sweeps().update()` is still called but `shown_` is not replaced.

- **View**: `TimeView view = ScopeView(settings)` from the Session's settings each frame. Gestures produce a new view and `session.setScope(WithView(settings, view))`: wheel → `ZoomAbout` within `kTimebases`; a drag → `WithView(*pressed_, Pan(ScopeView(*pressed_), GetMouseDragDelta(0).x, width))`, from the settings at the press (per-frame deltas under half a tenth of a division, about 3 px, would round away); `MouseWheelH` → pixels added to `panRest_`, applied the same way from the current settings and reset once the delay moves; double-click or `T` → delay 0. Click → marker; right click / `Esc` → clear. Each gesture frame calls `setScope`, which drops the Sweep in progress (`ScopeState::configure`): in normal mode none completes during a drag; accepted, the stored Sweep is redrawn meanwhile.
- **Header line 1**: `Hook (O)`, `-`, timebase combo (as today), `+`, Trigger combo, `auto`/`normal`, `Stop`/`Run`, state text (`stopped` while stopped).
- **Header line 2**: `position` + the delay field (as today: typed, refused in red), `T`, grey `1 V/div`. The `-`, `+`, `0` delay buttons go.
- **Axis strip** (18 px) over the screen: `DrawAxisLabels`, the off-screen Trigger (replaces the bottom-left `T -800 ns`).
- **Screen**: bands only for hooked Channels in Channel order (none: one band with the hint); `bandHeight = max(50, (avail.y - footer) / bands)` where `footer` holds two Hook row lines and the status line. Grid from `Grid(view)` instead of fixed tenths (same lines when the delay is a whole number of divisions).
- **Stored Sweep at the current settings**: the column cache is keyed by serial, width, rails *and the view*; columns map `shown_` samples through the current `view`, so they fall where they were recorded; columns outside `[shown_->start, shown_->start + 10 * shown_->timebase]` stay empty (nothing drawn). The cache is rebuilt per frame only while the view changes (a drag), at most 4 × width columns: within the UI budget (measured in phase 3).
- **Hook rows (S5)**: two per line (`1`, `2` / `3`, `4`), each: colour, number, label or `free`, then the value at `cursor` (hover or preset) else `marker_`: the sample at that time in `shown_` (binary search on `Sample::time`), `H`/`L`/`X` from `Sample::value`, `Z` when `Sample::floating`, + `LogicVolts` with one decimal, or the rail's volts; nothing outside the Sweep. Then `×`.
- **Status line**: `StatusLine` with the hint `wheel: zoom   drag: position   click: marker   right-click: clear   double-click: T`.

## `bw_ui` (C11)

- `--sample 100ns`: `ParseDuration`, must be one of `kAnalyzerPeriods`; `--rate` removed (usage updated).
- `--view 1us[:+20us]`: time per division (must equal `StepAtLeast` of itself: a 1-2-5 step), optional centre from T (sign allowed: `ParseDuration` on the text after the sign). Applied as `CentredOn` + `Clamp` when the first Capture arrives, instead of the first view; a step outside the analyzer's zoom limits (below the period, above `Fit`) is clamped into them.
- `--marker +2us`, `--cursor +3.7us`: signed times from T. Each window gets `preset(std::optional<TimeView> view, std::optional<Time> marker, std::optional<Time> cursor)`: the scope sets its marker at once; the analyzer at its first Capture (a new Capture clears the marker). The cursor is drawn (line, values, status) whenever the mouse does not hover the canvas.
- `ParseSignedDuration(text)` in `main.cpp`: an optional `+`/`-`, then `ParseDuration`; `0` accepted.

## Recipe (ADD_INSTRUMENT)

- Layers table, Window row: views and gestures through `src/view/time_view.hpp`, marks through `src/ui/instrument_marks.hpp`, so a new instrument window looks and moves like the others.
- CLI row: `--sample`, `--view`, `--marker`, `--cursor`.
- Gotchas: the "analyzer opens fitted to the whole Capture … temporarily set the view" item becomes: "the analyzer opens at its first view (10 points per division around T); `--view`, `--marker`, `--cursor` set the rest for a screenshot". New items: trackpad wheels are fractional: step on whole notches (`WheelNotches`); own `ImGuiKey_MouseWheelX` as well as `Y`. The font gotcha extended: `Δ`, `−`, `◀` are missing from both default fonts too.
- Reference commits for P5f.

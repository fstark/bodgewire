# P6f — TV controls — Design

Spec: [TV_CONTROLS.md](TV_CONTROLS.md). Plan: [TV_CONTROLS_PLAN.md](TV_CONTROLS_PLAN.md). Recipes: [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md) UI layer (a case window: `PlaceNext`, `KeepAspect`, `CaseClose`). No device, Session or focus change; `TvPicture` gains its frame's line count; the taller TV moves the keyboard's default place down (below).

## Files

| File | Change |
|---|---|
| `src/view/tv_controls.{hpp,cpp}` (new, bw_view), `src/view/CMakeLists.txt` | the controls' state and arithmetic (below); bw_view links `bw_devices` (for `kTvColumns`, `kTvRows`) |
| `tests/view/tv_controls_test.cpp` (new), `tests/CMakeLists.txt` | spec, Tests |
| `src/runner/tv_consumer.{hpp,cpp}`, `tests/runner/tv_consumer_test.cpp` | `TvPicture::lines` (below) |
| `src/view/bench_places.hpp` | `kTvAspect` 0.96; `kTvBezel`, `kTvChin`, `kTvPicture` go (the screen's fractions move to `tv_window.cpp`). `kKeyboardTop` follows: 401.6 → 454.4, the keyboard's room 298 → 246 px, so on the 1264 × 716 bench the Typewriter's keyboard comes out about 571 wide (was 640), the BW-80's membrane about 455 (was 552); the BW-2001's stays 640 |
| `src/ui/tv_view.{hpp,cpp}` | the last frame kept, uploaded through a tone (below) |
| `src/ui/tv_window.{hpp,cpp}` | the monitor drawn, its controls live (below) |
| `src/ui/shelf_drawings.cpp` | `Tv`: the new look |
| `src/ui/main.cpp` | `--tv`; `tvWindow.draw(shelf.tvOut, view)` |
| `docs/GLOSSARY.md`, `docs/recipes/ADD_CONNECTOR_DEVICE.md`, `docs/ARCHITECTURE.md` | below |

## TV controls (`src/view/tv_controls.hpp`)

ImGui-free, tested like `focus` and `binder`.

```cpp
// The monitor's four knobs, left to right on its strip (P6f).
enum class TvKnob : std::uint8_t { HPos, VPos, Bright, Contrast };
inline constexpr std::array<TvKnob, 4> kTvKnobs{TvKnob::HPos, TvKnob::VPos, TvKnob::Bright,
                                                TvKnob::Contrast};
const char* Label(TvKnob knob);  // "H-POS", "V-POS", "BRIGHT", "CONTRAST"
double Mark(TvKnob knob);        // its starting turn: 0.5, 0.5, 0.25, 0.75

// The Tv frame's part the screen shows, in Tv cells (columns, rows): [x0, x1) × [y0, y1).
struct PictureWindow {
    float x0 = 0, x1 = kTvColumns, y0 = 0, y1 = kTvRows;
};
inline constexpr float kWindowLeft = 80, kWindowRight = 496, kWindowTop = 20;

// The picture's shift, fractions of the screen's width and height; right and down positive.
struct PictureShift {
    float x = 0, y = 0;
};
// The black level and gain a cell's level goes through.
struct TvTone {
    float black = 0, gain = 1;
    bool operator==(const TvTone&) const = default;
};

// The monitor's controls: the knobs' turns (0..1) and its power switch, for the run.
class TvControls {
public:
    double turn(TvKnob knob) const;
    void set(TvKnob knob, double turn);  // clamped to 0..1
    void reset(TvKnob knob);             // back at its mark
    bool on() const { return on_; }
    void setOn(bool on) { on_ = on; }
    bool underscan() const { return underscan_; }  // the hidden trimmer
    void setUnderscan(bool underscan) { underscan_ = underscan; }
    // Underscan: the whole frame. Else columns kWindowLeft..kWindowRight and rows kWindowTop to
    // the frame's last painted row: a frame of `lines` line starts paints rows 0..lines (row 0 the
    // part before the first, Tv::startFrame/startLine), so y1 = min(lines + 1, kTvRows); kTvRows
    // when `lines` is under kWindowTop + 100 (no frame yet, or no sensible one).
    PictureWindow window(int lines) const;
    // H-POS and V-POS: (turn − 0.5) / 2 of the picture each way; clockwise right, and up.
    PictureShift shift() const;
    // CONTRAST: gain = turn / 0.75 (0..4/3); BRIGHT: black = (turn − 0.25) · 0.4 (−0.1..0.3).
    TvTone tone() const;

private:
    std::array<double, 4> turns_{0.5, 0.5, 0.25, 0.75};
    bool on_ = true;
    bool underscan_ = false;
};

// A cell's level (0 black .. 1 white, a Tv cell) as an 8-bit grey: lround(clamp(black + gain ·
// cell, 0, 1) · 255). At the marks, today's TvView grey.
std::uint8_t Grey(float cell, TvTone tone);

// --tv: comma-separated `hpos:N%`, `vpos:N%`, `bright:N%`, `contrast:N%` (N 0..100, a decimal
// allowed) `off` and `underscan`, applied over `start`; nullopt on any bad item.
std::optional<TvControls> ParseTvSettings(std::string_view list, TvControls start = {});
```

`shift()`: `{(turn(HPos) − 0.5) / 2, −(turn(VPos) − 0.5) / 2}`. `Grey` computes in float as `TvView::show` does today, so the marks give the same bytes (the test checks `Grey(c, {}) == lround(clamp(c, 0, 1) · 255)` for c over −0.5..1.5 in steps of 1/512). `ParseTvSettings` splits on commas itself (no `main.cpp` helper in bw_view); the percent parsed by `ParseKnob`'s digit loop (`src/machine/knob_setting.cpp`, copied: bw_view does not link bw_machine), so `-1%`, `1e1%` and `inf%` are refused. The test cases are named `tv controls: …`, tagged `[view]` (ctest's names are the Catch names, so `-R "tv controls"` selects them).

## The frame's line count (`src/runner/tv_consumer.hpp`)

`TvPicture` gains `int lines = 0;  // lines started in the frame (0 when off or before the first)`; `TvConsumer::advanceTo` publishes `{tv_.completedFrame(), tv_.isLocked(), tv_.completedStats().lines}`. `tests/runner/tv_consumer_test.cpp`: a picture published from a known signal carries its line count (the existing MONITOR test's `Feed`: `vdrive` falls every 260 lines, so `lines == 260`).

## The tone on upload (`src/ui/tv_view.hpp`)

```cpp
void show(const TvPicture& picture);  // keeps the frame, uploads it through the tone
void setTone(TvTone tone);            // a change uploads the kept frame again
```

`int lines() const`: the last *locked* picture's `lines` (0 before one, so the window's 320), so the window keeps still while the Tv is unlocked, free-running or off. `show` copies `picture.frame.cells` into a kept `std::vector<float> cells_` (512 × 320 floats, once a new picture arrives: at most the TV's 60 frames a second) and calls `upload()`; `upload()` is today's loop with `Grey(cell, tone_)`. `setTone` returns at once when the tone is unchanged (every frame), so a turned knob costs one upload, and a paused Machine's picture follows the knobs. The constructor uploads black as today.

## The monitor (`src/ui/tv_window.hpp`)

```cpp
class TvWindow {
public:
    void draw(bool& open, TvView& view);
    TvControls& controls() { return controls_; }  // --tv
private:
    TvControls controls_;
};
```

`draw` first calls `view.setTone(controls_.tone())` (out or away, so a `--tv` tone is uploaded before the first picture is drawn), then returns when `!open`. The window as today (`PlaceNext(Tv, {kTvFirstWidth, kTvFirstWidth · kTvAspect})`, `KeepAspect(kTvAspect, 380)` (labels 9 px at least), padding 0, `NoTitleBar | NoSavedSettings | NoScrollbar | NoScrollWithMouse | NoCollapse`), `WindowBg` the shell, rounding 6.

**Geometry**, fractions of the width `w` from the window's top left (`at`), as measured on the photo:

| | |
|---|---|
| shell | the window, cream `(226, 220, 200)`, a darker edge line `(170, 164, 148)` |
| front | inset 0.025 `w` left, right and top, to the bottom; charcoal `(48, 49, 52)`, its top corners rounded 0.03 `w` (`RoundCornersTop`), the bottom square to the window's; a lighter top edge `(78, 79, 84)` 1 px |
| bezel | the screen's rect grown by 0.018 `w`, `(30, 30, 33)`, rounding 0.06 `w` |
| screen | x 0.105 to 0.895 (0.79 `w`), y from 0.13 `w`, 4:3 (0.5925 `w` high); rounding `r` 0.045 `w` |
| strip | x 0.09 to 0.91, y 0.76 to 0.925 `w`; `(24, 24, 26)`, a silver frame `(165, 165, 168)` 1.5 px, rounding 0.01 `w` |
| labels | `H-POS`, `V-POS`, `BRIGHT`, `CONTRAST` centred above each knob at y 0.79 `w`; `POWER` above the switch; `ON` left and `OFF` right of it; font 0.024 `w`, `(210, 210, 210)` |
| knobs | centres x 0.17, 0.285, 0.40, 0.515 `w`, y 0.86 `w`; a black skirt radius 0.026 `w` `(14, 14, 15)`, a chrome cap radius 0.019 `w` (a vertical `AddRectFilledMultiColor`-like shading is not available for discs: a light disc `(205, 200, 190)` with a smaller offset highlight `(240, 238, 232)` up-left), a dark notch line `(40, 40, 42)` 0.004 `w` from the cap's centre at 0.6 to 1.0 of its radius at the knob's angle (`kStart` + turn · `kSweep`, as the paddle's: 270° from lower left) |
| lamp | x 0.60 `w`, y 0.86 `w`, radius 0.008 `w`: on `(235, 45, 35)` with a halo `(235, 45, 35, 70)` radius 0.016 `w`; off `(85, 22, 20)` |
| switch | a slot x 0.70 to 0.82 `w`, y 0.845 to 0.875 `w`, `(10, 10, 11)`; the slider half the slot's width, `(60, 60, 64)` with a light top line, at the left (ON) or right (OFF); `POWER` at y 0.80 `w` centred on the slot, `ON` and `OFF` at the slot's mid height outside it |
| trimmer | below (unlabelled) |
| `×` | `CaseClose()` (top right, on the shell and front) |

**The screen** (`PushClipRect(screen)`): OFF, the glass `(36, 40, 38)`; ON, black, then the whole frame drawn so that the window fills the screen, moved by the shift: with `win = controls_.window(view.lines())`, `scale = {size.x / (win.x1 − win.x0), size.y / (win.y1 − win.y0)}`, the image's top left `min + shift · size − {win.x0 · scale.x, win.y0 · scale.y}` and its size `{kTvColumns · scale.x, kTvRows · scale.y}` (`AddImage`, clipped by the screen). `TvView::lines()` gives the window's bottom. `PopClipRect`, then the corners rounded by drawing over them: `AddRect(screen grown by r/2, …, kBezel, r · 1.5, 0, r)` (a stroke `r` wide centred `r/2` outside the screen's edge, its inner edge rounded `r`, covers the square corners), clipped to the bezel's rect so it does not widen the bezel. Then a faint glass sheen: a 1 px line `(255, 255, 255, 18)` along the top inside edge. The picture is drawn on the draw list, not as an item, so a drag on it moves the TV, as before.

**The knobs** (each under `PushID`): an `InvisibleButton` of the skirt's square grown to 0.032 `w` radius (so a press there turns the knob, not the case). Active with a vertical mouse delta: `set(turn − dy / kDragTurn)`. Hovered: own the wheel (`SetItemKeyOwner(ImGuiKey_MouseWheelY)`), `set(turn + wheel · kWheelStep)`. `kDragTurn` 200, `kWheelStep` 0.01, `kStart`, `kSweep` copied from `knob_window.cpp` (its anonymous namespace); a double-click (`IsMouseDoubleClicked(0)` while hovered) `reset`; tooltip `H-POS 31 %` then `drag up or down, or the mouse wheel; double-click: back to its mark`; a hover disc `(255, 255, 255, 30)` over the skirt.

**The switch**: an `InvisibleButton` over the slot; a click `setOn(!on())`; tooltip `POWER: the monitor's own switch (the Machine's is on the shelf)`.

**The trimmer**: in the front below the strip, centre x 0.87 `w`, y 0.945 `w`, radius 0.009 `w`: a disc a shade darker than the front `(40, 41, 44)` with a 1 px slot line `(20, 20, 22)` across it at 30°, no label. An `InvisibleButton` of radius 0.014 `w` (inside the case's 0.96 `w`); a click toggles `setUnderscan`; tooltip `UNDERSCAN trimmer: the whole frame` when off, `UNDERSCAN trimmer: the picture as a real monitor shows it` when on. The case height leaves room: the front reaches 0.96 `w`.

`CaseClose()` last, as today.

## Shelf drawing (`shelf_drawings.cpp`, `Tv`)

In the 56 × 56 cell: the shell `(220, 214, 194)` from (7, 6) to (49, 47), rounding 4; the front `(50, 50, 54)` inset 2 px left, right and top; the screen (12, 10) to (44, 34), rounding 3, lit `kTvOn` / `kScreenOff` as today; the strip `(24, 24, 26)` (11, 37) to (45, 44); four light dots `(200, 196, 186)` radius 1.3 at x 15, 19.5, 24, 28.5, y 40.5; a red dot at x 33 (lit when out, dark away: the shelf shows out or away, not the monitor's switch); two dark feet as today's legs shortened (y 47 to 50).

## `main.cpp`

`Options::tv`: `std::optional<TvControls>`; `--tv LIST` parsed by `ParseTvSettings`, a bad list a usage error (as `--knob`). In `main()`, beside the `--knob` check: with `options.tv` and no `session.tv()`, `bw_ui: this Machine has no TV` and usage, exit `kExitUsage`. In `RunWindow`, after `TvWindow tvWindow;`: `tvWindow.controls() = *options.tv`. Usage line: `[--tv hpos:N%,vpos:N%,bright:N%,contrast:N%,off,underscan]`. The draw call drops `session.powered()`.

## Recipes and glossary

- GLOSSARY, **Equipment**: the TV a monitor in a small 1970s set's look, its strip's `H-POS`, `V-POS` (the picture's place), `BRIGHT`, `CONTRAST` and its own `POWER` switch, kept for the run (P6f).
- ADD_CONNECTOR_DEVICE, the monitor bullet: the controls are UI only (`src/view/tv_controls`): the Tv device shows the same frame whatever they are.
- ARCHITECTURE: the P6f row done, with the date and links; P7's note on the monitor's adjustments marked answered by P6f; a new row after P6h, **P6i Picture margins**: the early Machines (Test Pattern, Bouncing Ball, Tennis, Message, Typewriter) start their pictures at the sync edge and the frame's first line, so a real monitor's window (P6f) cuts their edges; give each a back porch and top blank lines so its picture sits inside the window (the user, 2026-11-17). The hold knobs for later in the P6f row (the user: a computer whose signal is not perfect, a monitor not exactly calibrated at start).

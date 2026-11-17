# P6d — Equipment objects — Design

Spec: [EQUIPMENT_OBJECTS.md](EQUIPMENT_OBJECTS.md). Plan: [EQUIPMENT_OBJECTS_PLAN.md](EQUIPMENT_OBJECTS_PLAN.md). UX: [UX_DESIGN.md](../p6b-ux-design/UX_DESIGN.md#equipment-on-the-bench). Recipes: [ADD_INSTRUMENT.md](../recipes/ADD_INSTRUMENT.md) Window and CLI layers; [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md) UI layer. No engine, consumer, Session, focus or placement change.

## Files

| File | Change |
|---|---|
| `src/view/bench_places.{hpp,cpp}` (new, bw_view), `src/view/CMakeLists.txt` | places, sizes, presets (below) |
| `tests/view/bench_places_test.cpp` (new), `tests/CMakeLists.txt` | spec, Tests |
| `src/ui/bench.{hpp,cpp}` (new) | the bench area above the shelf, `PlaceNext`, the case `×`, the aspect constraint (below) |
| `src/ui/instrument_marks.{hpp,cpp}` | `BeginInstrument(name, open, BenchObject)`: place, first size, `NoCollapse`, the size button (below) |
| `src/ui/analyzer_window.cpp`, `scope_window.cpp`, `meter_window.cpp` | their `SetNextWindowSize`/`Pos` go; `BeginInstrument` with their object |
| `src/ui/tv_view.{hpp,cpp}` | `draw(ImVec2)` replaced by `texture()` (the window draws the picture) |
| `src/ui/tv_window.{hpp,cpp}` (new) | the TV case (below) |
| `src/ui/knob_window.{hpp,cpp}` (new) | the knob boxes; `DrawKnobs`' keys move here (below) |
| `src/ui/keyboard_window.{hpp,cpp}`, `matrix_keyboard_window.{hpp,cpp}` | `NoTitleBar`, the case `×`, `PlaceNext(Keyboard, …)`, the first width fitted (below); `titleHeight_` gone |
| `src/ui/manual_view.{hpp,cpp}`, `src/ui/manual_window.{hpp,cpp}` (new) | `ManualWindows` moves to `manual_window` (it now holds a `ChipPanel`, whose header includes `manual_view.hpp`): the `Manuals` window with `User Manual`, `Chip`, `Repairs` tabs |
| `src/ui/brief_window.{hpp,cpp}` (new) | the brief note |
| `src/ui/side_panel.{hpp,cpp}` | deleted; `DebugStats` and the Debug section to `src/ui/debug_window.{hpp,cpp}` (new), `SentText` with it |
| `src/ui/shelf.{hpp,cpp}`, `shelf_drawings.{hpp,cpp}` | `kBoardShare` gone; `Item::Brief` and its drawing; `briefOut`, `hasBrief`; the TV and knob toggle their windows by name (`ToggleWindow`); the manual's tooltip `Manuals` |
| `src/ui/main.cpp` | the Board across the whole width; the windows drawn in place of the side panel; `--debug`, `--manual chip` (below) |
| `machines/110-led-computer/docs/LED_COMPUTER_DESIGN.md` | "the Debug panel's Seed" → the `--debug` window's |
| `src/ui/CMakeLists.txt` | new files in, `side_panel.cpp` out |
| `src/ui/board_view.hpp` | `TargetHits`' comment (no side panel) |
| `docs/recipes/ADD_INSTRUMENT.md`, `ADD_CONNECTOR_DEVICE.md`, `docs/GLOSSARY.md`, `docs/ARCHITECTURE.md` | below |

## Places and presets (`src/view/bench_places.hpp`)

ImGui-free, so tested like `focus` and `time_view`. Coordinates are logical px in the **bench area**: the main window's content area above the shelf (where the Board is drawn), origin at its top left; 1264 × 716 in a 1280 × 800 window (padding 8, the shelf 64, spacing 4 above it).

```cpp
// The objects bw_ui places on the bench (P6d): the equipment, and the brief and Debug windows.
enum class BenchObject : std::uint8_t {
    Tv, Keyboard, Knob, Analyzer, Scope, Meter, Manual, Brief, Debug
};

struct Extent { float width = 0, height = 0; };
// The window's pivot (0..1 of its size) goes to (x, y): (1, 0) puts its top-right corner there.
struct Place { float x = 0, y = 0, pivotX = 0, pivotY = 0; };

inline constexpr float kBenchMargin = 16;
// The TV case's proportions, fractions of its width: bezel each side and on top, picture 4:3,
// chin below.
inline constexpr float kTvBezel = 0.06f, kTvChin = 0.12f;
inline constexpr float kTvPicture = 1 - 2 * kTvBezel;               // width
inline constexpr float kTvAspect = kTvBezel + kTvPicture * 0.75f + kTvChin;  // height / width
inline constexpr float kTvFirstWidth = 440;
inline constexpr float kKeyboardFirstWidth = 640;
inline constexpr Extent kKnobBox{140, 160};  // one paddle controller

Place DefaultPlace(BenchObject object, Extent bench);
// The first size of an object that is not an instrument: the TV and knob from their constants
// (`count` paddles), the keyboard kKeyboardFirstWidth narrowed until it fits between its place and
// the bench's bottom margin (`aspect` height / width), the manuals, brief and Debug windows' sizes.
Extent FirstSize(BenchObject object, Extent bench, float aspect = 1, int count = 1);
// Instruments only (assert otherwise): the two presets; the first size is normal.
Extent NormalSize(BenchObject instrument);
Extent MinimalSize(BenchObject instrument);
// The size button: the preset further from `current` (by the sum of the two sides' distances).
Extent NextPreset(Extent current, Extent minimal, Extent normal);
```

`DefaultPlace` (m = `kBenchMargin`, W × H the bench):

| Object | x, y | pivot |
|---|---|---|
| Tv | W − m, m | 1, 0 |
| Keyboard | W − m, m + `kTvFirstWidth · kTvAspect` + m | 1, 0 |
| Knob | m, m | 0, 0 |
| Analyzer | m, H − m | 0, 1 |
| Scope | W / 2, H − m | 0.5, 1 |
| Meter | W − m, H − m | 1, 1 |
| Manual | m, m + `kKnobBox.height` + m | 0, 0 |
| Brief | W / 2, m | 0.5, 0 |
| Debug | m, H / 2 | 0, 0.5 |

Sizes: Analyzer 940 × 440 / 560 × 260, Scope 680 × 460 / 420 × 300, Meter 320 × 200 / 240 × 150 (normal / minimal). First sizes (no presets): Manual 480 × 460, Brief 340 × 120 (clear of the TV at the top centre), Debug 420 × 140. On the 1264 × 716 bench: the TV case 440 × 370; the keyboard's top at 402, room for 298 px: the BW-2001's chiclets (aspect 0.36) and the typewriter (0.43) keep 640 px, the ZX80's membrane (0.54) narrows to about 550. The instruments overlap when out together at normal size (spec).

## Bench helpers (`src/ui/bench.hpp`)

```cpp
// The bench area in screen coordinates: the main viewport's work area less the shelf, the main
// window's bottom padding and the spacing above the shelf.
ImRect BenchArea();  // ImVec2 min, max (imgui_internal's ImRect)
// SetNextWindowPos at DefaultPlace (ImGuiCond_Appearing: each time it comes out) and
// SetNextWindowSize(first, ImGuiCond_FirstUseEver).
void PlaceNext(BenchObject object, ImVec2 first);
// SetNextWindowSizeConstraints keeping height = width * aspect, at least `minWidth` wide.
void KeepAspect(float aspect, float minWidth);
// The case's put-away mark: a 14 px circle with a ×, `inset` px in from the top-right corner of
// the current window; faint, bright when hovered. True when clicked.
bool CaseClose(float inset = 6);
```

`BenchArea` is the main viewport's work area less the main window's `WindowPadding` on each side, `kShelfHeight` and `ItemSpacing.y` at the bottom: the rect the Board is drawn in; `main.cpp` sizes the Board from it. `KeepAspect` stores the aspect in a static the callback reads (one window at a time sets it before its `Begin`), as the keyboards' `titleHeight_` did.

**Dragging a case.** A window with `NoTitleBar` and no `NoMove` moves when dragged on any of its area not taken by an item (ImGui's default, `ConfigWindowsMoveFromTitleBarOnly` false). The TV picture is an `Image` (no item ID: a drag there moves the TV, as a real set is picked up anywhere); the keyboards' caps and the knobs are buttons, so a press there types or turns instead. The resize grip in the bottom-right corner stays (as on today's keyboards).

## Instrument windows (`instrument_marks`)

`BeginInstrument(const char* name, bool& open, BenchObject object)`:

1. `PlaceNext(object, NormalSize(object))`.
2. `ImGui::Begin(name, &open, NoSavedSettings | NoCollapse)`.
3. When visible, the **size button**: from `ImGui::GetCurrentWindow()->TitleBarRect()` (imgui_internal), a square of the title bar's height left of the close button (whose square is the last `GetFrameHeight()` px, as `InstrumentClicked` assumes). Saved cursor; `PushClipRect(title.Min, title.Max, false)` (not intersected: after `Begin` the clip rect is the inner rect, without the title bar), `SetCursorScreenPos`, `InvisibleButton("##size")`, drawn as two nested squares in `ImGuiCol_Text` (bright when hovered), tooltip `Minimal / normal size`; `PopClipRect`, cursor restored. Clicked → `SetWindowSize(NextPreset(GetWindowSize(), MinimalSize, NormalSize))`.

A click on the size button counts as a click in the window (it gives the instrument the focus, as a click on its title bar does); only the close button is excluded (P6c). The three windows drop their own `SetNextWindowSize`/`SetNextWindowPos` lines and the analyzer its `shelf.hpp` include.

## The TV (`src/ui/tv_window.hpp`)

```cpp
inline constexpr const char* kTvWindowName = "TV";
class TvWindow {
public:
    void draw(bool& open, const TvView& view, bool powered);  // powered: the pilot lamp
};
```

`PlaceNext(Tv, {kTvFirstWidth, kTvFirstWidth * kTvAspect})`, `KeepAspect(kTvAspect, 200)`, padding 0, `Begin(kTvWindowName, &open, NoTitleBar | NoSavedSettings | NoScrollbar | NoScrollWithMouse | NoCollapse)`, window background the case colour. Drawn from the window's rect (width `w`): the case a rounded rect, dark grey (`IM_COL32(52, 52, 56)`, a lighter top edge); the picture's black well at (`kTvBezel·w`, `kTvBezel·w`) with a slight lip, and `ImGui::Image(view.texture(), {kTvPicture·w, 0.75·kTvPicture·w})` in it; in the chin a small green pilot lamp lit while `powered`, two small dummy knobs at the right. `CaseClose()`. A Machine without a TV never has the window (the shelf grays the TV; `main.cpp` passes `open` false).

## The knob (`src/ui/knob_window.hpp`)

```cpp
inline constexpr const char* kKnobWindowName = "Knob";
class KnobWindow {
public:
    // Up/Down (Q/A for the first of two) turn the knobs, out or away, unless text input has the
    // keyboard: DrawKnobs' keys as they were.
    void keys(Session& session);
    void draw(bool& open, Session& session);
};
```

The knobs found once (`session.knobs()` scans the parts, as the shelf learned in P6c). `PlaceNext(Knob, FirstSize(Knob, bench, 1, n))`, `KeepAspect(160 / (n·140), 0.6·n·140)` (height / width). One box per controller side by side, scaled by `s = width / (n·140)`: a dark rounded box, a large black knob (radius 44·s) with a ridged rim and a white pointer at angle −135° + 270°·turn, the reference printed under it, a tick at each end of the sweep. The knob is an `InvisibleButton` over its circle: dragging it turns by −`MouseDelta.y` / 200 a px (up clockwise), the wheel by `kKnobWheelStep` per notch with `SetItemKeyOwner(MouseWheelY)`; tooltip as the slider's was. `CaseClose()`.

## Keyboards

Both windows: `NoTitleBar` added; `titleHeight_` and its term in the size constraint go (`KeepAspect(aspect, kMinWidth)` replaces the hand callback); `PlaceNext(Keyboard, FirstSize(Keyboard, bench, aspect))` replaces their position, `kInitialWidth` gives way to `kKeyboardFirstWidth`; `CaseClose()` after the caps; `NoFocusOnAppearing` kept. The matrix window's ID (`Keyboard J5###matrix keyboard`) stays as it is. The case `×` sits in the case's top-right margin: the ASCII and PET cases have a label strip on the left only; the ZX80's top band holds the line and words drawn from the left, the right end clear (checked in the screenshots).

## Manuals window (`manual_view`)

`ManualWindows::draw(bool& open, const Manuals&, BoardView&, const LoadedCircuit&, std::span<const std::string> log)` (`Session::log()`'s type). `kManualWindowName` becomes `"Manuals###manuals"` (`InFront` and `SetWindowFocus` hash from the `###`). `PlaceNext(Manual, FirstSize(Manual, …))`. `showChip()` makes the next draw select the Chip tab (`ImGuiTabItemFlags_SetSelected`), for `--manual chip`. A tab bar: `User Manual` (today's view, `no user manual`); with a Board, `Chip` (the side panel's Chip section: `board.route(chip_.draw(*scene, *chip, manuals, design))` or `hover or click a chip`) and `Repairs` (the log or `none yet`).

## Brief (`src/ui/brief_window.hpp`)

`kBriefWindowName = "###brief"`; `DrawBrief(bool& open, const Loaded&)`: `PlaceNext(Brief, FirstSize(Brief, …))`, title bar `Puzzle brief`, the Puzzle's title at 1.3× and the brief wrapped, a pale yellow window and title bar (`ImGuiCol_WindowBg`, `TitleBg`, `TitleBgActive` pushed before `Begin`), dark text pushed after `Begin` (pushed before, it would darken the title and its close cross), `NoSavedSettings | NoCollapse`. Shown only when `loaded.brief` is not empty.

## Debug (`src/ui/debug_window.hpp`)

`DebugStats` as today; `DrawDebug(bool& open, Session&, const DebugStats&, std::uint64_t& seed)`: `PlaceNext(Debug, FirstSize(Debug, …))`, window `Debug`, the side panel's Debug section's content. `--debug` (usage updated) sets it open at start; closing it closes it for the run.

## Shelf

- `Item::Brief` after `Item::Knob`: `{Item::Brief, "Puzzle brief", nullptr, 1}`; its drawing a yellow square note, slightly turned, three grey lines, its corner folded; lit when out (brighter yellow); grayed when `!hasBrief` (`No brief: not a Puzzle`). Click → `ToggleWindow(briefOut, kBriefWindowName)`.
- `Item::Tv` → `ToggleWindow(tvOut, kTvWindowName)`; `Item::Knob` → `ToggleWindow(knobOut, kKnobWindowName)`: they are windows now, brought to the front as the keyboard is.
- The manual's entry `Manuals: User Manual, chip, repairs`; the knob's `Knob: drag it, mouse wheel, or the arrow keys`. Eighteen items: the hint keeps about 230 px at 1280.
- `kBoardShare` removed (its users were the side panel's split and the keyboards' first position).

## `main.cpp`

- Defaults: `shelf.tvOut = session.tv() != nullptr`, `shelf.knobOut = !session.knobs().empty()`, `shelf.briefOut = shelf.hasBrief = !loaded.brief.empty()`; the rest as P6c.
- Layout: `boardView.draw({available.x, available.y})` with `available` from `BenchArea()`'s height; no `SameLine`, no `side` child; then the shelf; `End`.
- After the main window, in this order (later ones in front at start): brief, TV, knob, manuals, instruments, keyboards, Debug. `knobWindow.keys(session)` every frame before them. Each window's `open` is its shelf flag; a case closed by its `×` clears it.
- `--debug`: `Options::debug`, a `bool debugOut`; the first flag with no value, so `ParseOptions` takes it before its `i + 1 >= argc` check. `--manual user|chip`: `chip` also calls `manualWindows.showChip()`. Usage updated.

## Recipes and glossary

- ADD_INSTRUMENT, Window row: `BeginInstrument(name, open, BenchObject)` places it (a `BenchObject`, a default place and two sizes in `src/view/bench_places.cpp`, tested) and gives it the size button; no `SetNextWindowPos`/`Size` of its own. Gotcha: a window placed with `ImGuiCond_Appearing` returns to its place every time it comes out; `FirstUseEver` keeps a size for the run.
- ADD_CONNECTOR_DEVICE, UI row: the device is a case window (`src/ui/knob_window.cpp`, `src/ui/bench.hpp`: `PlaceNext`, `KeepAspect`, `CaseClose`, no title bar), out from its shelf drawing; its keys work while it is away; the side panel is gone.
- GLOSSARY, **Equipment**: each object has a default place it returns to when brought out, and a free size; the instruments have two presets, minimal and normal, from their title bar; the TV, keyboard and knob are drawn cases with no title bar.
- ARCHITECTURE: the P6d row done; the P6e row's "the Chip section moves in" becomes the Chip tab already in the manuals window.
- ADD_INSTRUMENT, CLI row: `--manual chip` among the screenshot flags; the Window row's "first position above the shelf" goes.

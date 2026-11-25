# P8f — Touch on tablets — Design

How [TOUCH.md](TOUCH.md) is built. Plan: [TOUCH_PLAN.md](TOUCH_PLAN.md). References in parentheses (B4, §R3, P1) are the spec's.

Untouched: the kernel, the Parts, the Machines, the Boards, the Session, the Runner, `bw`, `BoardScene::pick`. New: the fingers and their script in `src/view`, their glue, the view buttons and the controls in `src/ui`. Changed: the Board's input, `TimeCanvas::read`, every window's sizes, the Launcher, the Shelf, the web page.

## Files

| File | Change |
|---|---|
| `src/view/fingers.hpp/.cpp` (new) | `FingerEvent`, `PointerChange`, `FingerFrame`, `Fingers`; `FingerScript`, `ScriptEvents` |
| `src/view/number_pad.hpp/.cpp` (new) | `PadKind`, `PadKeys`, `PadTyped` |
| `src/view/viewport.hpp/.cpp` | `Viewport::magnified`, `Viewport::frameUpTo` |
| `src/view/time_view.hpp` | `SnapToEdge` |
| `src/view/bench_places.hpp/.cpp` | `kViewLane`; the keyboard's and the Multimeter's `DefaultPlace`; `NextPreset` with the full size |
| `src/view/shelf_layout.hpp/.cpp` | `ShelfWidth` |
| `src/view/binder.hpp/.cpp` | `Binder::frame` offering the Contents tab |
| `src/view/bench_options.hpp/.cpp` | `--touch`, `--tap`, `--double-tap`, `--hold`, `--drag`, `--pinch`, `--window` |
| `src/ui/touch.hpp/.cpp` (new) | the fingers' glue: SDL events in, ImGui's pointer and `Touch()` out, the tolerances, the typing zones |
| `src/ui/controls.hpp/.cpp` (new) | `ApplyStyle`, `Stepper`, `Choice`, `ChoicePopup`, `NumberField`, `DragScroll`, `CornerGrip`, `PinchResize` |
| `src/ui/view_buttons.hpp/.cpp` (new) | the view buttons' column |
| `src/ui/main.cpp` | both frame loops fed by `touch`; lit-first repairs; the question above the pointer; the turn notice |
| `src/ui/tool_click.hpp/.cpp` | `SameRepair` |
| `src/ui/board_view.hpp/.cpp` | pinch, the chip's double-click, the finger's reach, the view buttons, the loupe, the cancel |
| `src/ui/gestures.hpp/.cpp` | `TimeCanvas::read`: pinch, the placed cursor, the flag's click, the pointer's slop, the vertical drag |
| `src/ui/bench.hpp/.cpp`, `instrument_marks.cpp` | the close disc, the first size, the fill, the size button's three sizes, the flag |
| `src/ui/analyzer_window.hpp/.cpp`, `scope_window.hpp/.cpp`, `meter_window.cpp` | the controls, the rows, the snap, the hints |
| `src/ui/manual_window.cpp`, `manual_view.hpp/.cpp`, `chip_panel.cpp`, `contents_page.cpp` | drag to scroll, a link's click at its release, the pinout's rows and "show", the Contents page's settings |
| `src/ui/tv_window.cpp`, `matrix_keyboard_window.hpp/.cpp`, `keyboard_window.cpp`, `knob_window.cpp` | the TV's hits and glass; the Hold switch; the typing zones; wording |
| `src/ui/launcher.hpp/.cpp`, `shelf.cpp`, `question_popup.cpp`, `thank_you_card.cpp`, `brief_window.cpp` | Stop, the drags, the forget hint, the narrow Shelf, the popups' sizes and places |
| `src/ui/CMakeLists.txt`, `src/view/CMakeLists.txt`, `tests/CMakeLists.txt` | the new sources and tests; the manifest copied beside the page |
| `src/ui/web/shell.html`, `src/ui/web/manifest.json` (new) | the page's touch behaviour, the home-screen app |
| `.github/workflows/web.yml` (on `main` and `touch`) | both branches built: `main` at the site's root, `touch` under `/touch/`; the manifest published |
| `tests/view/fingers_test.cpp`, `number_pad_test.cpp` (new); `viewport_test.cpp`, `time_view_test.cpp`, `bench_places_test.cpp`, `shelf_layout_test.cpp`, `binder_test.cpp`, `bench_options_test.cpp` | the cases below |
| `docs/GLOSSARY.md`, `AGENTS.md`, `docs/recipes/ADD_INSTRUMENT.md`, `docs/ARCHITECTURE.md`, `docs/architecture-review/ARCHITECTURE_REVIEW.md` | §11 |

## 1. The fingers (§R1, §R3; P1)

### `src/view/fingers.hpp`

```cpp
// One finger's event, in window px.
struct FingerEvent {
    enum class Kind : std::uint8_t { Down, Move, Up, Cancel };  // Cancel: the system took it
    Kind kind;
    std::uint64_t finger;  // SDL's id
    float x, y;
    bool typing = false;  // Down: on a keyboard's keys (a typing zone)
};

inline constexpr float kTapSlop = 10;       // px a finger may move and still tap or hold
inline constexpr double kHoldTime = 0.4;    // s without moving: a hold
inline constexpr double kLingerTime = 2.5;  // s the pointer stays after a tap or a hold
inline constexpr float kPinchSlop = 10;     // px two fingers' distance or centre must change first

// A change of the one pointer ImGui sees, in order.
struct PointerChange {
    bool present;  // false: parked off the screen
    float x, y;
    bool down;
};

// What the fingers are doing this frame.
struct FingerFrame {
    std::vector<PointerChange> pointer;  // since the last frame
    bool cancelled = false;  // this frame: the press under way is void (its release clicks nothing)
    bool hold = false;       // one finger, down kHoldTime within kTapSlop: until it lifts
    bool two = false;        // two fingers down: the pointer at their centre, up
    float pinch = 1;         // their distance over last frame's, once past kPinchSlop
    float panX = 0, panY = 0;  // their centre's move since last frame, px, once past kPinchSlop
};

class Fingers {
public:
    void event(const FingerEvent& event, double now);  // seconds, any origin
    FingerFrame frame(double now);                     // once a frame, after its events
    void reset();  // no finger down any more (focus lost): the pointer parked, nothing clicked
    void mouse();  // a real mouse moved: no linger left to park it
};
```

The rule, as states:

- **None.** The pointer parked, or lingering until its time is out (`frame` parks it).
- **One.** A `Down` puts the pointer there, then down. Past `kTapSlop` it is a drag. Still for `kHoldTime` (seen by `frame`), it is a hold, and stays one whatever it does next. Its `Up`: the pointer up; after a tap or a hold it lingers `kLingerTime`, after a drag it is parked. Its `Cancel`: as a second finger's cancel, then parked.
- **Two.** A second `Down` cancels the first: `cancelled` for that frame, the pointer up **where it is** (never off the screen while down: a hand-written drag would read that place), then at the two fingers' centre, where it hovers. If the first `Down` has not left for ImGui yet (both in one frame), it never does. Each frame, once the distance or the centre has changed by `kPinchSlop` since the second `Down`: `pinch` and `pan`. The first `Up` or `Cancel` ends it: the pointer parked, the other finger ignored until it lifts. A third finger is ignored.
- **Typing.** A second `Down` in a typing zone while the first finger went down in one is no gesture: the first is released where it is (its key is up), and the second becomes the one finger. Two thumbs type.

A fast tap whose `Down` and `Up` reach the same frame still gives ImGui its three changes (there, down, up): ImGui's queue spreads them over frames (its trickle for a touch screen, `imgui.cpp` `UpdateInputEvents`).

What a second finger cannot undo: a press that acted at once (a Board button, a slide Switch, the TV's POWER). The spec says so.

### `FingerScript`

```cpp
// A scripted gesture (--tap, --double-tap, --hold, --drag, --pinch), window px.
struct FingerScript {
    enum class Kind : std::uint8_t { Tap, DoubleTap, Hold, Drag, Pinch };
    Kind kind;
    float x, y;
    float dx = 0, dy = 0;  // Drag
    float factor = 1;      // Pinch: the fingers' distance at the end over the start's (60 px)
};
struct TimedFingerEvent {
    double at;  // s from the screen's start
    FingerEvent event;
};
// Every script's events in time order: the k-th starts at 0.3 s + 0.6 s × k; a tap lifts 50 ms
// later; a double tap is two taps 150 ms apart; a drag and a pinch move in 12 steps over 0.3 s,
// then lift; a hold never lifts.
std::vector<TimedFingerEvent> ScriptEvents(std::span<const FingerScript> scripts);
```

### Tests (`tests/view/fingers_test.cpp`, cases `fingers: …`, `[view]`)

a tap: there, down, up, lingering, parked after 2.5 s; a down and an up in one frame; a slide within the slop is a tap, past it a drag that parks; a hold after 0.4 s, kept while sliding, lingering after; a finger that moved first never holds; a new tap during the linger; a second finger cancels: `cancelled`, up where it was, then the centre; two downs in one frame leave no press; the pinch's ratio and the pan, nothing inside the slop; the first of two to lift ends it and the other is ignored; a third finger ignored; a `Cancel` of one finger; typing: the second key takes over, no gesture; `reset`; `mouse` ends the linger without parking; the script's events (a tap's times, a double tap's, a drag's, a pinch played through the fingers ends at its factor).

## 2. The pointer's glue (§R2; P1–P3)

### `src/ui/touch.hpp`

```cpp
// Once, before SDL_Init: SDL_HINT_TOUCH_MOUSE_EVENTS "0" (no mouse made from the first finger),
// SDL_HINT_PEN_TOUCH_EVENTS "0" (a pen is a mouse only).
void TouchInit();
// Each screen (the Launcher, a bench), after its ImGui context: fresh fingers, the script's clock
// at 0. `emulated` (--touch): the mouse stands in for one finger. `script`: px of `window`.
void TouchStart(SDL_Window* window, bool emulated, std::span<const FingerScript> script);
// In the event loop, first: true for an event that is the fingers' and not ImGui's backend's.
bool TouchEvent(const SDL_Event& event);
// After the backend's NewFrame (gpu.newFrame()), before ImGui::NewFrame: the script's due events,
// the pointer queued for ImGui, the tolerances.
void TouchNewFrame();
const FingerFrame& Touch();  // this frame's; all at rest with a mouse
bool FingerPointer();        // ImGui's MouseSource is the touch screen
// A window whose items are typed on with two fingers (the keyboards): call inside it each frame.
void TouchTypingZone();
// For a scope: no finger padding around the items made in it (keys, items set edge to edge).
struct NoFingerPadding {
    NoFingerPadding();
    ~NoFingerPadding();
    ImVec2 saved;
};
```

- **Which events.** `SDL_EVENT_FINGER_DOWN`, `_MOTION`, `_UP`, `_CANCELED` whose device is a screen (`SDL_GetTouchDeviceType(touchID) == SDL_TOUCH_DEVICE_DIRECT`) and that name a window; a trackpad's (indirect, no window) and SDL's own `SDL_MOUSE_TOUCHID` and `SDL_PEN_TOUCHID` are swallowed and dropped. `SDL_EVENT_WINDOW_FOCUS_LOST`: `Fingers::reset()` (a lift the browser never sent).
- **Places.** A finger event's `x`, `y` are 0 to 1 of its window (`SDL_GetWindowSize`); with ImGui's viewports enabled the window's position is added, as ImGui's SDL3 backend does for the mouse.
- **Emulated.** The left button's down, its motion while down and its up become one finger's events; the wheel, the other buttons, the motion with no button and the window's mouse enter and leave are swallowed.
- **Queued for ImGui.** Each change: `io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen)`, `io.AddMousePosEvent` (`-FLT_MAX` twice when parked), `io.AddMouseButtonEvent(0, down)` when it changed. On `cancelled`, `ImGui::ClearActiveID()` first (`imgui_internal.h`): the item pressed lets go without a click.
- **Kept.** While the pointer is the fingers' (a finger event came last, or emulated, or scripted), its place is queued again every frame: on a desktop ImGui's SDL3 backend queues the real cursor's place each frame (`ImGui_ImplSDL3_UpdateMouseData`), and the fingers' must come after it. This is why `TouchNewFrame` runs after `gpu.newFrame()`. A real mouse's motion or button (not emulated) gives the pointer back: `Fingers::mouse()`.
- **The tolerances**, each frame: `io.MouseDragThreshold` 10 or 6, `io.MouseDoubleClickTime` 0.35 or 0.30, `io.MouseDoubleClickMaxDist` 24 or 6, `style.TouchExtraPadding` 6 or 0. The Board's reach, the time screens' slop and the snap read `FingerPointer()` themselves.
- **Typing zones.** `TouchTypingZone()` records the current window's rectangle for the next frame's events; a `Down` inside one has `typing` set. `KeyboardWindow::draw` and `MatrixKeyboardWindow::draw` call it and hold a `NoFingerPadding`.
- `src/ui/main.cpp`: `TouchInit()` before `SDL_Init`; in `RunLauncher` and `RunBench`, `TouchStart` first, `if (TouchEvent(event)) continue;` first in the event loop (before the keys' captures), `TouchNewFrame()` between `gpu.newFrame()` and `ImGui::NewFrame()`. A bench opened from the Launcher keeps `touch` and `window` (today its options are made anew); the script is the first screen's only.

### Flags (`src/view/bench_options.hpp/.cpp`; P3)

`Options` (not `BenchOptions`: the Launcher takes them too) gains `bool touch`, `std::vector<FingerScript> fingers` and `std::optional<std::pair<int, int>> window`. `--touch`; `--tap X:Y[,X:Y…]`, `--double-tap X:Y`, `--hold X:Y`, `--drag X:Y:DX:DY`, `--pinch X:Y:FACTOR`, each setting `touch`; `--window W:H`, the main window's size (a tablet's: `1180:820`, `1080:810`). Tests in `bench_options_test.cpp` (`bench options: …`): each flag parsed, a bad one refused; the usage test lists them.

## 3. The Board (B1–B7, T1–T3)

### `src/view/viewport.hpp`

```cpp
// The loupe's view: this one `factor` times larger, `at` at the centre of a `size` panel.
Viewport magnified(Point at, double factor, ScreenPoint size) const;
// As frame(), but never past `maxPixelsPerMm` (still within [fitted, kMaxPixelsPerMm]).
void frameUpTo(Bounds box, double share, double maxPixelsPerMm);
```

Tests (`viewport_test.cpp`, `view: viewport …`): the magnified view keeps `at` at the centre and the factor; `frameUpTo` on a small chip stops at the pitch asked, on a 40-pin chip fills the share.

### `src/ui/board_view.hpp/.cpp`

- `BoardClick` gains `bool finger` (the pointer was a finger's) and `bool loupe` (released from the loupe).
- **The cancel.** `Touch().cancelled`: the press under way is dropped at once (`pressed_`, `dragging_`, `wirePress_`, `cornerPressed_`, `windowPressed_`, `held_`, the loupe). Its release, a frame or two later, then finds nothing pressed.
- **Two fingers.** While `Touch().two`: no highlight, no tooltip.
- **B1.** In `input()`, the panel hovered and `Touch().two`: `viewport_.zoomAt(mouse, log(pinch) / log(Viewport::kZoomStep))` and `viewport_.pan({panX, panY})`.
- **B2.** `ImGui::IsMouseDoubleClicked` on a `HitKind::Component` pick that is no Board button (`buttonAt()` empty), the owner the probe or an Instrument: the release gives no click; the chip is selected and framed by `frameUpTo(*scene_->boundsOf(target, kFocusPadMargin), 0.9, 44 / 2.54)`.
- **B3.** `reach()`: 22 px for both reaches when `FingerPointer()`; `markReach()`: 22 px. `BoardScene::pick` is unchanged: a pad within reach still wins over the copper beside it (the cutters and the iron pick copper first; the loupe reaches the rest). `buttonAt()`: the body under the point; else, for a finger and with no pick there, the nearest `SwitchTag`'s courtyard within 22 px.
- **B5.** A `ViewButtons` (§4), not on a still: its items first in `draw()`, its clicks flip, toggle the look, toggle `copper_`, fit, toggle `hold_`. `reach().bodies` is `!KeyAlt && !copper_ && !copperTool`. A press on the column is not the Board's.
- **D1 on the Board.** With `hold_`, a press on a push button or a key sets `latchToggle_` (as a right-click does) and holds nothing.
- **B7.** The refusal is drawn centred above `refusal_->at`, 14 px clear, kept inside the panel.
- **B4, the loupe.** The scene's drawing (`draw()` from the traces to the leads' tags, today one body) becomes `drawBoard(ImDrawList&, const Viewport&, ImVec2 origin, ImVec2 panel)`; `corner_`, the wire's band to the pointer, the refusal, the flip corner and the view buttons stay in `draw()`, for the panel's pass only. When `Touch().hold` begins on a press the Board took (not on a button, the corner, a memory window, the column), `loupe_` is set: no pan; each frame the pick is taken at the finger with the mouse's reach divided by the loupe's factor; `drawBoard` is called again through `viewport_.magnified(at, 3, {140, 140})`, clipped to a 140 px square whose lower edge is 44 px above the finger (beside the finger when the panel's top leaves no room), with a rim, a crosshair and the pick's tooltip text above it. The release gives a click with `loupe` set, or none off the Board's outline.
- **T2.** With the Wire tool the loupe's press on a pad is the wire's first pin (`wirePress_`), its release the second, as the drag today.
- **`setArmed(std::optional<Point> at, std::string action)`**: the action's name drawn above that point, in the refusal's place and look but neutral.

### Lit-first (`src/ui/main.cpp`, `src/ui/tool_click.hpp`; T1, T3)

```cpp
// The same named target: both swap one chip, resolder one pin, or join or part the same two pins.
// A mend and a cut name a point on copper: never the same by name.
bool SameRepair(const RepairAction& a, const RepairAction& b);

struct Armed {  // a repair a finger's first tap lit (RunBench)
    Point at;   // the tap's, nm
    RepairPlan plan;
    std::chrono::steady_clock::time_point until;  // kLingerTime after the tap
};
```

After `boardView.takeClick()`, for a finger's click that is not the loupe's:

1. Something is armed, in time, and the click is within the finger's reach (22 px, as nm) of `armed.at`: the armed plan goes through `StepFor` as a click's does today, and is disarmed. No `ToolClick`.
2. Else `ToolClick` runs. It returns a plan, and `StepFor` says `Apply` or `Cycle`: when the armed plan's first repair is the `SameRepair` (a second tap elsewhere on the same chip), it goes through; else it is armed instead (`boardView.setArmed` with "Swap U7", "Solder here", "Cut here" or "Solder the wire", then ": tap again"), and a wire's start is put back so its band still shows.
3. `StepFor` saying `Ask` opens the question at once (T3): its OK is the confirmation.

Any other click, a tool change and the time disarm.

T3: `QuestionPopup::open` gets the pointer's place moved 14 px up and the pivot `{0.5, 1}`, kept inside the main viewport's work area by its size (measured at its first frame): the question sits above the pointer, whole.

### `src/view/binder.hpp` (B6)

`Binder::frame(bool out, std::optional<std::size_t> selected, bool hasContents = false)`: a selection different from the last frame's, with contents, sets `contents_` to it and turns nothing. `ManualWindows::draw` passes `machine->contents(reference).has_value()` for the selected Component. A later `openContents` (another chip's window clicked) stands until the selection changes again. Test (`binder_test.cpp`, `binder: …`): the tab shows, the divider stays, `openContents` is not undone the next frame.

### `src/view/bench_places.hpp` (B5's lane)

`inline constexpr float kViewLane = 52;` the keyboard's and the Multimeter's `DefaultPlace` are `w - m - kViewLane`. `bench_places_test.cpp`'s default-places case is rewritten for it (the keyboard no longer under the TV's right edge, the Multimeter 52 px in).

## 4. The view buttons (B5, D1)

### `src/ui/view_buttons.hpp`

```cpp
enum class ViewButton : std::uint8_t { Fit, Look, Copper, Hold, Flip };
struct ViewButtonsLook {
    bool plain, copper, hold, back;  // drawn lit: the plain look, Copper on, Hold on, the back side
};
// The column at the panel's bottom right, 44 px a button, Flip lowest: the items first (call
// before the Board's input), the drawings by draw(). `hold`: whether the Hold button is there
// (a Board with a button or a key). The button clicked; whether the pointer is on the column.
class ViewButtons {
public:
    std::optional<ViewButton> items(ImVec2 panelMin, ImVec2 panelMax, bool hold);
    bool hovered() const;
    void draw(ImDrawList& list, const ViewButtonsLook& look) const;
};
```

Drawn as the Shelf's items are (`shelf_drawings.cpp`'s look: a dark tray, a light drawing, gold when on), each with a tooltip: "Fit the whole Board", "Real or plain look (Tab)", "Copper: pick under the chips (Option)", "Hold: a click latches a button", "Flip the Board (`)". Phase 2 has four; Hold comes in phase 3.

## 5. The controls (I6–I8, M1, O2, O3; Sizes)

### `src/view/number_pad.hpp`

```cpp
enum class PadKind : std::uint8_t { Hex, Pattern, Duration };
// The pad's keys, four a row; "" leaves a cell empty. "<" is backspace, "OK" confirms, a unit
// ("ns", "us", "ms") is appended and confirms.
std::span<const std::string_view> PadKeys(PadKind kind);
// `text` after `key`; unchanged when it does not fit: past `limit` characters, a second point, a
// letter the kind has not. `fresh` (nothing typed yet: the field's value is shown): a digit or a
// letter replaces the text, backspace edits it.
std::string PadTyped(std::string text, std::string_view key, PadKind kind, std::size_t limit,
                     bool fresh);
```

Hex: `C D E F / 8 9 A B / 4 5 6 7 / 0 1 2 3 / "" "<" OK OK`; Pattern: the same with `x` in the empty cell; Duration: `7 8 9 < / 4 5 6 ns / 1 2 3 us / 0 . OK ms`. Tests (`number_pad_test.cpp`, `number pad: …`): digits appended, the first key replacing a fresh value, backspace, the limit, one point, `x` only in a pattern, a unit appended.

### `src/ui/controls.hpp`

```cpp
inline constexpr float kControl = 28;  // a control's height: the style's frame
inline constexpr float kBig = 44;      // a new control's, a popup button's
// Each ImGui context: FramePadding {8, (kControl - 13) / 2}, ScrollbarSize 20, GrabMinSize 20 (M2).
void ApplyStyle();
// −, the value, +: the buttons kBig square. + is the next value, − the one before. A click on the
// value opens every value in two columns of kBig lines. True when `index` changed.
bool Stepper(const char* id, std::span<const std::string> values, std::size_t& index,
             float valueWidth);
// A text on a line of kBig controls (beside a Stepper), centred on its height.
void BigText(const char* text, ImU32 colour = 0);
// Buttons side by side, the `index`-th lit. True when it changed. `height`: kBig on a line that
// holds a Stepper, so the line's controls are one height.
bool Choice(const char* id, std::span<const char* const> labels, std::size_t& index,
            float height = kControl);
// The value as a button; a click opens the choices as a strip of kBig buttons.
bool ChoicePopup(const char* id, std::span<const char* const> labels, std::size_t& index,
                 float width);
// A number's field: its text in a frame; a click opens the pad beside it, never under the
// pointer, showing the value. The pad's keys and the PC's edit a copy; true once, when
// confirmed: `text` then holds it. `refused`: the field red.
bool NumberField(const char* id, std::string& text, PadKind kind, float width,
                 bool refused = false);
bool NumberPadOpen();  // the PC's keys are the pad's this frame
// In a scrolling child, after its content: a drag on it scrolls it (the press of the item it
// began on let go: ClearActiveID), with a glide; so do two fingers. `taken`: something else has
// the drag (a time screen): nothing.
struct DragScroll {
    void update(bool taken = false);
    float velocity = 0;
    bool active = false;
};
// Last in a window: a drawn grip in its bottom right corner, 28 px, its item 44 px inside the
// window; a drag resizes the window (its constraints still apply).
void CornerGrip();
// Last in a window: two fingers on it and `taken` false (no time screen, no page took them):
// resized by their pinch about its centre.
void PinchResize(bool taken = false);
```

- The values a `Stepper` gets run from coarse to fine, so + is always the finer step: the scope's timebases longest first (its + zooms in, as today's button), the analyzer's periods as `kAnalyzerPeriods` is.
- `Stepper`'s list, `Choice`, `ChoicePopup`'s strip and the pad's keys are items set edge to edge: made under a `NoFingerPadding`.
- The pad reads the PC's keys by code (`ImGui::IsKeyPressed`: the digits and the keypad's, A to F, X, the point and the keypad's, N, U, M and S for the units, Backspace, Enter, Escape): no SDL text event arrives outside a text field. A layout whose point needs Shift types it on the pad. Paste and a caret inside the value are given up (the spec says so). The Debug window's seed stays an `InputScalar`.
- `RunBench`'s `textField` becomes `io.WantTextInput || NumberPadOpen()`, so the PC's keys reach ImGui, not the Machine, while the pad is open.
- `ImGuiScreen` calls `ApplyStyle()`.

### Drag to scroll (M1, O1, I13)

- The binder's `PageId` child and the Contents page's `view` child are `ImGuiWindowFlags_NoMove` (a drag in them no longer moves the binder: `imgui.cpp` `StartMouseMovingWindow`) and call `DragScroll::update()`.
- `TextFlow::text` (`manual_view.cpp`) reports a click at the left button's **release** on the word with no drag since the press (`io.MouseDragMaxDistanceSqr[0]` under the threshold), for the mouse too, tested with `ImGuiHoveredFlags_AllowWhenBlockedByActiveItem`: today it acts at the press, so a scroll begun on a link would follow it. Its hit is the word's rectangle; for a finger, 6 px wider each side and as high as its line (M3): ImGui's padding would let the words of two lines both answer.
- The analyzer's `rows` child: `DragScroll::update(canvasActive)` for a drag on the labels; on the lanes the canvas decides (§6).

## 6. The time screens (I1–I5, I13)

### `src/ui/gestures.hpp`

`TimeCanvasInput` gains `std::optional<float> scrub` (a finger's hold: its x, each frame), `float scrollY` (a vertical drag's step) and `float pressY`. `TimeCanvas::read(float width, int divisions, std::optional<float> markerX, float top, bool vertical)`:

- the slop is the pointer's: `kClickSlop` (3 px) for the mouse, `kTapSlop` for a finger;
- `vertical` (the analyzer): once the press has moved past the slop, the larger of its two moves decides; a vertical drag gives `scrollY` each frame and no `dragStep`, no `dragFromPress`, no `marker`. Without `vertical` (the scope), as today;
- hovered with `Touch().two`: `zoom += WheelNotches(log2(pinch), pinchRest_)`, `pan += panX` (I1), `scrollY += panY`;
- active with `Touch().hold`: `scrub` set, no drag, no `marker` at the release (I2);
- a release without a move within 14 px of `markerX` (22 for a finger) and 28 px of `top`: `clearMarker`, no `marker` (I3);
- `Touch().cancelled`: the press is void: no `marker`, no `released` action.

### The windows

- `presetCursor_` becomes `placedCursor_`: set by `--cursor` and by `scrub`. The cursor is the hover, else the placed one.
- The analyzer applies `scrollY` to its `rows` child (`ImGui::SetScrollY`).
- I4, `src/view/time_view.hpp`:

    ```cpp
    // The edge nearest `at` within `reach`, among `sorted` (ascending, through `proj`); none
    // out of reach.
    template <std::ranges::random_access_range R, class Proj = std::identity>
    std::optional<double> SnapToEdge(R&& sorted, double at, double reach, Proj proj = {});
    ```

    The analyzer calls it on the row's `runs(channels).start` past its first (points; the reach 12 px in points), the scope on its band's `Sweep::channels[c]` through `&Sample::time` (ps). For a finger's Marker and scrub only. Tests (`time_view_test.cpp`, `time view: …`): the nearest edge, none out of reach, an empty range, a projection.
- `DrawMarker`'s flag is 28 × 20 px.
- I5: both `kGestures` lines; the analyzer's empty hint: "Clip, then click a chip pin. The pin / bus switch clips a whole bus."

## 7. The Instruments' controls and rows (I6–I13)

The header lines grow: the columns' places (`kLabelWidth`, `kValueX`, `kTriggerX`, `kUnclipX`), the scope's footer and the normal and minimal sizes (`NormalSize`, `MinimalSize`) are measured again so nothing is cut; the screenshots check it.

- **Scope**: line 1 a `Stepper` on the timebases, longest first (the − and + buttons gone into it; a pinch and the wheel still zoom about the pointer); `Choice` 1 2 3 4 and `Choice` rise / fall for the Trigger; `Choice` auto / normal; Run. Line 2: the position as a `NumberField` (`PadKind::Duration`, shown as today's text), T. Hook rows: `ImGui::Button("×")` at `kControl`.
- **Analyzer**: line 1 Arm, the phase, a `Stepper` on `kAnalyzerPeriods`; line 2 `Choice` on `kPositions` (their times as labels) and the captured time; line 3 "n of 64 clipped", `Choice` pin / bus (`bool bus_`, `bool bus() const`), Group, the view's −, +, Fit, T at `kBig` wide. `kLaneHeight` 28, `kOverviewHeight` 28. A row, under a `NoFingerPadding`: its name a `Selectable` as high as the row; the fold arrow row-high; a Channel's Trigger a `ChoicePopup`; a group's pattern a `NumberField` (`PadKind::Pattern`); `×` a `kControl` button, on a group opening a popup of two `kBig` buttons, Ungroup and Unclip all (Shift and × still unclips).
- **I9**: `RunBench` sets `click->shift` when the owner is the analyzer and `analyzerWindow.bus()`.
- **Meter**: `Choice` V / Continuity; `×` a `kControl` button.

## 8. The windows (O1–O6, M4–M6, D1–D3, L7)

- **O5** `CaseClose`: a 40 px item in the window's top right corner, the 22 px disc drawn in it at its place today, at 60 % until hovered.
- **O6** `PlaceNext`: `first` through `FitBench(first, bench.extent())`.
- **O4** `TogglePop`: with `ImGuiConfigFlags_ViewportsEnable` as today; without (the web build), the window's place and size kept and the window given the bench's (an object with `KeepAspect`: the largest that fits, centred); again, back. `BeginInstrument`'s size button: `NextPreset(current, minimal, normal, full)` goes minimal → normal → full (the bench) → minimal, from the nearest of the three to `current`. Tests (`bench_places_test.cpp`, `bench places: …`): the round, from each size and from one between.
- **O2, O3**: `CornerGrip()` last in each object's window; `PinchResize()` in the TV's, the paddle box's, the binder's (`taken`: its page scrolled this frame) and the Instruments' (`taken`: their time screen hovered). Not the keyboards': two fingers there type.
- **M4, M5** `chip_panel.cpp`: a pin's `Selectable` is `kControl` high; each pin has a "show" button at its row's outer end (a DIP row holds two pins: one at each end) whose click is `hits.clicked` with `frame` asked: `TargetHits` gains `bool frame`, which `BoardView::route` passes to `RouteLink` beside Ctrl and Cmd. The binder's `kMinWidth` grows by the two buttons. "Zoom to", "All datasheets", the Contents page's Hex / Code / Glyphs and the fold headings: `ImGui::Button` or framed at `kControl`.
- **M6** `contents_page.cpp`: `HexField` becomes a `NumberField` (`PadKind::Hex`), `Combo` a `Choice`, `Slider` a `Stepper`.
- **D1** `MatrixKeyboardWindow`: a slide switch "HOLD" drawn at the right of the strip above the keys, `bool hold_`; with it on, a left click on a key toggles `latched_`.
- **D4** `MatrixKeyboardWindow::drawCaps`: a left click on a key that is no SHIFT, HOLD off, also calls `session.inputs().press(reference, MatrixPress{key, shifted})`: the press queued in simulation time that the PC's keys use.
- **D2** `tv_window.cpp`: the knobs', the slot's and the trimmer's items at least 44 px.
- **D3**: an `InvisibleButton` over the glass; the loupe while the pointer is free over the window and not `Touch().two` (today's), or while that item is active, then drawn 80 px above the pointer.
- **L7** `question_popup.cpp`, `thank_you_card.cpp`: buttons `{110, kBig}` and `kBig` high.

## 9. The Launcher and the Shelf (L1–L6, L9)

- **L1**: under the making's two lines, `ImGui::Button("Stop", {110, kBig})`, which does what Esc does.
- **L2**: a press on a picture that drags past the threshold moves `machines_` (`jump` to its value less the drag over the pictures' pitch); the release selects the nearest Machine. A drag gives no click.
- **L3**: the chevrons' items 44 px.
- **L4**: a sideways drag on the post-its' row moves `postIts_` the same way; the rule that keeps the selected post-it in view rests until the selection changes. A vertical drag on a post-it with more text than fits adds to `noteScroll_`. A drag gives no click.
- **L9**: the forget hint's item `kControl` high, its text centred in it.
- **L5** `src/view/shelf_layout.hpp`: `float ShelfWidth(std::span<const ShelfCell>, const ShelfMetrics&)`: what the trays need with a tray's gap between the two sides. `Shelf::draw` takes the metrics `{48, 4, 20}` when they fit, else `{44, 4, 12}`. Tests (`shelf_layout_test.cpp`, `shelf layout: …`): the width of the BW-2001's Shelf; the narrow metrics chosen under it.
- **L6**: the Shelf's `kItems` names and tooltips, the paddle's and the TV's knob tooltips ("drag up or down; wheel"), the analyzer's hints, the Board's "Option: the copper under it" ("Copper: the copper under it").

## 10. The web page (P4, P5, L8)

SDL's window fills the document (`SDL_WINDOW_FILL_DOCUMENT`): at its creation SDL hides every other child of the body and sizes the canvas from `window.innerWidth` and `innerHeight` at each resize. So nothing in the page can show over the app, and the page's own height and padding do nothing.

- `shell.html` (P4): on `html, body, canvas`: `user-select: none`, `-webkit-user-select: none`, `-webkit-touch-callout: none`, `overscroll-behavior: none` (no selection, callout or bounce on a hold or a drag); `<link rel="manifest" href="manifest.json">`, `apple-mobile-web-app-capable`, `mobile-web-app-capable`. No `viewport-fit=cover`: the browser keeps the canvas inside the safe area by itself.
- L8: the "Not made for phones and tablets" notice (shown while the page loads) only when the screen's shorter side is under 600 px.
- **The turn notice** is the app's, not the page's: in both loops, a finger in use and the window taller than wide, a card over everything: "Turn the tablet: Bodgewire is laid out for landscape." (`src/ui/main.cpp`, checked by `--touch --window 820:1180`).
- `manifest.json`: name, `display: fullscreen`, `orientation: landscape`, the page's colours; copied beside the page by `src/ui/CMakeLists.txt` as the service worker is.
- `web.yml` (P5), the same file on `main` and `touch`: on a push to `main` and by hand (`workflow_dispatch`, run from `main`: the Pages environment deploys from `main` only). It builds `main` into `site/`, then, when `origin/touch` exists, checks it out beside and builds it into `site/touch/`; each copy step takes `manifest.json` when the build made one. After a push of `touch`: `gh workflow run web.yml --ref main`.

## 11. Docs

- `docs/GLOSSARY.md`: **Finger**, **Loupe**, **View Buttons**, **Hold Switch**, **Number Pad**, **Stepper**; **Marker** and **Flip Corner** amended.
- `AGENTS.md`: the `--touch` flags in "The agent can't see the screen"; the `/touch/` page in "Web"; `src/ui/controls.hpp` and `src/ui/touch.hpp` in "Shared helpers".
- `docs/recipes/ADD_INSTRUMENT.md`: a window's controls from `controls.hpp`; `TimeCanvas::read`'s new arguments.
- `docs/ARCHITECTURE.md`: the P8f row (its link to this folder with the docs' commit).
- `docs/architecture-review/ARCHITECTURE_REVIEW.md` and `src/ui/gestures.hpp`: their pointers to `docs/mobile/MOBILE.md` now to `TOUCH.md`.

## Checks

- Unit tests: `ctest --preset release -j -R "^(fingers|number pad|view|time view|bench options|bench places|binder|shelf layout): "` (about 75 cases, under a second).
- Sanitizers: the same pattern on the debug preset, phases 1 and 5; the checkout has no debug build yet: `cmake --build --preset debug --target bw_tests` once.
- Screenshots, release, viewed (window px are the 1280 × 800 window's); a scripted finger's shot gives its own `--after`:
  - phase 1: the Counter with `--tap` on a pin (its Net lit, its tooltip above), and `--after 4s` (nothing lit);
  - phase 2: the BW-2001 with `--pinch`; the view buttons; a chip framed by `--double-tap`;
  - phase 3: the Launcher making (`--make`) with Stop; the analyzer with pin / bus; a scope with `--hold` on its screen;
  - phase 4: every window once, since the style moves them all: each Instrument, the binder's pages, a number pad open, a popup, each Machine's bench;
  - phase 5: the BW-2001 with `--hold` on a pin (the loupe); the cutters with the power tapped off, then one `--tap` (armed) and two (cut);
  - phases 6 and 7: the binder after `--drag`; an Instrument at its full size; the Launcher after `--drag`; the BW-2001's Shelf in `--window 1080:810`.
- The web build once a phase. What only a browser's fingers can show is the user's first sitting.
- The full release suite once, before the merge.

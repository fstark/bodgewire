# P7b — Launcher look — Design

How [LAUNCHER_LOOK.md](LAUNCHER_LOOK.md) is built. Plan: [LAUNCHER_LOOK_PLAN.md](LAUNCHER_LOOK_PLAN.md). Numbers in parentheses (§3) are the spec's "What the player sees" items.

The kernel, Parts, the builder, devices, Machines, Puzzles and manuals are untouched. New: the lanes' logic and the framing rule in `src/view`, unit tested; pieces of the bench's drawing made shareable in `src/ui`; the launcher rewritten.

## Files

| File | Change |
|---|---|
| `src/view/launcher_lanes.hpp`, `.cpp` (new) | `Lane`, `LauncherLanes`, `Glide`, `PictureLeft` |
| `src/view/output_frame.hpp`, `.cpp` (new) | `OutputFrame` |
| `src/view/board_scene.hpp`, `.cpp` | `BoardScene::lamp(k)` |
| `src/view/CMakeLists.txt` | the two new files |
| `tests/view/launcher_lanes_test.cpp`, `output_frame_test.cpp` (new), `board_scene_test.cpp`, `tests/CMakeLists.txt` | `launcher lanes: …`, `output frame: …`, `board scene: lamp` |
| `src/session/session.hpp`, `.cpp` | `HasTv(const LoadedCircuit&)` (`makeLive` keeps its own nets, commented to agree) |
| `src/ui/loaded.hpp`, `loaded.cpp` (new) | `loaded.hpp` (P7a's `Loaded`, `DesignOf`) gains `Load` and `Print`, moved from `main.cpp`'s anonymous namespace into `bw`, defined in the new `loaded.cpp` |
| `src/ui/board_tags.hpp`, `.cpp` (new) | the Board's lamps, switches and repairs as `BoardView` tags, from a Session or from a still Machine |
| `src/ui/board_view.hpp`, `.cpp` | `setStill(std::optional<Bounds>)`; `kPanelBackground` moved to the header |
| `src/ui/tv_window.hpp`, `.cpp` | `DrawMonitor`, the case's drawing, used by `TvWindow::draw` |
| `src/ui/tv_view.hpp`, `.cpp` | `clear()` |
| `src/ui/brief_window.hpp`, `.cpp` | `DrawNote`, the paper's drawing, used by `DrawBrief` |
| `src/ui/shelf.hpp`, `shelf.cpp` | `kShelfWood`, `kShelfEdge`, `kTrayShadow` moved to the header |
| `src/ui/launcher.hpp`, `.cpp` | rewritten: stills and the live run loaded in the background, the lanes |
| `src/ui/main.cpp` | `RunWindow` uses `board_tags`; `RunLauncher` with a `TvView` and the speed line; `--select`; the live run stopped before a bench |
| `src/ui/CMakeLists.txt` | `loaded.cpp`, `board_tags.cpp` |
| `docs/GLOSSARY.md`, `docs/recipes/ADD_MACHINE.md`, `ADD_PUZZLE.md`, `docs/ARCHITECTURE.md` | Launcher, Still; the framing; a brief's length; P7b done, P7c |

## 1. The logic (`src/view`, unit tested)

### `launcher_lanes.hpp`

```cpp
namespace bw {

enum class Lane : std::uint8_t { Machines, PostIts };

// The launcher's selection (P7b): a Machine, and in it a post-it (0: "Start the Machine", p + 1:
// Puzzle p). The top lane always has post-it 0.
class LauncherLanes {
public:
    // Each Machine's Puzzle count, in list order. The selection is clamped (a list read anew).
    void setPuzzles(std::vector<std::size_t> counts);
    void left();   // the previous Machine (post-it 0) or post-it; the ends stop
    void right();
    void down();   // to the post-its, the post-it kept
    void up();     // to the Machines, post-it 0
    // Clicks: a Machine (top lane, post-it 0), or a post-it of the selected Machine (bottom lane;
    // post-it 0 too). Out of range: clamped.
    void selectMachine(std::size_t machine);
    void selectPostIt(std::size_t postIt);
    Lane lane() const;
    std::size_t machine() const;
    std::size_t postIt() const;
    std::optional<std::size_t> puzzle() const;  // postIt() - 1; nullopt for "Start the Machine"

private:
    std::vector<std::size_t> puzzles_;
    Lane lane_ = Lane::Machines;
    std::size_t machine_ = 0, postIt_ = 0;
};

// A value easing towards its target (the lanes' scroll, P7b): exponential, 98 % of the way in a
// quarter of a second; snapped to the target when within `snap` (in the value's units: 0.002 of a
// Machine on the top lane, 0.5 px on the post-its' row).
struct Glide {
    double value = 0, target = 0;
    double snap = 0.002;
    void jump(double to);              // no glide (the start, coming back from a bench)
    void update(double seconds);       // value += (target - value) * (1 - exp(-seconds / kGlideTau))
    bool moving() const;               // value != target
};
inline constexpr double kGlideTau = 0.064;  // s: e^(-0.25 / 0.064) ≈ 2 %

// The top lane's anchors, as shares of the lane's width: the selected picture centred in
// [kPictureFrom, kPictureTo]; the previous one's right edge at kHintLeft, the next one's left edge
// at kHintRight (P7b).
inline constexpr double kPictureFrom = 0.06, kPictureTo = 0.64, kHintLeft = 0.045,
                        kHintRight = 0.955;
// The x of a picture's left edge, as a share of the lane's width, for its distance `d` (its index
// minus the glide's value) and its width `w` (a share): piecewise linear through the three anchors
// (d = -1: right edge at kHintLeft; d = 0: centred; d = 1: left edge at kHintRight), extended
// linearly past them.
double PictureLeft(double d, double w);

}  // namespace bw
```

`PictureLeft` is piecewise because the paragraph sits right of the selected picture: the next Machine is further away than the previous one, so the two halves move at different rates, but every picture's left edge is continuous in `d` (it glides with no jump). Past `|d| > 1` the slope of the nearer half continues, so a picture two away is off the lane.

### `output_frame.hpp`

```cpp
namespace bw {

inline constexpr std::int64_t kFrameMargin = 30'000'000;     // nm of Board around the lamps
inline constexpr std::int64_t kSmallBoardSide = 130'000'000;  // nm: both sides at most: whole
inline constexpr double kWholeShare = 0.6;                     // a frame covering more: whole

// The part of a Machine's Board the launcher shows (P7b): its lamps' courtyards (BoardScene::lamp:
// every LED and digit), kFrameMargin around them, clamped to the Board's bounds. The Board's
// bounds instead when it has a TV, has no lamp, is small (both sides ≤ kSmallBoardSide), or when
// the frame's area is more than kWholeShare of the Board's.
Bounds OutputFrame(const BoardScene& scene, bool hasTv);

}  // namespace bw
```

`BoardScene::lamp(k)`: `info_[k].part && !info_[k].part->lamps.empty()` (`PartInfo::lamps`, as `Session::leds` reads them).

From the Boards' sizes (Edge.Cuts) and lamp positions today: Counter 126 × 44 mm, LED Chaser 87 × 94, Dice 108 × 82, Knob Meter 122 × 118: small, whole. Stopwatch 110 × 143 (three digits along the top), LED Computer 87 × 152 (eight LEDs in a row), BW-1 189 × 255 (six digits): framed. The TV Machines: whole.

### Tests

- `tests/view/launcher_lanes_test.cpp` (`[view]`): `launcher lanes: start` (Machines, 0, post-it 0, no puzzle); `left and right` (in each lane, the ends stop, a Machine with no Puzzles stays at post-it 0 in the bottom lane); `another Machine starts the Machine` (right in the top lane after a down-up resets the post-it; `selectMachine` too); `down and up` (the post-it kept going down, 0 going up); `select` (`selectPostIt` gives the bottom lane, clamped; `setPuzzles` with fewer Machines clamps); `glide` (98 % of the way after 0.25 s, never past the target, at it and not `moving` after 1 s; `jump`); `picture left` (d = 0 centred for w = 0.3; d = -1 right edge at `kHintLeft`; d = 1 left edge at `kHintRight`; d = 0.5 between; continuous at 0 from both sides; d = 2 past the right edge, d = -2 past the left).
- `tests/view/output_frame_test.cpp` (`[view]`), on `board_scene_test.cpp`'s cheap fixture (`LoadNetlistFile` of the design netlist and `ReadKicadBoardFile` of its Board, `test::NetlistPath`, `test::BoardPath`; no `LoadMachine`, so the BW-2001 does not cost seconds in debug): `output frame: small Boards whole` (Counter, LED Chaser, Dice, Knob Meter: the scene's bounds); `output frame: framed on the lamps` (Stopwatch, LED Computer, BW-1: every lamp's courtyard inside the frame, the frame inside the Board, its area below `kWholeShare` of the Board's; the Stopwatch's frame touching the Board's top, clamped; the BW-1's not touching the Board's bottom); `output frame: a TV, whole` (the Stopwatch with `hasTv`: its bounds); `output frame: HasTv` (`HasTv` of each Machine's design circuit: true for 050–090, 130, 140, false for the other seven).
- `board scene: lamp`: the Counter's `D1` is a lamp, `U1` not.

`bw_tests` links `bw_session` and `bw_machine` (`tests/CMakeLists.txt`), so `HasTv` is reachable from a view test. Check the fixture's names (`test::NetlistPath`, `test::BoardPath`) in `tests/paths.hpp` for the Machines' directories.

## 2. Pieces shared with the bench (`src/ui`, behaviour unchanged)

### `HasTv` (`session.hpp`)

`bool HasTv(const LoadedCircuit& circuit)`: `ConnectorNet` of `VIDEO.VIDEO` and `VIDEO.SYNC_N`, or of `MONITOR.VIDEO_N`, `HDRIVE`, `VDRIVE`: the test `makeLive` uses to make a `TvConsumer`. `makeLive` keeps its own nets (it needs them), with a comment that the two must agree; the `output frame: the repository` test checks `HasTv` against the Machines with a TV.

### `Load` (`loaded.hpp`, `loaded.cpp`)

`main.cpp`'s `Print` and `Load` move unchanged out of its anonymous namespace into `bw`: `loaded.hpp` (which already holds `Loaded` and `DesignOf`) declares them, the new `loaded.cpp` defines them. The launcher's live run calls `Load(file, std::nullopt)` on a background thread (§3).

### `board_tags.hpp`

```cpp
namespace bw {

// A Session's lamps as LedTags (its brightness), its switches as SwitchTags (pressed as it reads
// them), on `scene`; the bench and the launcher's live run (P7b).
std::vector<LedTag> LedTagsOf(const BoardScene& scene, const Session& session);
std::vector<SwitchTag> SwitchTagsOf(const BoardScene& scene, const Session& session);
// A still Machine's (P7b): every lamp dark, every switch released, from the design Circuit.
std::vector<LedTag> StillLedTags(const BoardScene& scene, const LoadedCircuit& design);
std::vector<SwitchTag> StillSwitchTags(const BoardScene& scene, const LoadedCircuit& design);
// The Machine's cuts, cracks, Bodge Wires and Solder Bridges onto `view`.
void ShowRepairs(BoardView& view, const Machine& machine);

}  // namespace bw
```

From `RunWindow`'s blocks: the LED tags (`session.leds()`); the switch look (`KEY`, `SLIDE`, else `Button`, by the Part's name) and legend (`SwitchOf`) as a helper both `SwitchTagsOf` and `StillSwitchTags` use; `RunWindow` keeps its `setPressed` loop (latched, slid, scripted, held, Board keys) and builds its tags with `SwitchTagsOf` after it, reading `session.pressed`. The still versions walk `design.parts`: a part whose `info->lamps` is not empty gives a tag per lamp at brightness 0; a `PartKind::Switch` gives a released tag (`SwitchOf(design, reference)` for the legend: the design Circuit is built, so its Switch devices exist). `ShowRepairs` is `RunWindow`'s `if (machine)` block.

### `BoardView::setStill`

```cpp
// The launcher's Board (P7b): no input (no hover, tooltip, click, pan or zoom), no flip corner, the
// view framed on `box` (filling the panel, centred) at every draw. nullopt: the bench's Board.
void setStill(std::optional<Bounds> box);
```

In `draw`: with a box, the child gets `ImGuiWindowFlags_NoInputs` (so the mouse passes through it to the launcher's buttons: ImGui hovers only items of the hovered window, and a child under the mouse would be it), `input(origin)` is not called (so `hovered_`, `highlight_`, `hoverTarget_` stay empty and nothing is clicked), the viewport is `fit` then `frame(*box, 1.0, 0)` whenever the panel's size changes (`frame` clamps to the fitted zoom, so a box larger than the Board is the Board whole), and the flip corner and the refusal are not drawn. The panel's `kPanelBackground` fill stays: the launcher's top lane is filled with the same colour. The child's ID is `"board"`: the launcher draws several, each inside its own `PushID`.

### `DrawMonitor` (`tv_window.hpp`)

```cpp
// The monitor's case (P6f) with its top-left at `at`, `width` wide (height width * kTvAspect):
// the cream shell filled (today the TV window's WindowBg) and its edge, front, bezel, screen,
// strip, knobs at `controls`' turns, lamp, power switch, trimmer. The
// picture from `view` when `controls.on()` and `view` is not null, else the dark glass; `hovered`
// a knob drawn hovered. Draws only: no item, no input.
void DrawMonitor(ImDrawList& list, ImVec2 at, float width, const TvControls& controls,
                 const TvView* view, std::optional<TvKnob> hovered = std::nullopt);
```

`TvWindow::draw` keeps its window (its `WindowBg` still `kShell`, under the same fill), its `InvisibleButton`s (placed first, from the same geometry), its tooltips and its `controls_` changes, then calls `DrawMonitor`. A still's monitor: `TvControls` with `setOn(false)` (the glass, the lamp off, the switch at OFF); the live one: default controls, `&view`.

### `DrawNote` (`brief_window.hpp`)

```cpp
// The post-it's paper (P6o's brief): the shadow under the curled corner, the paper with that
// corner cut, the glued strip, the curl; `lift` px of extra shadow (the launcher's selected one).
void DrawNote(ImDrawList& list, ImVec2 min, ImVec2 max, float lift = 0);
inline constexpr float kNoteStrip = 16;  // the glued strip's height
inline constexpr ImVec4 kNoteInk{0.12f, 0.11f, 0.09f, 1.0f};
```

`DrawBrief` draws its paper with it, unchanged; brief_window.cpp's `kStrip` and `kInk` become the header's `kNoteStrip` and `kNoteInk`. With `lift > 0`: a soft shadow (the paper's outline offset by `lift` down and right, `IM_COL32(0, 0, 0, 90)`) under the whole note.

### `TvView::clear()`

The kept frame blanked (black cells), `lines_ = 0`, uploaded at the next frame: the launcher's live run changing Machine does not show the previous Machine's picture.

## 3. The launcher (§1–14)

### Layout (window 1280 × 800 at start; every size from the main viewport's work area)

- **The window**: `RunLauncher`'s full-window window gains `ImGuiWindowFlags_NoScrollWithMouse` (the wheel would scroll it: `NoDecoration` has only `NoScrollbar`) and a zero `WindowPadding` (pushed around its `Begin`), so the hints and the wood reach the window's edges.
- **Top lane**: the work area's top `kTopShare = 0.66` of the height, filled with `kPanelBackground` (the Board panel's colour, exported from `board_view.hpp`), so a still's panel fill does not show as a rectangle.
- **Picture area**: `kPictureTo - kPictureFrom` of the width, the lane's height less 24 px above and below. Each Machine's picture is its frame's aspect fitted into it (a TV Machine, or one without a still yet or without a Board: the whole area), so its width share `w` differs per Machine.
- **Paragraph**: from `kPictureTo + 0.02` to `kHintRight - 0.02` of the width, centred vertically in the lane: the name (P7a's 1.5× font), the paragraph wrapped (`Displayable`), both at alpha `1 - 2|d|` (clamped to [0, 1]) of the selected Machine's `d`, its x following the picture's `PictureLeft(d, w)` shift.
- **Bottom lane**: the rest; the wood (`kShelfWood`) with a 2 px `kShelfEdge` line along its top.
- **Post-its**: 190 px wide; all of a Machine as tall as its tallest (its title at 1.2× wrapped, `Spacing`, its brief wrapped, at 168 px; at least 120 px, at most the lane's height less 32), top at the lane's top + 20; "Start the Machine" at x = 24; the Puzzles from x = 24 + 190 + 32 (a 2 px `kTrayShadow` groove at x = 24 + 190 + 16), 20 px apart, clipped to `[24 + 190 + 32, width)`. The row's scroll (a `Glide` in px, `snap` 0.5) keeps the selected Puzzle's post-it inside the clipped part with 20 px to spare; post-it 0 leaves it where it is.
- **Selected post-it**: drawn 8 px higher, `DrawNote(…, lift 6)`; with the bottom lane active, a 4 px `kFocusGold` bar 6 px under it.
- **Top lane active**: `kFocusGold` chevrons `‹` and `›` (two lines each, 3 px, 18 px tall), 10 px outside the selected picture's left and right edges, at its mid-height, only towards a neighbour that exists (the right one in the 0.02 gap before the paragraph).
- **Error line**: P7a's, in red, at the wood's top left, above the post-its; cleared at any change of selection (P7a cleared it on selecting a Machine).

### Pictures

For Machine `i` at `d = i - glide.value` (`|d| < 1.6`, else not drawn), each inside `PushID(i)`:

- **Without a TV**: a `BoardView` child at `PictureLeft(d, w)` of the lane's width, its frame's aspect fitted to the area, `setStill(frame)`. The selected Machine's, once live: the live `BoardView` (its Session's Board) with the still's frame (a Puzzle's cracks do not change the outline); else the still's `BoardView`.
- **With a TV**: the picture is the whole area. The Board child sits at the area's left, as tall as the area, as wide as the Board's aspect gives (at most the area's width), framed whole (`setStill(scene.bounds())`). The monitor is `kMonitorShare = 0.92` of the area's height tall (width from `kTvAspect`), right-aligned in the area, vertically centred, so only the Board's left part shows (spec decision).
- **Overlays**: ImGui draws a parent window's draw list under its child windows, so nothing drawn on the launcher's window after a Board child shows over it. The monitor and the red "Could not load NN-name.bwp: see the terminal" box are drawn in a second child, `"over"`, made after the Board's over the same rect with `ImGuiWindowFlags_NoInputs | NoBackground | NoScrollbar | NoScrollWithMouse` and zero padding: sibling children render in the order they are made.
- **No still** (not loaded yet, a netlist that failed, no Board): the area at the picture's place, nothing drawn but, without a Board, "No Board" centred in grey (and the monitor if `HasTv` said so: a Machine without a Board has no still, so its `tv` comes from the live Session's `tv()` once live, else none).
- The live picture's tags each frame: `LedTagsOf`, `SwitchTagsOf`, `ShowRepairs`; a still's once at its making: `StillLedTags`, `StillSwitchTags`.

`BoardView`'s legend font is the screen's (`setLegendFont`, each draw: the fonts are new with each `ImGuiScreen`).

### Stills, loaded in the background

Loading a Machine with its Board (`LoadMachine`: the Board file, `CheckBoard`, islands) takes up to 0.26 s in release for the BW-2001 (3.4 s in debug); the BW-80, BW-1, Tennis and Typewriter 0.03–0.10 s; the small ones under 0.02 s. None of it runs on the UI thread.

```cpp
struct StillData {          // made on the loader's thread
    Machine machine;        // LoadNetlistMachine: not built, never powered
    BoardScene scene;       // board(), pristineBoard(), designCircuit()
    bool tv = false;        // HasTv(designCircuit())
    Bounds frame;           // OutputFrame (a TV Machine: bounds)
};
struct Still {              // on the UI thread
    std::unique_ptr<StillData> data;
    BoardView view;         // on a copy of data->scene, setStill(frame), the still tags set
};
```

A `StillLoader` (in `launcher.cpp`): one `std::jthread` started at the launcher's first show with the netlists in load order (the selected Machine, then alternately the next and previous outwards), each `LoadNetlistMachine` (a netlist without a Board, `!HasBoard`, or one that fails: a null result, its diagnostics on stderr) and `StillData` made on that thread, pushed into a vector under a `std::mutex`; `take()` on the UI thread each frame moves the finished ones out into `std::map<std::filesystem::path, Still>`. The stop token is checked between Machines; the destructor (`~Launcher`, at quitting) joins it. It runs once for the launcher's life (the `Launcher` lives in `main`, across benches); a list read anew with a Machine not loaded (a directory added meanwhile) has no still. `Load` and `LoadNetlistMachine` read files and build data with no shared mutable state (the Parts' catalogue is static and constant); the debug ASan run covers them (there is no TSan build).

### The live run, loaded in the background

```cpp
struct Live {
    std::filesystem::path file;                // the entry selected
    std::chrono::steady_clock::time_point since;  // when it was selected
    std::optional<std::future<Loaded>> loading;   // std::async(std::launch::async, Load, file, nullopt)
    std::optional<Loaded> loaded;              // its Session powered on; nullopt: not yet, or failed
    bool failed = false;
    std::optional<BoardView> view;             // on its Session's drawn Board, when it has one
};
```

Each frame, after the keys and clicks, the entry's file (the selected Machine's netlist, or its Puzzle's `.bwp`):

- **Changed** from `live_.file`: the view reset, then `loaded.reset()` (the Session's runner joined, P7a's order: `Loaded` holds a `Session`, which has a move constructor but no assignment, so `reset` then `emplace`), `failed` false, `tv.clear()`, `file` and `since` set. A `loading` still running is kept aside in a `std::vector<std::future<Loaded>>` of abandoned loads, each dropped once `wait_for(0)` says ready (a `std::async` future's destructor blocks until its load ends: never on the UI thread mid-glide).
- **Rested** (`kSettle = 0.15 s` since `since`) with nothing loaded or loading: `loading = std::async(std::launch::async, Load, file, std::nullopt)`.
- **Loaded** (`wait_for(0)` ready): a Session: `loaded.emplace(std::move(result))`, `session.powerOn(1)` (real time, the bench's default seed), and with a Board the view on `*session.drawnBoard()`, `machine()->pristineBoard()`, `designCircuit()`, `setStill(the still's frame)` (no still yet: `OutputFrame` of that scene). No Session: `failed`, the red box over the still.
- **Running**: the TV's pictures to the `TvView` (`pictures().update()`, `view.show`) as `RunWindow` does. Nothing is pressed or typed: no `setPressed`, no knobs touched.

`Launcher::stop()` resets the view and the Session, then waits for `loading` and the abandoned loads (called by `main` before a bench's `Load`, and on quitting: at most one load's time). `Launcher::liveStatus()` (not `const`: `Session::runner()` is not) gives the runner's status for the speed line.

`Load` keeps its `LoadManuals` (unused by the launcher, cheap next to `LoadMachine`).

### `Launcher` (`launcher.hpp`)

```cpp
class Launcher {
public:
    ~Launcher();                     // stop(); the still loader joined
    void setList(MachineList list);  // the lanes' counts; selection clamped; the loader started once
    void select(const std::filesystem::path& file);  // an entry (EntryOf): Machine and post-it;
                                                     // not in the list: the selection kept
    void setError(std::string text);
    const MachineList& list() const { return list_; }
    // Each frame, inside a window filling the main one: keys, clicks, the loads, the live run,
    // both lanes. The entry to open (Enter, Space, a click on a post-it).
    std::optional<std::filesystem::path> draw(TvView& tv, ImFont* legends);
    void stop();
    std::optional<RunnerStatus> liveStatus();
private: …
};
```

P7a's `select(std::size_t machine)` becomes `select(file)` (the entry left; `MachineIndexOf` no longer needed by `main`). `select` and `setList` jump the glides (no slide on showing).

### Keys and clicks

Keys from ImGui (`IsKeyPressed`, with repeat): `LeftArrow`, `RightArrow`, `DownArrow`, `UpArrow`; `Enter`, `KeypadEnter`, `Space` open the selected entry. ImGui's keyboard navigation is off (`ImGuiScreen` never sets `NavEnableKeyboard`), so the arrows are the launcher's.

Clicks, on the launcher's window (the Board and overlay children take no input, §2): each post-it an `InvisibleButton` over its rect (the lifted one's too): `selectPostIt`, then open it. Each neighbour's picture an `InvisibleButton`: `selectMachine(i)`. The live picture: an `InvisibleButton` giving the top lane (`up()`). The buttons are made after the children, with `SetCursorScreenPos`.

### `main.cpp`

- `RunLauncher` makes a `TvView(gpu.device())`, calls `launcher.draw(view, screen.legends())`, presents `gpu.present(&view)`; at its screenshot prints the speed line as `RunWindow` does (`liveStatus`'s `simTime`, a `SpeedRatio`, UI ms, fps) when a live run exists. It takes the `ImGuiScreen` (for the fonts). Its window: §3 Layout.
- After `RunLauncher` returns (a file, or quit): `launcher.stop()` before `Load`.
- The selection after a bench: `launcher.select(left)` (the file just left) instead of `select(MachineIndexOf(…))`.
- `--select FILE`: `Options::select`; no file, not with `--visit` (usage errors); `EntryOf` of the list or the usage error `'F' is not a Machine or Puzzle of 'DIR'` (as `--visit`); the launcher selects it before its first show. Not a bench flag (`benchFlag` unset). The usage line gains `[--select FILE]`.
- `RunWindow`: its LED, switch and repairs blocks call `board_tags` (§2).

## 4. Docs

- GLOSSARY: **Launcher** rewritten (two lanes, the live run, post-its, the keys; P7a, P7b); **Still** (a Machine's Board drawn unpowered in the launcher, its neighbours', P7b).
- ADD_MACHINE: the launcher frames a Machine on its lamps by itself (30 mm around, whole when small or with a TV): look at the launcher's screenshot (`--select` of the netlist) when adding a Machine.
- ADD_PUZZLE: the brief is read on a post-it 190 px wide: at most about 220 characters (the longest today, the BW-80's Blank page, 204); `--select` of the Puzzle shows it.
- ARCHITECTURE: P7b done with the date and links; a row **P7c** (wanted): the zoom from the live picture into the bench, the post-it flying to its place on the Board, and back.

## Checks

- `launcher lanes: `, `output frame: `, `board scene: ` and `view: ` (the Board scene's other cases; release and debug: C++ in `src/view`).
- `src/ui` has no unit tests (`bw_tests` compiles none of it): its checks are the screenshots, viewed (release): the bench of the Stopwatch, the BW-2001's Odd E and the BW-1 after phase 2 (as before); the launcher at start; `--select` of the Stopwatch netlist, of the BW-2001's Odd E, of the BW-1 netlist, of the BW-80's last Puzzle; `--visit` of the Counter's Dark LED.
- Speed: `--select machines/130-bw2001/puzzles/01-odd-e.bwp --screenshot … --after 4s` against `bw_ui machines/130-bw2001/puzzles/01-odd-e.bwp` (P7a: 0.26×, UI 1.4 ms); the UI time with `--select machines/130-bw2001/bw2001.bwn` (the BW-1 and BW-80 stills beside it) recorded.
- Debug (ASan, UBSan): `--select` of the Typewriter's netlist and of the Stopwatch's; `--visit` of P7a's three (the Counter's Dark LED, the Test Pattern, the Typewriter); the known `flip_corner`/`chip_body` UBSan reports (P7a phase 2) not counted.
- The full release suite (shared code: `bw_view`, `bw_session`).

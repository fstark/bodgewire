# P6o — Shelf and keys — Design

How [SHELF_KEYS.md](SHELF_KEYS.md) is built. Plan: [SHELF_KEYS_PLAN.md](SHELF_KEYS_PLAN.md). Numbers in parentheses (§7) are the spec's "What the player sees" items.

Only `bw_ui` changes (`src/ui`, ImGui-free logic in `src/view`) and the player's and developer's docs. The kernel, Parts, the builder, the Session, devices, Machines and Puzzles are untouched: every repair still goes through `Session::repair`, which refuses with the power on.

## Files

| File | Change |
|---|---|
| `src/view/machine_keys.hpp`, `.cpp`, `tests/view/machine_keys_test.cpp` | removed |
| `src/view/focus.hpp`, `.cpp`, `tests/view/focus_test.cpp` | `escape()` and `EscapeStep` removed |
| `src/view/machine_slot.hpp`, `.cpp` (new), `tests/view/machine_slot_test.cpp` (new) | the Machine slot's click and double-click on its group |
| `src/view/power_cycle.hpp`, `.cpp` (new), `tests/view/power_cycle_test.cpp` (new) | the popup's decision |
| `src/view/flip_corner.hpp`, `.cpp` (new), `tests/view/flip_corner_test.cpp` (new) | the dog-ear's size and hit area |
| `src/view/board_scene.hpp`, `.cpp`, `tests/view/board_scene_test.cpp` | `InsideOutline` (the anonymous `Contains` made public under that name: the pencil and the iron on the Board only) |
| `src/view/bench_places.hpp`, `.cpp`, `tests/view/bench_places_test.cpp` | `ReturnPlace` (popped back in where it was) |
| `src/view/CMakeLists.txt`, `tests/CMakeLists.txt` | the new files |
| `src/ui/main.cpp` | the keys' routing; F2, `--keys`, MachineKeys, Esc gone; the repair plan and the popup; new flags |
| `src/ui/keyboard_capture.*`, `matrix_capture.*`, `board_keys.*` | comments (F2); `on` now set by main.cpp without MachineKeys |
| `src/ui/keyboard_window.*`, `matrix_keyboard_window.*`, `tv_window.*`, `knob_window.*` | the keys' gold removed; `draw` returns nothing (all four); knob tooltip |
| `src/ui/window_order.*` | `InFrontOf`; `BringToFront`'s comment |
| `src/ui/board_view.hpp`, `.cpp` | L, F, `+`/`−`, Esc, keypad gold, `takePress` removed; the flip corner; `takeClick` off the Board under the pencil |
| `src/ui/shelf.hpp`, `.cpp`, `shelf_drawings.hpp`, `.cpp` | the trays, the run switch, the Machine slot, double-click pops, no letters, no shortcuts |
| `src/ui/bench.hpp`, `.cpp` | `TogglePop`; `SendHome`, `PopOut`, `PopIn` removed; pop-in returns where it was; `KeepAspect` with a stretch |
| `src/ui/manual_window.hpp`, `.cpp` | the pop-out control removed; the binder's height |
| `src/ui/brief_window.hpp`, `.cpp` | the note dragged |
| `src/ui/power_cycle_popup.hpp`, `.cpp` (new), `src/ui/CMakeLists.txt` | the popup |
| `src/ui/analyzer_window.*`, `scope_window.*` | Esc removed, with `focused()` and `focused_` (they kept Esc from the Board); the hints' key letters (`Clip (C)`, `Hook (O)`) |
| `machines/{120-bw1,130-bw2001,140-bw80}/manuals/user.md` | F2 reworded (the Message's `F2` is a Net of its font: unchanged; the spec's §4 named it in error) |
| `machines/045-knob-meter/manuals/user.md`, `machines/070-tennis/manuals/user.md` | "click the box, then hold…", "The keys work once you have clicked on the knob box": the keys work always |
| `machines/120-bw1/docs/BW1.md`, `machines/140-bw80/docs/BW80.md` | F2 |
| `docs/recipes/ADD_BOARD_DEVICE.md`, `ADD_CONNECTOR_DEVICE.md` | the keys model |
| `docs/recipes/ADD_INSTRUMENT.md` | no shelf key, no `SendHome`, no home double-click; `focused()` gone; the double-click pops out |
| `docs/GLOSSARY.md` | Shelf, Equipment, Focus, Typing, Pencil Mark, Active Layer; Machine Slot, Flip Corner new |
| `docs/ARCHITECTURE.md` | the P6o row |

## 1. Keys (§1–4)

### The routing, `main.cpp`

`MachineKeys`, `applyKeys`, the `toMachine` / `toBench` bookkeeping, `KeysClick` and the F2 branch go. Each frame, before the event loop:

```cpp
const bool text = textField;  // last frame's io.WantTextInput: a text field takes the keys
capture.on = !capture.reference.empty() && !text;
matrixCapture.on = !matrixCapture.reference.empty() && !text;
boardKeys.on = !boardKeys.keys.empty() && !text;  // only without a keyboard, as today
knobsOn = hasKnobs && !text;                       // out or not (§1)
```

The event loop, in this order for each event:

1. **Tab and `` ` ``**: on a `KEY_DOWN`, not a repeat, no text field, no Cmd (`SDL_KMOD_GUI`): Tab flips the plain look; a key whose character is `` ` `` (`SDL_GetKeyFromScancode(scancode, mod & SHIFT, false) == '`'`, so any layout, any Shift state) flips the Board (`boardView.toggleLayer()`). Either sets `dropText`, and the event is consumed. The Tab handling at `ImGui::IsKeyPressed(ImGuiKey_Tab)` goes.
2. **The text event a flip key would type**: a `TEXT_INPUT` while `dropText` is consumed and clears it; any `KEY_DOWN` clears it too (a layout that sends no text for Tab leaves nothing to drop).
3. **Cmd**: a key event with Cmd held goes to ImGui untouched (quit, the app's own), except Cmd+V while `matrixCapture.on`, which the matrix capture takes as today (its paste). Ctrl+V on the matrix is unchanged (the matrix capture tests `GUI | CTRL`).
4. `knobKeys.handle(event, knobsOn)`; then the three captures, as today.
5. Not taken: a key event (`KEY_DOWN`, `TEXT_INPUT`, `TEXT_EDITING`) that is not a modifier goes to ImGui only while `text`; otherwise it is dropped. Key-ups and modifiers always reach ImGui (as today, so no key stays held there and a Shift-click stays one).

The captures' own code is unchanged but for comments: they already take every key while `on`, and Escape is already the Machine's (STOP, BREAK, `0x1B`, the BW-1's `ST`). The `sync` calls stay. `KeyboardCapture` takes Ctrl combinations as control codes; with Cmd excluded at step 3 it never sees a Cmd combination.

`--keys machine` and its check in `main()` go; `Options::keysToMachine` goes. `ParseOptions` returns usage for `--keys`, as for any unknown flag.

### What goes with the shortcuts

- `shelf.cpp`: the `P S M R K W G`, `C O V`, `U` blocks; `Entry::key` and its corner letter (`kKeyLetter`); the tooltips' `(X)`.
- `board_view.cpp`: `keys()` (L, F, Esc, `+`/`−`), `escape_`, `takeEscape()`. The wheel zooms, a double-click on bare Board fits, as today.
- `focus.hpp`: `EscapeStep`, `escape()`. Esc's last step lifted the probe's tip (`LiftTip`); its mouse equivalent: a probe click on bare Board (nothing picked) lifts it (`session.touch(std::nullopt)`; today it does nothing). Decided in the design (2026-11-19), reported to the user. The wire is cancelled by a second click on its first pin, which `main.cpp` already does (`wireStart == end`: reset, no repair), or by taking another tool (`lastOwner`).
- `analyzer_window.*`, `scope_window.*`: the Esc that clears the marker (a right-click on the screen already does), and `focused()` / `focused_`, which only kept that Esc from the Board. The empty-state hints lose `(C)` and `(O)`: "Clip, then click a chip pin…", "Hook, then click a chip pin."
- The gold for keys: `TvWindow::draw` and `KnobWindow::draw` lose `gold`; they and `KeyboardWindow::draw`, `MatrixKeyboardWindow::draw` return `void` (the return said "clicked: keys to the Machine"); the keyboard windows draw no frame (`capture.on` is no longer drawn); `BringToFront`'s comment loses "a keyboard window then takes the typing"; `BoardView::setKeypadGold`, `keypadGold_`, `kKeypadMargin`, `takePress`, `BoardPress`, `press_` go; the shelf's keyboard frame and its "Keys to the Machine · F2…" hint go. `kFocusGold`'s comment says only the focus.
- `knob_window.cpp` tooltip: "Q and A" / "Up and Down" without "(the keys while the Machine has them: a click on the box)".

### Unit tests

`machine_keys_test.cpp` goes with `MachineKeys`. `focus_test.cpp` loses "Esc steps back", and "the pencil is a tool" loses its `escape(false)` lines (`focus_test.cpp:90`); the rest stays. The routing itself is SDL code in `main.cpp`: checked by reading and by the user (the spec's Tests).

## 2. The shelf (§5–9)

### Items and order

`Item` (`shelf_drawings.hpp`) becomes:

```cpp
enum class Item : std::uint8_t {
    Power, RunSwitch,                                   // controls
    Pencil, Probe, Puller, Iron, Cutters, Wire,         // tools
    Analyzer, Scope, Meter,                             // instruments
    Brief, Manual, Machine                              // this Machine, right-aligned
};
```

`Run`, `Speed`, `Layer`, `Fit`, `Tv`, `Keyboard`, `Knob` go (the `Tv`, `Keyboard`, `Knob` drawing functions are reused by the Machine slot; the run switch's drawing function is `RunSlide`, so it does not hide the type). `shelf.cpp`'s `kItems` carries each item's group:

```cpp
enum class Tray : std::uint8_t { Controls, Tools, Instruments, Machine };
struct Entry { Item item; const char* name; Tray tray; float width; };  // width in cells
```

`RunSwitch` is 2 cells wide (three positions need the room the probe has); `Machine` is `MachineSlotCells(...)` wide (below); the rest 1, the probe 2. Layout: the first three trays left to right from `kGroupGap / 2`, `kGroupGap` between trays and `kCellGap` inside one; the right tray right-aligned at `width − kGroupGap / 2`, its width summed first so nothing else moves: the brief and the binder, then, set apart by `kGroupGap / 2` and a 1 px divider line in the tray, the Machine slot (§5: `[Brief Binder | Machine slot]`, one tray); the hint between the instruments' tray and it, clipped to that gap.

**Trays** (§5): each tray is drawn before its items as a rounded rectangle `kTrayPad` (4 px) around its cells, in `kTray` (a shade lighter than `kShelfWood`, starting at `IM_COL32(62, 50, 40, 255)`), with a 1 px darker inner line along its top for a recessed look. The colours are tuned on screenshots with the user (spec §5); the constants sit together at the top of `shelf.cpp`.

### The run switch (§6)

`Speed` (the runner's) stays; `shelf_drawings.hpp` adds the switch's state (the drawing needs it; `shelf.hpp` includes `shelf_drawings.hpp`), and `shelf.hpp` the power-on:

```cpp
enum class RunSwitch : std::uint8_t { Pause, RealTime, Max };
// shelf.hpp. Power on with the switch: at Pause it jumps to RealTime first (powering on always
// runs).
void PowerOn(Session& session, RunSwitch& run, std::uint64_t seed);
```

`PowerOn` sets `run = RealTime` and `session.setSpeed(Speed::RealTime)` when it was `Pause`, then `session.powerOn(seed)`. Every power-on in `bw_ui` goes through it: the shelf's power switch, the popup's cycle (§4), start-up.

A click on the switch picks the position under the mouse (thirds of the drawing). `Pause`: `session.runner().pause()`, only while powered (with the power off a click on `‖` does nothing, §6). `RealTime` or `Max`: `session.setSpeed(...)`, then `session.runner().resume()` while powered. The switch's state is `main.cpp`'s, passed to `Shelf::draw` in place of `Speed&`. Its drawing (`RunSlide` in `shelf_drawings.cpp`, replacing `Run` and `Speed`): `‖`, `1×`, `Max` printed over a three-position slot, the slider at the chosen third, the `‖` print grayed while unpowered; `ItemLook` gets `RunSwitch run` (replacing `maxSpeed`) and `powered`. Tooltip: "Pause, real time, or as fast as possible".

`--run pause|1x|max` (new) sets the switch at start. `pause` powers on with `Session::powerOn(seed, pauseAt)` at 0.2 s of simulated time (not through `PowerOn`, which would move it to `1×`) and leaves the switch at `Pause`, so a screenshot shows `‖` over a picture already drawn; `max` sets the speed before power-on.

### The Machine slot (§7)

`src/view/machine_slot.hpp`:

```cpp
// One of the Machine's objects in the slot (P6o): out (shown), in front of every other window
// but the slot's own, popped out to its own desktop window.
struct SlotObject { bool out = false, front = false, popped = false; };

enum class SlotClick : std::uint8_t { BringOut, ToFront, PutAway };
// A click: any away, all out in front; all out but some behind another window, all to the front;
// all out and in front, all away. Empty: BringOut (never called so: the slot is inert).
SlotClick ClickSlot(std::span<const SlotObject> objects);
// The double-click's second click: all popped out, all back in; else all out and popped out.
bool PopAll(std::span<const SlotObject> objects);  // true: pop out; false: pop in
// The slot's width in cells: one per device, at least one (a grayed monitor when none).
int MachineSlotCells(bool tv, bool keyboard, bool knob);
```

`Shelf` keeps `tvOut`, `keyboardOut`, `knobOut` (the windows read them) and the window names (`kTvWindowName`, `keyboardWindow`, `kKnobWindowName`). On a click on the slot it builds one `SlotObject` per device the Machine has (in TV, keyboard, knob order), `front` from `InFrontOf(name, group)` (below), `popped` from `PoppedOut`, and applies `ClickSlot`: `BringOut` sets every `*Out` and brings each window to the front; `ToFront` brings each to the front; `PutAway` clears them. The double-click's second click (`MouseClickedLastCount >= 2`) after the first: every `*Out` set, then `PopAll` pops each out (`TogglePop` on those not popped) or each in (`TogglePop` on those popped).

`--away tv,keyboard,knob` (new): those of the Machine's objects start put away (they start out), for the spec's "one put away" screenshots.

`window_order.hpp` adds:

```cpp
// True when every floating window drawn in front of `name` is one of `group` (`name` visible).
bool InFrontOf(const char* name, std::span<const char* const> group);
```

the scan `InFront` already does, going on past the group's own windows. A popped-out window is in `g.Windows` with the others and ImGui raises it on focus, so the same scan serves.

**The drawing** (`shelf_drawings.cpp`): `Item::Machine` with `ItemLook::slot` = `{bool tv, keyboard, knob; bool tvLit, keyboardLit, knobLit}`. Left to right in its cells: the monitor (today's `Tv`, its screen lit when out), the keyboard (today's `Keyboard`, one generic drawing, its lamp lit when out), the paddle box (today's `Knob`, its lamp lit when out). None of the three: the monitor alone at the grayed alpha, the slot inert, its tooltip "No TV, keyboard or knobs on this Machine". The five Machine cases of §7 come out of these rules; no Machine has a keyboard and knobs together (the knobs then take the keys the keyboard does not, as today).

Tooltip: the devices named ("TV and keyboard", "Paddle controllers"…), then "click: out, to the front, or away; double-click: their own windows (or back)".

### Double-click pops out (§8)

`bench.hpp`:

```cpp
// The shelf's double-click (P6o): popped out, back on the bench where it was before it popped
// out (ReturnPlace); else out to its own desktop window beside the main one, from where its window
// last was (BesidePlace), or from its default place and first size if it never came out. From the
// next frame. `window`: its Begin name.
void TogglePop(BenchObject object, const char* window);
```

It uses `FindWindowByName` for the window's last rectangle, as `SendHome` did. The shelf calls it only on a double-click's second click, after the first made the object out at least once, so the window always exists; if `FindWindowByName` still finds none (never drawn), it falls back to `PopOutAtStart`, which pops it from its default place and first size when `PlaceNext` next runs. `SendHome`, `PopOut`, `PopIn` (the binder's control's, from inside the window) and `home_` go: an object comes out where it was left (spec, Home). `PopOutFrom` also records the object's place on the bench before it leaves (`benchAt_[i]`: its offset from `BenchArea().min`, the default place for a `--pop` at start); popping in sets `at_` to

```cpp
// bench_places.hpp: where an object popped back in goes: `offset` (from the bench's top left)
// moved just enough that its `size` lies inside the bench, at least kBenchMargin from its edges
// when it fits (the bench may have shrunk since).
Place ReturnPlace(Place offset, Extent size, Extent bench);
```

after `FitBench` has fitted the size, as today.

The shelf's second click per item: the Machine slot as above; an instrument: `focus.key(owner)` (out, with the focus), `BringToFront`, `TogglePop`; the binder: `manualOut = true`, `TogglePop`. The brief, the tools and the switches have no double-click: each of the two clicks acts as a click (two clicks on the power switch are an off and an on). The first click of a double-click always acts as a single click, as today with home; the second makes the object out before popping, so a first click that put it away does no harm.

The binder's `PopControl` and its constants go (`manual_window.cpp`).

### The power-off tools (§9)

`NeedsPowerOff` stays (it names the tools); `unpowered` and `kFocusBlocked` go: never grayed, never red. Their tooltip adds "With the power on: power-cycles the Machine". The hint loses "Power off first".

## 3. The Board view (§10, §11)

### The flip corner

`src/view/flip_corner.hpp`:

```cpp
inline constexpr std::int64_t kFlipCornerSide = 8'000'000;  // nm: 8 mm along each edge
inline constexpr double kFlipCornerMinPx = 28;               // never smaller on screen
// The dog-ear's side in nm at `pixelsPerMm`: kFlipCornerSide, at least kFlipCornerMinPx.
std::int64_t FlipCornerSide(double pixelsPerMm);
// The dog-ear: the triangle cut off the outline's bounds' bottom-right corner (Board y down).
// True when `at` is on it, its edges included.
bool OnFlipCorner(Point at, Bounds bounds, std::int64_t side);
```

`BoardView::draw`, after the parts, marks and leads (it is on top: a control): the triangle (`bounds.max.x − side, bounds.max.y`), (`bounds.max.x, bounds.max.y − side`), `bounds.max`, filled with the other side's trace colour in the current look (seen from the front: the back's; from the back: the front's), a darker diagonal line and a soft shadow along it, as a page's turned corner. In the plain look too.

`BoardView::input`: a left click released on the corner (not dragged) toggles the layer and makes no `BoardClick`, whatever the owner, so the pencil, the tools and the instruments never see it; hovering it shows "Flip the Board" and no Board tooltip. A double-click on it is two flips (no fit). The corner also wins at the press: `buttonAt()` returns nothing on it (no button held, no slide moved, no latch toggled by a right-click), and a right-click on it sets no `rightClick_` (no pencil clear).

**Its size** (spec "about 10 mm"): 8 mm. Every Board outline is a `gr_rect`; with 10 mm the dog-ear covers the Typewriter's `SW1.2` (`CLEAR`, 6.7 mm from the corner along the diagonal) and Tennis's `J4.4` (7.5 mm); 8 mm puts its diagonal 5.7 mm from the corner, clear of both. A test checks it on every Machine (phase 3): no pad of any Board within the dog-ear at 8 mm. Zoomed out until the pixel floor makes the corner larger than 8 mm, what lies under it is reached by zooming in: accepted.

`--side back` (new) starts with the back as the Active Layer.

### The pencil on the Board

`board_scene.hpp`: `bool InsideOutline(std::span<const Point> outline, Point at);` (even-odd crossings; the outline is closed with arcs flattened). `main.cpp`'s pencil case returns at once when `!InsideOutline(scene.outline(), click->at)`: neither a mark nor a clear. The right-click clear is limited the same way. `InsideOutline` is `board_scene.cpp`'s existing even-odd `Contains`, made public under that name. Marks already placed are unaffected (none can be off the Board now; `--mark` is not checked: a test flag).

## 4. Repairs with the power on (§12–14)

### The plan

`main.cpp`'s click switch no longer calls `session.repair` for the four tools. It builds, for the click, what the tool would do:

```cpp
struct RepairPlan {
    std::vector<RepairAction> tries;        // in order; the first Done wins
    std::optional<std::size_t> swapped;     // the puller's component: its marks leave with it
    std::string unknown;                    // the refusal if none is Done ("" : none)
};
```

- Puller: a click on a component → `{Swap}`, `swapped` = it.
- Iron: on a pad → `{Resolder, Mend(layer, at)}` (the mend at the trace's contact or the click, as today); elsewhere on the Board → `{Mend}`; off the outline (`InsideOutline`) → no plan. A mend is always `Done` in the Session, so without this an iron click anywhere would power-cycle; inside the Board it must stay possible everywhere, as a crack is not shown (spec §12's "a pin or crack" read as "on the Board").
- Cutters: a wire → `{Unwire}`; a bridge → `{Unbridge}`; a straight trace → `{Cut}`; each with `unknown` "Cut: click a trace or a wire" (today's refusal after any of them); a curved trace or nothing → refused at once as today, no plan.
- Wire: the first pin sets `wireStart` (no plan, with the power on too); the second, a different pin → `{Wire}`; the same pin cancels.

No plan: nothing opens (§12). A plan goes to:

```cpp
// src/view/power_cycle.hpp
enum class RepairStep : std::uint8_t { Apply, Ask, Cycle };
// What a click on the Board does with a tool that needs the power off: nothing when it would
// apply no repair; with the power off, apply it; on, ask (the popup), or power-cycle at once once
// "Don't ask again" was ticked.
std::optional<RepairStep> StepFor(bool wouldRepair, bool powered, bool dontAsk);
```

- `Apply`: `apply(plan)`: each try through `session.repair` until one is `Done` (then `marks.swapped(*plan.swapped)` if set); none: `refuse(plan.unknown)` if not empty.
- `Ask`: the plan is kept as `pending`, the popup opens at the mouse.
- `Cycle`: at the click-handling point of a frame (before the buttons' and `--type` blocks): `session.powerOff()`, `apply(plan)`, and `powerOnNext = true`; the next frame calls `PowerOn(session, run, seed)` before its event loop, so before `TvConsumer* tv = session.tv()` (a repaired Machine's power-on rebuilds `live_`: an older `tv` would dangle). That frame in between is unpowered, as in a manual power cycle, so its blocks release latched buttons, restart `--type` and reset `wasPowered`; the speed ratio sees the time go back; the instruments' captures, the pencil marks and the Session's knobs are kept as by a manual cycle (§14).
- The popup's **OK** is answered at the end of a frame (it is drawn last), past that point: it sets `cycleNext = pending`, and the next frame runs the `Cycle` at its click-handling point. "Don't ask again" cycles in the click's own frame, already at that point.

A refusal after a cycle (a resolder that falls through to a mend is always `Done`; an unknown can only come from a cut whose trace the Machine does not find) shows as with the power off.

### The popup, `src/ui/power_cycle_popup.hpp`

```cpp
enum class PopupAnswer : std::uint8_t { None, Ok, Cancel };
class PowerCyclePopup {
public:
    void open(ImVec2 at);       // at the click, from the next draw
    PopupAnswer draw();         // each frame, last; Cancel also when closed by a click elsewhere
    bool dontAsk() const { return dontAsk_; }  // for the run, every tool; never saved
private: ...
};
```

An ImGui popup (`OpenPopup` at `at`, `BeginPopup` with `NoMove | NoSavedSettings | AlwaysAutoResize`): the text of §13 wrapped at about 300 px, the "Don't ask again" checkbox, **OK** and **Cancel**. A click outside closes it (ImGui's own), which `draw` reports as `Cancel`; ImGui blocks the windows under an open popup from hover, so that click acts nowhere else. Keys never reach it: they go to the Machine (§1); no text field in it. On `Ok`, `main.cpp` schedules the `Cycle` above with `pending` for the next frame; on `Cancel` it drops `pending` (a started wire is reset too: its second click was the repair). The checkbox counts only with OK: ticked then cancelled, the next repair asks again.

`--ask REPAIR` (new, the `--repair` syntax): at start, with the power on, the popup open for that repair at the Board panel's centre, for the screenshot; OK applies it as a click would.

## 5. Bench objects (§15, §16)

### The binder's height

`bench.hpp`: `KeepAspect(float aspect, float minWidth, const char* window = nullptr, float* stretch = nullptr)`. ImGui calls the size callback on every `Begin` with the desired size equal to the current one, and a corner dragged straight down leaves the width unchanged too, so a size comparison cannot tell the edges from the corner. Instead, before `Begin`, `KeepAspect` looks up the window (`FindWindowByName(window)`) and whether ImGui's active item is its top or bottom resize border (`g.ActiveId == GetWindowResizeBorderID(window, ImGuiDir_Up / ImGuiDir_Down)`, `imgui_internal.h`). Only then does the callback take the desired height and set `stretch = clamp(height / (width × aspect), 0.7, 2.5)`; otherwise it keeps `height = width × aspect × stretch` (the corner and the side edges scale in proportion; a double-click on a border, ImGui's auto-fit, too). The minimum constraint is `{minWidth, minWidth × aspect × 0.7}`, so ImGui's clamp before the callback does not stop a shorter binder. `ManualWindows` holds `stretch_` (1 at start, kept for the run, the same object popped out or not) and passes the first size `{600, 600 × aspect × stretch_}`. The page child already takes `h` (`manual_window.cpp`: `size.y = h − 2·kCoverEdge − 20`), so a taller binder shows more lines; the tabs are checked on a tall screenshot (their positions are laid out along the height). Edge resizing is on by default (`io.ConfigWindowsResizeFromEdges`, with the SDL3 backend's mouse cursors).

`--binder-height N%` (new) sets `stretch_` at start (70–250 %), with `--manual` or `--pop manual`.

### The brief dragged

`DrawBrief` keeps a `BriefNote` state (`offset`, default `{kInset, kInset}`; `dragging`): after `Begin`, a left press on the note's window (`IsWindowHovered()`, so an open popup blocks it) not on its × (`!IsAnyItemHovered()`) starts dragging, `io.MouseDelta` moves `offset` while the button is down, a release stops it. Position: `corner + offset` each frame (`ImGuiCond_Always`, as today). The offset survives folding the note away. About ten lines; if it grows past that, it is dropped (spec §16) and this section says so. `--brief DX:DY` (new) sets the offset at start, px.

## 6. Flags

| Flag | Change |
|---|---|
| `--keys machine` | removed |
| `--run pause\|1x\|max` | new: the run switch at start |
| `--side front\|back` | new: the Active Layer at start |
| `--ask REPAIR` | new: the popup open at start for that repair |
| `--binder-height N%` | new: the binder's stretch at start |
| `--brief DX:DY` | new: the brief's offset from the Board's corner, px |
| `--away tv,keyboard,knob` | new: those of the Machine slot's objects put away at start |

`kUsage` lists them. `--pop tv,keyboard,knob` stay (an object of the Machine slot popped out alone; the slot pops them together).

## 7. Docs

- User Manuals (BW-1, BW-2001, BW-80): the BW-2001's "On a PC keyboard, click the keyboard's window, or press F2. Each PC key then types…" becomes "Each PC key types…"; the BW-80's "Or click the keyboard window, or press F2, and type on the PC keyboard" becomes "Or type on the PC keyboard"; the BW-1's "Click the keypad or the shelf's keyboard first, or press F2. Then …" becomes "The PC keys are the hex keys: …".
- `BW1.md`, `BW80.md`: "(Typing, F2)", "focus the window (F2)" → the keys always go to the Machine.
- `ADD_BOARD_DEVICE.md`, `ADD_CONNECTOR_DEVICE.md`: a device that takes keys has them always (but a text field, Cmd, Tab, `` ` ``); no gold, no click to take them.
- `ADD_INSTRUMENT.md` (phase 1: its key on the shelf, `focused()` keeping Esc; phase 2: `SendHome`, the home double-click, `PopOut`/`PopIn`): no shelf key; the double-click pops it out and in; it comes out where it was left.
- The Knob Meter's and Tennis's User Manuals: the knob keys work without a click on the box.
- `GLOSSARY.md`: **Shelf** (the trays, the run switch, the Machine slot, no letters); **Machine Slot** (new); **Equipment** (no home; the double-click pops out; the binder's height; the binder's control gone); **Focus** (no Esc, no red; gold only the focus); **Typing** (every key to the Machine but the four exceptions; no F2, no gold); **Pencil Mark** (on the Board only, no `G`); **Active Layer** (the Flip Corner, `` ` ``); **Flip Corner** (new); **Brief** (dragged).
- `ARCHITECTURE.md`: the row.

## Risks

- **A text field's first key**: `textField` is last frame's `WantTextInput`. A field becomes active on the click, a frame before any key, so no key is lost; a field left by a click gives the keys back the next frame.
- **The flip key's text**: some layouts (French, German) send `` ` `` as a dead key: the flip happens on the key-down, but the OS still composes the next letter with it (`è`), which every capture drops as not ASCII: one keystroke lost after a flip on those layouts. Accepted.
- **Typing while a popped-out keyboard is not focused**: the captures take SDL's events whichever window has the system focus, as since P6j; text input is started on the focused window (`sync`), so typing works in the main window too.
- **The Machine slot's front test across desktop windows**: ImGui's order is its own, not the desktop's; a popped-out object behind another application counts as in front. Accepted: a second click brings it.

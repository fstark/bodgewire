# P6c — Shelf and focus — Design

Spec: [SHELF_FOCUS.md](SHELF_FOCUS.md). Plan: [SHELF_FOCUS_PLAN.md](SHELF_FOCUS_PLAN.md). UX: [UX_DESIGN.md](../p6b-ux-design/UX_DESIGN.md#focus). Recipe: [ADD_INSTRUMENT.md](../recipes/ADD_INSTRUMENT.md): only its Tool and Window layers change; its CLI layer gains `--focus`. No engine, consumer, Session or placement change.

## Files

| File | Change |
|---|---|
| `src/view/focus.{hpp,cpp}` (new, bw_view) | the focus owner and the instruments out (below) |
| `src/view/CMakeLists.txt` | `focus.cpp` |
| `tests/view/focus_test.cpp` (new), `tests/CMakeLists.txt` | spec, Tests |
| `src/ui/board_view.{hpp,cpp}` | `Tool`, `tool()`, `setTool()`, `takePicked()` removed; `setOwner(Owner)` set each frame by `main.cpp` (the switch-body rule reads it); `keys()` keeps `L`, `F`, `+`, `-`, `Esc` only |
| `src/ui/shelf.{hpp,cpp}` (new) | the shelf: layout, groups, clicks, graying, tooltips, hint, tool and instrument keys, `U`; `kBoardShare` moves here (below) |
| `src/ui/shelf_drawings.{hpp,cpp}` (new) | `Item` and one vector drawing per item into a cell (below) |
| `src/ui/toolbar.{hpp,cpp}` | removed; `ToolButton` deleted (its only users were the toolbar and the three lead buttons) |
| `src/ui/instrument_marks.{hpp,cpp}` | `kFocusGold`, `BeginInstrument` (Begin with the gold frame), `InstrumentClicked` (below): the windows never include the shelf |
| `src/ui/analyzer_window.{hpp,cpp}`, `scope_window.{hpp,cpp}`, `meter_window.{hpp,cpp}` | lead button removed; `draw(bool& open, bool owner, Session&)` (no `BoardView`: only the lead buttons used it) returns true when clicked; gold frame while `owner`; first positions above the shelf; the meter's `×` (below) |
| `src/ui/keyboard_window.cpp`, `matrix_keyboard_window.cpp` | include `shelf.hpp` for `kBoardShare` (was `toolbar.hpp`); first position above the shelf |
| `src/ui/side_panel.{hpp,cpp}` | Probe section and `DrawLamps` removed; TV and Knob sections drawn only when the shelf has them out (`DrawKnobs`' keys still turn the knob when hidden) |
| `src/ui/main.cpp` | the `Focus`; click handling by owner; the iron; `Esc`; window clicks and closes; layout with the shelf at the bottom; `--focus` (below) |
| `src/ui/CMakeLists.txt` | `shelf.cpp`, `shelf_drawings.cpp` in, `toolbar.cpp` out |
| `docs/recipes/ADD_INSTRUMENT.md` | Tool and Window rows, CLI row, a gotcha (below) |
| `docs/recipes/ADD_CONNECTOR_DEVICE.md` | UI row: the device's section is shown from its shelf drawing |
| `docs/GLOSSARY.md` | **Focus**, **Shelf**, **Equipment** under Instruments and views |
| `docs/ARCHITECTURE.md` | P6c row done |

## Focus (`src/view/focus.hpp`)

ImGui-free, so it is unit tested like `time_view`.

```cpp
// What a click on a pin does: one Board tool or one instrument owns it (P6c).
enum class Owner : std::uint8_t { Probe, Puller, Iron, Cutters, Wire, Analyzer, Scope, Meter };

bool IsInstrument(Owner owner);  // Analyzer, Scope, Meter

enum class EscapeStep : std::uint8_t { CancelWire, ToProbe, LiftTip };

class Focus {
public:
    Owner owner() const { return owner_; }
    bool out(Owner instrument) const;            // its window shown
    void take(Owner tool);                       // a Board tool: shelf click or key
    void key(Owner instrument);                  // C, O, V: out, owner; never away
    void shelfClick(Owner instrument);           // away → out+owner; out → owner; owner → away
    void bringOut(Owner instrument);             // start-up flags: out, owner unchanged
    void clicked(Owner instrument);              // a click in its window: owner (if out)
    void closed(Owner instrument);               // window closed: away; owner → Probe
    EscapeStep escape(bool wireStarted);         // applies ToProbe; the caller does the rest
private:
    Owner owner_ = Owner::Probe;
    std::array<bool, 3> out_{};                  // Analyzer, Scope, Meter
};
```

`escape`: `wireStarted` → `CancelWire` (owner unchanged); else owner not `Probe` → owner becomes `Probe`, `ToProbe`; else `LiftTip`. `take` with an instrument and `key`/`shelfClick`/`clicked`/`closed`/`bringOut` with a tool are programming errors (`assert`). Names follow the shelf: `Puller` is the Swap repair's tool, `Iron` Mend and Resolder's, `Cutters` Cut's (GLOSSARY keeps the repairs' names).

## Board and clicks (`main.cpp`)

- One `Focus focus` beside the windows. Each frame, before `boardView.draw`, `boardView.setOwner(focus.owner())`; a change of owner from the previous frame resets `wireStart` (today's `takePicked` did). Pressing `W` again mid-wire no longer cancels it (the owner does not change); `Esc` does.
- The click `switch` on `focus.owner()`: `Probe`, `Puller` (Swap), `Cutters` (Cut), `Wire` as today under their old cases, refusal texts unchanged (`Cut: straight traces only`, `Clip: …`, `Hook: …`). `Analyzer`/`Scope` as `Clip`/`Hook`; `Meter` as `Meter`. A `Refused` result calls `shelf.refused("Power off first")` beside the Board's cursor flash (the banner's reasonless flash is gone).
- **The iron**: powered → `Refused` (as Mend and Resolder). A pad pick → `ResolderRepair` on it; `Unknown` (not a chip signal pin: `chipSignalPin`) falls through to a mend. Else `MendRepair` at the trace pick's contact, or at the click (today's Mend). `Session::repair` returns `Done` for every mend (it logs it and drops a blob whether or not a crack was there, as Mend does today), so the iron never refuses on bare board; unchanged, as the Session is. Resolder is `Done` on a sound chip pin too (`Machine::resolder`), as a real iron reflows any joint.
- **`Esc`** (`boardView.takeEscape()`, still ignored while the analyzer or scope window has keyboard focus): `focus.escape(wireStart.has_value())`: `CancelWire` → `wireStart.reset()`; `LiftTip` → `session.touch(std::nullopt)`; `ToProbe` → nothing more.
- **Windows**: `bool open = focus.out(X); if (window.draw(open, focus.owner() == X, session)) focus.clicked(X); if (!open) focus.closed(X);` for the three instruments. The User Manual keeps its own `bool`, `shelf.manualOut`.
- **Keyboards**: one `shelf.keyboardOut`, true at start when the Machine has a keyboard of either kind (today's windows open at start). Each frame `keyboardOpen = keyboardOut && capture.reference` set, `matrixOpen = keyboardOut && matrixCapture.reference` set; a window closed by its own button sets `keyboardOut` false. A Machine has one keyboard (P4o, P5q).
- **Layout**: the main window's content is the Board and side panel at `available.y - kShelfHeight - spacing`, then the shelf across the whole width.
- **`--focus NAME`** (`probe`, `swap`, `iron`, `cut`, `wire`, `analyzer`, `scope`, `meter`; usage updated, unknown name → usage, exit 2): `take` or `key` after the other flags; `--clip`/`--group`, `--hook`, `--meter` call `bringOut`. Without a Board it is accepted and has no effect (no window, tools inert).

## Instrument windows

- The `ToolButton` at the head of each header goes; the next item starts the line.
- `BeginInstrument(name, open, owner)` in `instrument_marks`: while `owner`, `PushStyleColor(ImGuiCol_Border, kFocusGold)` and `PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2)` before `Begin`, popped right after (the style is read at `Begin`).
- `InstrumentClicked()` after `Begin`: `IsWindowHovered(RootAndChildWindows | AllowWhenBlockedByActiveItem)` (a click while a text field is active counts) and `IsMouseClicked(Left)`, **except on the close button** (the title bar's last `GetFrameHeight()` px on the right): ImGui closes on the release, a frame after the press, so a press there counted as a click would give the closing window the focus and its close would then hand it to the probe, taking it from the tool that had it.
- First positions (`ImGuiCond_FirstUseEver`) move up by `kShelfHeight` where they would reach the shelf: the analyzer's `WorkSize.y - 460` becomes `WorkSize.y - 460 - kShelfHeight`; the keyboards' `WorkSize.y - size.y - 16` becomes `WorkSize.y - size.y - 16 - kShelfHeight`. P6d sets real default places.
- Meter: after `red: U1.1 leg`, `SameLine` and `SmallButton("×")` → `session.liftLeads()` (shown when a lead is placed). `focused()` on the meter is not needed (it has no `Esc` of its own).

## Shelf (`src/ui/shelf.hpp`)

```cpp
inline constexpr float kBoardShare = 2.0f / 3.0f;  // from toolbar.hpp
inline constexpr float kShelfHeight = 64.0f;       // logical px, the child's height

class Shelf {
public:
    void draw(Session& session, BoardView& board, Focus& focus, Speed& speed, std::uint64_t seed);
    void refused(std::string reason);  // amber hint for 2 s
    // Equipment that is not an instrument, shown or hidden from the shelf; today's defaults:
    // the TV and knob sections shown, the keyboard out when there is one (set by main.cpp).
    bool tvOut = true, keyboardOut = false, manualOut = false, knobOut = true;
    bool wireStarted = false;
    std::string typing;  // the keyboard typed on; empty: not typing
private: …  // the reason and its time, as Toolbar's
};
```

- **Layout**: a child window `shelf`, full width, `kShelfHeight` high, a dark wood-brown background (`IM_COL32(48, 38, 30, 255)`) with a lighter top edge, so it reads as a shelf under the Board. Cells are squares of `kShelfHeight - 16` (48 px), 4 px apart, 20 px between groups; the probe's cell is twice as wide (lamps). Controls, tools, equipment from the left; the hint takes what is left at the right, one line, vertically centred, clipped (`PushClipRect`) to the shelf. At 1280 px: 18 cell widths × 48 + 14 gaps × 4 + 3 group gaps × 20 ≈ 980 px, about 290 px of hint, about 44 characters of the 13 px font: every hint below fits.
- **A cell** is an `InvisibleButton` (`##item` under the item's ID), drawn by `DrawItem(list, item, min, max, look)` (below), then: grayed → the drawing at 35 % alpha (each colour scaled in the drawing, not a second pass); owner → a 2 px `kFocusGold` rounded frame round the cell; the key letter, where the item has one, in the top-right corner in the UI font at 0.8 scale, grey; the tooltip; a faint frame on hover, so every cell reads as clickable.
- **Controls**: `Power`: a toggle switch, lever up and a green `ON` when powered, down and grey `OFF`; the LIMIT lamp (red when `session.currentLimited()`) drawn inside its cell's lower corner, with the tooltip `Bench supply in current limit` when lit. Click → `powerOn(seed)` / `powerOff()`. `Run`: a play triangle when paused, two pause bars when running; grayed while off. `Speed`: a two-position slide labelled `1×` and `Max`, the knob at the current one; click flips. `Layer`: two stacked squares, the front one green (F.Cu) or teal (B.Cu) per `board.activeLayer()`, labelled `F` or `B`; click → `toggleLayer()`. `Fit`: four corner brackets round a small board; click → `fit()`.
- **Tools** (grayed and inert without a Board; Puller, Iron, Cutters, Wire grayed but takeable while powered): `Probe`: a pen with its tip to the left and three lamps on the body, red `H`, green `L`, amber `P`, lit from `session.lamps()`; its tooltip is `Logic probe (P)` and, when the tip is down, the text the side panel showed (`U1.1 CKB · leg`, `bare board`; built as `SidePanel` did from `scene->pick` and `ContactSide`). `Puller`: a U-shaped chip extractor over a small DIP. `Iron`: a soldering iron, grey handle and copper tip, a solder drop. `Cutters`: side cutters, two jaws and two red handles. `Wire`: a coil of green wire with a stripped end. Click → `focus.take(…)`.
- **Equipment**: `Tv`: a small CRT; lit (grey-blue glow) when out; grayed without `session.tv()`. `Keyboard`: a keyboard outline with key rows and a pilot lamp; grayed with neither `keyboards()` nor `matrixKeyboards()`. `Analyzer`, `Scope`: a box with a screen, lit when out (the analyzer's screen with square lanes, the scope's with a sine). `Meter`: a handheld meter, its display lit when out. `Manual`: a book, open when out. `Knob`: a round knob with a pointer and a pilot lamp; grayed without `knobs()`. Instruments: click → `focus.shelfClick`; grayed and inert without a Board. The others toggle their `ShelfState` flag.
- **Tooltips**: `Name (key)`, with today's button tips kept (the analyzer's `shift-click: its whole bus`, the meter's `continuity with the power off`), and a state: `out`, `away`, `has the focus`; grayed: `No TV on this Machine`, `No keyboard on this Machine`, `No knob on this Machine`, `No Board`, `Power off first` (puller, iron, cutters, wire).
- **Hint**, the first that applies: a refusal (amber, 2 s, from `refused`); `typing` (`Typing on KBD · F2 to stop`, `Typing on Board keys · F2 to stop`); the owner is grayed by power (`Power off first`, amber); `wireStarted` (`Wire: click the second pin`); the meter owns the focus in continuity, powered off (`Meter: red lead next` / `black lead next` per `session.blackNext()`). Otherwise nothing.
- **Keys** (no `WantTextInput`, read in `Shelf::draw`; the typing modes eat key events before ImGui as today): with a Board, `P`, `S`, `M` and `R`, `K`, `W` → `take`; `C`, `O`, `V` → `key`. `U` toggles the manual (moved from the toolbar). `L`, `F`, `+`, `-`, `Esc` stay in `BoardView::keys()`.

## Drawings (`src/ui/shelf_drawings.hpp`)

```cpp
enum class Item : std::uint8_t {
    Power, Run, Speed, Layer, Fit,                      // controls
    Probe, Puller, Iron, Cutters, Wire,                 // tools
    Tv, Keyboard, Analyzer, Scope, Meter, Manual, Knob  // equipment
};

struct ItemLook {
    float alpha = 1.0f;     // 0.35 grayed
    bool lit = false;       // equipment out, power on, run running
    ProbeLamps lamps{};     // Probe only
    bool front = true;      // Layer only
    bool maxSpeed = false;  // Speed only
    bool limit = false;     // Power only
};
void DrawItem(ImDrawList& list, Item item, ImVec2 min, ImVec2 max, const ItemLook& look);
```

Each drawing is built from rects, rounded rects, circles, lines and triangles in coordinates relative to the cell (`u = width / 56`), no text except the speed's `1×`/`Max`, the layer's `F`/`B` and the probe's lamp letters (ImGui's default fonts have `×`, ADD_INSTRUMENT gotcha). Colours: each scaled by `look.alpha` by a small drawing helper local to `shelf_drawings.cpp` (board_view's `WithAlpha` is file-local). The drawings are judged in the release screenshots and adjusted there; none of them is the hardware's real look yet (equipment objects are P6d).

## Side panel

`SidePanel::draw` gains `bool tvOut, bool knobOut`. TV section: drawn when `tvOut && session.tv()` (the `no video` box and `DrawNoVideo` go: the grayed TV says it). Knob: `DrawKnobs(session, knobOut)`, the keys handled first as today, the section only when `knobOut`. The Probe section goes; the probe text helper moves to `shelf.cpp`.

## Recipes and glossary

- ADD_INSTRUMENT, Tool row: `src/view/focus.hpp` (an `Owner`), `src/ui/shelf.cpp` + `shelf_drawings.cpp` (a cell, a drawing, a key), the click case in `main.cpp`. Window row: no lead button; `draw(open, owner, …)` returns a click; gold frame while owner; first position above the shelf. CLI row: `--focus`. Gotcha: style colours and vars that frame a window are read at `Begin`: push before, pop after.
- ADD_CONNECTOR_DEVICE, UI row: the section is shown when the device's shelf drawing has it out; keys work either way.
- GLOSSARY: **Focus** (what a click on a pin does: a Board tool or an instrument owns it, shown in gold; `Esc` gives it back to the probe; not ImGui's keyboard focus, which the windows' `focused()` and Typing Mode mean). **Shelf** (the strip along the bottom of the window holding the controls, the Board's tools and the equipment, the same on every Machine; an item that does not apply is grayed). **Equipment** (the objects brought out from the shelf or put away: TV, keyboard, instruments, manuals, knob; _Avoid_: instrument for the TV or keyboard). Instrument's `_Avoid_: equipment` becomes `_Avoid_: Tool (too broad); equipment (broader: also the TV, keyboard, manuals)`, and says the Logic Probe sits among the Board's tools on the shelf (it is held, not brought out).

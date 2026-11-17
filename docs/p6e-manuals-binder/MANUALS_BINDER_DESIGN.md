# P6e — Manuals binder — Design

Spec: [MANUALS_BINDER.md](MANUALS_BINDER.md). Plan: [MANUALS_BINDER_PLAN.md](MANUALS_BINDER_PLAN.md). UX: [UX_DESIGN.md](../p6b-ux-design/UX_DESIGN.md#equipment-on-the-bench). Recipes: [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md) UI layer (a case window: `PlaceNext`, `KeepAspect`, `CaseClose`), [ADD_INSTRUMENT.md](../recipes/ADD_INSTRUMENT.md) CLI layer. No engine, Session, focus or placement change.

## Files

| File | Change |
|---|---|
| `src/view/binder.{hpp,cpp}` (new, bw_view), `src/view/CMakeLists.txt` | the dividers' state (below) |
| `tests/view/binder_test.cpp` (new), `tests/CMakeLists.txt` | spec, Tests |
| `src/manual/manual.{hpp,cpp}` | `DatasheetIndex` (below) |
| `tests/manual/manual_test.cpp` | `manual: datasheet index` |
| `src/view/bench_places.cpp`, `tests/view/bench_places_test.cpp` | `FirstSize(Manual)` 600 × 500; the test's first sizes unchanged in shape (inside the bench) |
| `src/ui/manual_window.{hpp,cpp}` | the binder drawn as a case, its four dividers (below) |
| `src/ui/manual_view.{hpp,cpp}` | paper colours; `PageTooltip` (below) |
| `src/ui/chip_panel.cpp` | paper colour for its power pins; `PageTooltip` for a pin's line |
| `src/ui/shelf.cpp`, `shelf_drawings.cpp` | the binder's tooltip and drawing |
| `src/ui/main.cpp` | `--manual user\|service\|datasheet\|repairs` |
| `docs/GLOSSARY.md`, `docs/recipes/ADD_INSTRUMENT.md`, `docs/recipes/ADD_PUZZLE.md`, `machines/120-bw1/docs/BW1_DESIGN.md`, `docs/ARCHITECTURE.md` | below |

## Binder state (`src/view/binder.hpp`)

ImGui-free, so tested like `focus`.

```cpp
// The binder's dividers, front to back (P6e).
enum class Divider : std::uint8_t { User, Service, Datasheets, Repairs };

// Which divider is open and which Part's sheet the index opened.
class Binder {
public:
    // Every frame, before drawing, out or away: `out` its window shown this frame, `selected`
    // the Board's selected Component. Coming out: the divider `show` asked for, else the User
    // Manual, the selection taken as seen (no turn), the opened Part closed. Out, a selection
    // different from the last frame's (not a deselection) turns to Datasheets and closes the
    // opened Part. Away, the selection is only remembered.
    void frame(bool out, std::optional<std::size_t> selected);
    Divider divider() const { return divider_; }
    void turn(Divider divider) { divider_ = divider; }  // its tab clicked
    void show(Divider divider) { shown_ = divider; }    // --manual: the next coming out
    const std::optional<std::string>& part() const { return part_; }
    void openPart(std::string part) { part_ = std::move(part); }  // a Part's name in the index
    void closePart() { part_.reset(); }                             // All datasheets

private:
    Divider divider_ = Divider::User;
    std::optional<Divider> shown_;
    std::optional<std::size_t> selected_;
    std::optional<std::string> part_;
    bool out_ = false;
};
```

`frame`: `if (out && !out_) { divider_ = shown_.value_or(User); shown_.reset(); part_.reset(); } else if (out && selected && selected != selected_) { divider_ = Datasheets; part_.reset(); }` then `selected_ = selected; out_ = out;`. So `--manual user --probe U1.1` stays at the User Manual (the start-up selection is seen on coming out), and `--manual datasheet --probe U1.1` shows U1's page (`show`).

## Datasheet index (`src/manual/manual.hpp`)

```cpp
// The Parts on a Machine, in netlist order of first use, each with its references in netlist
// order (P6e's index).
struct IndexEntry {
    std::string part;  // PartInfo::name: the key of Manuals::datasheets
    std::vector<std::string> references;
};
std::vector<IndexEntry> DatasheetIndex(const LoadedCircuit& design);
```

From `design.parts` (`LoadedPart::info->name`, entries with a null `info` skipped).

## The binder (`src/ui/manual_window.hpp`)

```cpp
inline constexpr const char* kManualWindowName = "###manuals";
class ManualWindows {
public:
    void draw(bool& open, const Manuals& manuals, BoardView& board, const LoadedCircuit& design,
              std::span<const std::string> log);
    void show(Divider divider) { binder_.show(divider); }  // --manual
private:
    Binder binder_;
    ManualView user_, service_, sheet_;
    ChipPanel chip_;
    std::vector<IndexEntry> index_;  // built on the first draw (the design does not change)
};
```

`showChip()` goes. `draw` calls `binder_.frame(open, board.selected())` first, then returns when `!open`.

**The window**: `PlaceNext(Manual, FirstSize(Manual, …))`, `KeepAspect(500.0f / 600, 500)`, `WindowPadding` 0, rounding 6, `WindowBg` the cover, `Begin(kManualWindowName, &open, NoTitleBar | NoSavedSettings | NoScrollbar | NoScrollWithMouse | NoCollapse)`, as the TV (`src/ui/tv_window.cpp`).

**Geometry** in fixed px from the window's rect (`at`, `w`, `h`); the page takes the rest:

| | |
|---|---|
| cover | the window, `IM_COL32(36, 60, 115)`, a lighter edge `(70, 100, 160)` |
| page | x from 34 to `w − 86`, y from 12 to `h − 12`; cream `(246, 242, 228)`, a 2 px shadow `(0, 0, 0, 60)` below and right |
| holes | three discs `(200, 195, 180)`, radius 5, at x 34 + 12, y at 0.2, 0.5, 0.8 of `h` |
| rings | at each hole, an open ring straddling the page's left edge: the upper half of an ellipse from the spine (x 22) to the hole (x 46), 10 px high, metal `(190, 190, 198)`, 3 px, a darker inner line |
| tabs | x from `w − 86` to `w − 14`; the i-th from y 30 + i · 50, 44 high, right corners rounded 6; colours pale yellow `(240, 225, 140)`, green `(170, 215, 160)`, blue `(165, 195, 235)`, pink `(235, 175, 190)`; the open one from the page's edge (joined) at full colour, the others from 6 px right of it at 80 % brightness; labels at 0.85× the font, near black, centred, two lines `User / Manual`, `Service / Manual`, one `Datasheets`, `Repairs`; Repairs without a Board at 40 % alpha, not clickable |
| `×` | `CaseClose()` (6 px inset: above the first tab) |

The open tab starts 2 px into the page, covering the page's shadow there. Each tab is an `InvisibleButton` over its rect (so a press there does not move the binder); a click calls `binder_.turn(divider)`.

**The page**: `SetCursorScreenPos(page.min + {24, 10})`, `BeginChild(id, page size − {32, 20}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground)` with the id per divider (`page user`, `page service`, `page datasheets`, `page repairs`: ImGui keeps each child's scroll, so each divider keeps its place). Pushed before it and popped after `EndChild` (`kPaper` colours): `Text` near black `(30, 30, 35)`, `TextDisabled` `(120, 115, 105)`, `Header`/`HeaderHovered`/`HeaderActive` tan `(225, 212, 180)`, `(215, 198, 160)`, `(205, 186, 145)`, `Button`/`ButtonHovered`/`ButtonActive` the same tans, `ScrollbarBg` transparent, `ScrollbarGrab`/`Hovered`/`Active` `(190, 180, 160)`, `(170, 160, 140)`, `(150, 140, 120)`, `Separator` `(200, 190, 170)`. A drag on the page's empty paper moves the binder (a child's drag moves its root window), as the TV's picture does.

**The dividers** (in the child):

- `User`: `board.route(user_.draw(manuals.user->blocks, board.scene(), design))`, else `no user manual` disabled.
- `Service`: the same with `manuals.service` and `service_`, else `no service manual`.
- `Datasheets` (each of its three modes under its own `PushID`, `chip`, `sheet`, `index`: the chip page's `Function`/`Pins` tree nodes and a sheet's headers would otherwise share open state): with a scene and `board.chip()` (hovered, else selected): `All datasheets` (a `SmallButton`: `board.deselect()`, `binder_.closePart()`), then `board.route(chip_.draw(...))` as today. Else with `binder_.part()`: `All datasheets` (`closePart()`), then the Part's sheet with `sheet_.draw(sheet->blocks, scene, design)`. Else the index: `Datasheets` at 1.3× (as a Markdown title), one line per entry: the Part's name in the code colour, underlined and clickable when it has a datasheet (`openPart`), then its references in the link colour, each its own item laid out with `SameLine` and wrapped at the right edge as `ManualView`'s `Flow` does; hovered → `TargetHits::hovered`, clicked → `clicked` (the Component, no pad), routed through `board.route` (which selects, so the next frame turns to its page). Without a scene the references are plain text.
- `Repairs`: the log, `none yet` disabled; only with a Board. Without one the tab is grayed, and the draw turns a Repairs binder to the User Manual (`binder_.turn(User)`, before the tabs) so `--manual repairs` shows the User Manual with its tab open.

`Flow` (in `manual_view.cpp`'s anonymous namespace) moves to `manual_view.hpp` as `class TextFlow` so the index reuses it, with the code and link colours as `kPageCode`, `kPageLink`; the index's title uses its own 1.3× (`kTitleScale` in `manual_window.cpp`).

## Paper colours (`manual_view`, `chip_panel`)

`manual_view.cpp`: `kText (30, 30, 35)`, `kBold (0, 0, 0)`, `kCode (150, 90, 10)`, `kLink (25, 80, 175)`. `CollapsingHeader` takes the pushed tans. `chip_panel.cpp`: `kPowerText` `(0.5, 0.48, 0.44)`; the DIP body, notch, pin-1 dot and the digit's drawing stay (they are drawings of parts, dark on paper reads as a chip).

```cpp
// A tooltip from a page: the UI's light text on its dark background, whatever the page pushed.
void PageTooltip(std::string_view text);
```

Pushes `Text` `(230, 230, 230)` inside `BeginTooltip`/`EndTooltip`. `ChipPanel`'s pin line uses it instead of `SetTooltip`. `CaseClose`'s tooltip is drawn in the parent, after the child's colours are popped.

## Shelf

- Tooltip `Manuals binder: User and Service Manuals, datasheets, repairs`; key `U`, `ToggleWindow(manualOut, kManualWindowName)` unchanged.
- Drawing (`shelf_drawings.cpp`, `Manual`), in the 56 × 56 cell: away, a closed binder: a blue cover `(40, 70, 130)` from (12, 8) to (42, 48), its spine darker (12..17) with three ring humps (small light arcs at y 16, 28, 40), three tabs (yellow, green, pink) sticking out 4 px at the right edge at y 12, 22, 32, a white label. Out (lit), open flat: two cream pages either side of a centre spine with three rings across it, grey lines of text, a yellow tab at the right page's edge.

## `main.cpp`

`Options::manual` becomes `std::optional<Divider>`; `--manual user|service|datasheet|repairs` (anything else: usage error, as today); `shelf.manualOut = options.manual.has_value()`; `manualWindows.show(*options.manual)`. Usage line updated. `userManual` and `chipTab` go.

## Recipes and glossary

- GLOSSARY: **Service Manual** "shown in the Chip panel" → "in the manuals binder, and each chip's role on its datasheet page"; **Datasheet**: "in the binder, opening at the selected chip"; **Equipment**: the manuals as a binder, a drawn case like the TV.
- ADD_INSTRUMENT, CLI row: `--manual chip` → `--manual datasheet` (with `--probe` a chip's page).
- ADD_PUZZLE (line 16) and `BW1_DESIGN.md` (the self-test line): the player now reads the whole Service Manual in the binder, not only the roles.
- ARCHITECTURE: the P6e row done, with the date and links, saying it comes out at the User Manual and turns to a chip's datasheet when one is selected (not "opening at the selected chip").

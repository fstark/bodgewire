# P6h — Pencil — Design

Spec: [PENCIL.md](PENCIL.md). Plan: [PENCIL_PLAN.md](PENCIL_PLAN.md). The tool follows [ADD_INSTRUMENT.md](../recipes/ADD_INSTRUMENT.md)'s Focus row (an `Owner`, an `Item` with its drawing and key, the click case in `main.cpp`), less what only an instrument has: a Board tool is taken (`Focus::take`), has no slot in `Focus::out_` and no window; that row gains a sentence saying so (phase 3). No Session, device, kernel or Part change: the marks are the UI's.

## Files

| File | Change |
|---|---|
| `src/view/pencil_marks.{hpp,cpp}` (new), `tests/view/pencil_marks_test.cpp` (new) | the marks (below) |
| `src/view/CMakeLists.txt`, `tests/CMakeLists.txt` | the new files |
| `src/view/focus.hpp`, `tests/view/focus_test.cpp` | `Owner::Pencil`, after `Wire`, before `Analyzer` (`Focus::slot` counts from `Analyzer`: unchanged) |
| `src/ui/shelf_drawings.{hpp,cpp}` | `Item::Pencil` after `Wire`; `Pencil(Pen&)` |
| `src/ui/shelf.cpp` | the entry, its owner, its key |
| `src/ui/board_view.{hpp,cpp}` | `setMarks`, `markReach`, `takeRightClick`; the switches not pressed under the pencil; the marks drawn |
| `src/ui/main.cpp` | the pencil's click, right-click, the swap, `--mark`, `--focus pencil` |
| `docs/GLOSSARY.md`, `docs/ARCHITECTURE.md`, `docs/recipes/ADD_INSTRUMENT.md` | below |

## The marks (`src/view/pencil_marks.hpp`)

ImGui-free, tested like `focus` and `machine_keys`.

```cpp
// A grease pencil's mark on the Board (P6h): the player's note, drawn whatever the tool or layer.
enum class MarkShape : std::uint8_t { Dot, Cross, Check };

struct PencilMark {
    Point at;                         // nm, Board coordinates
    MarkShape shape;
    std::optional<std::size_t> body;  // the Component whose body it is on: gone with a swap
};

class PencilMarks {
public:
    std::span<const PencilMark> marks() const { return marks_; }
    // A pencil click at `at`: the nearest mark within `reach` nm takes the next shape (dot,
    // cross, check, then none: removed); with none in reach, a dot there, on `body`.
    void click(Point at, std::optional<std::size_t> body, std::int64_t reach);
    // A Shift-click or right-click: the nearest mark within `reach` removed; false: none there.
    bool clear(Point at, std::int64_t reach);
    void swapped(std::size_t component);  // the marks on its body removed
    void add(PencilMark mark);            // --mark

private:
    std::optional<std::size_t> nearest(Point at, std::int64_t reach) const;

    std::vector<PencilMark> marks_;
};
```

`nearest`: the smallest Euclidean distance (as `double`, from the `int64` nm) that is `<= reach`; the last on a tie (the one drawn on top). `click` on a found `Check` erases it (`std::vector::erase`, the order of the rest kept: the later drawn on top). Test cases `pencil marks: …` `[view]`, as the spec lists.

## The tool

- `src/ui/main.cpp`'s `switch (focus.owner())` has no `default` and `-Werror` is on: phase 1 adds `case Owner::Pencil: break;` there, phase 2 fills it.
- `src/view/focus.hpp`: `Owner { Probe, Puller, Iron, Cutters, Wire, Pencil, Analyzer, Scope, Meter }`. `Focus::take`, `escape` work unchanged. `focus_test`: a case `focus: the pencil is a tool` (`IsInstrument(Owner::Pencil)` false; `take(Pencil)` owns it; `escape(false)` → `ToProbe`).
- `src/ui/shelf_drawings`: `Item::Pencil` between `Wire` and `Tv`. `Pencil(Pen& p)` in the 56-unit cell, drawn diagonally as the iron: a white paper-wrapped body (`line(46, 46, 22, 22, kWhite, 9)`), the paper's spiral as four short dark-grey strokes across it, the wax tip (`line(22, 22, 13, 13, IM_COL32(215, 215, 210, 255), 5)`, a `disc` at its end), the pull string a thin dark line from the body; a small grey check scribbled below left (`polyline`, 3 points) — what it does.
- `src/ui/shelf.cpp`: `kItems` gains `{Item::Pencil, "Grease pencil: mark the Board; click a mark: dot, cross, check, none; shift- or right-click: clear", "G", 1}` after Wire (the array 19); `GroupEnds` moves from `Wire` to `Pencil`; `OwnerOf(Item::Pencil)` → `Owner::Pencil`; `kTools` gains `{ImGuiKey_G, Owner::Pencil}` (the array 7). `NeedsPowerOff` unchanged (not the pencil). The click's `default` branch already takes the tool and gives the keys to the bench. The shelf grows one cell (52 px): the hint starts further right.

## The Board (`src/ui/board_view`)

- `setMarks(std::span<const PencilMark>)`, kept in `marks_` (as `setCuts`).
- Constants: `kMarkSize = 4'000'000` (nm across; 2.5 mm at first, too small), `kMarkMinPx = 10`, `kPencil = IM_COL32(205, 205, 200, 235)`, `kPencilEdge = IM_COL32(0, 0, 0, 110)`.
- `markReach()`: half the drawn size in nm at the current zoom, `max(kMarkSize / 2, kMarkMinPx / 2 · nmPerPixel)`: a click on what is drawn is on the mark.
- Drawn after the bodge wires and the wire's rubber band, before the probe's tip (above the parts, the references, the selection and the pin names; under the tip, clips, hooks, leads and the refusal), for every layer and in the plain look: `s = max(map.pixels(kMarkSize), kMarkMinPx)`, stroke `t = max(2, 0.18 s)`, each stroke first in `kPencilEdge` at `t + 2`, then `kPencil` at `t`. Dot: a disc of radius `s / 4` (edge radius `+1`). Cross: `(−s/2, −s/2)–(s/2, s/2)` and `(−s/2, s/2)–(s/2, −s/2)`, around the centre. Check: a polyline `(−0.45 s, 0)`, `(−0.15 s, 0.35 s)`, `(0.45 s, −0.4 s)` (y down).
- A double-click on bare Board fits the view (`input`, `IsMouseDoubleClicked && !picked`); not under the pencil, where two quick clicks change a mark from dot to cross.
- The switches: `buttonAt` (in `input`) already returns none under `Owner::Puller`; it returns none under `Owner::Pencil` too, so a left press there is a Board click (`pressed_`), a slide does not move and a right-click toggles no latch. A press on a Board key then reports `BoardPress{nullopt}`, so it gives the PC keys back to the bench, as under the puller (`main.cpp`'s press routing).
- `takeRightClick()`: `std::optional<Point>`, set in `input` where the right button is released without dragging over the panel (the same test as the latch toggle), to the Board point under the cursor; taken each frame by `main.cpp` whoever owns the focus (discarded unless the pencil). Right-drag pans from the first pixel while the release test uses ImGui's drag threshold, so a shaky right-click may pan a pixel or two and still clear: accepted.

## `main.cpp`

- `PencilMarks marks;` beside `wireStart`.
- `ParseOwner` gains `{"pencil", Owner::Pencil}` (its array 9); the usage line `--focus probe|swap|iron|cut|wire|pencil|analyzer|scope|meter` and `[--mark dot|cross|check:X:Y,...]`.
- `--mark`: in `ParseOptions`, each item (`SplitCommas`) split at `:` into a shape name and `X`, `Y` in millimetres (`ParseMillimetres`, `#include "board/kicad_board.hpp"`), kept as `Options::mark` (`std::vector<std::pair<MarkShape, Point>>`); a bad item makes `ParseOptions` fail (the usage error). In `main()`, beside the `--repair` loop, marks with no Board (`!session.machine()`) are the usage error. In `RunWindow`, once `boardView` exists, each becomes `marks.add({at, shape, boardView.scene()->bodyAt(at)})`.
- The click: the selection (`select` / `deselect`) skipped when `focus.owner() == Owner::Pencil`. `case Owner::Pencil`: `click->shift` → `marks.clear(click->at, boardView.markReach())`; else `marks.click(click->at, scene->bodyAt(click->at), boardView.markReach())`. Nothing else (no probe touch, no refusal).
- The right-click: `if (const auto at = boardView.takeRightClick(); at && focus.owner() == Owner::Pencil) marks.clear(*at, boardView.markReach());`, after the click.
- The swap: in `case Owner::Puller`, when the swap is `Done`, `marks.swapped(*click->component)`. `--repair swap:` runs before any mark exists: nothing to clear.
- Each frame, with the other tags: `boardView.setMarks(marks.marks())`.

## Docs

- `docs/recipes/ADD_INSTRUMENT.md`: the Focus row says a Board tool is taken, with no `out_` slot or window (the pencil's case).
- `docs/GLOSSARY.md`: **Pencil Mark** after **Marker** (a grey dot, cross or check the player puts on the Board with the grease pencil, a note for the session; a click on it changes its shape, a Shift-click or right-click clears it; one on a chip's body leaves with the chip when it is swapped; _Avoid_: Marker (that is the Instruments' time)); **Shelf**: the Board's tools list the pencil; **Focus**: red only for a tool that needs the power off (the pencil never).
- `docs/ARCHITECTURE.md`: the P6h row done with the date, the screenshots' result, the links; the `src/view` list if it names its modules.

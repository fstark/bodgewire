# P6d — Equipment objects

Roadmap step P6d ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the second build step of [UX_DESIGN.md](../p6b-ux-design/UX_DESIGN.md#equipment-on-the-bench) (P6b), after the shelf and focus (P6c). Design: [EQUIPMENT_OBJECTS_DESIGN.md](EQUIPMENT_OBJECTS_DESIGN.md). Plan: [EQUIPMENT_OBJECTS_PLAN.md](EQUIPMENT_OBJECTS_PLAN.md). Recipes: [ADD_INSTRUMENT.md](../recipes/ADD_INSTRUMENT.md) (Window layer, the CLI layer's `--manual chip`), [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md) (UI layer). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

After P6c the screen is the Board on two thirds of the width, a side panel on the last third (Puzzle, TV, Knob, Chip, Repairs, Debug) and the shelf. The TV is a side-panel section, a picture in a box; the knob a slider; the instruments and keyboards are floating windows that open wherever ImGui first put them and stay there. P6b settled that the equipment become objects floating over the Board, each with a default place it returns to, a free size with two presets, brought out and put away from the shelf, and that the side panel goes.

## User decisions (2026-11-17)

| Question | Decision |
|---|---|
| The side panel's other sections, until P6e (binder) and P6f (post-it) | interim homes: the Puzzle brief a small floating note, out at start, with its own drawing on the shelf (P6f makes it the post-it); Chip and Repairs tabs in the User Manual window (P6e makes that window the binder); Debug behind `--debug`, a small window |
| Minimal and normal | a button in the title bar, on the objects that have one (the instruments). The objects without a title bar get no presets for now (a double-click on some part of them, maybe later); they keep a free size |
| The objects' frame | the TV, the keyboard and the knob are drawn cases with no title bar: dragged by their body, a small `×` drawn on the case puts them away, resized from the corner keeping their proportions. The instruments keep their title bars |
| Default places | UX_DESIGN's: the TV top right, the keyboard under it (right-aligned), the knob top left, the instruments along the bottom above the shelf (analyzer left, scope centre, meter right) |

Decided while designing (2026-11-17), for the user to overrule:

- **Back in its place, not its size.** An object comes out at its default place every time (ImGui's `Appearing`), keeping the size the player gave it during the run. Its first size is its normal one.
- **The Board takes the whole width** above the shelf; `kBoardShare` goes.
- **Normal and minimal sizes** (logical px, the window's outer size): analyzer 940 × 440 and 560 × 260, scope 680 × 460 and 420 × 300, meter 320 × 200 and 240 × 150. The button goes to whichever preset is further from the current size, so it reads as a toggle and also brings a hand-resized window back to a preset. The title bar's collapse arrow goes (`NoCollapse`): a collapsed instrument would be a third size.
- **Cases** (first sizes; the proportions fixed): the TV a dark grey monitor, 440 px wide, a 4:3 picture inset in a bezel, a chin with a pilot lamp and two dummy knobs (P6h makes position controls); the knob a dark box per paddle controller side by side, each a large black knob with a white pointer and its reference printed, 140 px a box. The keyboards as today without their title bar (the case already carries the Machine's name and reference). The `×` a small circle in the case's top-right corner, faint until hovered.
- **The knob turned** by a vertical drag on it (up turns clockwise), the mouse wheel over it, and the keys as today (Up/Down, Q/A for a second paddle), also while it is away. Its tooltip says so. The pointer sweeps 270°.
- **Other default places**: the brief note top centre (340 × 120, clear of the TV); the manuals window on the left under the knob's place (480 × 460, room for a pinout); the Debug window at the bottom centre (under the scope's place, clear of the manuals).
- **Overlaps accepted.** The places are UX_DESIGN's for one object at a time: the three instruments at normal size are 1940 px wide together on a 1264 px bench, so the scope covers the analyzer's middle and the meter its right end, and a normal scope reaches under the TV's corner. The one last brought out is in front; minimal sizes and putting away answer the rest. The test checks each object inside the bench and the brief clear of the TV, not that objects never overlap.
- **What the side panel showed always, now on demand**: the chip's pinout is in the manuals window's Chip tab (out with `U`); the title is the brief note's on a Puzzle and only the OS window's on a Machine or netlist run; Debug only with `--debug` (no key to open it at run time).
- **`--manual chip`** brings the manuals window out at its Chip tab, for screenshots (`--manual user` as before).
- **The brief's shelf drawing** is a yellow note with three lines, last in the equipment group (after the knob), grayed on a run that is not a Puzzle (no brief). Its window has a title bar (`Puzzle brief`), the Puzzle's title large and the brief wrapped: it is not equipment, and P6f replaces it.
- **The manual window** is titled `Manuals`, tabs `User Manual`, `Chip` (the side panel's Chip section as it was, `hover or click a chip` when none; absent without a Board) and `Repairs` (the log, `none yet`; absent without a Board). The shelf drawing and `U` are unchanged.
- **Start-up**: the TV, the knob and the brief out where the Machine or Puzzle has one, the keyboard out where it has one, the instruments as their flags say (P6c), the manuals away unless `--manual user`.

## What is new

1. **Places and presets** — `src/view/bench_places.{hpp,cpp}` (ImGui-free, unit tested): each object's default place in the work area above the shelf (an anchor and a pivot), the instruments' two sizes, the preset the size button goes to.
2. **Instrument windows** — `BeginInstrument` takes the object's kind: the default place on appearing, the normal size first, the size button in the title bar, no collapse.
3. **Cases** — the TV (`src/ui/tv_window.{hpp,cpp}`) and knob (`src/ui/knob_window.{hpp,cpp}`) become windows drawn as cases; both keyboards lose their title bar; the bench area, the default place, the case `×` and the aspect constraint shared (`src/ui/bench.{hpp,cpp}`).
4. **Side panel removed** — `side_panel.{hpp,cpp}` deleted: the brief window and its shelf drawing, Chip and Repairs tabs in the manuals window (`src/ui/manual_window.{hpp,cpp}`, `--manual chip`), `--debug`. The Board is drawn across the whole width.
5. **Docs** — ADD_INSTRUMENT (Window and CLI rows), ADD_CONNECTOR_DEVICE (UI row), GLOSSARY (**Equipment**: default place, size presets), the ARCHITECTURE rows (P6d done; P6e: the Chip tab already in the manuals window), the LED computer's design note on the Debug panel's seed.

Nothing below the UI changes: engines, consumers, the Session, the kernel, Parts, Machines and Puzzles are untouched.

## Tests

- **Places** (`tests/view/bench_places_test.cpp`): on the 1264 × 716 bench of a 1280 × 800 window, every object at its first size (instruments at both presets; keyboards at aspects 0.36, 0.43, 0.54; two knobs) lies wholly inside the bench at its default place; the TV's top-right corner and the meter's bottom-right corner sit at the margins; the scope and brief are centred, the brief clear of the TV; the keyboard's top is under the TV's first bottom; a keyboard keeps 640 px when it fits and narrows when not. Presets: from normal the button gives minimal, from minimal normal, from a hand size nearer normal minimal.
- No UI tests exist; clicks (the size button, the case `×`, a drag on a case, the knob) are checked by reading the code and by the user.
- **Screenshots** (release, viewed): the Stopwatch with every instrument (`--clip`, `--hook`, `--meter`: their places along the bottom, no side panel, the brief note); the BW-2001 (the TV case top right, the keyboard under it without a title bar); Tennis (two knob boxes top left); `tests/data/bw1_bench.bwn` (no Board, the brief grayed); the manuals window's Chip tab with `--manual chip --probe U1.1`; `--debug`.
- **Release:** the 110 with every instrument as in P6c: 1.00×, UI under ~1 ms. The full release suite passes (no test reaches the UI but `bench_places`).

## Later

- P6e makes the manual window the binder (opening at the selected chip); P6f the brief a post-it on the Board; P6h the TV's position controls and the keyboard drawer.
- Presets on the cases (a double-click, maybe), if wanted.

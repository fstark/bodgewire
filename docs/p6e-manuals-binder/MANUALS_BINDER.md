# P6e — Manuals binder

Roadmap step P6e ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the third build step of [UX_DESIGN.md](../p6b-ux-design/UX_DESIGN.md#equipment-on-the-bench) (P6b), after the equipment objects (P6d). Design: [MANUALS_BINDER_DESIGN.md](MANUALS_BINDER_DESIGN.md). Plan: [MANUALS_BINDER_PLAN.md](MANUALS_BINDER_PLAN.md). Recipes: [ADD_INSTRUMENT.md](../recipes/ADD_INSTRUMENT.md) (the CLI layer's `--manual`), [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md) (UI layer: a case window). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

After P6d the manuals are an ImGui window titled `Manuals` with three tabs: `User Manual`, `Chip` (the hovered or selected chip: role, pinout with its Nets, the Datasheet's Function and Pins) and `Repairs`. The Service Manual is never shown as a document, only each chip's role line taken from it; a Datasheet is seen only through a chip on the Board. P6b settled one object for all of them: a ring binder with tabbed dividers, opening at the selected chip, the Chip section part of it.

## User decisions (2026-11-17)

| Question | Decision |
|---|---|
| The look | a drawn binder, a case with no title bar like the TV: a cover with rings on the left, paper pages (dark text on cream), coloured divider tabs on the right edge; dragged by its cover, put away by the case `×`, resized keeping its proportions; the shelf drawing a ring binder |
| The dividers, front to back | `User Manual`; `Service Manual` (the whole Components list, its references as links); `Datasheets` (the chip's sheet: reference, role, pinout with Nets, Function, Pins; with no chip, an index of the Machine's Parts with their references, a reference selecting its chip); `Repairs` (the work log) at the back |
| When it turns to a datasheet | it comes out at the User Manual; selecting a chip on the Board turns it to that chip's datasheet, whatever divider is open; on the Datasheets divider the page follows the hovered chip (as the Chip tab did) |

Decided while designing (2026-11-17), for the user to overrule:

- **Every time it comes out, the User Manual**, even with a chip already selected: the jump is on a selection made while it is out (or `--manual datasheet`). A selection made while it is away does not turn it.
- **What counts as selecting**: any Board click that selects a part today: on its body, and on one of its pins whatever tool or instrument has the focus (a probe, clip, hook or lead put on a pin of another chip selects that chip, so it turns the binder too); a reference clicked in a manual (`BoardView::route` selects its chip) or in the index; a part chosen by `--probe` at start. A click that deselects (bare board, the selected part again) does not turn the page. If turning on every pin touch proves a nuisance in play, the user may narrow it to body clicks.
- **The Datasheets page**, by precedence: the hovered chip, else the selected chip, else a Part opened from the index, else the index. Moving the mouse across the Board flips it, as the Chip tab did; over the binder it shows the selection.
- **The index**: `Datasheets` large, then one line per Part on the Machine in netlist order of first use: the Part's name (a link opening its sheet) and its references (links: hover marks the chip on the Board, a click selects it and so turns to its page). A Part with no datasheet is listed without the name link. Long reference lists wrap.
- **A Part opened from the index** (by its name, or on a run with no Board) is its Datasheet as written: title, Function, Pins. A chip's page keeps today's Chip section: header, role, `Zoom to`, the drawn pinout with its Nets, Function open, Pins folded.
- **Back to the index**: an `All datasheets` button at the top of every sheet; on a chip's page it deselects the chip.
- **Without a Board** (a netlist-only run such as `tests/data/bw1_bench.bwn`, which has no manuals: `no user manual`, `no service manual`): the index with its references as plain text (no targets), a Part's sheet from its name; the Repairs tab grayed, not clickable, and `--manual repairs` opens the User Manual. Every Machine directory has a Board and both manuals today.
- **Each divider keeps its scroll** for the run (one scrolling page per divider).
- **Geometry** (first size 600 × 500, the proportions fixed, at least 500 wide (a BW-2001 pinout row needs about 126 px a side); fixed px, the page takes the rest): the cover dark blue with a lighter edge; on the left a 34 px spine strip with three rings straddling the page's punched holes; the cream page from there to 86 px from the right, 12 px from the top and bottom; the four tabs stacked down the right edge from 30 px, each 72 × 44 px, its divider's colour (pale yellow, green, blue, pink), the open one joined to the page and brighter, the others set back. The text stays at the UI's size: a larger binder shows more of the page. The `×` in the cover's top-right corner, above the tabs. Default place unchanged (left, under the knob's place); 500 high fits the 508 px there on the 1264 × 716 bench.
- **Paper colours** everywhere a manual is drawn (`ManualView`, the Chip section): text near black, bold black, code a dark amber, links a dark blue, folding headers a light tan; tooltips from the page keep the UI's light text on dark.
- **`--manual user|service|datasheet|repairs`**: out at that divider; `--manual chip` goes (`datasheet` with `--probe U1.1` is its screenshot).
- **The shelf drawing**: a blue ring binder seen closed, three coloured tabs at its right edge and rings on its spine; out: open, two cream pages either side of the rings. Tooltip `Manuals binder: User and Service Manuals, datasheets, repairs`, key `U` as before.
- **The window** keeps the ID `###manuals` (the shelf brings it to the front by it).

## What is new

1. **Binder state** — `src/view/binder.{hpp,cpp}` (ImGui-free, unit tested): the open divider, the jump on a selection, the User Manual on coming out, the Part opened from the index.
2. **Datasheet index** — `DatasheetIndex(const LoadedCircuit&)` in `src/manual/manual.{hpp,cpp}` (unit tested): each Part on the Machine with its references, netlist order of first use.
3. **The binder** — `src/ui/manual_window.{hpp,cpp}` drawn as a case: cover, rings, page, tabs; the four dividers. `ManualView` and `ChipPanel` in paper colours.
4. **Shelf and CLI** — the binder's shelf drawing and tooltip; `--manual user|service|datasheet|repairs`.
5. **Docs** — GLOSSARY (**Service Manual**, **Datasheet**, **Equipment**: the binder), ADD_INSTRUMENT's CLI row, ARCHITECTURE (P6e done).

Nothing below the UI changes but one function in `bw_manual`: engines, the Session, the kernel, Parts, Machines and Puzzles are untouched.

## Tests

- **Binder** (`tests/view/binder_test.cpp`): it comes out at the User Manual, also with a chip selected; a new selection while out turns to Datasheets from any divider and clears an opened Part; the same selection kept, or a deselection, does not turn; a selection made while away does not turn it when it comes out; `show(divider)` (the CLI) wins over coming out at the User Manual and keeps the start-up selection from turning it; turning to a divider by its tab; opening a Part from the index and going back to it.
- **Index** (`tests/manual/manual_test.cpp`): on a small netlist, Parts in order of first use, each with its references in netlist order, a Part used twice listed once.
- No UI tests exist: the tabs, `All datasheets`, the jump and the hover are checked by reading the code, by screenshots and by the user.
- **Screenshots** (release, viewed): the Stopwatch with `--manual user`, `--manual service`, `--manual datasheet --probe U1.1` (a chip's page) and `--manual datasheet` (the index), `--manual repairs`; the BW-2001's index (many Parts, wrapped references); `tests/data/bw1_bench.bwn --manual datasheet` (no Board: plain references, Repairs grayed) and `--manual repairs` (the User Manual's `no user manual`); a BW-2001 chip's page (`--manual datasheet --probe UF3.1`); the shelf with the binder out and away.
- **Release:** the 110 with every instrument as in P6d: 1.00×, UI about 1 ms. The full release suite passes.

## Later

- P6f makes the brief a post-it, P6h the TV's controls and the keyboard drawer.
- A Datasheet's drawn pinout for a Part opened from the index (no chip, so no Nets), if wanted.

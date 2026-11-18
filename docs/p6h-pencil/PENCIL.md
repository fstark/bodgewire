# P6h — Pencil

Roadmap step P6h ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): a new Board tool on the shelf, a white grease pencil (a china marker) to mark the Board during a session. Design: [PENCIL_DESIGN.md](PENCIL_DESIGN.md). Plan: [PENCIL_PLAN.md](PENCIL_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

A repair is a search: the player probes chip after chip and keeps in their head which ones are fine, which pin looked wrong, which chip was already swapped. On a real bench a technician marks the Board with a china marker (a tick on a chip checked, a cross on a suspect). Nothing in `bw_ui` lets the player write down what they found; on the BW-2001 or the BW-80, with forty chips, the head is not enough.

## User decisions (2026-11-18)

| Question | Decision |
|---|---|
| The tool | a white grease pencil (a china marker) on the shelf, among the Board's tools (the row) |
| A click | on a chip or anywhere on the Board puts a grey mark there; a click on a mark changes its shape: dot, cross, checkmark, no mark; Shift-click or right-click clears it (the row) |
| Visible | always, whatever the tool or layer (the row) |
| Its key | `G` (grease pencil) |
| A mark on a chip | where the click was; a mark on a chip's body goes away when that chip is swapped with the puller (the real mark would leave with the chip); a mark anywhere else stays |
| Size | in Board millimetres, growing with the zoom; the user unsure 1.5 mm is large enough (below) |
| A switch under the pencil | a click marks it and never presses it (as the puller: a switch is only a place to mark) |

Decided while designing (2026-11-18), for the user to overrule:

- **Size**: 2.5 mm across (a pin pitch, a third of a DIP's width), never under 10 px so it reads zoomed out. One constant, set again after the user sees it.
- **The colour**: a light grey `(205, 205, 200)` stroke over a faint dark edge, so it reads on the green, on a black chip and on the tin pads. Not the selection's near-white nor any gold.
- **Strokes**: a dot (a filled disc, a third of the size across), a cross (two strokes corner to corner), a checkmark (a short stroke down, a long one up to the right).
- **On a mark**: a click within the drawn mark's half-size of its centre; of two overlapping marks the nearest. A click off every mark puts a new dot, even next to one.
- **A chip's body**: the courtyard off its pads (`BoardScene::bodyAt`, as for the switches). A mark on a pin's pad belongs to the Board: it stays when the chip is swapped.
- **No selection**: a pencil click does not select the chip (the selection's gold and wiring would cover what was just marked), and does not move the probe.
- **Over everything on the Board**, under the probe's tip and the instruments' tags (clips, hooks, leads), whatever the layer and in the plain look too.
- **For the session**: marks live as long as `bw_ui` runs; power off and on keeps them; nothing is saved.
- **The pencil works with the power on** (it is not a repair): never framed red.
- **Esc** gives the probe the focus back, as with every tool.
- **`--mark SHAPE:X:Y,...`** (`bw_ui`): marks at start, `dot`, `cross` or `check` at Board millimetres (as `--repair cut:`), for screenshots; **`--focus pencil`**.

## What is new

1. **Pencil marks** — `src/view/pencil_marks.{hpp,cpp}` (ImGui-free, unit tested): the marks, what a click, a clear and a swap do to them.
2. **The tool** — `Owner::Pencil`; the shelf's drawing after the wire, key `G`.
3. **The Board** — the marks drawn; under the pencil a right-click reported and the switches not pressed.
4. **`main.cpp`** — a pencil click, a clear, a swap clearing the chip's marks; `--mark`, `--focus pencil`.
5. **Docs** — GLOSSARY (**Pencil Mark**, the Shelf's tools), ARCHITECTURE.

The kernel, Parts, the builder, devices, Session, Machines and Puzzles are untouched.

## Tests

- **Pencil marks** (`tests/view/pencil_marks_test.cpp`): a click on bare Board puts a dot; clicks on it give cross, check, then none; a click beyond the reach puts a second mark; of two in reach the nearest changes; a clear removes the nearest and reports none in reach; a swap removes the marks on that body only.
- **Focus** (`tests/view/focus_test.cpp`): the pencil is a tool, not an instrument; Esc gives the probe the focus from it.
- No UI tests exist: the clicks are checked by reading the code, by screenshots and by the user.
- **Screenshots** (release, viewed): the Stopwatch with `--focus pencil` and a dot, a cross and a check (on a chip, on bare Board, on a button), the shelf's pencil framed; the BW-80 zoomed out (the floor); the same marks with `--look plain` and with B.Cu active (Board view `L` is a key: checked by reading, the layer not being a flag).
- **Release:** the 110 with every instrument (1.00×, UI about 1 ms). The full release suite passes.

## Later

- Marks saved with a Puzzle's progress, if P7 keeps progress.
- Other colours (a red pencil for the faults found), if the grey alone is not enough.

# Board View — Specification (P2f)

First of three milestones that complete P2's **first playable** (see [ARCHITECTURE.md](../ARCHITECTURE.md), Connectivity and visibility, Roadmap): P2f shows the Board, P2g probes it while it runs ([BOARD_PROBE.md](../p2g-board-probe/BOARD_PROBE.md)), P2h repairs it ([BOARD_REPAIR.md](../p2h-board-repair/BOARD_REPAIR.md)). Here the player sees the Board in `bw_ui`, zooms, pans and can find a crack by eye. Builds on [UI_SHELL.md](../p2e-ui-shell/UI_SHELL.md), [FAULTS.md](../p2d-faults/FAULTS.md) and the Board reader of [BOARD_LAYOUT.md](../p2a-board-layout/BOARD_LAYOUT.md). Presentation: [GAME_DESIGN.md](../GAME_DESIGN.md), Presentation.

Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Decisions

- **One See-through Board**, drawn from the Machine's `.kicad_pcb` (outline, Traces, pads, vias, Component courtyards): both Layers in the same top-down frame at their KiCad coordinates, the Active Layer on top at full opacity, the other dimmed beneath it. No flip, no KiCad silkscreen.
- **The UI does not leak Faults.** Drawing, hover text and Net highlighting come from the design Netlist and the fault-free Copper, so highlighting runs across a crack. A crack is drawn as what it is, a gap in the Trace's copper: hard to see unless zoomed in (appearance "subtle"), which is the point. A Stuck-at Fault is invisible. Only instruments (P2g) read electrical Nets.
- **The TV is optional.** A Machine without a VIDEO connector opens too; P2e required one only because the TV was all there was to see.
- **Screenshot option** for checks without macOS screen-recording permission: `bw_ui` saves its own window to a PNG, then exits.
- **Look and feel are first guesses.** Colours, zoom limits and pick rules are tuned once the user can play with them; the spec only fixes what the tests check.

## Scope

In: Board panel (draw, zoom, pan, Active Layer, hover, Net highlight); Component courtyards read from the `.kicad_pcb`; `.bwn` files run as fault-free Machines when their Board exists; Machines without VIDEO; `--screenshot`; headless tests for everything that is not drawing.

Out: probing on the Board (P2g); Repairs from the UI (P2h); schematic view; developer mode showing Faults; settings persistence; sound.

## Behaviour

### S1 Loading

- `bw_ui FILE.bwp` loads the Puzzle as in P2e. `bw_ui FILE.bwn` loads it as a Machine without Faults when `boards/<stem>/<stem>.kicad_pcb` exists next to its `netlists/` directory, otherwise as a netlist alone, with the Board panel showing `no board`.
- No VIDEO connector is no longer an error: the Machine runs and the TV panel shows `no video` (e.g. `bw_ui netlists/counter.bwn`).
- The window title stays `Bodgewire — <machine name>`.

### S2 Layout

- Board panel on the left (about two thirds of the width), right column with, top to bottom: TV, view controls (Active Layer, Fit), power controls (P2e), status line (P2e). P2g and P2h add rows to this column.
- Default window 1280 × 800; everything resizes with it.

### S3 Drawing

- Board: dark green inside the outline. Copper: Traces with their width and round ends, arcs as arcs, pads with their shape, size and rotation, vias as rings. Front copper and back copper have distinct colours; pads and vias (on both Layers) are drawn above both.
- Components: their footprint's courtyard (`F.CrtYd` in the `.kicad_pcb`, read with the pads), the reference (U5) inside, a mark at pin 1.
- Active Layer: F.Cu or B.Cu, F.Cu by default; toggled by a button or the `L` key.
- A crack is a real gap in the Trace (P2d geometry), nothing more.

### S4 Navigation

- Opens fitted: the whole outline visible, centred. Mouse wheel zooms around the cursor, from the fitted view up to 100 px/mm; right-button drag (or middle) pans; `F` or a Fit button refits.
- Crack size: P2d cuts 0.3 mm between Trace ends on a 0.2 mm Trace; with round ends the visible gap is 0.1 mm, ≈ 10 px at 100 px/mm and under 1 px fitted (≈ 5 px/mm on the test-pattern Board).

### S5 Hover and highlight

- Hovering shows one line under the cursor: a pad → `U1 pin 14 QA · H0` (reference, pin number, pin name, design Net; `—` when the Pin is on no Net); a Trace or via → its design Net; inside a courtyard → `U5 74LS161`; elsewhere nothing.
- Pads, vias and Traces of the hovered design Net are highlighted (amber) on both Layers, crossing cracks (fault-free Copper).
- Hit priority (first guess): pad, then via, then Trace on the Active Layer, then Component. Pads and vias hit on either Layer (through-hole).

### S6 Screenshot

- `bw_ui FILE --screenshot OUT.png [--after 1s]` runs normally (power on, `1×`), and after the given wall-clock duration (default 1 s, `bw run` duration syntax) saves the window's pixels to `OUT.png` and exits 0. Write error → message on stderr, exit 1.

### S7 Performance

- The test pattern still runs at ≈ 1.00× at `1×` with the Board panel shown.
- UI frame time < 16 ms with the Board fitted and at full zoom (test-pattern Board, ~500 Trace segments).

### S8 Checks

- Headless (no SDL/ImGui):
  - every Component of both Boards has a courtyard;
  - hit testing on the test-pattern Board: a point on each kind of object returns it, with the S5 priority; a point off everything returns nothing;
  - design-Net highlight from a Trace on one side of the `broken_trace` crack includes the Trace on the other side;
  - `.bwn` with and without a Board loads as S1; the counter Machine (no VIDEO) loads and runs.
- `--screenshot` of the test pattern and of the counter at 1 s: a PNG of the window's pixel size (×2 on Retina) exists; the agent reads it to check the drawing.
- Manual: the Done-when check.

## Done when

- S1–S8 implemented; tests pass in debug and release.
- The user opens `bw_ui puzzles/broken_trace.bwp`, finds the crack by zooming along the highlighted Net, and the look is good enough to build P2g on.

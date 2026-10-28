# Board View — Specification (P2f)

Last milestone of P2 (see [ARCHITECTURE.md](../ARCHITECTURE.md), Connectivity and visibility, Instruments, Roadmap) and of the **first playable**: the player sees the Board, probes it by clicking, powers off, repairs it and powers on again, all in `bw_ui`. Builds on [UI_SHELL.md](../p2e-ui-shell/UI_SHELL.md), [FAULTS.md](../p2d-faults/FAULTS.md) and the Board reader of [BOARD_LAYOUT.md](../p2a-board-layout/BOARD_LAYOUT.md). Presentation: [GAME_DESIGN.md](../GAME_DESIGN.md), Presentation.

Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Decisions

- **One See-through Board**, drawn from the Machine's `.kicad_pcb` (outline, Traces, pads, vias, Components): both Layers in the same top-down frame at their KiCad coordinates, the Active Layer on top at full opacity, the other dimmed beneath it. No flip, no KiCad silkscreen.
- **The UI does not leak Faults.** Drawing, hover text and Net highlighting come from the design Netlist and the fault-free Copper, so highlighting runs across a crack. A crack is drawn as what it is, a gap in the Trace's copper: too small to see at full-Board zoom, visible when zoomed in (appearance "subtle"). A Stuck-at Fault is invisible. Only the Logic Probe reads electrical Nets.
- **Three tools**: Probe, Swap, Mend (the P2d Repairs). Probing works at any time; Repairs only with the power off, as decided in P2d. Nothing ever says whether a Repair fixed something: the player powers on and looks.
- **One Logic Probe**, the P2d model (HIGH, LOW, PULSE lamps), fed by the runner's counted watches (P2e).
- **Screenshot option** for checks without macOS screen-recording permission: `bw_ui` saves its own window to a PNG, then exits.
- **No repair validation, no undo.** The grid coming back on the TV is the win in P2; Repair Validation is P3.

## Scope

In: Board panel (draw, zoom, pan, Active Layer, hover, Net highlight); Probe, Swap and Mend tools; probe lamps; repair log; `.bwn` files run as fault-free Machines when their Board exists; `--screenshot`; headless tests for everything that is not drawing.

Out: schematic view; Bodge Wires, Trace cuts and other Repairs (P3); scope, analyzer, multimeter (P3); Repair Validation (P3); developer mode showing Faults; undo; part inventory; settings persistence; sound.

## Behaviour

### S1 Loading

- `bw_ui FILE.bwp` loads the Puzzle as in P2e. `bw_ui FILE.bwn` loads it as a Machine without Faults when `boards/<stem>/<stem>.kicad_pcb` exists next to its `netlists/` directory, otherwise as a netlist alone, with the Board panel showing `no board`.
- The window title stays `Bodgewire — <machine name>`.

### S2 Layout

- Board panel on the left (about two thirds of the width), right column with, top to bottom: TV, probe lamps, tools and Active Layer, power controls (P2e), repair log, status line (P2e).
- Default window 1280 × 800; everything resizes with it.

### S3 Drawing

- Board: dark green inside the outline. Copper: Traces with their width and round ends, arcs as arcs, pads with their shape, size and rotation, vias as rings. Front copper and back copper have distinct colours; pads and vias (on both Layers) are drawn above both.
- Components: a body outline around their pads, the reference (U5) inside, a mark at pin 1.
- Active Layer: F.Cu or B.Cu, F.Cu by default; toggled by a button or the `L` key; shown in the tools row.
- A crack is a real gap in the Trace (P2d geometry), nothing more.

### S4 Navigation

- Opens fitted: the whole outline visible, centred. Mouse wheel zooms around the cursor, from the fitted view down to 50 px/mm; right-button drag (or middle) pans; `F` or a Fit button refits.
- At 50 px/mm a crack on a 0.2 mm Trace (0.3 mm gap, P2d) is 15 px wide; fitted it is only 2–3 px, easy to miss.

### S5 Hover and highlight

- Hovering shows one line under the cursor: a pad → `U5 pin 14 QA · H0` (reference, pin number, pin name, design Net); a Trace or via → its design Net; a Component body → `U5 74LS161`; elsewhere nothing.
- Pads, vias and Traces of the hovered design Net are highlighted (amber) on both Layers, crossing cracks (fault-free Copper).
- Hit priority: pad, then via, then Trace on the Active Layer, then Component body. Pads and vias hit on either Layer (through-hole).

### S6 Probe tool

- Default tool (`P`). A left click on Copper touches it: a pad or via touches its island on both Layers, a Trace point touches the island of the Active Layer's Copper at that point, cracks included (so probing past a crack reads the far side). A click off Copper lifts the probe.
- The probe tip is drawn where it touches and stays there across power cycles and Repairs; it reads the island at that point in the current Copper.
- Lamps: HIGH, LOW, PULSE as in FAULTS.md S5 (pulse stretched 100 ms simulated). All dark when the power is off or the probe is lifted. Lamps follow the latest completed slice, refreshed every UI frame.
- Moving the probe costs nothing on other Nets: only the touched Net is watched, from the moment it is touched (ARCHITECTURE, Instruments).

### S7 Repair tools

- **Swap** (`S`): a left click on a Component body swaps it for a new one of the same Part (FAULTS.md S3).
- **Mend** (`M`): a left click on the Active Layer solders across that point: mends a crack within 1 mm on that Layer (FAULTS.md S3). A solder blob is drawn there whether or not a crack was mended.
- Both are ignored while the power is on; the tools row then reads `power off to repair`. Unlimited, in any order, as often as wanted.
- The repair log lists each action in order: `swap U5`, `mend F.Cu 52.30 41.00`. It does not say whether anything changed.
- Power on after Repairs rebuilds the Nets from the repaired Board (P2d); the TV then shows the repaired Machine. Seed and speed are kept.

### S8 Screenshot

- `bw_ui FILE --screenshot OUT.png [--after 1s]` runs normally (power on, `1×`), and after the given wall-clock duration (default 1 s, `bw run` duration syntax) saves the window's pixels to `OUT.png` and exits 0. Write error → message on stderr, exit 1.
- `--probe REF.PIN` (optional) touches that Pin's pad at start, so the lamps and tip show in the screenshot. Unknown pin → usage error, exit 2.

### S9 Performance

- The test pattern still runs at ≈ 1.00× at `1×` with the Board panel fitted and the probe on a busy Net (e.g. CLK).
- UI frame time < 16 ms with the Board fitted and at full zoom (test-pattern Board, ~500 Trace segments).

### S10 Checks

- Headless (no SDL/ImGui), on the test-pattern Board:
  - hit testing: a point on each kind of object returns it, with the S5 priority; a point off Copper returns nothing;
  - design-Net highlight from a Trace on one side of each Puzzle's crack includes the Trace on the other side;
  - probing a Trace point beyond the `broken_trace` crack reads a different electrical Net from the near side, and the same Net after a mend there;
  - Repairs are refused while powered and accepted while off; mend blobs and the repair log record every action, mended or not;
  - for each Puzzle: probe at the faulty Pin or past the crack → the P2d lamps; right Repair, power on → TV frame equals the fault-free frame (through the runner, as in P2e S4);
  - `.bwn` with and without a Board loads as S1.
- `--screenshot` of the test pattern at 1 s: a PNG of the window size exists; the agent reads it to check the drawing.
- Manual: the Done-when play-through.

## Done when

- S1–S10 implemented; tests pass in debug and release.
- The user plays the three Puzzles in `bw_ui` end to end, without reading the `.bwp` files: find the Fault with the probe (and zoom for the crack), power off, repair, power on, grid back.

# Board Repair — Specification (P2h)

Last of three milestones that complete P2's **first playable** (see [ARCHITECTURE.md](../ARCHITECTURE.md), Connectivity and visibility, Roadmap): the player powers off, repairs the Board, powers on and sees the grid back, all in `bw_ui`. Builds on [BOARD_VIEW.md](../p2f-board-view/BOARD_VIEW.md) (P2f), [BOARD_PROBE.md](../p2g-board-probe/BOARD_PROBE.md) (P2g) and the Repairs of [FAULTS.md](../p2d-faults/FAULTS.md).

Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Decisions

- **Three tools**: Probe (P2g), Swap, Mend (the P2d Repairs). "Tool" here is a click mode, not an Instrument. Probing works at any time; Repairs only with the power off, as decided in P2d.
- **Nothing says whether a Repair fixed something**: the player powers on and looks. The drawn Copper never changes: a mended crack stays drawn under its blob, otherwise the drawing would tell which mends worked.
- **Any Component can be swapped**, Connectors included (same code; a bad Connector may be a Fault later).
- **No repair validation, no undo.** The grid coming back on the TV is the win in P2; Repair Validation is P3.

## Scope

In: tool selection; Swap and Mend from the Board panel; solder blobs; repair log; rebuilding the Nets at power-on; `--repair` for screenshots; headless tests; the end-to-end play-through.

Out: Bodge Wires, Trace cuts and other Repairs (P3); Repair Validation (P3); undo; part inventory.

## Behaviour

### S1 Tools

- Tools row in the right column: Probe (`P`, default), Swap (`S`), Mend (`M`), with the Active Layer toggle. Changing tool keeps the probe where it is; only the Probe tool moves or lifts it (P2g S1).
- **Swap**: a left click on a Component (one of its pads or inside its courtyard) swaps it for a new one of the same Part (FAULTS.md S3); a click elsewhere does nothing. First guess, tuned by play.
- **Mend**: a left click on the Active Layer solders across that point: mends a crack within 1 mm on that Layer (FAULTS.md S3). A solder blob is drawn there on that Layer, somewhat visible at full-Board zoom, whether or not a crack was mended.
- Swap and Mend are ignored while the power is on; the tools row then reads `power off to repair`. Unlimited, in any order, as often as wanted.

### S2 Repair log

- Lists each action in order, under the power controls: `swap U5`, `mend F.Cu 52.30 41.00`. It does not say whether anything changed.

### S3 Power on after Repairs

- Rebuilds the Nets from the repaired Board (P2d); the TV then shows the repaired Machine. Seed and speed are kept.
- The probe tip stays where it was and reads the island at that point in the repaired Copper.
- Power-on after Repairs feels immediate: rebuild under 100 ms on the test-pattern Board (release).

### S4 Screenshot

- `--repair swap:U3,mend:F.Cu:137.59:99.17` (optional, same syntax as `bw run --repair`) applies Repairs before the first power-on, as if done in the UI (blobs, log), so they show in a `--screenshot`; combines with `--probe` (P2g S3). Bad Repair → usage error, exit 2.

### S5 Checks

- Headless (no SDL/ImGui), through the runner:
  - Repairs are refused while powered and accepted while off; mend blobs and the repair log record every action, mended or not;
  - touching both sides of the `broken_trace` crack reads the same Net after a mend there and power-on;
  - the drawn Board is the same before and after a mend;
  - for each Puzzle: right Repair, power on → TV frame equals the fault-free frame (as in P2e S4); a wrong swap does not.
- `--screenshot --repair` on `broken_trace`: the agent reads the PNG to check blob, log and grid.
- Manual: the Done-when play-through.

## Done when

- S1–S5 implemented; tests pass in debug and release.
- The user plays the three Puzzles in `bw_ui` end to end, without reading the `.bwp` files: find the Fault with the probe (and zoom for the crack), power off, repair, power on, grid back.

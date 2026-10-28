# UI Usability — Implementation Plan (P3a)

Design: [UI_USABILITY_DESIGN.md](UI_USABILITY_DESIGN.md)
Spec: [UI_USABILITY.md](UI_USABILITY.md)

P3a DONE 2026-10-28. Phases 1–4 and 6–8: commits 465e757..743ee9f (340 tests, debug and release). Release: `broken_trace --probe U1.CLK` at 1.00× after 3 s, `ui` 0.4 ms fitted; 0.7 ms with the Service Manual window and Chip panel open. User check: "way better than before"; manuals and Datasheets approved; pin names moved outside the chip (a7c86f0).

## Later (from the user check)

- Selection still feels odd in places (hovered vs selected Chip, picks); not yet described in detail.
- The Puzzle section's `When it works` is closed by default; revisit once the layout settles.

## Implementation notes (differences from the design)

- Snapping: half the pin pitch is capped at 1.27 mm, so a wide axial resistor does not grab a big area of the Board.
- Pin names sit left/right of the pads for a tall chip and above/below for a wide one (courtyard shape), not by the pad's own direction to the centre: the end pins of a DIP were labelled below the pad otherwise.
- Manual reference check: a backticked `[A-Z]+[0-9]+` that is a design Net name (`H0`, `C7`, `V8`) is not a reference.
- The UI font has no en/em dash: manual text, tooltips and the pinout show `-` instead (`Displayable`).
- The Puzzle section's User Manual excerpt is a `When it works` tree node, closed by default: open, it pushed the Chip section below the window.
- `ChipPanel::draw` also takes the design `LoadedCircuit` (for links in the Datasheet text).
- `--probe REF.PIN` also selects that Component (Chip panel and pin names in screenshots).
- `bw_ui --screenshot` prints `t`, speed ratio and `ui` ms on stdout, since the Debug section is collapsed.
- `dead_counter.bwp` brief reworded after looking at its TV picture (bands of grid that roll).

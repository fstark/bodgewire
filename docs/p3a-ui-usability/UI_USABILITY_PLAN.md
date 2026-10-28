# UI Usability — Implementation Plan (P3a)

Design: [UI_USABILITY_DESIGN.md](UI_USABILITY_DESIGN.md)
Spec: [UI_USABILITY.md](UI_USABILITY.md)

Phases 1–4 and 6–8 done 2026-10-28, commits 465e757..743ee9f (340 tests, debug and release). Release: `broken_trace --probe U1.CLK` at 1.00× after 3 s, `ui` 0.4 ms fitted; 0.7 ms with the Service Manual window and Chip panel open.

What remains needs the user at the screen.

| Phase | Description | Status |
|-------|-------------|--------|
| 5 | User check A: usability half | deferred: merged into the user check below |
| 7 | Content review by the user | waiting for the user |
| 9 | User check, Done-when, roadmap | waiting for the user |

## User check (phases 5, 7 and 9)

- Release build: `build/release/src/ui/bw_ui puzzles/<name>.bwp` for the three Puzzles.
- Navigation and snapping: left-drag pans, double-click on free board fits, `+`/`-` zoom, `Esc` lifts the probe; hovering pads, Traces and vias from a few pixels away; a B.Cu Trace picked with F.Cu active.
- Pin names on the hovered or selected Chip once pads are ≥ 10 px; `ui` < 16 ms at 100 px/mm (Debug section).
- Banner: amber `power off first` with Swap or Mend while powered; a refused click flashes it.
- Content: read the four manuals (`manuals/`) and the 14 Datasheets (`datasheets/`); wording and level of detail.
- Done-when (spec): knows what the Board should do and what each chip is for, sees the state, hits pins and Traces without hunting.
- Then: [ARCHITECTURE.md](../ARCHITECTURE.md) roadmap P3a row marked done; commit `"ui usability: phase 9 — user check"`.

Record tuning wishes here.

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

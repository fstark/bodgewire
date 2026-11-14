# P5f — Instrument polish — Implementation Plan

Spec: [INSTRUMENT_POLISH.md](INSTRUMENT_POLISH.md). Design: [INSTRUMENT_POLISH_DESIGN.md](INSTRUMENT_POLISH_DESIGN.md). Inventory: [INVENTORY.md](INVENTORY.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Time view maths, `FormatTime` moved, `FormatFrequency` | d3759d1 |
| 2 | Analyzer window, shared marks, `--sample`, `--view`, `--marker`, `--cursor` | 2bf1bb3 |
| 3 | Scope window | f24ff06 |
| 4 | Release check, recipe, glossary, docs, full suite | this commit |

P5f implemented 2026-11-14, user check pending.

## Implementation notes

- Docs review (before the docs commit), fixed: `Δ`, `−`, `◀` in neither UI font (`dt`, ASCII, drawn arrows; the screenshot check dropped); the scope's drag measured from the press (per-frame deltas rounded away at tenths of a division) and the horizontal wheel accumulated; the analyzer's old `FormatFrequency` ambiguous in phase 1; the overview and axis drawn after the canvas input, one width for every strip; the Board's `Esc` dropped while an instrument window has focus (its keys are global by design); `TraceView` renamed `TimeView` (a Trace is copper, GLOSSARY); the phase 2 screenshot at `1us` (at `2us` a bus cycle is too narrow for its text); the record from the first sampled point (`Capture::trigger` is always the pre-Trigger count); the grid at 200 px labels every 5th; test name prefixes; recipe gotchas (wheel X, glyphs); `Z` from `Sample::floating`; `--view` clamped to the zoom limits; GLOSSARY Marker in the spec.
- Written while the review ran: phase 1 and most of the analyzer window. The `time_text` move was staged when the docs were committed and went into that commit; both commits were redone (local, not pushed) so each builds on its own.
- Grid labels are spaced in logical pixels: the analyzer's lanes are about 540 px at its 940 px first size, so at 1 µs/div it labels every 2nd division.
- Axis labels that don't fit before the strip's end, or that lie under the off-screen Trigger's arrow, are left out (seen in the screenshots: `+10 µs` cut at the right, `+1 µs` under `T 800 ns`).
- The scope opens at 680 × 460, at P5e's place below the Multimeter's reading (a first try at 700 × 480 higher up hid it).
- Full debug suite: 911 tests pass in 503 s.
- Measured: release `bw_ui netlists/led_computer.bwn` with 32 Clips at 100 ns with the `8000` Trigger, 4 Hooks at 200 ns/div with a 1 µs delay, the probe on `U1` `RW`, the meter on `D1` `A`: **1.00×, UI 0.6 ms**.
- Screenshots viewed: the analyzer's first view (1 µs/div about T, `F80E F80F F810 8000 F811 F812 01FF` on `U1 A`); with `--view 1us:+5.35us --marker +2us --cursor +3.7us` (marker tag, values at the cursor, `dt 1.7 µs (588 kHz)`, a value pinned against the right edge); `--view 1ms` (the record in the overview, darker outside it); the scope with 4 Hooks, a 1 µs delay, marker and cursor (axis times, Trigger arrow in the strip, `H 3.5 V`/`L 0.2 V` in the rows, `dt 600 ns (1.67 MHz)`).
- Gestures (wheel, drag, swipe, right-click, `Esc`, Stop, the stored Sweep redrawn while zooming) are checked by reading the code only: the agent cannot drive the mouse. User check.
- `Esc` in an instrument window clears its marker; before P5f it also reached the Board (dropping the probe, the meter's leads or a wire's start), and no longer does.
- After the user check began (2026-11-14): axis labels printed `+100 µs` twice at 500 ns/div (three significant digits), and the scope's position hid its last digit (`98.4` for 98.45 µs). `FormatTimeStep` gives the digits the step needs (labels: the time per division; position and the off-screen distance: a tenth of one), and the label spacing comes from the widest label in view instead of a fixed 60 px.

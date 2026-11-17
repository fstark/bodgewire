# P6f — TV controls — Implementation Plan

Spec: [TV_CONTROLS.md](TV_CONTROLS.md). Design: [TV_CONTROLS_DESIGN.md](TV_CONTROLS_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | TV controls, a picture's line count, the case's proportions | b8d9de6 |
| 2 | The monitor, shelf drawing, `--tv` | caaf8a2 |
| 3 | Glossary, recipes, ARCHITECTURE (P6i row) | 9f949f5 |

P6f implemented 2026-11-17, user check pending.

## Implementation notes

- Docs review (before the docs commit), fixed: the window's bottom one row lower (a frame of N line starts paints rows 0..N); the window from the last locked frame (no jump while unlocked); `--tv`'s percent parsed by `ParseKnob`'s loop (no `from_chars`: `-1%`, `1e1%`, `inf%` refused); bw_view links `bw_devices`; the keyboard's default size under the taller TV stated; the corner masks kept inside the bezel; the trimmer's button inside the case; the minimum width 380 (labels 9 px); the paddle's wheel rate; where `--tv` is wired in `main.cpp`; test cases `tv controls: …` `[view]`.
- Written while the review ran: phase 1, most of phase 2.
- Phase 1: release and debug `-R "tv controls|bench places|tv consumer"`.
- Phase 2: the window's top moved from row 20 to 28 after the BW-80's screenshot: its sync junk (the dotted rows of its vertical sync) reaches row 27 at the left. A PAL field blanks 25 lines, so 28 stays a real monitor's.
- The screen's rounded corners: a stroke clipped to the bezel cut the bezel's own corners square, and a concave path fill came out as a chamfer, so each corner is a fan of triangles from the square corner to the arc, drawn without the anti-aliased fringe (which left a dark tick at the screen's corner).
- Screenshots viewed: the BW-2001 at start (text left of centre in the real window) and `--tv hpos:42%` (centred); the BW-80 normal (white, no junk) and `--tv underscan` (its sync junk, the dotted rows and the black sync bars); the Typewriter (first row cut, its keyboard narrower under the TV, as expected); the Test Pattern with `--tv bright:60%,contrast:40%` and `--tv off` (dark glass, lamp out, the slider at OFF); the shelf drawing; `--tv off` on the Counter: `this Machine has no TV`.
- Full release suite: 1430 tests passed.
- Speed gate: the 110 with every instrument 1.00×, UI 0.7 ms. The BW-2001 with its TV out, against the build before P6f: 0.25× both, UI 4.3–4.4 ms (was 3.9–4.0: the frame kept and toned on upload).
- Clicks (turning a knob by drag and wheel, the double-click back to the mark, the power switch, the trimmer, dragging the TV by its case) are checked by reading the code only: the agent cannot drive the mouse. User check.

# Board View — Implementation Plan (P2f)

Design: [BOARD_VIEW_DESIGN.md](BOARD_VIEW_DESIGN.md)
Spec: [BOARD_VIEW.md](BOARD_VIEW.md)

All 6 phases implemented 2026-10-28, commits ceb671e..72b5427 (289 tests, debug and release).

## Results

- Release, `bw_ui puzzles/broken_trace.bwp`, Board fitted: 1.00× at `1×`, status `ui 1.3 ms`, TV locked.
- Screenshots (`--screenshot`, 2560 × 1600 on Retina) of the test pattern, counter (`no video`) and broken_trace show both Layers, pads, vias, courtyards, references and pin-1 marks.
- Exit codes: screenshot ok 0, unwritable path 1, `--after` alone 2.

## Remaining: user check (spec Done-when)

- [ ] Open `bw_ui puzzles/broken_trace.bwp`, hover H0, zoom along the highlight and find the crack.
- [ ] Status `ui` < 16 ms at 100 px/mm (not measurable headlessly: needs the mouse wheel).
- [ ] Look good enough to build P2g on; record tuning wishes here.

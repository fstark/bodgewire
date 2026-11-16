# P6c — Shelf and focus — Implementation Plan

Spec: [SHELF_FOCUS.md](SHELF_FOCUS.md). Design: [SHELF_FOCUS_DESIGN.md](SHELF_FOCUS_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Focus model and its tests | fd4c2e1 |
| 2 | The focus and the shelf in the UI: Board, clicks, windows, the iron, `Esc`, `--focus`; the shelf replaces the toolbar; side panel | 4ad6982 |
| 3 | Release check, screenshots, recipes, glossary, docs | this commit |

P6c implemented 2026-11-17, user check pending.

## Implementation notes

- Docs review (before the docs commit), fixed: fences matched no ctest (`-R "focus"`, lowercase); `kFocusGold`, `BeginInstrument` and `InstrumentClicked` in `instrument_marks`, not the shelf; `ToolButton` deleted, `kBoardShare` to `shelf.hpp`, the keyboard windows in the Files table and moved above the shelf; the iron never refuses (`Session::repair` mends wherever asked); a press on a window's close button no longer counts as a click (it took the focus from the tool, then gave it to the probe); cells 48 px so the hints fit; start-up defaults and the one keyboard flag written; `AllowWhenBlockedByActiveItem`; phases 2 and 3 merged (an intermediate toolbar on the focus would have been written to be deleted).
- Written while the review ran: phase 1, most of phase 2.
- The analyzer window includes `shelf.hpp` for `kShelfHeight` only (its first position).
- Grayed items are never drawn lit (the Stopwatch's knob showed its lamp), and an inert owner has no gold (the probe on a bench netlist).
- The shelf finds the Machine's keyboards and knobs once, not per frame (`Session::keyboards()` and `knobs()` scan the parts).
- Measured, BW-2001 Puzzle 01 at `--after 4s`: 0.36×, UI 2.7–3.0 ms, the same as at HEAD before P6c (0.36×, 2.7–2.8 ms, a worktree build): the BW-2001 under `bw_ui` was already below real time, not this step's.
- Screenshots viewed (phase 2): Stopwatch with `--probe U1.1` (LOW lit on the probe, TV and keyboard grayed, powered tools grayed); with `--hook U1.1 --meter U1.1 --focus scope` (gold on the scope's window and drawing, no lead buttons, the meter's `×`, scope and meter lit); with `--focus iron` (`Power off first` in amber, the iron gold); the BW-2001 (TV and keyboard lit, the keyboard window above the shelf); `tests/data/bw1_bench.bwn` (tools, instruments, layer and fit grayed). UI 0.1–0.9 ms on all but the BW-2001.
- Clicks (shelf cells, window clicks, close buttons, keys, `Esc`) are checked by reading the code only: the agent cannot drive the mouse. User check.
- Full release suite: 1413 tests pass in 75 s.
- Release gate: `machines/110-led-computer/led_computer.bwn --group U1.A0:8000,U1.D0 --sample 100ns --hook U1.RW,U1.A0,U1.A1,U1.D0 --timebase 200ns --delay 1us --probe U1.RW --meter D1.A --focus analyzer`: **1.00×, UI 0.9 ms**; the focused analyzer keeps its gold behind the scope.

# P6c — Shelf and focus — Implementation Plan

Spec: [SHELF_FOCUS.md](SHELF_FOCUS.md). Design: [SHELF_FOCUS_DESIGN.md](SHELF_FOCUS_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Focus model and its tests | fd4c2e1 |
| 2 | The focus and the shelf in the UI: Board, clicks, windows, the iron, `Esc`, `--focus`; the shelf replaces the toolbar; side panel | |
| 3 | Release check, screenshots, recipes, glossary, docs | |

## Phase 1 — Focus

`src/view/focus.{hpp,cpp}`, `src/view/CMakeLists.txt`, `tests/view/focus_test.cpp`, `tests/CMakeLists.txt` (design, Focus; spec, Tests).

Fence: `cmake --build --preset release && ctest --preset release -j -R "focus"`; debug the same `-R` (ASan/UBSan, new logic).
Commit: `view: the focus, one owner of a click on a pin (P6c)`

## Phase 2 — The focus and the shelf in the UI

One phase: an intermediate toolbar rewired to the focus would be code written to be deleted.

- `board_view.{hpp,cpp}`: `Tool` and its members out, `setOwner`, `keys()` cut (design, Files).
- `instrument_marks.{hpp,cpp}`: `kFocusGold`, `BeginInstrument`, `InstrumentClicked` (design, Instrument windows).
- The three windows: lead buttons out, `draw(open, owner, session)` returning the click, gold frame, first positions, meter `×`.
- `shelf.{hpp,cpp}`, `shelf_drawings.{hpp,cpp}` new; `toolbar.{hpp,cpp}` removed; `src/ui/CMakeLists.txt`; the keyboard windows on `shelf.hpp`, above the shelf (design, Shelf, Drawings).
- `side_panel.{hpp,cpp}`: Probe section out, TV and Knob from the shelf (design, Side panel).
- `main.cpp`: `Focus`; the click switch by owner; the iron; `Esc`; window calls; keyboards; layout with the shelf; `--focus`, `bringOut` for `--clip`/`--group`/`--hook`/`--meter` (design, Board and clicks).

Fence: release build; `ctest --preset release -j -R "focus"`; screenshots viewed, Stopwatch = `machines/040-stopwatch/puzzles/01-stuck-at-zero.bwp`: with `--probe U1.1` (lamps, controls, TV grayed), with `--hook U1.1 --meter U1.1 --focus scope` (gold on the scope's drawing and window, no lead buttons, the meter's `×`), with `--focus iron` (hint, iron gold and grayed); the BW-2001 Puzzle 01 (TV and keyboard out, the keyboard above the shelf); `tests/data/bw1_bench.bwn` (tools and instruments grayed). UI ms printed by each under ~1 ms. No ctest reaches `src/ui`.
Commit: `ui: the shelf replaces the toolbar; the focus on a tool or an instrument, in gold; one iron (P6c)`

## Phase 3 — Finish

- Release gate: the 110 (`machines/110-led-computer/led_computer.bwn`) with 32 Clips at 100 ns, 4 Hooks, the probe and the meter, as in P5f: 1.00×, UI under ~1 ms.
- Recipes ADD_INSTRUMENT, ADD_CONNECTOR_DEVICE; GLOSSARY (design, Recipes and glossary).
- Full release suite `ctest --preset release -j`.
- ARCHITECTURE row done with the date and links; this plan cut down to the phase table and notes.

Commit: `docs: P6c done, shelf and focus`

## Implementation notes

- Docs review (before the docs commit), fixed: fences matched no ctest (`-R "focus"`, lowercase); `kFocusGold`, `BeginInstrument` and `InstrumentClicked` in `instrument_marks`, not the shelf; `ToolButton` deleted, `kBoardShare` to `shelf.hpp`, the keyboard windows in the Files table and moved above the shelf; the iron never refuses (`Session::repair` mends wherever asked); a press on a window's close button no longer counts as a click (it took the focus from the tool, then gave it to the probe); cells 48 px so the hints fit; start-up defaults and the one keyboard flag written; `AllowWhenBlockedByActiveItem`; phases 2 and 3 merged (an intermediate toolbar on the focus would have been written to be deleted).
- Written while the review ran: phase 1, most of phase 2.
- The analyzer window includes `shelf.hpp` for `kShelfHeight` only (its first position).
- Grayed items are never drawn lit (the Stopwatch's knob showed its lamp), and an inert owner has no gold (the probe on a bench netlist).
- The shelf finds the Machine's keyboards and knobs once, not per frame (`Session::keyboards()` and `knobs()` scan the parts).
- Measured, BW-2001 Puzzle 01 at `--after 4s`: 0.36×, UI 2.7–3.0 ms, the same as at HEAD before P6c (0.36×, 2.7–2.8 ms, a worktree build): the BW-2001 under `bw_ui` was already below real time, not this step's.
- Screenshots viewed (phase 2): Stopwatch with `--probe U1.1` (LOW lit on the probe, TV and keyboard grayed, powered tools grayed); with `--hook U1.1 --meter U1.1 --focus scope` (gold on the scope's window and drawing, no lead buttons, the meter's `×`, scope and meter lit); with `--focus iron` (`Power off first` in amber, the iron gold); the BW-2001 (TV and keyboard lit, the keyboard window above the shelf); `tests/data/bw1_bench.bwn` (tools, instruments, layer and fit grayed). UI 0.1–0.9 ms on all but the BW-2001.
- Clicks (shelf cells, window clicks, close buttons, keys, `Esc`) are checked by reading the code only: the agent cannot drive the mouse. User check.

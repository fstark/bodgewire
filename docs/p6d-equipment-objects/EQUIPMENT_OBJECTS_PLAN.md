# P6d — Equipment objects — Implementation Plan

Spec: [EQUIPMENT_OBJECTS.md](EQUIPMENT_OBJECTS.md). Design: [EQUIPMENT_OBJECTS_DESIGN.md](EQUIPMENT_OBJECTS_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Places and presets, and their tests | 95294db |
| 2 | The equipment as objects: instrument places and the size button, the TV and knob cases, keyboards without a title bar; the side panel removed (brief, manuals tabs, Debug); the Board across the width | a9d2060 |
| 3 | Release check, screenshots, recipes, glossary, docs | this commit |

P6d implemented 2026-11-17, user check pending.

## Implementation notes

- Docs review (before the docs commit), fixed: fences matched no ctest (`-R "bench places"`); the knob's aspect inverted; `--debug` the first flag with no value; the repairs log a span; `case_marks` named `bench` in the spec; the TV's `powered`; `PushClipRect` not intersected; the bench defined once (1264 × 716); the ZX80's aspect 0.54; the brief narrowed clear of the TV; every first size in `bench_places` and tested; the brief's dark text pushed after `Begin`; the instruments' overlap stated and accepted; what the side panel showed always, now on demand, stated; `--manual chip` for the Chip tab's screenshot; ARCHITECTURE P6e and the LED computer's seed line.
- Written while the review ran: phase 1, most of phase 2.
- Phase 2: full release suite 1417 tests in 75 s; debug `-R "bench places|focus"`. Screenshots viewed: Stopwatch with every instrument, BW-2001, BW-80, Tennis, typewriter, `bw1_bench.bwn`, `--manual chip --probe U1.1 --debug`. Debug first sat halfway down the left, on the manuals window: moved to the bottom centre. The brief's first height is its text's (`{340, 0}`), not 120. The meter drawn after the scope, so at start the smallest instrument is in front. `ManualWindows` moved to `manual_window`: `chip_panel.hpp` includes `manual_view.hpp`.
- Release gate (phase 3): the 110 with every instrument (`--group U1.A0:8000,U1.D0 --sample 100ns --hook U1.RW,U1.A0,U1.A1,U1.D0 --timebase 200ns --delay 1us --probe U1.RW --meter D1.A --focus analyzer`): **1.00×, UI 0.8–1.1 ms**. BW-2001 under `bw_ui` 0.36×, UI 2.9 ms, as before P6d.
- Clicks (the size button, the case ×, dragging a case, turning the knob, the shelf toggling the TV, knob and brief) are checked by reading the code only: the agent cannot drive the mouse. User check.

# P6d — Equipment objects — Implementation Plan

Spec: [EQUIPMENT_OBJECTS.md](EQUIPMENT_OBJECTS.md). Design: [EQUIPMENT_OBJECTS_DESIGN.md](EQUIPMENT_OBJECTS_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Places and presets, and their tests | |
| 2 | The equipment as objects: instrument places and the size button, the TV and knob cases, keyboards without a title bar; the side panel removed (brief, manuals tabs, Debug); the Board across the width | |
| 3 | Release check, screenshots, recipes, glossary, docs | |

## Phase 1 — Places and presets

- `src/view/bench_places.{hpp,cpp}` as the design; `src/view/CMakeLists.txt`.
- `tests/view/bench_places_test.cpp` (spec, Tests: the bench 1264 × 716; keyboard aspects 0.36, 0.43, 0.54; two knobs; the manuals, brief and Debug sizes); `tests/CMakeLists.txt`.

Fence: `cmake --build --preset release && ctest --preset release -j -R "bench places|focus"`; debug the same (`cmake --build --preset debug && ctest --preset debug -j -R "bench places"`), it is C++ logic. Commit: `view: default places and size presets for the bench (P6d)`.

## Phase 2 — The equipment as objects

In this order, building as it goes:

1. `src/ui/bench.{hpp,cpp}`; `BeginInstrument` with its `BenchObject` and the size button; the three instrument windows' own placement removed.
2. `TvView::texture()`; `tv_window`; `knob_window` (with `DrawKnobs`' keys).
3. Both keyboards: `NoTitleBar`, `KeepAspect`, `PlaceNext`, `CaseClose`.
4. `manual_window` (moved out of `manual_view`) with its tabs and `showChip`; `brief_window`; `debug_window`; `side_panel` deleted.
5. Shelf: `Item::Brief` and its drawing, the TV and knob as windows, `kBoardShare` gone.
6. `main.cpp`: layout, windows, defaults, `--debug`, `--manual chip`.

Screenshots viewed (release, `--after 1s` unless said): the Stopwatch 01 with `--clip U1.1 --hook U1.1 --meter U1.1` (instrument places, the brief, no side panel); the BW-2001 01 at `--after 3s` (the TV case, the keyboard under it); the BW-80 01 (the membrane narrowed under the TV, its `×` clear of the words); Tennis 01 (two knobs); the typewriter 01; `tests/data/bw1_bench.bwn`; Stopwatch with `--manual chip --probe U1.1` (the Chip tab); `--debug`.

Fence: `cmake --build --preset release && ctest --preset release -j` (the full suite: shared UI code, every Machine's screen); `clang-format` on the C++ files touched. Debug: `ctest --preset debug -j -R "bench places|focus"`. Commit: `ui: the equipment as objects with default places and sizes; the side panel removed (P6d)`.

## Phase 3 — Release check and docs

- Release gate: `machines/110-led-computer/led_computer.bwn --group U1.A0:8000,U1.D0 --sample 100ns --hook U1.RW,U1.A0,U1.A1,U1.D0 --timebase 200ns --delay 1us --probe U1.RW --meter D1.A --focus analyzer`: 1.00×, UI under ~1 ms.
- Recipes ADD_INSTRUMENT, ADD_CONNECTOR_DEVICE; GLOSSARY **Equipment**; ARCHITECTURE row P6d done with the date and links, P6e's row; LED_COMPUTER_DESIGN's seed line; this plan cut to the table and notes.

Fence: full release suite. Commit: `docs: P6d done, equipment objects`.

## Implementation notes

- Docs review (before the docs commit), fixed: fences matched no ctest (`-R "bench places"`); the knob's aspect inverted; `--debug` the first flag with no value; the repairs log a span; `case_marks` named `bench` in the spec; the TV's `powered`; `PushClipRect` not intersected; the bench defined once (1264 × 716); the ZX80's aspect 0.54; the brief narrowed clear of the TV; every first size in `bench_places` and tested; the brief's dark text pushed after `Begin`; the instruments' overlap stated and accepted; what the side panel showed always, now on demand, stated; `--manual chip` for the Chip tab's screenshot; ARCHITECTURE P6e and the LED computer's seed line.
- Written while the review ran: phase 1, most of phase 2.

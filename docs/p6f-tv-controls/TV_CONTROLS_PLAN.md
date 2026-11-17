# P6f — TV controls — Implementation Plan

Spec: [TV_CONTROLS.md](TV_CONTROLS.md). Design: [TV_CONTROLS_DESIGN.md](TV_CONTROLS_DESIGN.md).

## Phase 1 — TV controls

- `src/view/tv_controls.{hpp,cpp}`, `src/view/CMakeLists.txt`; `tests/view/tv_controls_test.cpp`, `tests/CMakeLists.txt` (design: TV controls; spec: Tests).
- `src/runner/tv_consumer.{hpp,cpp}`, `tests/runner/tv_consumer_test.cpp`: `TvPicture::lines`.
- `src/view/bench_places.hpp`: `kTvAspect` 0.96, the three fractions gone; `tv_window.cpp` keeps compiling with the old fractions as local constants until phase 2.

Fence: release build; `ctest --preset release -R "tv controls|bench places|tv consumer"`; debug build, the same `-R`. Commit: `view, runner: TV controls (knobs, switch, trimmer, window, shift, tone, --tv parsing), a picture's line count, the TV case 0.96 high`.

## Phase 2 — The monitor

- `src/ui/tv_view.{hpp,cpp}`: the kept frame, `setTone` (design: the tone on upload).
- `src/ui/tv_window.{hpp,cpp}`: the VM-4209 drawn, knobs, lamp and switch live (design: the monitor).
- `src/ui/shelf_drawings.cpp`: `Tv`.
- `src/ui/main.cpp`: `--tv`, the draw call.
- Screenshots viewed (spec: Tests): `machines/050-test-pattern` as it comes out; `machines/130-bw2001` at start and `--tv hpos:N%` (N found by trying until centred, written in the notes); `machines/140-bw80` normal and `--tv underscan`; `machines/090-typewriter` (first row cut; its keyboard narrower under the taller TV); the BW-80's membrane keyboard under it; `--tv bright:60%,contrast:40%`; `--tv off`; the shelf drawing out and away (a Machine with no TV: grayed).
- The speed gate: the 110 with every instrument (as P6e) and the BW-2001 with its TV out, the ratio and UI ms printed by `--screenshot`.

Fence: full release suite; the screenshots above; the speed gate. Commit: `ui: the TV in a small 1970s monitor's look, a real monitor's window, H-POS, V-POS, BRIGHT, CONTRAST, its power switch and an underscan trimmer; --tv`.

## Phase 3 — Docs

- GLOSSARY, ADD_CONNECTOR_DEVICE, ARCHITECTURE with the P6i row (design: Recipes and glossary); this plan cut down to the status table and notes.

Fence: links checked by reading. Commit: `docs: P6f done, TV controls`.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | TV controls, the case's proportions | |
| 2 | The monitor, shelf drawing, `--tv` | |
| 3 | Glossary, recipes, ARCHITECTURE | |

## Implementation notes

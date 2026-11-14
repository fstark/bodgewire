# P5f — Instrument polish — Implementation Plan

Spec: [INSTRUMENT_POLISH.md](INSTRUMENT_POLISH.md). Design: [INSTRUMENT_POLISH_DESIGN.md](INSTRUMENT_POLISH_DESIGN.md). Inventory: [INVENTORY.md](INVENTORY.md).

Every phase ends with its fence: `cmake --build --preset debug` clean (reconfigure first when files are added), `clang-format` on the C++ files touched, the tests the phase names green in debug, one commit of this step's own paths only. The windows have no tests: they are checked with release screenshots, viewed. The UI is shared code: the full debug suite runs at phase 4.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Time view maths, `FormatTime` moved, `FormatFrequency` | |
| 2 | Analyzer window, shared marks, `--sample`, `--view`, `--marker`, `--cursor` | |
| 3 | Scope window | |
| 4 | Release check, recipe, glossary, docs, full suite | |

## Phase 1 — Time view

- `src/view/time_text.{hpp,cpp}` moved from `src/ui` (`git mv`), includes updated, `FormatFrequency` added; `src/view/time_view.{hpp,cpp}` (design, Time view); `bw_view` links `bw_instruments`.
- `tests/view/time_view_test.cpp`, `tests/view/time_text_test.cpp` (spec, Tests: everything but the screenshots and the release ratio).
- The analyzer's local `FormatFrequency` deleted (it would be ambiguous with the new one); its Rate combo uses the new one until phase 2.
- Test names: `"time view: …"`, `"time text: …"`.
- Fence: `ctest --preset debug -j 12 -R "time view|time text|view:"`. Commit: `view: time view maths shared by the instruments`.

## Phase 2 — Analyzer window and flags

- `src/ui/instrument_marks.{hpp,cpp}` (design, Shared drawing); `analyzer_window.{hpp,cpp}` (design, Analyzer window); `main.cpp` flags (design, `bw_ui`): `--sample` replaces `--rate`, `--view`, `--marker`, `--cursor` (the scope's `preset` as an empty stub until phase 3).
- `Esc`: `focused()` on both windows (the scope's returns false until phase 3), `main.cpp` drops the Board's `Escape` while one is focused (design, Analyzer window).
- Release screenshots, viewed:
  - `bw_ui netlists/led_computer.bwn --group U1.A0:8000,U1.D0 --clip U1.RW:0,U1.SYNC,U1.PHI2 --sample 100ns --screenshot …/la_first.png --after 1s`: first view at 1 µs/div around T, bus values readable, orange Trigger, grid, axis on top, overview box.
  - the same with `--view 1us:+5us --marker +2us --cursor +3.7us` (about 57 px a cycle at the 940 px window: bus text fits): values cut at both edges drawn open, `M` tag, status line with `dt` and frequency, values column at the cursor.
  - the same with `--view 1ms` (fit-sized): the area outside the Capture darker.
- Fence: `ctest --preset debug -j 12 -R "time view|time text|session:|analyzer"` (nothing below the UI changed; these guard the build). Commit: `ui: analyzer window on the shared time view`.

## Phase 3 — Scope window

- `scope_window.{hpp,cpp}` (design, Scope window); `ScopeWindow::preset` wired in `main.cpp`.
- Release screenshots, viewed: `bw_ui netlists/led_computer.bwn --hook U1.PHI0,U1.SYNC,U1.A0 --timebase 200ns --delay 1us --marker +1.5us --cursor +2.1us …`: 3 bands sharing the height, axis times `+1 µs` …, the Trigger arrow in the axis strip, values in the Hook rows, `1 V/div`, status line. UI ms with 4 Hooks noted (the view-keyed column cache).
- Gestures (wheel, drag, Stop, right-click) checked by reading the code paths once more (the agent cannot drive the mouse).
- Fence: `ctest --preset debug -j 12 -R "time view|time text|session:|scope"`. Commit: `ui: scope window on the shared time view`.

## Phase 4 — Release check, recipe, docs, full suite

- Release `bw_ui netlists/led_computer.bwn` with every instrument as in P5e (32 Clips at 100 ns with the `8000` Trigger, 4 Hooks at 200 ns/div with a 1 µs delay, the probe on `U1` `RW`, the meter on `D1` `A`): 1.00×, UI under ~1 ms, noted.
- Recipe ADD_INSTRUMENT (design, Recipe); GLOSSARY **Marker**; INVENTORY's last column gets the outcome (done / dropped).
- ARCHITECTURE: P5f row done with the date and links.
- This plan cut to the status table and implementation notes, "user check pending".
- Fence: full debug suite. Commit: `docs: P5f implemented, user check pending`.

## Implementation notes

- Docs review (before the docs commit), fixed: `Δ`, `−`, `◀` in neither UI font (`dt`, ASCII, drawn arrows; the screenshot check dropped); the scope's drag measured from the press (per-frame deltas rounded away at tenths of a division) and the horizontal wheel accumulated; the analyzer's old `FormatFrequency` ambiguous in phase 1; the overview and axis drawn after the canvas input, one width for every strip; the Board's `Esc` dropped while an instrument window has focus (its keys are global by design); `TraceView` renamed `TimeView` (a Trace is copper, GLOSSARY); the phase 2 screenshot at `1us` (at `2us` a bus cycle is too narrow for its text); the record from the first sampled point (`Capture::trigger` is always the pre-Trigger count); the grid at 200 px labels every 5th; test name prefixes; recipe gotchas (wheel X, glyphs); `Z` from `Sample::floating`; `--view` clamped to the zoom limits; GLOSSARY Marker in the spec.
- Written while the review ran, before the docs commit: phase 1 and most of the analyzer window.

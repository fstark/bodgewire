# P8i — The Logic Analyzer redesigned — Plan

Spec: [LOGIC_ANALYZER.md](LOGIC_ANALYZER.md). Design: [LOGIC_ANALYZER_DESIGN.md](LOGIC_ANALYZER_DESIGN.md).

## Status

| Phase | What | Commit |
|---|---|---|
| 1 | The shared case pieces; the scope rebuilt on them, unchanged | (this commit) |
| 2 | The rows' order, the arming, the Board's discs by row | |
| 3 | The analyzer's case layout | |
| 4 | The window: case, strip, corner, screen, plates; ARM resumes a paused run | |
| 5 | Grouping by ticks, rows reordered by dragging | |
| 6 | Docs, README shot, web build, finish | |

## Phase 1 — The shared case pieces (design §1)

1. `src/view/case_strip.{hpp,cpp}`: `Box` and `BandAt` moved from `scope_layout`, the strip's sizes, `LayOutStepper`, `Badge`, `LayOutBadge`, `SizeKeyBox`; `scope_layout.cpp` built on them; `kScopeStrip`, `kScopeClose` renamed `kCaseStrip`, `kCaseClose` (scope window and tests).
2. `src/ui/case_keys.{hpp,cpp}`: the keys, LEDs, `DrawLamp` with its name, `DrawRocker` of 2 or 3 halves, `DrawCaseFrame`, `DrawGroup`, `DrawGroupLabel`, `DrawBadge`, `DrawBezel`, `TimeGrid`; `scope_case` keeps `DrawScopeCase` on them.
3. `kAxisHeight` and `UnitRows` in `time_view` (`TimebaseRows` gone, its case rewritten as `time view: unit rows`, the analyzer's periods too); `DrawCursorTag` in `instrument_marks`; the scope through them.
4. Screenshots of the scope after, against the ones taken from `main` before: the case, strip and plates pixel for pixel, the traces by eye (they vary run to run); the timebase grid opened; the minimal size.

Fence: release `^(time view|scope layout|scope reading|bench places): ` and the full release suite; the scope's case unchanged. Commit: `view, ui: a drawn case's strip and keys shared, the scope built on them (P8i phase 1)`.

## Phase 2 — Rows, arming, discs (design §2)

1. `AnalyzerSetup`: `order_`, `rows()`, `move()`, `label()`, `inCapture()`, `poweringOn()`; `clip()` and `clipGroup()` arm; `Session::powerOn` calls `poweringOn()`.
2. The old window lists its rows from `rows()` (same drawing) and colours them `RowColour`; `ClipTagsOf` and the disc numbered by row.
3. Tests: the five `session:` cases; `a capture is not re-armed by a repair` replaced.

Fence: release `-R "^(session|analyzer)"`, debug as in the design's Checks; a screenshot of the LED computer's Board with its discs. Commit: `session: the analyzer's rows in order, a new Clip and every power-on arm (P8i phase 2)`.

## Phase 3 — The case's layout (design §3)

1. `src/view/analyzer_layout.{hpp,cpp}` and its tests; `MinimalSize(Analyzer)` 880 × 340.

Fence: release `^(analyzer layout|bench places): `. Commit: `view: the Logic Analyzer's case laid out (P8i phase 3)`.

## Phase 4 — The window (design §4, but grouping and reordering)

1. `analyzer_window` rewritten, no child window: the case, the strip and its items, the corner (`PIN | BUS`, the count; `GROUP` drawn, not yet working), the overview and the axis, the screen to its bottom, the rows scrolled by hand, the plates (swatches, names, values at the cursor, Marker or Trigger, Trigger keys and `ChoicesPopup`, the bus pattern's field, ×), the lanes (dimmed while armed, empty when not in the Capture), the cursor's tag, the mouse's snap.
2. Removed: `BeginInstrument`, `StatusLine`, `DrawOffScreenTrigger`, `OffScreenText`, `OffScreenWidth`, `DrawAxisLabels`' `arrow`, `StatusText`, `OffScreenTrigger` and their cases, `AnalyzerSetup::disarm`; `ChoicePopup` becomes `ChoicesPopup` (AGENTS.md's list).
3. `Resume` in `shelf`; `main.cpp` resumes on `takeRunAsked()`.
4. Screenshots: the LED computer, the counter at the minimal size, the bug's repro, `ARM` while paused, the SAMPLE grid, a Trigger key's choices, twenty Clips scrolled, `--window 1080:810`.

Fence: the full release suite; the screenshots viewed; UI ms. Commit: `ui: the Logic Analyzer as a drawn case, its rows labelled on the screen (P8i phase 4)`.

## Phase 5 — Grouping and reordering (design §4.8, §4.9)

1. Tick boxes with two rows or more; `GROUP` / `UNGROUP`.
2. Plates dragged up or down; `setup.move` after the drawing.
3. Screenshots: two plates ticked, `GROUP` tapped (one bus row); a group ticked, `UNGROUP`; a plate dragged (`--drag`), the Board's discs renumbered.

Fence: release `-R "^(session|analyzer)"`; the screenshots viewed. Commit: `ui: the analyzer's rows grouped by ticks and moved by their plates (P8i phase 5)`.

## Phase 6 — Docs and finish (design §5)

1. GLOSSARY, ADD_INSTRUMENT.md (its Window row rewritten for the drawn case), AGENTS.md's helpers, the README shot, README_SPEC checked, the manuals' setups checked.
2. `cmake --build --preset web`.
3. The full release suite; ARCHITECTURE's row done; this plan cut down to the status table and notes.

Fence: the full release suite. Commit: `docs: P8i done, GLOSSARY, recipe, README shot (P8i phase 6)`.

## Implementation notes

- 2026-11-26, review (a fresh agent, the three docs against the code): 12 must-fixes, all fixed in the docs before phase 1. `OffScreenTrigger` still used by `DrawAxisLabels`' `arrow` (both go); `TimebaseRows` gone, its case rewritten; `kAxisHeight` moved to `view` (a layout there cannot include `ui/`), `BandAt` to `case_strip`; no child window for the rows (it would take the corner grip's hover and draw over it, and its scrollbar would narrow the lanes under a full-width axis); every key's hit box at least 28 px; ungrouping asked of the user (`GROUP` becomes `UNGROUP`); the reused Channel's test a session case through `inCapture`; `DrawRocker` called with `std::array` (no span from a braced list in C++23); ADD_INSTRUMENT's Window row to rewrite; the spec's § numbers in the design; the power-on arming in `AnalyzerSetup`, the spec fixed. Taken from the suggestions: what goes with the old header listed in the spec; `ChoicePopup` into `ChoicesPopup`; `disarm` removed; the canvas covering the screen below the rows; the plates' keys before the plate's button; the corner drawn in the screen's look; the overview's item 28 px high; below 880 px not handled; the main shot with the bus Trigger lit; the debug regex with the scope-hooking case.
- 2026-11-26, phase 1: as designed. The scope's shots after, against the four from `main`: the delayed view with its ◀ key, the timebase grid and the full size pixel-identical; the four-Hook shot differs only in the bottom band's traces (1137 × 85 px), where two runs of `main` differ too. `UnitRows` drops empty rows (the scope's three, the analyzer's three). The old analyzer keeps `DrawOffScreenTrigger` until phase 4 but its axis labels no longer leave room for its arrow. Checks: release `^(time view|scope layout|scope reading|bench places): ` (36), the full release suite (1710).

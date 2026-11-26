# P8h — The Oscilloscope redesigned — Implementation Plan

Spec: [OSCILLOSCOPE.md](OSCILLOSCOPE.md). Design: [OSCILLOSCOPE_DESIGN.md](OSCILLOSCOPE_DESIGN.md).

Screenshots: `build/release/src/ui/bw_ui machines/010-counter/puzzles/01-dark-led.bwp --hook … --focus scope --screenshot /tmp/x.png --after 1s` (`2s` with a scripted finger), viewed. A scripted finger's window px are read off a first shot (a shot is twice the window). The debug regex never takes the BW-80's or BW-2001's cases: `DEBUG_R="^(session: (the scope|hooks)|scope (reading|layout): |time view: )"`.

## Status

| Phase | Description | Commit |
|---|---|---|
| 1 | The view behind the plate, the cursor's tag, the timebase rows, the readings, the layout | `e201870` |
| 2 | The bands' order; the Board's Hook numbers | |
| 3 | The screen: bands, plates, labels, Trigger on the labels, cursor tag, snap | |
| 4 | The case and the strip: logo, keys, LEDs, timebase grid, presets on the case | |
| 5 | Reordering the bands by their labels | |
| 6 | Docs, README shot, speed, full suite | |

## Phase 1 — Pure: the view, the readings, the layout

1. `ScopeView` and `WithView` take `lead`; `CursorTagText`; `TimebaseRows` (design §1).
2. `src/view/scope_reading.{hpp,cpp}`: `BandReading` (design §2).
3. `src/view/scope_layout.{hpp,cpp}`: `Box`, `LayOutScopeCase`, `ScopeBands`, `BandAt` (design §4); both in `src/view/CMakeLists.txt`.
4. Tests: the three `time view:` cases, `tests/view/scope_reading_test.cpp`, `tests/view/scope_layout_test.cpp`, in `tests/CMakeLists.txt`.

Fence: release `ctest --preset release -j -R "^(time view|scope reading|scope layout): "`; debug `ctest --preset debug -j -R "^(time view|scope reading|scope layout): "`. Commit: `view: the scope's view behind a plate, cursor tag, timebase rows, band readings and case layout (P8h phase 1)`.

## Phase 2 — The bands' order

1. `ScopeSetup`: `order_`, `order()`, `move()`; `hook` appends and takes the Trigger when its source is not hooked; `unhook` erases and moves the source to the top band (design §3).
2. `HookTag::place`; `HookTagsOf` by `order()`; `BoardView` draws the place's digit (design §3).
3. Tests: the three `session:` cases (design §3).

Fence: release `ctest --preset release -j -R "^(session|scope): "`; debug `ctest --preset debug -j -R "$DEBUG_R"`; screenshots: `--hook U1.CLK,U1.QA,U1.QB` (discs 1, 2, 3), then the same with a `--tap` on `U1.QA`'s pad (the scope in focus: the tap unhooks it; discs 1 on CLK, 2 on QB). Commit: `session: the scope's bands in order, the Board's Hooks numbered by it (P8h phase 2)`.

## Phase 3 — The screen

1. `DrawAxisLabels(…, arrow)` (design §5).
2. `ScopeWindow`, still in its ImGui window with its header for now: the screen's item allowing overlap, first; bands from `ScopeBands` in `order()`, gaps, six volt lines each, the time lines across, the glow; the plates (`kPlate`, opaque) with `T` (its edge a drawn triangle), name, ×, reading (`BandReading` over the visible part, cached by serial, `to` and power); the 0 V arrows at the plates' edge; `lead` through `ScopeView` / `WithView`; the Trigger on the labels (the header's source choice removed); the Marker and cursor clipped right of the plates, the cursor's tag; the mouse's snap; ×, `T`'s unhook-side effects and the settings applied after the drawing (design §5, step 7); the Hook rows, the status line, `kGestures` and `DrawOffScreenTrigger` out (design §5).

Fence: release `ctest --preset release -j -R "^(time view|scope reading|scope layout|session|scope): "`, then the full release suite (`instrument_marks` is shared with the analyzer); screenshots: one Hook; three with `--marker +2us --cursor +5us` (the tag, `dt`, the values on the plates); four; a `--tap` on the middle band's × of three (two bands, the Board's discs 1, 2); the power off (a `--tap` on the Shelf's power switch); the 050 README setup; `--touch --hold` on a band (the cursor snapped to an edge); a double-click and a wheel over a plate change nothing (a `--double-tap` on a plate: the position stays); the analyzer's axis unchanged (`--clip` a pin, `--view 1us:+5us`). Commit: `ui: the scope's screen in bands with labels on it, the cursor's tag, the mouse's snap (P8h phase 3)`.

## Phase 4 — The case and the strip

1. `NextInstrumentSize`, `DrawSizeIcon`, `InstrumentClicked(closeZone)` (design §6); `BeginInstrument` calls the first two.
2. `FieldLook` and `NumberField(…, look)` (design §6).
3. `src/ui/bw_logo.{hpp,cpp}`: `DrawBwLogo`; `src/ui/scope_case.{hpp,cpp}`: the drawing (design §6); both in `src/ui/CMakeLists.txt`.
4. `ScopeWindow`: begun as a case; the keys, LEDs, the position's `NumberField` with its look, the rocker, the lamp (from `shown_`), the badge, the size key, `CaseClose`; the timebase grid popup; Run/Stop and `stopped_` gone (design §6).
5. The scope's minimal size 620 × 400 (`bench_places.cpp`).

Fence: release `ctest --preset release -j -R "^(bench places|focus|scope|session|number pad): "`, then the full release suite (`controls` and `instrument_marks` are shared); screenshots: normal with three Hooks; minimal with four (a `--tap` on the size key); full (another); `--pop scope`; the timebase grid open (a `--tap` on the time LED); the position's pad open (a `--tap` on the position LED: the pad in the style's colours); the logo (a crop of the 2× shot); the analyzer's title bar and size button unchanged; `--window 1080:810` (a tablet's bench). The web build (`source ~/emsdk/emsdk_env.sh && cmake --preset web && cmake --build --preset web`), its page served and looked at. Commit: `ui: the scope as a drawn HP-like case, its strip of keys and the BW logo (P8h phase 4)`.

## Phase 5 — Reordering

1. The plate's drag (design §7): the dragged plate follows the pointer; the release moves the band.

Fence: release `ctest --preset release -j -R "^(session|scope): "`; screenshots: a `--drag` of the third band's plate to the top (bands and the Board's discs reordered, colours kept); a `--tap` on a plate (nothing moves). Commit: `ui: the scope's bands reordered by dragging their labels (P8h phase 5)`.

## Phase 6 — Docs and finish

1. The three manual setups by pin (design §8).
2. GLOSSARY, ADD_INSTRUMENT.md (design §8).
3. README: `python3 -I tools/readme_shots.py scope`, the picture viewed; README_SPEC if its words changed.
4. Speed: `machines/140-bw80` with four Hooks: 1.00×, UI under 1 ms.
5. ARCHITECTURE's P8h row done (date, one line, links); this plan cut to the status table and the notes.

Fence: the full release suite `ctest --preset release -j`; the phase 3–5 screenshots retaken once and viewed. Commit: `docs: P8h done, the scope's manuals by pin, GLOSSARY, recipe, README shot (P8h phase 6)`.

## Implementation notes

- 2026-11-26, review (a fresh agent, the three docs against the code): 13 must-fixes, all fixed in the docs before phase 1. The screen's item allows overlap (`IsItemHovered` ignores later items otherwise: a plate's double-click would reset the position, the corner grip would be dead); ×, `T` and the drop applied after the drawing and the settings; a new Hook takes the Trigger when its source is not hooked; the readings in the existing formatters' trimmed form (`1 MHz`, `high 1 µs`) with "high" defined (One, not floating); the ◀ ▶ ▲ ▼ drawn as triangles; `NumberField` given a look that does not leak into its pad; the logo's counter filled white after the letters (`AddConcavePolyFilled` has no holes); one list of `scope_case`'s functions; the debug regex kept off the BW-80's and BW-2001's `session:` cases; the removal screenshot added; the mouse's snap left to the user (every scripted pointer is a finger); the logo checked in a crop. Taken from the suggestions: the layout pure and tested in `src/view`; the strip's widths fitted to 620 px (the words only from 719); the readings over the visible part, with the power in their cache key; the Marker and cursor clipped right of the plates, the 0 V arrows at their edge; the slop per pointer; a shared size icon; the minimum size clamped to the bench; the lamp from the Sweep shown; the zoom about the visible centre; the full suite after phases 3 and 4.
- 2026-11-26, phase 1: as designed. The cursor's tag's `dt` unsigned when positive (`dt 3 µs`, `dt -2 µs`). The layout's strip comes to 609 px at the minimal 620; the words from 719. Checks: release and debug `^(time view|scope reading|scope layout): ` (21 cases).
- 2026-11-26, phase 2: as designed; `unhook` of a Channel not hooked does nothing. Screenshots: the counter with three Hooks, the scope dragged off the Board (`--drag 450:202:0:420`): discs 1, 2, 3; a `--tap 436:366` on `U1.QA`'s pad unhooks it: QB's disc reads 2. Checks: release `^(session|scope): ` (72), debug `$DEBUG_R` (26).

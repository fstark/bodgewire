# P7b — Launcher look — Implementation Plan

Spec: [LAUNCHER_LOOK.md](LAUNCHER_LOOK.md). Design: [LAUNCHER_LOOK_DESIGN.md](LAUNCHER_LOOK_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | The logic: `LauncherLanes`, `Glide`, `PictureLeft`, `OutputFrame`, `BoardScene::lamp`, `HasTv`, their tests | |
| 2 | Shared pieces, the bench unchanged: `Load` moved, `board_tags`, `BoardView::setStill`, `DrawMonitor`, `DrawNote`, `TvView::clear`, the shelf's and panel's colours | `943ea95` |
| 3 | The launcher: stills and the live run loaded in the background, the two lanes, keys and clicks, `--select`, the speed line | this commit |
| 4 | Docs, release check with screenshots, debug, full release suite | |

## Phase 1 — The logic

1. `src/view/launcher_lanes.*` and `src/view/output_frame.*` as designed (§1); `BoardScene::lamp`; `HasTv` in `session.hpp`/`.cpp`, `makeLive` commented to agree.
2. CMake: the two files in `bw_view`; the two tests in `tests/CMakeLists.txt`.
3. Tests as designed (§1, Tests), on the cheap fixture (no `LoadMachine`). A Machine that disagrees with the design's expectation is reported; the rule's constants are tuned only in phase 3, on the screenshots.

Fence: release and debug `ctest -R "^(launcher lanes|output frame|board scene|view): "`; full release suite (shared code: `bw_view`, `bw_session`). Commit: `view: the launcher's lanes and each Machine's output frame (P7b phase 1)`, its body naming the checks.

## Phase 2 — Shared pieces

1. `Load`, `Print` into `bw`, declared in `loaded.hpp`, defined in a new `src/ui/loaded.cpp` (§2).
2. `src/ui/board_tags.*`; `RunWindow` uses `LedTagsOf`, `SwitchTagsOf` (after its `setPressed` loop), `ShowRepairs`.
3. `BoardView::setStill` (§2): `NoInputs` on its child, no input, no flip corner, no refusal, framed at every resize; `kPanelBackground` in the header.
4. `DrawMonitor` (the shell filled) out of `TvWindow::draw`; `DrawNote`, `kNoteStrip`, `kNoteInk` out of `DrawBrief`; `TvView::clear`; `kShelfWood`, `kShelfEdge`, `kTrayShadow` in `shelf.hpp`.
5. `clang-format` on the changed C++ files.

Fence: release build (no ctest reaches `src/ui`: the screenshots are the check); screenshots viewed, the same as before the phase: `bw_ui machines/040-stopwatch/stopwatch.bwn --after 1s` (digits lit, buttons with legends), `machines/130-bw2001/puzzles/01-odd-e.bwp --after 3s` (the monitor, the brief), `machines/120-bw1/bw1.bwn --after 1s` (keys, the slide switch). Debug build; debug `bw_ui --visit` of P7a's three (the Counter's Dark LED, the Test Pattern, the Typewriter): no new sanitizer report. Commit: `ui: the monitor, the post-it, the Board's tags and Load shareable; a still Board view (P7b phase 2)`, its body naming the checks.

## Phase 3 — The launcher

1. `Launcher` rewritten (§3): the window's flags and padding, layout, the still loader thread, the live run's background load after the selection rests, pictures with their overlay children, post-its, chevrons, the error line, keys, clicks.
2. `main.cpp`: `RunLauncher` with a `TvView`, the fonts, the speed line; `launcher.stop()` before a bench's `Load` and on quitting; `select(left)`; `--select`.
3. Screenshots viewed (design, Checks); the framing constants tuned if a frame looks wrong (the rule's tests updated with them); the BW-2001's Odd E speed from `--select` against its bench's; the UI time with the BW-2001 selected.
4. Usage errors: `--select` with a file, with `--visit`, of an unknown file.

Fence: release build; release `ctest -R "^(launcher lanes|output frame|machine list): "` (if the constants changed); the screenshots; debug build, debug `--select` of the Typewriter and the Stopwatch and `--visit` of P7a's three: no new sanitizer report. Commit: `ui: the launcher's two lanes, the selected Machine live and its post-its (P7b phase 3)`, its body naming the checks.

## Phase 4 — Docs and checks

1. GLOSSARY, ADD_MACHINE, ADD_PUZZLE, ARCHITECTURE (P7b done, P7c) as designed (§4).
2. Release check: the screenshots again, viewed (the launcher at start, the Stopwatch, Odd E, the BW-1, the BW-80's last Puzzle); the BW-2001's Odd E from `--select` 4 s and from the command line, speed and UI time.
3. Full release suite.
4. This plan cut down to the phase table and notes.

Fence: full release suite. Commit: `docs: P7b done, the launcher's look in the glossary and recipes (P7b phase 4)`.

## Implementation notes

- Review (2026-11-22), fixed: loads off the UI thread (the BW-2001 loads in 0.26 s, not 30 ms): a still loader thread, the live run loaded by `std::async` once the selection rests 0.15 s; clicks reach the launcher through `NoInputs` children; the monitor and the error box in an overlay child (a parent's drawing is under its children); `loaded.hpp` exists (only `loaded.cpp` new); a Machine without a Board; `liveStatus` not `const`; the launcher window without wheel scrolling or padding, its top lane in `kPanelBackground`; the TV Machine's Board rect; the chevrons beside the picture; the glide's numbers and per-lane snap; `HasTv` beside `makeLive`; the output frame tests on the cheap fixture (no `LoadMachine` in debug); the `view: ` cases in the fence; screenshots as `src/ui`'s check and in phase 4; commit bodies naming the checks; P7a's three named; the longest brief 204; Odd E for the speed; `DrawMonitor` fills the shell; `kTrayShadow` exported; the error line cleared on any change; `select` of a file not listed; `Loaded` reset then emplaced; no `NoNav` (navigation is off).
- Phase 1 (2026-11-22): as designed; the repository's Machines agree with the design's expectations. `HasTv` works on `LoadNetlistFile` alone (the BW-2001's netlist loads without its ROMs). Checks: release and debug `-R "^(launcher lanes|output frame|board scene|view): "` (28), full release suite 1493/1493.
- Phase 2 (2026-11-22): `SwitchTagsOf` reads the design Circuit (as `Session::switches` does) where `RunWindow` read the built one: the same Parts and Switches. `RunWindow`'s slide test is by the Part's name only. The refusal needs no guard in a still (`refuse` is never called on one). Screenshots viewed, as before: the Stopwatch, Odd E (the monitor, the brief), the BW-1 (keys, the slide switch). Debug `--visit` of P7a's three: exit 0, only the known `flip_corner`/`chip_body` UBSan reports.
- Phase 3 (2026-11-22): tuned on the screenshots: `kFrameMargin` 45 mm (30 mm framed the Stopwatch and the BW-1 too tight to recognise; every Machine stays whole or framed as before, the tests unchanged); `kPictureFrom` 0.07, `kHintLeft` 0.035 and the paragraph from `kPictureTo + 0.04` (the chevrons touched the left hint and the paragraph). A still's data (scene, tags, frame) is made on the loader's thread and the Machine dropped there; only the `BoardView` is made on the UI thread. The post-its' row jumps to its start when another Machine is selected. Screenshots viewed: the launcher at start (the Counter live, the LED Chaser's still at the right); the Stopwatch (its digits framed, its resistors and chips below); Odd E (the monitor large with `READY.`, the Board's corner, the post-it lifted with the gold bar, the BW-1 and BW-80 stills); the BW-1 (six digits and the keypad, both chevrons); the BW-80's White screen (no still at the right, the BW-2001's monitor at the left, the row scrolled to the last post-it); `--visit` of Dark LED (back on it, the LED dark). Speed: Odd E 0.26×, UI 1.9 ms from `--select` (bench 0.26×, 1.5 ms); the BW-80's White screen 0.16× (bench 0.15×). Usage errors: `--select` with a file, with `--visit`, of an unknown file (exit 2). Checks: release `-R "^(launcher lanes|output frame|machine list): "` (15); debug `--select` of the Typewriter and the Stopwatch and `--visit` of P7a's three: exit 0, only the known `flip_corner`/`chip_body` UBSan reports.

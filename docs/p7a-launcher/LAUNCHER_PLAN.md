# P7a — Launcher — Implementation Plan

Spec: [LAUNCHER.md](LAUNCHER.md). Design: [LAUNCHER_DESIGN.md](LAUNCHER_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | The list: `machine_list`, its tests | `c36d5ce` |
| 2 | The loop and the launcher: ImGui per screen, `ResetBench`, `present(TvView*)`, `RunLauncher`, no file, `--machines`, `--visit` | `4538aaf` |
| 3 | The way back: the shelf's Leave tray, `QuestionPopup`, the leave question, `--leave` | `343e65a` |
| 4 | Docs, release check, debug `--visit`, full release suite | this commit |

## Implementation notes

- Review (2026-11-22), fixed: the leave question opens upwards from the click (downwards it left the main window and became a desktop window macOS shows under it); a visited bench ends by `Options::visitFor`, a command-line `--screenshot` as Quit; the whole `machines/` search before `SDL_Init`; the grayed arrow unreachable from the build tree, checked by reading; `std::filesystem` without throws, `--machines DIR` with no Machine a usage error, `weakly_canonical` comparisons and `EntryOf`; `Item::Leave`'s own cases in the shelf; a failed visit exits `kExitErrors`; `benchFlag` in `ParseOptions`, `--leave` without a value, `argv[1]` only; `--leave` with `--ask` or without a `machines/` a usage error; the other globals named; `std::optional<Loaded>`; `Displayable` and the manual's font sizing in the launcher; the listed name as a launcher bench's title; the `--visit` check's limits stated; "for the run" becomes "for the bench" in GLOSSARY, ADD_INSTRUMENT and comments; temporary directories per case; a Puzzle naming another Machine reported and left out; the selection kept or clamped.

- Phase 1 (2026-11-22): as designed. Checks: release and debug `-R "^machine list: "` (4), full release suite 1481/1481.
- Phase 2 (2026-11-22): an entry is a plain `Selectable` with its title and brief drawn over it by the draw list (no `AllowOverlap`: nothing else is an item there). `Launcher::list()` gives the loop the list for `--visit`'s check and the working Machine's title. Screenshots viewed: the launcher at start (the Counter, its 3 Puzzles); `--visit` of Odd E (the BW-2001 selected, its 16 Puzzles, the pane scrolling); the Stopwatch's Stuck at zero from the command line as before. Usage errors as designed (`--probe U1.1`, `--run 1x`, `--visit` without `--screenshot`, with a file, of an unknown file, `--machines` of a directory without Machines or missing). Debug `--visit` of Dark LED, the Test Pattern and the Typewriter: exit 0, no ASan report. UBSan reports signed overflows in `flip_corner.cpp:19` and `chip_body.cpp:64` (a Board point at `INT64_MIN`, seemingly the mouse outside the window); a single command-line bench reports the same 3 before this step's code runs anything new: not P7a's, left for the user.
- Phase 3 (2026-11-22): the question opens just above the Leave tray (`Shelf::leaveAt`), not at the click: at the click its bottom-left covered the arrow and the left of the shelf. Screenshots viewed: the Counter's Dark LED (the tray, the arrow live); `--leave --mark` on it (the question above the arrow, inside the window); `--ask swap:U5` (the power-cycle question as before, its box). `--leave --ask` a usage error. Debug `--visit` of the three Machines again: exit 0, no ASan report (UBSan's `flip_corner`/`chip_body` reports as in phase 2).
- Phase 4 (2026-11-22): GLOSSARY (Launcher, the Shelf's arrow, "for the bench" in Brief, Contents and Equipment), ADD_MACHINE (the title and first paragraph, a Machine's directory, the test's count), ADD_PUZZLE (title and brief listed, a wrong `machine` left out), ADD_INSTRUMENT and the comments ("for the bench": `bench.hpp`, `brief_window.hpp`, `contents_page.hpp`, `memory_contents.hpp`, `tv_controls.hpp`). Release check: the BW-2001's Odd E for 4 s, 0.26× and UI 1.4 ms from the command line, 0.26× and 1.3 ms from `--visit`. Full release suite 1481/1481.
- For the user to check (a screenshot cannot click): clicking through the launcher; the arrow with no work (back at once) and with a repair or a mark (the question, OK and Cancel, a click elsewhere); a popped-out window closed on leaving; typing on the Typewriter or the BW-2001 after coming back; a broken Puzzle's red line.

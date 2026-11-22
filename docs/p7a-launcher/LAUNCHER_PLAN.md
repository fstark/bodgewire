# P7a — Launcher — Implementation Plan

Spec: [LAUNCHER.md](LAUNCHER.md). Design: [LAUNCHER_DESIGN.md](LAUNCHER_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | The list: `machine_list`, its tests | |
| 2 | The loop and the launcher: ImGui per screen, `ResetBench`, `present(TvView*)`, `RunLauncher`, no file, `--machines`, `--visit` | |
| 3 | The way back: the shelf's Leave tray, `QuestionPopup`, the leave question, `--leave` | |
| 4 | Docs, release check, debug `--visit`, full release suite | |

## Phase 1 — The list

Design §1.

1. `src/machine/machine_list.{hpp,cpp}`: `PuzzleEntry`, `MachineEntry`, `MachineList`, `ListMachines`, `FindMachinesDir`, `MachineIndexOf`, `EntryOf`; `bw_machine` links `bw_manual` PRIVATE.
2. `tests/machine/machine_list_test.cpp` (added to `tests/CMakeLists.txt`'s list): `machine list: a directory`, `the repository`, `find machines`, `machine index`.

**Fence**: release build; `ctest --preset release -j -R "^machine list: "`; debug build and the same `-R` in debug; the full release suite (shared code: `bw_machine`'s link). Commit: `machine: the Machines and Puzzles of a machines/ directory, for the launcher (P7a phase 1)`.

## Phase 2 — The loop and the launcher

Design §2, §3, §5.

1. `ResetBench()` in `bench.{hpp,cpp}`; `GpuRender::present(TvView*)`.
2. `main.cpp`: `Options::file` optional, `benchFlag`, `--machines`, `--visit`, `visitFor`, the usage line; `PrepareBench` out of `main`; `ImGuiScreen`; `RunWindow` returning `BenchResult` (Back only from a visit in this phase); `RunLauncher`; `main`'s loop with `std::optional<Loaded>`; the window's title per screen; `machinesDir`'s search before `SDL_Init`; the exit code of a failed visit.
3. `src/ui/launcher.{hpp,cpp}`.
4. Screenshots, viewed: `bw_ui --screenshot` (the Counter selected, its 3 Puzzles); `bw_ui --visit machines/130-bw2001/puzzles/01-odd-e.bwp --screenshot` (the BW-2001 selected, 16 Puzzles); `bw_ui machines/040-stopwatch/puzzles/01-stuck-at-zero.bwp --screenshot` as before.
5. Usage errors checked by hand: `bw_ui --probe U1.1`, `bw_ui --run 1x`, `bw_ui --visit x.bwp` (no screenshot), `bw_ui FILE --visit …`, `bw_ui --visit nowhere.bwp --screenshot x`, `bw_ui --machines /tmp`.

**Fence**: release build; screenshots viewed; the debug build and its `--visit` of the Counter's Dark LED, the Test Pattern and the Typewriter (design, Checks) with no sanitizer report. Commit: `ui: bw_ui with no file opens a launcher of the Machines and Puzzles (P7a phase 2)`.

## Phase 3 — The way back

Design §4.

1. `git mv src/ui/power_cycle_popup.{hpp,cpp} src/ui/question_popup.{hpp,cpp}`; `QuestionPopup`'s constructor and `open`'s pivot; `main.cpp`'s power-cycle question on it.
2. `Item::Leave`, its drawing; `Tray::Leave`, `kItems`, `canLeave`, `leaveClicked`; `Item::Leave`'s own case in both of `Shelf::draw`'s switches.
3. `RunWindow`: the leave click, work or not, the leave question opening upwards, Back; `--leave` (usage errors with `--ask`, without a `machines/`).
4. Screenshots, viewed: the Counter's Dark LED (the new tray, the arrow live); `--leave --repair swap:U5` on Dark LED (the question above the arrow, inside the window). The grayed arrow is unreachable from the build tree (design §2): checked by reading.

**Fence**: release build; screenshots viewed; the debug `--visit` again. Commit: `ui: a tray at the shelf's left end goes back to the launcher, asking when work is lost (P7a phase 3)`.

## Phase 4 — Docs and release check

Design §6.

1. GLOSSARY (Launcher, the Shelf, "for the bench"), ADD_MACHINE, ADD_PUZZLE, ADD_INSTRUMENT; the "for the run" comments (design §6).
2. Release check: the BW-2001 opened from `--visit` and from the command line, the speed ratio and UI ms printed alike.
3. ARCHITECTURE's row done with the date and links, its "ImGui kept" clause corrected; this plan cut down to the phase table and notes.

**Fence**: `ctest --preset release -j` (full). Commit: `docs: P7a done, the launcher in the glossary and recipes (P7a phase 4)`.

## Implementation notes

- Review (2026-11-22), fixed: the leave question opens upwards from the click (downwards it left the main window and became a desktop window macOS shows under it); a visited bench ends by `Options::visitFor`, a command-line `--screenshot` as Quit; the whole `machines/` search before `SDL_Init`; the grayed arrow unreachable from the build tree, checked by reading; `std::filesystem` without throws, `--machines DIR` with no Machine a usage error, `weakly_canonical` comparisons and `EntryOf`; `Item::Leave`'s own cases in the shelf; a failed visit exits `kExitErrors`; `benchFlag` in `ParseOptions`, `--leave` without a value, `argv[1]` only; `--leave` with `--ask` or without a `machines/` a usage error; the other globals named; `std::optional<Loaded>`; `Displayable` and the manual's font sizing in the launcher; the listed name as a launcher bench's title; the `--visit` check's limits stated; "for the run" becomes "for the bench" in GLOSSARY, ADD_INSTRUMENT and comments; temporary directories per case; a Puzzle naming another Machine reported and left out; the selection kept or clamped.


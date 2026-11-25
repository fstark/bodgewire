# P8e — Saved progress — Implementation Plan

Spec: [SAVED_PROGRESS.md](SAVED_PROGRESS.md). Design: [SAVED_PROGRESS_DESIGN.md](SAVED_PROGRESS_DESIGN.md).

## Status

| Phase | Description | Commit |
|---|---|---|
| 1 | `Progress`, its file, `ProgressKey`; `--progress`, `--forget` | |
| 2 | `ProgressStore`; the bench saves at its tick | |
| 3 | The Launcher: ticks, hint, forget | |
| 4 | Docs, release check, web build | |

## Phase 1 — Progress and `--progress`

1. `src/machine/progress.hpp`, `.cpp` as design §1; `progress.cpp` in `src/machine/CMakeLists.txt`.
2. `tests/temp_files.hpp` with `WriteFile` (from `machine_list_test.cpp`, which then uses it); `tests/machine/progress_test.cpp` (design §1 Tests), in `tests/CMakeLists.txt`.
3. `Options::progress`, `Options::forget` and their flags (design §3), their cases in `bench_options_test.cpp`.

Fence: release `ctest --preset release -j -R "^(progress|bench options|machine list): "`; debug the same. Commit: `machine: Progress, the repaired Puzzles' keys and their file; bw_ui --progress and --forget (P8e phase 1)`.

## Phase 2 — Kept, and saved at the bench's tick

1. `src/ui/progress_store.hpp`, `.cpp` (design §2, `EM_JS_DEPS` included), in `src/ui/CMakeLists.txt`.
2. `main`: the store and `Progress` after `SDL_Init`; `RunBench`'s `repaired` callback and the key per bench (design §4).

Fence: release build; the hand check: `build/release/src/ui/bw_ui machines/030-dice/puzzles/01-dead-button.bwp --repair wire:SW1.1:R9.2 --progress /tmp/bw_p8e.txt --screenshot /tmp/bw_p8e_bench.png --after 5s` (rm the file first): the shot's brief ticked, `/tmp/bw_p8e.txt` holds `030-dice/puzzles/01-dead-button.bwp`; the same without `--repair`: no key added; `--screenshot` without `--progress` writes no file in the pref path (its `progress.txt` unchanged or absent). Commit: `ui: the player's progress kept, saved at the bench's tick (P8e phase 2)`.

## Phase 3 — The Launcher

1. `DrawTick` out of `DrawBrief` (design §5).
2. `Launcher::setProgress` (with the forget callback), `askForget`, the post-its' ticks, the hint, Backspace/Delete and the question (design §5); `main` passes the Progress and the store's `clear`, and `--forget` to `askForget`.

Fence: release build; screenshots viewed: `bw_ui --progress /tmp/bw_p8e.txt --select machines/030-dice/puzzles/01-dead-button.bwp --screenshot /tmp/bw_p8e_launcher.png` (the file from phase 2 plus `030-dice/puzzles/03-seven.bwp` added by hand): two ticks, the hint; the same with `--forget`: the question under the hint; a file with only `999-gone/puzzles/01-x.bwp`: no tick, no hint; a bench on Dead button with the ticked file (`--screenshot`, no repair): its brief unticked (the brief's tick through `DrawTick` checked in the phase 2 shot again). The key itself: only the user. Commit: `ui: the Launcher's ticks and forgetting them (P8e phase 3)`.

## Phase 4 — Docs and finish

1. GLOSSARY (Saved progress, Launcher, Brief, Repair validation), AGENTS.md's screenshot line, README_SPEC.md's launcher sentence and README.md regenerated, text only (design §6).
2. Web: `source ~/emsdk/emsdk_env.sh && cmake --preset web && cmake --build --preset web` builds.
3. The full release suite; ARCHITECTURE row done with the date and links; this plan cut to the table and notes.

Fence: the full release suite; the web build. Commit: `docs: P8e done, Saved progress in the glossary (P8e phase 4)`.

## Implementation notes

- Review (2026-11-25), must-fixes, fixed in the docs before phase 1: a directory reads as "" on macOS, so `ReadProgressFile` rejects a non-regular file; `EM_JS_DEPS` for `stringToNewUTF8`/`UTF8ToString` (else the web load is silently empty); the Launcher's key cache dropped for `ProgressKey(machine, puzzle)` (no file system, the cache was never built for the first row). Should-fixes taken: always `localStorage` on the web; the forget saved inside the Launcher's frame; no write after a failed read; each save re-reads and merges (two instances); "Delete: forget the ticks" (Proggy has no ⌫); GLOSSARY's Launcher and Brief entries; README_SPEC's launcher sentence; `--forget` for a screenshot of the question. Nits: counts, the example key, `std::less<>`, `WriteFile` shared in `tests/temp_files.hpp`, the sanitizer runs in the spec.

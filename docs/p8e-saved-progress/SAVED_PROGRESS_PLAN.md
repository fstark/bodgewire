# P8e — Saved progress — Implementation Plan

Spec: [SAVED_PROGRESS.md](SAVED_PROGRESS.md). Design: [SAVED_PROGRESS_DESIGN.md](SAVED_PROGRESS_DESIGN.md).

## Status

| Phase | Description | Commit |
|---|---|---|
| 1 | `Progress`, its file, `ProgressKey`; `--progress`, `--forget` | `f70abbb` |
| 2 | `ProgressStore`; the bench saves at its tick | `290412d` |
| 3 | The Launcher: ticks, hint, forget | `1626917` |
| 4 | Docs, release check, web build | this commit |

## Implementation notes

- Review (2026-11-25), must-fixes, fixed in the docs before phase 1: a directory reads as "" on macOS, so `ReadProgressFile` rejects a non-regular file; `EM_JS_DEPS` for `stringToNewUTF8`/`UTF8ToString` (else the web load is silently empty); the Launcher's key cache dropped for `ProgressKey(machine, puzzle)` (no file system, the cache was never built for the first row). Should-fixes taken: always `localStorage` on the web; the forget saved inside the Launcher's frame; no write after a failed read; each save re-reads and merges (two instances); "Delete: forget the ticks" (Proggy has no ⌫); GLOSSARY's Launcher and Brief entries; README_SPEC's launcher sentence; `--forget` for a screenshot of the question. Nits: counts, the example key, `std::less<>`, `WriteFile` shared in `tests/temp_files.hpp`, the sanitizer runs in the spec.
- Phase 1 (2026-11-25): as designed; `--forget` rejected with a file or `--visit`, as `--select` is. Checks: release and debug `^(progress|bench options|machine list): ` (22 cases).
- Phase 2 (2026-11-25): as designed; `ProgressStore::add` keeps the key for the run when the re-read fails. Checks: Dead button with `--repair wire:SW1.1:R9.2 --progress /tmp/bw_p8e.txt --screenshot --after 5s`: brief ticked, the key in the file; Loaded dice unrepaired adds nothing; `--screenshot` without `--progress` makes no pref path.
- Phase 3 (2026-11-25): as designed; the hint's `ImVec2` named `forgetExtent` (the making plate's `hintExtent` shadows it under `-Wshadow`). Screenshots viewed: Dice with Dead button and Seven ticked and the hint; `--forget`: "Forget every repaired Puzzle?" under the hint at the title band's right; an unlisted key only: no tick, no hint; Dead button reopened: brief unticked; the brief's tick after a repair unchanged (`DrawTick`).
- Phase 4 (2026-11-25): the web link failed: clang-format had split `===` in the `EM_JS` JavaScript (`== =`); the bodies now between `// clang-format off` and `on`. Web checked in the browser (served locally): a key put in `localStorage` ticks Dark LED on the Launcher after a reload; Delete, then OK, writes the empty progress back. Docs: GLOSSARY (Saved progress; the Launcher, Brief and Repair validation entries), README_SPEC and README (one sentence), AGENTS.md (screenshots and `--progress`, `--forget`). Checks: the full release suite (1656 tests; 51 min this time, the machine busy with the web build after it), the web build.

# P8e — Saved progress

Roadmap step P8e (ARCHITECTURE.md#roadmap): the Puzzles the player has repaired are kept across launches, and their post-its on the Launcher carry the owner's tick. Design: [SAVED_PROGRESS_DESIGN.md](SAVED_PROGRESS_DESIGN.md). Plan: [SAVED_PROGRESS_PLAN.md](SAVED_PROGRESS_PLAN.md).

## Why

P8c ticks a Puzzle's post-it on the bench once Repair validation sees it completed, but the tick lasts only as long as the bench: back on the Launcher, nothing shows which Puzzles the player has done, and quitting forgets everything. With 114 Puzzles over 14 Machines, the player needs to see where they stopped. P8c left it for later (REPAIR_VALIDATION.md, Later).

## User decisions

From the roadmap row (2026-11-25) and the questions asked (the user, 2026-11-25):

- A Puzzle is known by its path under `machines/`: `030-dice/puzzles/02-loaded-dice.bwp`. A renamed or renumbered Puzzle file loses its tick.
- A Random fault (P8d) is not counted: its post-it never gets a tick.
- Forgetting: a key on the Launcher, asked first with the question popup.
- A ticked Puzzle reopened is fresh: the fault is back, the brief has no tick until repaired again, the Thank-you Card comes again as today. The tick is on the Launcher only.
- P7's "no 'solved' marks" (2026-11-22) is lifted by this step (the roadmap row).

Chosen without asking:

- Saved at the bench's tick (`RepairAck::ticked()` first true, 3 s after a power-on of a completed Machine), at once, not at quitting: a crash or a closed browser tab keeps it.
- Never taken back: a later Repair that breaks the Machine again takes the bench's tick off (P8c), not the saved one.
- Native: a text file, `progress.txt` in SDL's pref path (`SDL_GetPrefPath("bodgewire", "bw_ui")`: `~/Library/Application Support/bodgewire/bw_ui/` on the Mac, `~/.local/share/bodgewire/bw_ui/` on Linux). Web: always the browser's `localStorage`, key `bodgewire.progress`, the same text (a `--progress` passed through the page's `?arg=` is ignored: its file would be lost on reload).
- The file: one key per line, sorted, a first line `# Bodgewire progress`; blank and `#` lines ignored. Keys of Puzzles not listed (another `--machines`, a Puzzle since removed) are kept, not shown.
- `--progress FILE` reads and writes that file instead. `--screenshot` without `--progress` uses no saved progress at all (nothing read, nothing written; `--visit` needs `--screenshot`): the agent's and the README's shots never depend on the player's, nor change it. `--forget` opens the forget question at start (the agent's screenshot, like `--leave`).
- The forget key: Backspace (labelled "delete" on the Mac) or Delete. Its hint, "Delete: forget the ticks" (the UI font has no ⌫), faint at the title band's right, only while some listed Puzzle is ticked.
- A file that exists but cannot be read: one line on stderr, the game goes on with no saved progress and never writes over that file. A write that fails: one line on stderr.
- Two instances (two windows, two browser tabs): each save re-reads and merges, so neither loses the other's ticks; a forget empties both's at the next save.

## What the player sees

1. **The tick on the Launcher.** A Puzzle's post-it whose key is saved carries the owner's ballpoint tick (P8c's pen, `kBallpoint`) in its bottom-right corner, over the brief's last lines; "Start the Machine" and "Random fault" never do.
2. **Saved at the bench's tick.** The moment the bench's brief gets its tick (P8c), the Puzzle is saved; back on the Launcher its post-it is ticked. Quitting and launching again keeps it, native and web.
3. **Reopened fresh.** Opening a ticked Puzzle gives the bench as before P8e: the fault, an unticked brief, the Thank-you Card again once repaired.
4. **Forgetting.** While some listed Puzzle is ticked, the title band's right shows "Delete: forget the ticks". Backspace or Delete opens the question "Forget every repaired Puzzle?" (OK, Cancel) under it; OK clears every tick, listed or not, and saves at once. Clicking the hint does the same.
5. **Random fault and the working Machine** are never saved.

## What is new

1. `Progress` (`src/machine/progress`): the set of keys, its text, `ProgressKey` (a listed Puzzle's key), reading and writing a file.
2. Where it is kept (`src/ui/progress_store`): the file (native, `--progress`) or `localStorage` (web), or nowhere (`--screenshot`, `--visit`).
3. `--progress FILE` and `--forget` in the flags' table (`src/view/bench_options`).
4. The bench saves at its tick (`src/ui/main.cpp`'s `RunBench`).
5. The Launcher draws the ticks, the hint, the question; `DrawTick` shared with the brief (`src/ui/brief_window`).

Untouched: the kernel, Parts, Machines, Puzzles, Repair validation, `RepairAck`.

## Tests

- Unit (`tests/machine/progress_test.cpp`, `[machine]`): `progress: text` (sorted, header, round trip, blank and comment lines, duplicates), `progress: add and clear`, `progress: key` (a listed Puzzle; the working Machine, a Vetting Script, an unlisted file give none), `progress: file` (missing file empty, written then read, unreadable reported).
- `tests/view/bench_options_test.cpp`: `--progress FILE` taken with and without a file, `--forget`.
- Sanitizers (debug): `^(progress|bench options|machine list): `.
- Screenshots (release, viewed): the Launcher with `--progress` on a file ticking two of the Dice's Puzzles (`--select` one of them): the ticks on their post-its, the hint; the same with `--forget`: the question under the hint; with no ticked Puzzle listed: no hint. A bench reopened on a ticked Puzzle: no tick on its brief.
- By hand (the agent, native): `bw_ui machines/030-dice/puzzles/01-dead-button.bwp --repair wire:SW1.1:R9.2 --progress /tmp/p.txt --screenshot /tmp/x.png --after 5s` (a bench given on the command line still finds `machines/`): the brief ticked in the shot, the key in `/tmp/p.txt`; then the Launcher with the same `--progress` shows the tick.
- Web: built (`cmake --build --preset web`); the `localStorage` path checked by the user in the browser (a tick kept after a reload).
- Only the user: repairing a Puzzle in `bw_ui`, quitting, relaunching, seeing the tick; the same on the web page; the forget key.
- The full release suite.

## Later

- A per-Machine count on the Launcher ("4 of 12"): not asked for.
- Keys that survive renaming a Puzzle file.
- Random faults counted.

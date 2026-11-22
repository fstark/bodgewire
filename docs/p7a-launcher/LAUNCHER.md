# P7a — Launcher

Roadmap step P7a ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): `bw_ui` with no file opens a launcher listing every Machine and its Puzzles; a click opens the bench as `bw_ui FILE` does; the bench has a way back. Design: [LAUNCHER_DESIGN.md](LAUNCHER_DESIGN.md). Plan: [LAUNCHER_PLAN.md](LAUNCHER_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

Today the game starts only from a terminal, with a file's path: a player has to know the `machines/` tree and type a path for each Puzzle. The content is all there (14 Machines, 114 Puzzles); what is missing is the way in. P7a is the bare minimum: a plain screen that lists the content and opens it, and a way back. Its look (a preview of each Machine, the drawn style of the bench) is P7b's.

## User decisions

From the roadmap row (the user, 2026-11-22):

| Question | Decision |
|---|---|
| Content | the full content, open: no scores, no unlocking, no "solved" marks |
| `bw_ui FILE [flags]` | as today |
| `bw_ui` with no file | the launcher, on the `machines/` found from the working directory up, then from the executable's directory up; `--machines DIR` to name it |
| What is listed | the Machines in their number order; each Machine's Puzzles in file order (easiest first), each with its title and brief |
| The bare Machine | every Machine has a "no fault" entry first (its `.bwn`), the Counter too |
| Machine names | the User Manual's title, its first paragraph the line under it; the directory name without a manual |
| ROMs | always the Machine's `roms/`; `--roms` stays a command-line flag |
| Back | a button on the bench, not Esc (the Machine's keys keep it); a question when leaving loses work; nothing saved. A bench opened from the command line goes back to the launcher too |
| The look | the bare minimum; the graphical launcher with previews is P7b |

Settled at the step's start (the user, 2026-11-22):

| Question | Decision |
|---|---|
| Where the way back sits | a tray of its own at the shelf's left end, before the controls |
| The launcher's layout | two panes: the Machines' names on the left; on the right the selected Machine's name, its paragraph, then its entries |
| How much of the manual | the whole first paragraph, wrapped |

Derived in the design (2026-11-22), reported to the user:

- **The first paragraphs** of the 14 User Manuals were read: none is weak (22 to 80 words, each a welcome and what the Machine does). None changes.
- **What "loses work" means**: a repair Done (the binder's Repairs divider is not empty) or a pencil mark on the Board. Instruments clipped or a knob turned are not work: they cost a click to redo.
- **A fresh bench every time**: a bench opened from the launcher starts as a fresh `bw_ui FILE` does: windows at their default places, nothing popped out, the run switch at `1×`. ImGui's context is made anew for each screen (the launcher, each bench); the desktop window and the GPU are kept.
- **Command-line flags** apply to the first bench only. With no file, only `--machines`, `--screenshot`, `--after` and `--visit` are accepted; a bench flag without a file is a usage error.
- **Two test flags**: `--visit FILE,...` (no file, with `--screenshot`) opens each file from the launcher in turn as if clicked, runs it `--after`, goes back, and finally shoots the launcher: the screenshot of a selected Machine's entries, and the sanitizer check of several benches in a row. `--leave` (with a file) opens the leave question at start, for its screenshot.
- **Where "back" goes** from a command-line bench: `--machines` if given; else the `machines/` holding the file's Machine; else the one found as with no file. None found: the shelf's way back is grayed. `--machines DIR` listing no Machine is a usage error.
- **A Puzzle that names another Machine** than its directory's netlist is left out of the list and reported on stderr, as one that does not parse: every Puzzle listed loads.
- **"For the run" becomes "for the bench"**: what the bench keeps (the power-cycle question's "Don't ask again", the brief's place, each chip's Contents settings, a popped-out window's place) starts afresh with each bench.

## What the player sees

### The launcher

1. `bw_ui` with no file opens the window (1280 × 800, titled "Bodgewire") at the launcher.
2. The left pane lists the Machines by name, in the order of their directories (`010-counter` first). The first is selected at start; after a bench, the Machine just left.
3. A click on a name selects it. The right pane shows its name, its User Manual's first paragraph (wrapped, without the manual's code marks), then its entries:
   - **The working Machine (no fault)** first;
   - each Puzzle in file order: its title, its brief under it.
4. A click on an entry opens the bench on it, as `bw_ui` on that file does: the Machine powered on, the brief out on a Puzzle. The window's title becomes "Bodgewire — " and the Puzzle's title, or the Machine's name as listed ("Counter"; a command-line bench keeps today's netlist stem, "counter").
5. A file that fails to load (a broken Puzzle) leaves the launcher shown, with a line in red "Could not load NN-name.bwp: see the terminal"; the diagnostics go to stderr as today.
6. Closing the window quits.

### The way back

7. The shelf gains a tray at its left end, before the power switch, holding one item: an arrow pointing left. Tooltip "Back to the Machines".
8. A click on it, with nothing to lose, closes the bench and shows the launcher at once.
9. With a repair Done or a pencil mark on the Board, it asks first, just above the arrow (opening upwards, so it stays inside the window and the shelf stays seen): "Back to the Machines? The repairs and pencil marks on this Board are lost." **OK** leaves, **Cancel** or a click elsewhere stays. Mouse only, as the power-cycle question.
10. Leaving closes everything of the bench: its popped-out windows, the instruments, the Machine. Nothing is saved; opening the same entry again starts it fresh.
11. With no `machines/` known (a `bw_ui` copied out of the repository, run on a netlist outside the tree), the arrow is grayed: "No Machines directory". From the build tree the executable's own search always finds the repository's `machines/`.

## What is new

To be settled file by file in the design:

1. **The list**: `src/machine/machine_list` scans a `machines/` directory into Machines (name, paragraph, netlist) and Puzzles (title, brief); finds `machines/` from a directory up.
2. **The loop**: `bw_ui`'s `main` alternates launcher and bench; ImGui's context made per screen; the bench's pop-out state reset.
3. **The launcher**: `src/ui/launcher`, the two panes.
4. **The way back**: the shelf's Leave tray; the power-cycle question generalized to ask this one too.
5. **Flags**: no file; `--machines DIR`, `--visit FILE,...`, `--leave`.
6. **Docs**: GLOSSARY (Launcher, the Shelf's new tray), ADD_MACHINE and ADD_PUZZLE (what the launcher shows), ARCHITECTURE.

The kernel, Parts, the builder, devices, Machines, Puzzles and manuals are untouched.

## Tests

- **Unit tests** (`machine list: …`): a made-up directory (Machines out of order, one without a netlist skipped, one without a manual named by its directory, Puzzles in file order, a broken Puzzle and one naming another Machine reported and left out, a missing directory listing nothing); the repository's `machines/` (14 Machines in order, the Counter first with its 3 Puzzles, the BW-2001 named "BW-2001", every Machine a paragraph without backticks, every Puzzle a title and a brief, no problems); `FindMachinesDir` from a nested directory and from one with none.
- **Screenshots** (release, viewed): the launcher at start (the Counter); `--visit` of a BW-2001 Puzzle (the launcher back at the BW-2001, its 16 Puzzles listed); a bench from the command line (the shelf's new tray: the same code as a launcher-opened one); `--leave` on a Puzzle with a repair (the question above the arrow).
- **Sanitizers**: the debug build's `bw_ui --visit` of three small Machines in a row (a Puzzle with a Board, a TV Machine, a keyboard Machine) runs clean. Not covered: a popped-out window closed on leaving (visited benches take no flags) and leaks (ASan on Apple Silicon has no leak detection).
- **By the user**: clicking through the launcher; the way back with and without work; a popped-out window closed on leaving; the keyboard Machine's typing after a return.
- **Release**: the full release suite passes.

## Later (P7b)

- The graphical launcher: a preview of each Machine, the drawn look.

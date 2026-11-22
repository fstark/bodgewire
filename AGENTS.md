Python: the scripts use the standard library only and run with `python3 -I`; `venv/` (git-ignored) is the interpreter the `tests/data/*/fetch.sh` scripts call.

## Build and test

- Release: `cmake --preset release && cmake --build --preset release`; tests `ctest --preset release -j` (a bare `-j`: every core). The release suite is the routine check: about 2½ minutes, and it runs the cases debug skips (the BW-2001's and BW-80's). The full release suite runs before every final commit (a roadmap step's end, a change to shared code). A change that one Part or Machine alone can see runs only the tests that can reach it in between, e.g. `ctest --preset release -j -R "2524|typewriter|catalogue|manuals"`, and the commit message says which.
- Debug (ASan + UBSan): `cmake --preset debug && cmake --build --preset debug`; tests `ctest --preset debug -j`. Only its sanitizers find what release misses (memory errors, undefined behaviour), so use it for changes to C++ logic in the kernel, Parts, builder or devices, and run only the tests that reach the change (`-R`), never the BW-2001 or BW-80 cases routinely: debug is about 0.004× on them. The full debug suite (about 15 minutes, two tennis cases about 14 minutes each) only when the user asks.
- The agent can't see the screen: `build/release/src/ui/bw_ui machines/NNN-name/puzzles/NN-x.bwp --screenshot /tmp/x.png --after 1s`, then view the PNG (it also prints the speed ratio and UI ms).
- `clang-format` on C++ files only, never on `CMakeLists.txt`.

## Implementing a roadmap step

"Implement P4g" (a row of the `docs/ARCHITECTURE.md` roadmap) means the whole of this, without being asked for each part:

1. **Ask first.** Read the row, the recipes it touches and the code it builds on, then ask the user the choices that are theirs (which chip, how many digits, which buttons…), each with a recommended option. Settle them before writing.
2. **Spec, design, plan** in `docs/pXy-name/`, named after the step (`SEVEN_SEGMENT.md`, `SEVEN_SEGMENT_DESIGN.md`, `SEVEN_SEGMENT_PLAN.md`), in the shape of [docs/recipes/ROADMAP_STEP.md](docs/recipes/ROADMAP_STEP.md):
   - spec: what is new and why, the user's decisions with the date, what the tests show;
   - design: how, file by file, citing recipe steps instead of repeating them;
   - plan: phases, each ending with a fence (checks plus one commit message), and a status table.

   A step that brings a Machine also writes `machines/NNN-name/docs/NAME.md` (lesson, circuit, when it works, Puzzles) and `NAME_DESIGN.md` (netlist, Board, Puzzle files, manuals).
3. **Independent review.** A fresh sub-agent gets only the paths of the three docs (and the Machine docs), not your conclusions. It checks the plan against the design and spec, and both against the code: APIs that exist, rules the builder enforces, recipe steps, test timings. Fix every must-fix in the docs, then commit the docs.
4. **Implement in one go.** Run every phase in order without stopping, one commit per phase, and update the plan's status table and implementation notes as you go. Stop only for what only the user can do. The main one is the KiCad Board: place it with `bw place`, give the routing constraints from the Machine design (ADD_MACHINE.md §3), and wait for the routed Board.
5. **Finish.** Release check with screenshots viewed, recipes fixed, ARCHITECTURE row set to done with the date, one line and links (the detail stays in the step's docs), plan cut down to the phase table and notes.

A row that brings only a Machine's spec (no code, no Puzzles) skips the plan and the review.

## Recipes

Before designing, planning or coding one of these, read its recipe instead of rediscovering it from the code:

- [docs/recipes/ADD_PART.md](docs/recipes/ADD_PART.md): a chip Part (74LSxx)
- [docs/recipes/ADD_BOARD_DEVICE.md](docs/recipes/ADD_BOARD_DEVICE.md): an LED, digit or button on the Board
- [docs/recipes/ADD_MACHINE.md](docs/recipes/ADD_MACHINE.md): netlist, manuals, Board, tests
- [docs/recipes/ADD_PUZZLE.md](docs/recipes/ADD_PUZZLE.md): a hand-placed fault
- [docs/recipes/ADD_CONNECTOR_DEVICE.md](docs/recipes/ADD_CONNECTOR_DEVICE.md): a device off the Board behind a Connector (paddle controller)
- [docs/recipes/ADD_INSTRUMENT.md](docs/recipes/ADD_INSTRUMENT.md): a measuring tool and its window
- [docs/recipes/ROADMAP_STEP.md](docs/recipes/ROADMAP_STEP.md): a roadmap step's spec, design and plan

Designs and plans cite recipe steps instead of repeating them. When a milestone finds a recipe wrong or incomplete, fix the recipe in the same commit.

## Docs

- Each Machine in its own directory `machines/NNN-name/` (P6a): netlist, `board/`, `manuals/` (the player's), `docs/` (its development docs), `roms/`, `puzzles/` (`NN-name.bwp`, easiest first). Datasheets, footprints and benchmarks stay shared (`datasheets/`, `footprints/`, `bench/`). Layout and loading rules: [ADD_MACHINE.md](docs/recipes/ADD_MACHINE.md#the-directory).
- Conventions: `docs/GLOSSARY.md`, `docs/STYLE.md`, `docs/NAMING.md`. Structure and roadmap: `docs/ARCHITECTURE.md`. Routing a Board in KiCad (the user's part): `docs/KICAD_GUIDE.md`.
- `docs/p*-*/` are finished milestones: history only, the code is the truth. Read one only when the task names it.
- The user edits docs in parallel: commit only your own paths.

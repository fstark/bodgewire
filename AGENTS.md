There is a venv environment in venv.

## Build and test

- Debug (ASan + UBSan): `cmake --preset debug && cmake --build --preset debug`; tests `ctest --preset debug -j 12`.
- Release: `cmake --preset release && cmake --build --preset release`.
- The agent can't see the screen: `build/release/src/ui/bw_ui puzzles/X.bwp --screenshot /tmp/x.png --after 1s`, then view the PNG (it also prints the speed ratio and UI ms).
- `clang-format` on C++ files only, never on `CMakeLists.txt`.

## Recipes

Before designing, planning or coding one of these, read its recipe instead of rediscovering it from the code:

- [docs/recipes/ADD_PART.md](docs/recipes/ADD_PART.md): a chip Part (74LSxx)
- [docs/recipes/ADD_BOARD_DEVICE.md](docs/recipes/ADD_BOARD_DEVICE.md): an LED, digit or button on the Board
- [docs/recipes/ADD_MACHINE.md](docs/recipes/ADD_MACHINE.md): netlist, manuals, Board, tests
- [docs/recipes/ADD_PUZZLE.md](docs/recipes/ADD_PUZZLE.md): a hand-placed fault
- [docs/recipes/ADD_INSTRUMENT.md](docs/recipes/ADD_INSTRUMENT.md): a measuring tool and its window

Designs and plans cite recipe steps instead of repeating them. When a milestone finds a recipe wrong or incomplete, fix the recipe in the same commit.

## Docs

- Conventions: `docs/GLOSSARY.md`, `docs/STYLE.md`, `docs/NAMING.md`. Structure and roadmap: `docs/ARCHITECTURE.md`.
- `docs/p*-*/` are finished milestones: history only, the code is the truth. Read one only when the task names it.
- The user edits docs in parallel: commit only your own paths.

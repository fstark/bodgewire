Python: the scripts use the standard library only and run with `python3 -I`; `venv/` (git-ignored) is the interpreter the `tests/data/*/fetch.sh` scripts call.

## Build and test

- Release: `cmake --preset release && cmake --build --preset release`; tests `ctest --preset release -j` (a bare `-j`: every core). The release suite is the routine check: about 2½ minutes, and it runs the cases debug skips (the BW-2001's and BW-80's). The full release suite runs before every final commit (a roadmap step's end, a change to shared code). A change that one Part or Machine alone can see runs only the tests that can reach it in between, e.g. `ctest --preset release -j -R "2524|typewriter|catalogue|manuals"`, and the commit message says which.
- Debug (ASan + UBSan): `cmake --preset debug && cmake --build --preset debug`; tests `ctest --preset debug -j`. Only its sanitizers find what release misses (memory errors, undefined behaviour), so use it for changes to C++ logic in the kernel, Parts, builder or devices, and run only the tests that reach the change (`-R`), never the BW-2001 or BW-80 cases routinely: debug is about 0.004× on them. The full debug suite (about 15 minutes, two tennis cases about 14 minutes each) only when the user asks.
- The agent can't see the screen: `build/release/src/ui/bw_ui machines/NNN-name/puzzles/NN-x.bwp --screenshot /tmp/x.png --after 1s`, then view the PNG (it also prints the speed ratio and UI ms). Headless, `build/release/src/cli/bw run FILE --time 1s` shows a Machine's outputs too: `--tv x.bmp` writes the TV's last frame, `--leds` prints each lamp's brightness (`led D1 0.50`, `led DS1.a 1.00`). `bw random machines/NNN-name/puzzles/random.bwr --seed N` makes a Random fault (P8d) and prints its `fault` line, the tries and the seconds; `bw_ui` opens a bench on one with the same file and `--seed`.
- Web (P8b): `source ~/emsdk/emsdk_env.sh && cmake --preset web && cmake --build --preset web` (emsdk 7.0.1, `bw_ui` only, no tests); serve `build/web/src/ui/` over HTTP and open `bw_ui.html`. Pushing `main` publishes it on GitHub Pages ([web.yml](.github/workflows/web.yml)). A change to how `bw_ui` draws needs its web path in `gpu_render_web.cpp` (SDL_Renderer, no pop-outs, no `--screenshot`), and no `try` may enclose a frame loop (Asyncify with wasm exceptions). Build it after such a change.
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

## Shared helpers

Before writing a small helper, look here: most of the project's duplicated code (2026-11-23) was a helper rewritten because the existing one wasn't found. A new helper that a second file needs goes in one of these files, or in a new one added to this list.

- Text ([src/text/text.hpp](src/text/text.hpp), below every layer): `ReadTextFile`, `SplitWords`, `Split(text, ',')`, `Trim`, `IsSpace`, `IsDigit`, `ParseDecimal` (`N` or `N.N` before a unit), `ParsePercent`.
- Units: durations and frequencies ([src/sim/time.hpp](src/sim/time.hpp): `ParseDuration`, `ParseFrequencyPeriod`); ohms and farads ([src/netlist/units.hpp](src/netlist/units.hpp)).
- A Puzzle's and a Vetting Script's lines ([src/machine/puzzle.hpp](src/machine/puzzle.hpp)): `QuotedValue` (`title "…"`), `IsMachineName`; a Fault back to its `fault …` line: `FormatFault`.
- Scripted inputs on a Simulator stepped by hand ([src/machine/input_edges.hpp](src/machine/input_edges.hpp)): `InputEdges` (presses, typing, knobs at a time, from `ParsePress`, `ParseTyping`, `ParseKnob`), `RunTo` (to a time, a millisecond at a time, each edge applied at its own).
- Random numbers the same on every platform: `Rng` ([src/sim/rng.hpp](src/sim/rng.hpp)); never `std::mt19937` with a distribution where a result must repeat.
- References and pins ([src/netlist/parser.hpp](src/netlist/parser.hpp)): `ParsePinRef` ("U3.7"), `IsReference`; finding a pin on a Part: `FindPin`, `PinNet` ([src/netlist/builder.hpp](src/netlist/builder.hpp)).
- Loading a netlist: `ReadNetlistFile` / `LoadNetlistFile(path, roms)` ([src/netlist/builder.hpp](src/netlist/builder.hpp)); printing diagnostics: `PrintDiagnostics` ([src/netlist/diagnostic.hpp](src/netlist/diagnostic.hpp)).
- Board geometry on Points ([src/board/geometry.hpp](src/board/geometry.hpp)): `Distance`, `NearestT`, `NearestOnSegment`, `SegmentDistance`, `RotatedOffset` (KiCad's rotation). Layers: `LayerName`, `ParseLayer` ("F.Cu", [src/board/board.hpp](src/board/board.hpp)); millimetres: `ParseMillimetres` ([src/board/kicad_board.hpp](src/board/kicad_board.hpp)), `FormatMillimetres`, `FormatPoint` ([src/machine/crack.hpp](src/machine/crack.hpp)).
- An instrument whose tip touches one Net: derive its Consumer from `SingleNetConsumer<State>` ([src/runner/single_net_consumer.hpp](src/runner/single_net_consumer.hpp)). One with Channels: `Rewatch` and `ChannelNets<Mask>` ([src/runner/channel_nets.hpp](src/runner/channel_nets.hpp)).
- Several Nets' recorded Samples in time order: `Recorder::inOrder` ([src/sim/recorder.hpp](src/sim/recorder.hpp)). A TV's Nets on a circuit, and its lamps': `FindTvNets`, `FindLedNets` ([src/session/session.hpp](src/session/session.hpp)). A TV or lamps on a Simulator stepped by hand (`bw run`, a test): `TvInput` ([src/devices/tv_input.hpp](src/devices/tv_input.hpp)), `LedInput` ([src/runner/led_input.hpp](src/runner/led_input.hpp)).
- Mouse gestures in a window ([src/ui/gestures.hpp](src/ui/gestures.hpp)): `TimeCanvas::read` (a canvas over time: zoom, pan, drag, marker), `ReadKnob` (a round knob).
- A visitor from lambdas for `std::visit`: `Overloaded` ([src/machine/repair.hpp](src/machine/repair.hpp)).

## Docs

- Each Machine in its own directory `machines/NNN-name/` (P6a): netlist, `board/`, `manuals/` (the player's), `docs/` (its development docs), `roms/`, `puzzles/` (`NN-name.bwp`, easiest first; `random.bwr`, its Vetting Script: Random faults, P8d). Datasheets, footprints and benchmarks stay shared (`datasheets/`, `footprints/`, `bench/`). Layout and loading rules: [ADD_MACHINE.md](docs/recipes/ADD_MACHINE.md#the-directory).
- Conventions: `docs/GLOSSARY.md`, `docs/STYLE.md`, `docs/NAMING.md`. Structure and roadmap: `docs/ARCHITECTURE.md`. Routing a Board in KiCad (the user's part): `docs/KICAD_GUIDE.md`.
- `README.md` is written from [docs/README_SPEC.md](docs/README_SPEC.md), its pictures by `tools/readme_shots.py` into `docs/readme/`: change the spec, then regenerate.
- `docs/p*-*/` are finished milestones: history only, the code is the truth. Read one only when the task names it.
- The user edits docs in parallel: commit only your own paths.

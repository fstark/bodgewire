Python: the scripts use the standard library only and run with `python3 -I`; `venv/` (git-ignored) is the interpreter the `tests/data/*/fetch.sh` scripts call.

## Build and test

- Release: `cmake --preset release && cmake --build --preset release`; tests `ctest --preset release -j` (a bare `-j`: every core). The release suite is the routine check: about 2½ minutes, and it runs the cases debug skips (the BW-2001's and BW-80's). The full release suite runs before every final commit (a roadmap step's end, a change to shared code). A change that one Part or Machine alone can see runs only the tests that can reach it in between, e.g. `ctest --preset release -j -R "2524|typewriter|catalogue|manuals"`, and the commit message says which.
- Debug (ASan + UBSan): `cmake --preset debug && cmake --build --preset debug`; tests `ctest --preset debug -j`. Only its sanitizers find what release misses (memory errors, undefined behaviour), so use it for changes to C++ logic in the kernel, Parts, builder or devices, and run only the tests that reach the change (`-R`), never the BW-2001 or BW-80 cases routinely: debug is about 0.004× on them. The full debug suite (about 15 minutes, two tennis cases about 14 minutes each) only when the user asks.
- The agent can't see the screen: `build/release/src/ui/bw_ui machines/NNN-name/puzzles/NN-x.bwp --screenshot /tmp/x.png --after 1s`, then view the PNG (it also prints the speed ratio and UI ms); a shot never reads or writes the player's saved progress unless `--progress FILE` is given (P8e), nor the saved text size (16 px unless `--text-size 20|24`), and `--forget` opens the Launcher's forget question. Headless, `build/release/src/cli/bw run FILE --time 1s` shows a Machine's outputs too: `--tv x.bmp` writes the TV's last frame, `--leds` prints each lamp's brightness (`led D1 0.50`, `led DS1.a 1.00`). `bw random machines/NNN-name/puzzles/random.bwr --seed N` makes a Random fault (P8d) and prints its `fault` line, the tries and the seconds; `bw_ui` opens a bench on one with the same file and `--seed`.
- A finger in a shot (P8f): `--touch` (the mouse stands in for one finger), and scripted fingers in window px (the 1280 × 800 window's; a shot is twice that): `--tap X:Y[,X:Y…]`, `--double-tap X:Y`, `--hold X:Y`, `--drag X:Y:DX:DY`, `--pinch X:Y:FACTOR`. The k-th begins 0.3 s + 0.6 s × k after the start: give an `--after` past it (`2s` for one). `--window W:H` is a tablet's window (`1080:810`, or `820:1180` upright); `--one-window` the web build's single window, where a double-click on an object fills the bench.
- Web (P8b): `source ~/emsdk/emsdk_env.sh && cmake --preset web && cmake --build --preset web` (emsdk 7.0.1, `bw_ui` only, no tests); serve `build/web/src/ui/` over HTTP and open `bw_ui.html`. Pushing `main` publishes it on GitHub Pages ([web.yml](.github/workflows/web.yml)); the same run builds the `touch` branch, while there is one, under `/touch/` (after a push of `touch`: `gh workflow run web.yml --ref main`). A change to how `bw_ui` draws needs its web path in `gpu_render_web.cpp` (SDL_Renderer, no pop-outs, no `--screenshot`), and no `try` may enclose a frame loop (Asyncify with wasm exceptions). Build it after such a change.
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
- Gestures in a window ([src/ui/gestures.hpp](src/ui/gestures.hpp)): `TimeCanvas::read` (a canvas over time: zoom, pan, drag, marker and its flag, a finger's hold, a vertical drag where rows scroll), `ReadKnob` (a round knob).
- The controls every window shares ([src/ui/controls.hpp](src/ui/controls.hpp), P8f), one size for the mouse and the finger (`kControl` 28 px, `kBig` 44): `Stepper`, `Choice`, `ChoicesPopup` (a drawn key's choices), `NumberField` and its pad in place of a `Combo`, a slider or an `InputText`, which a finger cannot work; `BigText`; `DragScroll` (a scrolling child dragged); `CornerGrip` and `PinchResize` (last in an object's window).
- A drawn case (the scope, P8h; the analyzer, P8i): its strip's sizes and boxes ([src/view/case_strip.hpp](src/view/case_strip.hpp): `Box`, `BandAt`, `LayOutStepper`, `LayOutBadge`, `SizeKeyBox`); its drawing ([src/ui/case_keys.hpp](src/ui/case_keys.hpp): frame, groups, badge, bezel, keys, LEDs, lamps, rockers, `TimeGrid`); the cursor's tag `DrawCursorTag` ([src/ui/instrument_marks.hpp](src/ui/instrument_marks.hpp)).
- A finger ([src/ui/touch.hpp](src/ui/touch.hpp)): `Touch()` (this frame's `two`, `pinch`, `panX`/`panY`, `hold`, `cancelled`), `FingerPointer()`, `PressCancelled()`, `TouchTypingZone()`, `NoFingerPadding`, `kFingerPadding`. Under it, pure and tested: `Fingers`, `ScriptEvents` ([src/view/fingers.hpp](src/view/fingers.hpp)). A finger changes the input only, never a control, a size or a text.
- An object on the bench ([src/ui/bench.hpp](src/ui/bench.hpp)): `PlaceNext`, `KeepAspect`, `CaseClose`, `TogglePop`, `PopOnDoubleClick`, `FillBenchWith` / `Unfill`; its places and sizes, pure: `FitBench`, `FillBench`, `ReturnPlace`, `NextPreset` ([src/view/bench_places.hpp](src/view/bench_places.hpp)).
- Text on a page (the binder, the Brief): `PushPageFont(PageFace::Text|Bold|Mono, scale)` ([src/ui/page_fonts.hpp](src/ui/page_fonts.hpp)), Atkinson Hyperlegible at the player's reading size (16, 20 or 24 px, the binder's aA key); `PushFixedPageFont` keeps `kPageFontSize` 16 px for what lays out in fixed places (a pinout, a memory chip's contents, the tabs, a `kControl`-high row). Never `ImGui::PushFont(nullptr, FontSizeBase × …)` there, which is ImGui's font. Its digits are proportional: line numbers up in a slot (`CalcTextSize("00")`), not with spaces.
- A visitor from lambdas for `std::visit`: `Overloaded` ([src/machine/repair.hpp](src/machine/repair.hpp)).

## Docs

- Each Machine in its own directory `machines/NNN-name/` (P6a): netlist, `board/`, `manuals/` (the player's), `docs/` (its development docs), `roms/`, `puzzles/` (`NN-name.bwp`, easiest first; `random.bwr`, its Vetting Script: Random faults, P8d). Datasheets, footprints and benchmarks stay shared (`datasheets/`, `footprints/`, `bench/`). Layout and loading rules: [ADD_MACHINE.md](docs/recipes/ADD_MACHINE.md#the-directory).
- Conventions: `docs/GLOSSARY.md`, `docs/STYLE.md`, `docs/NAMING.md`. Structure and roadmap: `docs/ARCHITECTURE.md`. Routing a Board in KiCad (the user's part): `docs/KICAD_GUIDE.md`. Kernel speed (each host's numbers, how to measure, what was kept and dropped, the ideas left): `docs/PERFORMANCE.md`; a kernel speed change updates it.
- `README.md` is written from [docs/README_SPEC.md](docs/README_SPEC.md), its pictures by `tools/readme_shots.py` into `docs/readme/`: change the spec, then regenerate.
- `docs/p*-*/` are finished milestones: history only, the code is the truth. Read one only when the task names it.
- The user edits docs in parallel: commit only your own paths.

# P8d — Procedural Puzzles — Implementation Plan

Spec: [PROCEDURAL_PUZZLES.md](PROCEDURAL_PUZZLES.md). Design: [PROCEDURAL_PUZZLES_DESIGN.md](PROCEDURAL_PUZZLES_DESIGN.md) (its sections are cited as D1 to D9).

## Status

| Phase | Description | Commit |
|---|---|---|
| 0 | The research: the BW-80's sample, `bw run --leds`, every Machine sampled, the spec | `aaf07b1`, `c1f086e`, `7b5197b`, `967390c`, `bd84a32` |
| 1 | A knob set at a time; the inputs out of `bw run` | this commit |
| 2 | The Vetting Script and its fourteen files | |
| 3 | Drawing a Fault | |
| 4 | Vetting, making a Puzzle, `bw random` | |
| 5 | The bench on a Random fault, the "Give up" page | |
| 6 | The Launcher's post-it and the making | |
| 7 | Docs, the release check | |

Every phase adds its sources to its `CMakeLists.txt` (`src/machine`, `src/session`, `src/ui`, `tests`), by hand: never `clang-format` there.

## Phase 1 — A knob set at a time; the inputs out of `bw run`

1. `KnobSetting::at`, `ParseKnob`'s third field and its error text; `tests/machine/knob_setting_test.cpp`'s pinned text (D1).
2. `src/machine/input_edges.{hpp,cpp}`: `InputEdge`, `InputEdges`, `RunTo`, moved from `Run` in `src/cli/main.cpp`; a timed knob is an edge (D1).
3. `Run` uses them; its outputs are unchanged. `kUsage`: `--knob REF:N%[:TIME]`.
4. `ApplyBenchOptions` refuses a timed `--knob` (D1).
5. Tests of D1.

Fence: release, the full suite; `bw run machines/045-knob-meter/knob.bwn --knob J2:42%,J2:87%:400ms --leds` reads 42 at `--time 300ms` and 86 at `--time 700ms` (the Knob meter shows 86 at 87 %); `bw run machines/140-bw80/bw80.bwn --time 300ms --tv x.bmp` gives the frame it gave before the move (`cmp`, the frame taken first). Debug: `ctest --preset debug -j -R "knob setting|input edges|run to|run_leds"`. Commit: `machine: a knob set at a time, bw run's inputs in a library (P8d phase 1)`.

## Phase 2 — The Vetting Script and its fourteen files

1. `QuotedValue` and `IsMachineName` declared in `puzzle.hpp` (D2).
2. `src/machine/vetting_script.{hpp,cpp}` (D2).
3. `MachineDir` of a `.bwr`; `MachineEntry::random`, `EntryRef::random`, `EntryOf`, `ListMachines` (D2).
4. The fourteen `machines/*/puzzles/random.bwr`, as D2's table has them.
5. Tests of D2, `vetting script: every Machine's inputs exist` among them: each file's references are checked here, where the files land.

Fence: release, the full suite (the Launcher shows no new post-it yet: `LauncherLanes` is fed in phase 6). Debug: `-R "vetting script|machine list|machine dir|puzzle: "`. Commit: `machine: a Machine's random.bwr, the Vetting Script (P8d phase 2)`.

## Phase 3 — Drawing a Fault

1. `FormatFault` (D3).
2. `Machine::check`, `Machine::clearFaults` (D3).
3. `src/machine/fault_draw.{hpp,cpp}` (D3).
4. Tests of D3.

Fence: release, the full suite. Debug: `-R "puzzle: |machine: (check|clearFaults)|fault draw"`. Commit: `machine: a Fault drawn at random, a Fault's line written back (P8d phase 3)`.

## Phase 4 — Vetting, making a Puzzle, `bw random`

1. `src/session/vetting.{hpp,cpp}` (D4).
2. `src/session/procedural_puzzle.{hpp,cpp}` (D5).
3. `bw random`, `tests/bw_random.cmake`, and in `tests/CMakeLists.txt` the `foreach` that adds one `procedural puzzle: NNN-name` ctest a Machine (D5).
4. Tests of D4 and D5.
5. D8: `bw random` on every Machine, seeds 1 to 20: the tries, the kinds, the golden's and the making's seconds into this plan's notes, beside the samples' numbers; a file that needs it corrected (the knobs' later positions). The wait on the BW-2001 and the BW-80 against D8's estimate.

Fence: release, the full suite; the fourteen `procedural puzzle: NNN-name` ctests' times noted (about 50 s summed, 12 s at most for one). Debug: `-R "vetting:|procedural puzzle: (five|the first|stopped|the same|it shows|010|020|030|040|045|050|080)|bw_random"` (the other Machines' ctests skip themselves in debug). Commit: `session: Vetting, a Procedural Puzzle made, bw random (P8d phase 4)`.

## Phase 5 — The bench on a Random fault, the "Give up" page

1. `Load` on a `.bwr`, `OpenedMachine::fault`; `main.cpp`: a `.bwr` with no Fault made (the terminal, `--visit`) has one made with `--seed`; the title kept (D6). No Next on the card yet.
2. `Divider::GiveUp`, `TabTops`, the tab and its page, the copy; `ManualWindows::draw`'s `fault` (D6).
3. `--seed`, `--show-fault`, `--manual giveup`, their refusals; the usage pinned by `tests/view/bench_options_test.cpp` (D6).
4. Tests of D6.

Fence: release, the full suite; screenshots viewed: the Counter's bench and its Brief, the page before and after `--show-fault`, the Message's binder with six tabs at `--binder-height`'s smallest, the BW-2001's bench; the Counter's with the `--repair` its Fault calls for at `--after 4s` (the card, no Next yet, the post-it's tick). Debug: `-R "bench options|binder"`, and debug `bw_ui machines/010-counter/puzzles/random.bwr --seed 1 --screenshot`. Web build. Commit: `ui: a Random fault's bench, its Brief, the binder's Give up page (P8d phase 5)`.

## Phase 6 — The Launcher's post-it and the making

1. `LauncherLanes` (D7).
2. `src/session/puzzle_maker.{hpp,cpp}` (D7).
3. The Launcher: the post-it and its "?", its place in the row and the row's scroll, `select` on a `.bwr`, the live picture, the making and its lines, Esc, `LauncherChoice`, `make()` (D7).
4. `ThankYouCard::draw`'s label; `main.cpp`: the choice to `Load`, the card's "Another random fault" back through the Launcher's making, no bench opened under `--screenshot`; `--make` (D7).
5. Tests of D7.

Fence: release, the full suite; screenshots viewed: the post-it selected (with `kPictureQuestion` on and off: both kept for the user), a line while making (`--make --after 0.5s`), the Counter's card with "Another random fault"; the Launcher's UI ms no higher than before. Debug: `-R "launcher lanes|puzzle maker|bench options"`, and debug `bw_ui --select machines/010-counter/puzzles/random.bwr --make --screenshot … --after 0.5s`. Web build; the page loaded in Chrome through Playwright, the post-it in the Launcher. Commit: `ui: the Launcher's Random fault post-it, a Puzzle made while lines loop (P8d phase 6)`.

No flag clicks the card or presses Esc: "Another random fault" played, Esc during a making, and a Random fault made in the browser are the user's checks, listed in the report at the end. No fence waits for them.

## Phase 7 — Docs, the release check

1. D9: GLOSSARY, the recipes, AGENTS.md, ARCHITECTURE (the row done), README_SPEC, `tools/readme_shots.py`, `README.md` and its pictures regenerated; the spec's Tests and Open brought up to what was built and measured.
2. Every screenshot of the design's Checks again, viewed; `bw random`'s table again (D8) against phase 4's.
3. The full release suite. This plan cut to the status table and the notes.
4. The report to the user: what only they can check (the design's Checks, last item), with the two screenshots of the "?" over the picture.

Fence: the full release suite; the web build. Commit: `docs: P8d done, Procedural Puzzles in the glossary, recipes and README (P8d phase 7)`.

## Implementation notes

- 2026-11-24, the review (a fresh agent, the three docs only), its must-fixes, all in the docs before phase 1:
  1. `Capture` is the Logic Analyzer's type (`src/instruments/logic_analyzer.hpp`) and a GLOSSARY term: what a Machine shows at a capture time is a `Shown`.
  2. The every-Machine test was one case nobody could run a Machine at a time: one ctest a Machine, the slow ones skipping themselves in debug, the debug command written out.
  3. Phase 1's fence read 87 on the Knob meter at 87 %: it shows 86.
  4. The binder's smallest height cannot be reached by a flag, and six tabs need a ROM: the tabs' layout is a tested function (`TabTops`), the screenshot is the Message's with `--contents`.
  5. Phase 6's fence had checks no flag can make (the card clicked, Esc, the browser): they are the user's, and `PuzzleMaker` moved to `src/session` so its stopping is unit tested.
  - Also from it: `RunTo`'s `slice` can stop a run and `Vetting::Make` can be stopped; the watched outputs are decided once on the fault-free build; an exception in a try counts as not showing; the fourteen files written out in D2 with their presses; `QuotedValue` and `IsMachineName` shared; the card's "Another random fault" moved to phase 6, where the Launcher can make one; `--make` under `--screenshot`; the lines' three dots typed as dots; `kPictureQuestion` and `sign` (Mark alone is a Pencil Mark's word).
- 2026-11-24, phase 1: as designed. `RunTo` also applies an edge whose time is already past (a caller that starts late), where `bw run`'s loop would have asked the Simulator to run backwards. Checks: release, the full suite (1583); `bw run`'s BW-80 frame, the Dice's lamps and the Typewriter's typed frame the same before and after the move; the Knob meter reads 42 at 300 ms and 86 at 700 ms with `--knob J2:42%,J2:87%:400ms`; `bw_ui --knob J2:42%:400ms` refused. Debug: `knob setting|input edges|run to|run_leds`.
- 2026-11-24, the user, after the docs: the wait on the slow Machines is not a problem at this stage; nothing is built for it (the spec's Open).
- 2026-11-24, phase 0: the research, before this plan. `bw run --leds` (`c1f086e`) and `LedInput`, `FindLedNets` are in; `tools/fault_sample.py` holds the samples' method and stays as it is.

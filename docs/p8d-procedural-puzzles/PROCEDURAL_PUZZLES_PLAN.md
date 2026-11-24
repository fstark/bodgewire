# P8d — Procedural Puzzles — Implementation Plan

Spec: [PROCEDURAL_PUZZLES.md](PROCEDURAL_PUZZLES.md). Design: [PROCEDURAL_PUZZLES_DESIGN.md](PROCEDURAL_PUZZLES_DESIGN.md) (its sections are cited as D1 to D9).

## Status

| Phase | Description | Commit |
|---|---|---|
| 0 | The research: the BW-80's sample, `bw run --leds`, every Machine sampled, the spec | `aaf07b1`, `c1f086e`, `7b5197b`, `967390c`, `bd84a32` |
| 1 | A knob set at a time; the inputs out of `bw run` | `3e936ae` |
| 2 | The Vetting Script and its fourteen files | `2029e8d` |
| 3 | Drawing a Fault | `f65927b` |
| 4 | Vetting, making a Puzzle, `bw random` | `1317e49` |
| 5 | The bench on a Random fault, the "Give up" page | `7a3c502` |
| 6 | The Launcher's post-it and the making | `7dac4f6` |
| 7 | Docs, the release check | `f186012` |

## Implementation notes

- 2026-11-25, phase 7: GLOSSARY (Procedural Puzzle, Vetting, Vetting Script new, the Launcher, the Brief, the Thank-you Card, the binder under Equipment, Knob, Capture), ADD_MACHINE (the Vetting Script's step), ADD_PUZZLE (a new kind of Fault: `FormatFault`, its draw), AGENTS.md (the helpers, `bw random`), ARCHITECTURE (the row done, a pointer under Faults), README and its spec (one sentence, the Launcher's picture regenerated with the post-it), the spec's Tests and Open as built and measured. Checks: every screenshot of the design's Checks taken again; `bw random`'s 280 Faults again, each the same as phase 4's with the same tries; release, the full suite (1650); the web build current. This plan cut to its table and notes.

  For the user, none blocking:
  - the words: the nine waiting lines (`src/ui/launcher.cpp`, `kMakingLines`), the Brief's sentence (`src/ui/loaded.hpp`), each Machine's `task` (its `puzzles/random.bwr`), the Give up page's (`src/ui/manual_window.cpp`);
  - the "?" over the live picture, on or off (`kPictureQuestion`; the four pictures were sent);
  - played by hand: a few Random faults on several Machines, the card's "Another random fault", Esc during a making on the BW-2001;
  - the web page: the post-it, a making in a browser on the Counter, the BW-2001 and the BW-80, its seconds;
  - the wait on the BW-2001 and the BW-80 (7 to 10 s), once felt;
  - not this step's: the signed overflow debug `bw_ui` reports at `src/view/flip_corner.cpp:19` on every bench.
- 2026-11-24, the review (a fresh agent, the three docs only), its must-fixes, all in the docs before phase 1:
  1. `Capture` is the Logic Analyzer's type (`src/instruments/logic_analyzer.hpp`) and a GLOSSARY term: what a Machine shows at a capture time is a `Shown`.
  2. The every-Machine test was one case nobody could run a Machine at a time: one ctest a Machine, the slow ones skipping themselves in debug, the debug command written out.
  3. Phase 1's fence read 87 on the Knob meter at 87 %: it shows 86.
  4. The binder's smallest height cannot be reached by a flag, and six tabs need a ROM: the tabs' layout is a tested function (`TabTops`), the screenshot is the Message's with `--contents`.
  5. Phase 6's fence had checks no flag can make (the card clicked, Esc, the browser): they are the user's, and `PuzzleMaker` moved to `src/session` so its stopping is unit tested.
  - Also from it: `RunTo`'s `slice` can stop a run and `Vetting::Make` can be stopped; the watched outputs are decided once on the fault-free build; an exception in a try counts as not showing; the fourteen files written out in D2 with their presses; `QuotedValue` and `IsMachineName` shared; the card's "Another random fault" moved to phase 6, where the Launcher can make one; `--make` under `--screenshot`; the lines' three dots typed as dots; `kPictureQuestion` and `sign` (Mark alone is a Pencil Mark's word).
- 2026-11-25, phase 6: as designed, but:
  - `PuzzleMaker` keeps a stopped making until its thread has ended (it is asked to stop, never waited for); the destructor joins them all. A new `start` never blocks on the one before.
  - The waiting line is on a dark plate in the middle of the dimmed wood, at 1.6 times the text's size, with "Making a random fault. Esc stops." under it: over the bare dimming the post-its' text showed through.
  - `Launcher::holdChoices()` is `--screenshot`'s rule: no choice is returned and a made Fault's line stays up. `Launcher::make(seed)` takes `--seed` with `--make`.
  - `ParseOptions`: `--make` needs `--select` of a `.bwr`; `--seed` is taken with it.
  - `kPictureQuestion` is on (a "?" at 18 % white over the live picture, 80 % of its height).

  Checks: release, the full suite (1650); screenshots viewed: the post-it selected on the Counter and on the BW-2001 (its row scrolled to the end, the working Machine live), each with `kPictureQuestion` on and off (the four kept for the user), a line while making (`--make --seed 1 --after 0.5s`), the Counter's card with "Another random fault", `--visit` of the Counter's `random.bwr` (the bench opened on a made Fault, then the Launcher back on its post-it); the Launcher's UI 0.1 to 0.4 ms, as before. Debug: `launcher lanes|puzzle maker|bench options`, and `bw_ui --select … --make` (no report but `flip_corner`'s). Web build: links, its data holds the fourteen `random.bwr`.

  Not done: the page loaded in Chrome through Playwright. Its browsers are cached on this Mac but its package is not installed, and installing one is the user's call: the web check is the user's, with the others.
- 2026-11-25, phase 5: as designed, but:
  - `MakeProceduralPuzzleFor(script, seed, roms, stop)` (`src/session/procedural_puzzle`) is the whole making for a file (read, the Machine loaded, vetted, made): `main()` calls it for a `.bwr` from the terminal or visited, phase 6's `PuzzleMaker` will on its thread.
  - `--seed`, `--show-fault` and `--manual giveup` are refused by `ParseOptions`, not `ApplyBenchOptions` (which sees the Session, not the file): a usage error as the other bad combinations. `--seed` needs a `.bwr` as the file or among `--visit`'s.
  - `TabTops(count, top, bottom, tabHeight, step)`: the bottom the last tab must end above, not the binder's height.
  - The texts are constants in `src/ui/loaded.hpp` (`kRandomFaultTitle`, `kRandomFaultBrief`); the page says "This board has one fault, picked at random. Nobody wrote it down, but this page can tell you what it is. Once shown, it stays shown.", then "The fault on this board:", the line, "A click on the line copies it." ("Copied."), "The board can still be repaired.": the user's to rewrite with the others.
  - The usage starts `bw_ui [file.bwn|file.bwp|random.bwr]`.

  Checks: release, the full suite (1640); screenshots viewed: the Counter's bench and its Brief, the Dice's with "Hold ROLL, then let go.", the page before and after `--show-fault`, the Message's binder at `--binder-height 70%` with its six tabs (`--contents U14`), the Counter's with `--repair unbridge:U1.6:U1.7` at 4 s (the card, the tick, no Next yet), the BW-2001's bench (seed 1: a dead screen). Debug: `bench options|binder|procedural puzzle: (made|a bad)`, and `bw_ui` on the Counter's `random.bwr` with the page shown. Web build: links.

  Found, not this step's: debug `bw_ui` reports a signed overflow at `src/view/flip_corner.cpp:19` (`OnFlipCorner`, P6o) on every bench, an Authored Puzzle's too. Left as it is, reported to the user.
- 2026-11-25, phase 4: as designed. `Vetting::Make` also refuses a Machine with no TV and no lamp; `Shows` takes two `Shown`s of different sizes for a difference. No file needed correcting: every golden has its picture at each capture time, the knobs' later positions included. Checks: release, the full suite (1631; the fourteen `procedural puzzle: NNN-name` ctests 0.3 s to 9.4 s each, 36 s summed, in parallel). Debug: `vetting:|procedural puzzle: (five|the first|stopped|the same|it shows|010|020|030|040|045|050|080)|bw_random` (43 s).

  D8, `bw random` on every Machine, seeds 1 to 20, six at a time (2026-11-25): all 280 made.

  | Machine | Tries, mean (most) | The golden's run | A making, median (worst) | Kinds made |
  |---|---|---|---|---|
  | 010 Counter | 2.8 (6) | 0.1 s | 0.3 s (0.8) | crack 7, stuck 6, bridge 5, joint 2 |
  | 020 LED chaser | 1.9 (6) | 0.1 s | 0.2 s (0.7) | stuck 7, crack 6, bridge 4, joint 3 |
  | 030 Dice | 2.4 (7) | 0.1 s | 0.2 s (2.5) | crack 7, stuck 7, joint 4, bridge 2 |
  | 040 Stopwatch | 1.7 (5) | 0.1 s | 0.2 s (1.0) | stuck 8, crack 5, joint 4, bridge 3 |
  | 045 Knob meter | 1.5 (5) | 0.1 s | 0.1 s (0.4) | stuck 7, bridge 5, crack 5, joint 3 |
  | 050 Test pattern | 1.3 (3) | 0.1 s | 0.1 s (0.3) | stuck 7, bridge 5, crack 5, joint 3 |
  | 060 Bouncing ball | 1.4 (4) | 0.6 s | 0.9 s (2.6) | stuck 7, bridge 5, crack 5, joint 3 |
  | 070 Tennis | 1.6 (5) | 0.9 s | 1.9 s (5.7) | stuck 8, crack 5, joint 4, bridge 3 |
  | 080 Message | 1.4 (4) | 0.1 s | 0.3 s (0.7) | stuck 10, crack 8, joint 1, bridge 1 |
  | 090 Typewriter | 1.2 (2) | 2.2 s | 4.5 s (7.1) | stuck 9, crack 5, bridge 2, joint 2, column 1, switch 1 |
  | 110 LED computer | 1.4 (4) | 0.3 s | 0.4 s (1.5) | stuck 9, crack 5, bridge 2, joint 2, column 1, switch 1 |
  | 120 BW-1 | 1.6 (4) | 0.5 s | 0.7 s (1.8) | stuck 9, crack 5, bridge 2, joint 2, column 1, switch 1 |
  | 130 BW-2001 | 1.3 (2) | 4.5 s | 9.6 s (14.4) | stuck 10, crack 8, joint 1, bridge 1 |
  | 140 BW-80 | 1.3 (4) | 5.9 s | 7.3 s (23.5) | stuck 10, crack 8, joint 1, bridge 1 |

  Against the samples: fewer tries than their show rates gave (1.2 to 2.8 a Puzzle, where 37 % to 81 % showing would give 1.2 to 2.7): the game's even draw over pins and the cell margin change little. Alone on the machine (seed 1, not six at a time) a making is 7.6 s on the BW-2001 and 5.0 s on the BW-80, the golden 3.6 s and 4.9 s of it: the spec's estimate (10 s and 9 s) holds as an upper mark. The same seed draws the same kinds on two Machines that have the same kinds: one seed a making in the game (`std::random_device`), so it does not show. No cell was made in 280: its 3 % and the five tries make it as rare as wanted.
- 2026-11-25, phase 3: as designed. A Bridge needs no chip-pin count from the Board: pin `k` of a chip with a pin `k + 1` in its `Component::pins()`. A Crack whose two draws fall on one pad is drawn again, as a refused one. `DrawKind` and `DrawFault` list the Machine's objects at each call (20 ms a thousand on the BW-1 in release: nothing beside a try). Checks: release, the full suite (1599). Debug: `puzzle: |machine: (check|clearFaults)|fault draw` (29 s, the BW-1's cracks).
- 2026-11-25, phase 2: as designed, but `machine` need not be the first line (as in a `.bwp`: one `machine` line, anywhere). Only the file named `random.bwr` is read; another `.bwr` in `puzzles/` is ignored. `VettingScript` keeps its `machine` and `task` line numbers for the list's problems. The `every Machine's inputs exist` case also checks that no input starts after the last capture time (a release may: the Dice's) and that a `task` is there exactly when the script has inputs. Until phase 5 a `.bwr` given to `bw_ui` is a load error, and `--select` of one selects its Machine's card. Checks: release, the full suite (1590). Debug: `vetting script|machine list|machine: MachineDir|puzzle: `.
- 2026-11-24, phase 1: as designed. `RunTo` also applies an edge whose time is already past (a caller that starts late), where `bw run`'s loop would have asked the Simulator to run backwards. Checks: release, the full suite (1583); `bw run`'s BW-80 frame, the Dice's lamps and the Typewriter's typed frame the same before and after the move; the Knob meter reads 42 at 300 ms and 86 at 700 ms with `--knob J2:42%,J2:87%:400ms`; `bw_ui --knob J2:42%:400ms` refused. Debug: `knob setting|input edges|run to|run_leds`.
- 2026-11-24, the user, after the docs: the wait on the slow Machines is not a problem at this stage; nothing is built for it (the spec's Open).
- 2026-11-24, phase 0: the research, before this plan. `bw run --leds` (`c1f086e`) and `LedInput`, `FindLedNets` are in; `tools/fault_sample.py` holds the samples' method and stays as it is.

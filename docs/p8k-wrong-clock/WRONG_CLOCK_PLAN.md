# P8k — Wrong Clock — Implementation Plan

Spec: [WRONG_CLOCK.md](WRONG_CLOCK.md). Design: [WRONG_CLOCK_DESIGN.md](WRONG_CLOCK_DESIGN.md).

## Status

| Phase | Description | Commit |
|---|---|---|
| 1 | The Fault kind: `fault clock`, the Oscillator's new period, swap and `completed()` | |
| 2 | The three early Puzzles: Counter, Stopwatch, Knob Meter at 3 MHz | |
| 3 | The four TV Puzzles: Message, Typewriter, BW-2001, BW-80 slowed | |
| 4 | Docs and the release check | |

## Phase 1 — The Fault kind

1. `Oscillator::setPeriod`, `period()` (design §1); `oscillator: setPeriod`.
2. `WrongClock`, `BuildFaults::clocks`, the `Builder` loop after the switches, the include (§1); `builder: a Wrong Clock reaches the oscillator`.
3. `ClockDecl` last in `FaultDecl`, the parser's `fault clock` line, `clock` in the catch-all error, `FormatFault` (§1); `puzzle: clock`, the clock line in `puzzle: FormatFault of each kind`, the catch-all text in `puzzle: errors`.
4. `FaultSet::clocks`, `Add`, `resolve(const ClockDecl&)` (the frequency's error returned), `swap`, `completed()`, `build()`, the comments (§1); `machine: clock fault errors`, `machine: a swap removes a Wrong Clock`, the clock in `machine: completed needs every Component and Joint Fault gone`.
5. `procedural_puzzle_test.cpp`'s `Repair`: a `ClockDecl` swapped.

Fence: debug `ctest --preset debug -j -R "^(oscillator|builder|puzzle|machine): |^procedural puzzle: [a-z]"`; release, the full suite (shared code). Commit: `machine: Wrong Clock, a can at another frequency (P8k phase 1)`, its body naming the checks.

## Phase 2 — The three early Puzzles

1. `04-quick-blink.bwp`, `09-fast-seconds.bwp`, `08-three-laps.bwp` by ADD_PUZZLE.md (spec §1–§3, design §2); each tried with `bw run` (`--leds`, `--knob`, `--press`, `--record X1.OUT --vcd FILE` for the period, `--repair swap:X1`) before its test; seeds 1–3.
2. Their cases (design §2 table) and names in each `Puzzles have a title and a brief`; `puzzle: every Puzzle's faults format back` 121 → 124; `machine list: the repository` with the Counter's 4 and its fourth title.
3. Their rows and counts in `COUNTER_DESIGN.md`, `STOPWATCH_DESIGN.md`, `KNOB_METER_DESIGN.md` and in `COUNTER.md`, `STOPWATCH.md`, `KNOB_METER.md`.
4. Speed ratios (`bw run FILE --time 2s`, release) in the notes; each at least 2.5×.
5. Screenshots (release, viewed): each bench (`bw_ui FILE --screenshot /tmp/x.png --after 2s`); the Counter's with `--hook X1.OUT`, the scope's band reading 3 MHz.

Fence: release `ctest --preset release -j -R "counter|stopwatch|knob|machine list|repair validation|puzzle"`; the screenshots viewed. Commit: `puzzles: Quick blink, Fast seconds, Three laps: a can three times too fast (P8k phase 2)`.

## Phase 3 — The four TV Puzzles

1. `07-slanted-page.bwp`, `07-giant-letters.bwp`, `17-slanted-banner.bwp`, `16-stray-cursor.bwp` (spec §4–§7, design §2); each tried with `bw run --tv` on seeds 1–3, its frame viewed against the spec's; the BW-80's brief written from `bw_ui`.
2. Their cases (the BW-2001 and BW-80 ones release only; the BW-80 on seed 1 with its own loop past 1 s) and names in each `Puzzles have a title and a brief`; `puzzle: every Puzzle's faults format back` 124 → 128; `machine list: the repository` with the BW-2001's 17.
3. Their rows and counts in `MESSAGE_DESIGN.md`, `TYPEWRITER_DESIGN.md`, `BW2001_DESIGN.md`, `BW80_DESIGN.md` and in `MESSAGE.md`, `TYPEWRITER.md`, `BW2001.md`, `BW80.md`.
4. Speed ratios in the notes.
5. Screenshots (release, viewed): each bench (the BW-2001's and BW-80's with an `--after` long enough at their speed to show the picture).

Fence: release `ctest --preset release -j -R "message|typewriter|bw80|bw2001|machine list|repair validation|puzzle"`; the screenshots viewed. Commit: `puzzles: Slanted page, Giant letters, Slanted banner, Stray cursor: a slow can (P8k phase 3)`.

## Phase 4 — Docs and the release check

1. ADD_PUZZLE.md, GLOSSARY.md, README_SPEC.md then README.md (counts and the fault kinds), the ARCHITECTURE row set to done (design §4).
2. A Launcher screenshot on a new post-it (`bw_ui --select machines/010-counter/puzzles/04-quick-blink.bwp --screenshot /tmp/x.png --after 2s`), viewed.
3. The full release suite.
4. This plan cut to the status table and the notes.

Fence: the full release suite; the screenshot viewed. Commit: `docs: Wrong Clock in the recipes, glossary and roadmap (P8k phase 4)`.

## Implementation notes

- 2026-11-28, review (fresh sub-agent, the three docs only). Must-fixes applied: the Stopwatch has no minutes (0.00–9.99): `fast_minutes` → `fast_seconds`, its symptom and check through `StartStop`; the hard-coded counts in `puzzle: every Puzzle's faults format back` (121) and `machine list: the repository` (Counter 3, BW-2001 16) added to phases 2 and 3; `resolve(ClockDecl)` returns the frequency's error, since `inject` takes Decls the parser never saw; the Stopwatch at 3 MHz runs 3.7×, under the 4× bar chosen without asking: the bar is now 2.5× natively (above real time on the web), keeping the user's 3×. Should-fixes applied: the catch-all parser error; the Counter's check (two `C19` changes in 400 ms, `CARRY1` in whole ps); the wrong swaps named from each Troubleshooting chart (Message and Typewriter `U1`); the BW-80 case's own loop past 1 s on seed 1, its picture described as the review saw it (a K about twice its size, flickering), the file renamed `big-cursor`; the debug fence keeping the Vetting cases out; the Machines' `NAME.md`; the README's per-Machine counts and fault kinds; the recipe's missing steps named; titles and briefs drafted in the spec; `LedComputer()` for the machine cases; §6 and §7 in directory order.
- 2026-11-28, phase 1: as designed. `Oscillator`'s constructor now calls `setPeriod`. `machine: clock fault errors` also checks `Machine::check` on the LED Computer's netlist; `machine: a swap removes a Wrong Clock` reads the built can's period before and after the swap and after `undoRepairs`. Checks: debug `-R "^(oscillator|builder|puzzle|machine): |^procedural puzzle: [a-z]"` (111 passed); the full release suite (1747 passed, 76 s).
- 2026-11-28, phase 2: as designed, the briefs as drafted. A third hard-coded test the review missed: `machine list: next puzzle` wanted nothing after the Counter's `03-too-fast` (now `04-quick-blink`, then nothing). The Counter's case records `X1.OUT`, not `CLK`: the file's `PinOf` reads any `C…` name as a counter bit. The Stopwatch reads exactly `3.00` one second after Start. Speed (`bw run --time 2s`): Counter 4.0×, Stopwatch 3.6×, Knob Meter 5.9×. Screenshots viewed: the three benches (each can marked `1.000MHz`), the Counter's with `--hook X1.OUT` (`3 MHz · 50 %`). Checks: release `-R "counter|stopwatch|knob|machine list|repair validation|puzzle"` (112 passed).
- 2026-11-28, phase 3: the BW-80 Puzzle is Stray cursor (`16-stray-cursor.bwp`), not Big cursor: on the bench the K is its usual size, alone in the middle of a white screen with a dark band at the bottom, and two shots 0.3 s apart show the same picture. The Typewriter's letters fill the screen at twice their size, not "a few huge letters": its brief rewritten. The TV cases check the can's period through a new `CanPeriod` (`tests/machine/test_points`); the Message and Typewriter cases see no locked frame in a second on seeds 1–3. Speed (`bw run --time 1s`/`2s`, seeds 1–3): Message 2.8–2.9×, Typewriter 2.5–2.6×, BW-2001 0.42–0.43×, BW-80 0.42–0.56×. Case times: Message 1.4 s, Typewriter 1.9 s, BW-2001 3.5 s, BW-80 8.9 s. Screenshots viewed: the four benches (the Message's and BW-2001's as the spec's frames). Checks: release `-R "message|typewriter|bw80|bw2001|machine list|repair validation|puzzle"` (216 passed).

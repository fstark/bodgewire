# P4g Ball Parts — Implementation Plan

Done 2026-11-09, user check pending.

Design: [BALL_PARTS_DESIGN.md](BALL_PARTS_DESIGN.md)
Spec: [BALL_PARTS.md](BALL_PARTS.md); Machine: [BOUNCING_BALL.md](../machines/060-bouncing-ball/BOUNCING_BALL.md), [BOUNCING_BALL_DESIGN.md](../machines/060-bouncing-ball/BOUNCING_BALL_DESIGN.md)

Each phase ends with its fence: the checks listed, `cmake --build --preset debug` and `ctest --preset debug -j 12` green, `clang-format` on the C++ files touched, then one commit with the message given (plus the attribution line). Only this step's paths are committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | 74LS27 gate package, Datasheet, tests | 09fbb1a |
| 2 | 74LS93 Part, Datasheet, tests | 079601f |
| 3 | 74LS107 Part, Datasheet, tests | 4b7cf99 |
| 4 | Ball netlist, manuals, headless fault-free tests, speed measured | 174aa85 |
| 5 | Ball Board (user layout), per-Machine loops | c89e3f0 |
| 6 | Ball Puzzles + Symptom/Repair tests | 58b1aec |
| 7 | Session test, release check, recipes, docs | this commit |

## Implementation notes

- Docs review (before phase 1): `VRCO` glitched 23 ns at the start of lines 255 and 511 (`LINE_END` on `ENT` reaches `RCO` combinationally), toggling `BV8` early; `LINE_END` moved to `ENP` of `U20`/`U21`, loads became 195/203, and ADD_MACHINE gained the gotcha.
- Phase 3: Catch2 `SECTION`s inside a `for` loop replaced by `GENERATE` (sections in loops are unreliable).
- Phase 4: the netlist worked as first written: box, dashed net and ball on the first `bw run --tv`, no warnings. Release headless: 2.12× real time (78 M evaluations per simulated 2 s, 3.2× the 050 test pattern's); gate 1.5× passed.
- Phase 4: the TV draws line 0 32 columns early (the line after vertical sync free-runs until the next horizontal sync), on 050 too: the top wall's first row is shifted. `Read()` counts it as a wall row and ignores its stray cells.
- Phase 4: a seed's ball can stay out of sight for ~0.4 s and turn before the first clear step, so the bounce case runs 2 s. One case per seed (about 60 s each in debug, run in parallel): the debug suite went from 40 s to 104 s.
- Phase 5: the user's layout passes `check-board` and DRC first time. `VLOAD_N` is routed `U24` pin 6 → `U21` pin 9 → `U20` pin 9 as asked, but `V2` runs `U3` pin 12 → `U15` pin 13 → `U7` pin 4 → `U8` pin 5: `U15` pin 13 is in the middle, so no crack cuts it off alone. The user chose to change the Solid net Puzzle rather than the layout (phase 6). UI at idle: 1.00×, 0.5 ms per frame without instruments; a measurement taken while the debug suite ran said 0.8×.
- Phase 6: Solid net became Wide net (user's choice, 2026-11-09): a crack on the two-pad `NET_A` (`U12` pin 8 → `U15` pin 9) leaves `NET_N` decoded from `H4`, `H5` and `V2`: a dashed band 16 dots wide. It is still a cracked input that only changes the picture. Both cracks at 50%.
- Phase 6: with `VLOAD_N` cracked, the ball never shows in two frames in a row (once every 512 lines, i.e. every 1.64 frames); the test counts jumps between successive sightings. The slow Symptom and Repair runs are separate cases (2 s each) so ctest runs them in parallel: the debug suite takes 108 s (was 104 s).
- Phase 7: `session: ball with every instrument` (8 Clips, 2 Hooks, probe, meter lead; TV locked after 1 s) passes. Release `bw_ui` on each of the five Puzzles with every instrument attached: 1.00×, UI 0.7–0.8 ms; screenshots viewed. Recipes: ADD_PART (74LS27, 74LS107 references; `GENERATE` for units), ADD_MACHINE (060 reference commits; read the routing before the Puzzles; measure speed on an idle machine).

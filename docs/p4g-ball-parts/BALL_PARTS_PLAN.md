# P4g Ball Parts — Implementation Plan

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
| 6 | Ball Puzzles + Symptom/Repair tests | this commit |
| 7 | Session test, release check, recipes, docs | |

## Phase 1 — 74LS27

Design §2. ADD_PART.md "Gate package".

- `gate_package.cpp` names, units, spec row; `delays::kLs27`; catalogue row; header comment.
- `datasheets/74LS27.md`.
- `gate_package_test.cpp`: `kLs27` among the looped parts.

Fence: `[parts]` and `manuals: every catalogue Part has a Datasheet` pass.
Commit: `parts: phase 1 — 74LS27`

## Phase 2 — 74LS93

Design §3. ADD_PART.md, "Which kind" (chip with state) and "Files".

- `src/parts/ls93.{hpp,cpp}`, `delays::kLs93*`, catalogue row, `src/parts/CMakeLists.txt`.
- `kOffCornerPower` in `catalogue_test.cpp`.
- `datasheets/74LS93.md`.
- `tests/parts/ls93_test.cpp` (design §8.1), in `tests/CMakeLists.txt`.

Fence: `ls93_test`, `catalogue_test`, manuals pass.
Commit: `parts: phase 2 — 74LS93`

## Phase 3 — 74LS107

Design §4. ADD_PART.md, "Which kind" (chip with state), "Files", and "Rules and gotchas" (falling-edge clocks).

- `src/parts/ls107.{hpp,cpp}`, `delays::kLs107`, catalogue row, CMake.
- `datasheets/74LS107.md`.
- `tests/parts/ls107_test.cpp` (design §8.1).

Fence: `ls107_test`, `catalogue_test`, manuals pass.
Commit: `parts: phase 3 — 74LS107`

## Phase 4 — Netlist, manuals, fault-free tests

ADD_MACHINE.md step 2; BOUNCING_BALL_DESIGN.md Netlist and Manuals.

- `netlists/ball.bwn` exactly as the Machine design (header comment: Machine 060, spec path, summary).
- `manuals/ball/user.md`, `service.md`; `"ball"` in `tests/machines/manuals_test.cpp`.
- `tests/machines/ball_test.cpp` with `Watch`, `Read`, and the fault-free cases of design §8.2 (`LoadNetlistFile` until the Board exists); `bw run netlists/ball.bwn --time 1s --tv /tmp/ball.bmp` in debug, picture viewed.
- Speed: release `time build/release/src/cli/bw run netlists/ball.bwn --time 2s`; note the ratio. Below 1.5× real time headless, stop and tell the user before the Board (the UI needs headroom for 1.00× with instruments).

Fence: the netlist loads without warnings; the fault-free cases pass for both seeds; the picture viewed shows box, net and ball; the speed is noted below.
Commit: `ball: phase 4 — netlist, manuals, fault-free test`

## Phase 5 — Board

ADD_MACHINE.md step 3; BOUNCING_BALL_DESIGN.md Board.

- Agent: `bw export-kicad netlists/ball.bwn -o boards/ball/ball.net`; hand the user the layout constraints of the Machine design (three rows by job, `J1` left, `J2` back; `V2` with `U15` pin 13 at an end of its route; `VLOAD_N` with `U24` pin 6 at an end; straight stretches next to both). **Wait for the user's layout.**
- `bw check-board netlists/ball.bwn boards/ball/ball.kicad_pcb` → `ok`; commit `ball.kicad_pro`, `ball.kicad_pcb` only.
- `"ball"` in the per-Machine loops (`boards_test`, `electrical_test`, `board_scene_test`); `ball_test.cpp` on `LoadNetlistMachine`.
- Release screenshot `bw_ui netlists/ball.bwn --screenshot /tmp/ball.png --after 1s`, viewed: the Board drawn, the ball on the TV.

Fence: `drc_ball` and the per-Machine loops pass.
Commit: `ball: phase 5 — Board, per-Machine loops`

## Phase 6 — Puzzles

ADD_PUZZLE.md; BOUNCING_BALL_DESIGN.md Puzzles.

- Read the routed `V2` and `VLOAD_N` segments in `ball.kicad_pcb`; pick the crack pads and percentages; fill them into the Machine design.
- `puzzles/wide_net.bwp`, `wraparound.bwp`, `stripe.bwp`, `jumping_ball.bwp`, `net_bounce.bwp`.
- `WideNetCrack()`, `JumpingBallCrack()` in `tests/machine/test_points.{hpp,cpp}`.
- The Puzzle cases of design §8.2; seeds and time windows set from a first run, noted below.

Fence: each Puzzle shows its Symptom and is fixed by its Repair; the debug suite time noted.
Commit: `ball: phase 6 — Puzzles`

## Phase 7 — Session test, release check, docs

ADD_MACHINE.md step 5.

- `session: ball with every instrument` (design §8.3).
- Release: `bw_ui puzzles/<each>.bwp --screenshot /tmp/<name>.png --after 1s` with every instrument attached; ratio 1.00×, UI time under ~1 ms; PNGs viewed.
- Recipes: ADD_PART (74LS107 as the JK reference, 74LS93 off-corner), ADD_MACHINE (reference commits; the `RCO`/`ENT` gotcha is added with the docs), anything found wrong on the way.
- `docs/ARCHITECTURE.md` P4g row: done, date, one-line summary, links; GAME_DESIGN unchanged. This plan cut down to the status table and notes ("user check pending").

Fence: debug and release suites pass; screenshots viewed.
Commit: `docs: P4g implemented, user check pending`

## Implementation notes

- Docs review (before phase 1): `VRCO` glitched 23 ns at the start of lines 255 and 511 (`LINE_END` on `ENT` reaches `RCO` combinationally), toggling `BV8` early; `LINE_END` moved to `ENP` of `U20`/`U21`, loads became 195/203, and ADD_MACHINE gained the gotcha.
- Phase 3: Catch2 `SECTION`s inside a `for` loop replaced by `GENERATE` (sections in loops are unreliable).
- Phase 4: the netlist worked as first written: box, dashed net and ball on the first `bw run --tv`, no warnings. Release headless: 2.12× real time (78 M evaluations per simulated 2 s, 3.2× the 050 test pattern's); gate 1.5× passed.
- Phase 4: the TV draws line 0 32 columns early (the line after vertical sync free-runs until the next horizontal sync), on 050 too: the top wall's first row is shifted. `Read()` counts it as a wall row and ignores its stray cells.
- Phase 4: a seed's ball can stay out of sight for ~0.4 s and turn before the first clear step, so the bounce case runs 2 s. One case per seed (about 60 s each in debug, run in parallel): the debug suite went from 40 s to 104 s.
- Phase 5: the user's layout passes `check-board` and DRC first time. `VLOAD_N` is routed `U24` pin 6 → `U21` pin 9 → `U20` pin 9 as asked, but `V2` runs `U3` pin 12 → `U15` pin 13 → `U7` pin 4 → `U8` pin 5: `U15` pin 13 is in the middle, so no crack cuts it off alone. The user chose to change the Solid net Puzzle rather than the layout (phase 6). UI at idle: 1.00×, 0.5 ms per frame without instruments; a measurement taken while the debug suite ran said 0.8×.
- Phase 6: Solid net became Wide net (user's choice, 2026-11-09): a crack on the two-pad `NET_A` (`U12` pin 8 → `U15` pin 9) leaves `NET_N` decoded from `H4`, `H5` and `V2`: a dashed band 16 dots wide. It is still a cracked input that only changes the picture. Both cracks at 50%.
- Phase 6: with `VLOAD_N` cracked, the ball never shows in two frames in a row (once every 512 lines, i.e. every 1.64 frames); the test counts jumps between successive sightings. The slow Symptom and Repair runs are separate cases (2 s each) so ctest runs them in parallel: the debug suite takes 108 s (was 104 s).

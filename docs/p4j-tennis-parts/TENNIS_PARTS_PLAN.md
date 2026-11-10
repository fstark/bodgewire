# P4j Tennis Parts — Implementation Plan

Design: [TENNIS_PARTS_DESIGN.md](TENNIS_PARTS_DESIGN.md)
Spec: [TENNIS_PARTS.md](TENNIS_PARTS.md); Machine: [TENNIS.md](../machines/070-tennis/TENNIS.md), [TENNIS_DESIGN.md](../machines/070-tennis/TENNIS_DESIGN.md)

Each phase ends with its fence: the checks listed, `cmake --build --preset debug` and `ctest --preset debug -j 12` green, `clang-format` on the C++ files touched, then one commit with the message given (plus the attribution line). Only this step's paths are committed. The status table and the notes are updated in the phase's commit.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | 74LS157 Part, Datasheet, tests | `422d284` |
| 2 | 74LS48 Part, shared segment fold, Datasheet, tests | `e3b596b` |
| 3 | Reversed controller, two players' keys | `2a72692` |
| 4 | Netlist stage 1: ball and box, no side walls | `624fe37` |
| 5 | Stage 2: paddles | `024fdcd` |
| 6 | Stage 3: hits, angles, speed-up | `4a50392` |
| 7 | Stage 4: misses, serve, scores, Start | `2390dd3` |
| 8 | Stage 5: score digits | `c07c1a0` |
| 9 | Stage 6: game over and attract | `fc268da` |
| 10 | Manuals | `892e7e6` |
| 11 | Board (user layout), per-Machine loops | this commit |
| 12 | Session test, release check, recipes, docs | |

## Phases

### 1. 74LS157

Design §2. `src/parts/ls157.{hpp,cpp}`, delays, catalogue row, `datasheets/74LS157.md`, `tests/parts/ls157_test.cpp`.

Fence: `ls157` tests; `catalogue` and `manuals: every catalogue Part has a Datasheet` green. Commit `parts: phase 1 — 74LS157`.

### 2. 74LS48

Design §3. `ReadSegments` moved into `ls47.{hpp,cpp}`; `src/parts/ls48.{hpp,cpp}`, delay, catalogue row, `datasheets/74LS48.md`, `tests/parts/ls48_test.cpp`.

Fence: `ls47` and `ls48` tests; the stopwatch and knob meter tests unchanged. Commit `parts: phase 2 — 74LS48`.

### 3. Reversed controller, keys

Design §4, §5. `Paddle` `reversed`, `CreatePaddle`, `datasheets/PADDLE.md`, `paddle_test` cases; `DrawKnobs` keys and rate; ADD_CONNECTOR_DEVICE.md.

Fence: `paddle` tests; `bw_ui` release on `netlists/knob.bwn --knob J2:42%` screenshot still reads 42 (the keys themselves are a user check: the agent cannot press keys). Commit `ui: phase 3 — reversed controller, two players' keys`.

### 4. Stage 1: ball and box

TENNIS_DESIGN.md § Stages, stage 1: `netlists/tennis.bwn` from `ball.bwn` with the header comment for 070. `tests/machines/tennis_test.cpp` with `Watch`, `Read` (no side walls) and `tennis: court`.

Fence: `bw run netlists/tennis.bwn --tv /tmp/tennis.bmp` loads without warnings and the picture shows no side walls; `tennis: court` green. Commit `tennis: phase 4 — ball and box`.

### 5. Stage 2: paddles

Stage 2 (`J3`, `J4`, `U27`–`U33`, …). `Read` finds paddles; `KnobForTop`; `tennis: paddles follow the knobs`. Release headless speed measured and noted.

Fence: no warnings; `tennis: court` and `paddles follow the knobs` green; `bw_ui` release screenshot with `--knob J3:0%,J4:100%` viewed (left paddle low, right high). Commit `tennis: phase 5 — paddles`.

### 6. Stage 3: hits, angles, speed-up

Stage 3. The test player `Follow`; `the paddle returns the ball`, `angles`.

Fence: no warnings; the stage's cases green, earlier ones still green. Commit `tennis: phase 6 — paddle hits`.

### 7. Stage 4: misses, serve, scores, Start

Stage 4. `a miss scores`, `rally speeds up`, `Start clears the scores`. Release headless speed measured and noted.

Fence: no warnings; cases green. Commit `tennis: phase 7 — misses and scores`.

### 8. Stage 5: score digits

Stage 5. `Read` reads the digit cells; `score digits`.

Fence: no warnings; cases green; `bw_ui` release screenshot after Start (`--press SW1:0ms:20ms`) viewed: 0 and 0 at the top. Commit `tennis: phase 8 — score digits`.

### 9. Stage 6: game over and attract

Stage 6. `game over`; every earlier case now presses Start first. Release headless speed measured: under 1.3× stops here and the user is told.

Fence: no warnings; every `tennis:` case green. Commit `tennis: phase 9 — game over`.

### 10. Manuals

TENNIS_DESIGN.md § Manuals: `manuals/tennis/user.md`, `service.md`; `tennis` in `manuals_test.cpp`.

Fence: `manuals` tests green. Commit `tennis: phase 10 — manuals`.

### 11. Board

[ADD_MACHINE.md](../recipes/ADD_MACHINE.md) step 3: export `boards/tennis/tennis.net`, give the user the layout constraints of TENNIS_DESIGN.md § Board, and wait. Then `check-board` `ok`, the per-Machine loops, `drc_tennis`, and `tennis_test.cpp` switched to `LoadNetlistMachine`.

Fence: Board tests, DRC, Board scene green. Commit `tennis: phase 11 — Board`.

### 12. Finish

`session: tennis with every instrument`; release `bw_ui` with every instrument attached on the netlist, during play and after a game over: 1.00×, UI time noted, screenshots viewed. Recipes (ADD_PART: 74LS157 and 74LS48 references; ADD_MACHINE: 070 reference commits, a netlist grown in stages). ARCHITECTURE row P4j done with the date and links, the 070 Puzzles row; this plan cut down to the table and notes, "user check pending".

Fence: full debug suite green. Commit `docs: P4j implemented, user check pending`.

## Implementation notes

- Docs review (before phase 1): the circuit held gate by gate. Fixed: hiding the ball through `VBALL` would have stopped its top and bottom bounces during the serve (it could come back in the blanking or a wall): the ball is now hidden only in the mixer (`BALLV_N`), `H3_N` moved to `U42`; the court test expects the ball seen at dots 2–3 and 92–93; the 74LS157 folds X like the 74LS47 (Kleene OR gives X for `S` at X with both data 1); no case reads `GAME_OVER` before stage 6, seeds are searched each time (power-on bits depend on the parts), every case presses Start from stage 6; the speed-up case moved to stage 4 (only a serve clears `FAST`); slow-ball cases count from the last contact; unused gates of a stage's chips tied; `Watch` gets a step hook, `sim.wake(id)`; `LoadNetlistFile` until the Board; simulated times per case.
- Phase 4: stage 1 loads without warnings; release headless 2.12× (as 060). `tennis: court` simulates 2.3 s in 67 s of debug time (0.034×). At power-on the ball may come in across the edge dots from outside the picture (as on 060), so the edge-dot check starts once the ball has been inside the court.
- Phase 5: paddles where TENNIS.md puts them on the first run (one wiring slip, `H34_LOW` fed from `H1`, caught by the picture before the test). Release headless 1.89×; `bw_ui` 1.00×, UI 0.3 ms. `paddles follow the knobs` 12 s debug.
- Phase 6: the circuit worked as first written (`PAD_HIT` one 2-dot pulse on the ball's line 4, `SHALLOW` and `VDIR` set from the quarter); release headless 1.81×. The test player needed two fixes: aim for the frame the ball reaches the paddle, not two frames ahead, and read the new speed one frame later (the ball's reload is on its first line, before line 4). Aiming at the paddle's top line can miss near a wall, so the no-miss check is its own case aiming at the middle; the four angles are four cases. Debug: each 4 s case about 160 s, the court 195 s; the suite 4 min 40 s.
- Phase 7: the circuit worked as first written: the serve lasts 579 ms (29 frames), the scorer's count goes 0 → 1, the ball leaves the loser's edge slow; Start clears the scores; slow until two turns to the left, then fast. Release headless 1.77×. With serves in the netlist a power-on can clear `FAST`, so the stage 3 cases take a seed (searched) with the ball fast and no serve, and count slow contacts too (two frames on the column; the second decides). Debug: the speed-up case 373 s (10 s simulated), the others about 200–225 s; the suite about 10 minutes.
- Phase 8: the digits drew right first time, but release headless fell from 1.77× to 1.25×: the glyph terms took `H0` and `H1` directly and switched on every dot across the screen. `H0` and `H1` now reach them only through two gates opened by `REGION` (`QH0_N`, `QH1_N`, `COL3`, `UNITS` on spare gates), and speed is back to 1.51×. `bw_ui` dropped `--press` without a Board (buttons were handled inside the Board block): fixed, so Start can be scripted on a netlist. The new chips change every seed's power-on draws: a seed with `FAST` at power-on may still start with the ball in a goal and a serve, so the rally cases run 6 s; the court case no longer asks for the ball at both edges (the miss case shows it at the edge). The digit checks went into `a miss scores`; `score digits` reads a left-at-10 seed. Debug: 6 s cases about 320 s, the speed-up case 483 s.
- Phase 9: game over worked as first written: 11 ends the game, paddles go, the ball bounces off the goals, the scores stay; Start brings back 0 to 0 and the paddles. Release headless 1.52× (gate 1.3×). Every case presses Start first; the TV may take half a second to lock, so early-frame checks start at 550 ms, and `Read` refuses a rolling picture (ASan caught a read past the frame that release did not). The serve is 28–32 frames, not 29–32 (the first `FRAME1` fall may come at once): docs fixed. The test reads the right score at game over, since a ball starting in a goal may score at power-on. Debug: game over about 330 s, the speed-up case 450–500 s; the suite about 9 minutes.
- Phase 11: the user's layout passed `check-board` (`ok`) on the first try. The game-play cases now run on the Board's Circuit (`BoardCircuit`, as in `ball_test`); the netlist-warnings case keeps the netlist. All 13 pass with new searched seeds. Release headless 1.46× on the Board (1.52× on the netlist); `bw_ui` 1.00×, UI time 1.3–1.5 ms. In `bw_ui`, a `--press` held for 20 ms from 5 ms is missed (the first UI frame covers it): script Start at 300 ms.

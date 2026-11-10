# P4j Tennis Parts — Implementation Plan

Design: [TENNIS_PARTS_DESIGN.md](TENNIS_PARTS_DESIGN.md)
Spec: [TENNIS_PARTS.md](TENNIS_PARTS.md); Machine: [TENNIS.md](../machines/070-tennis/TENNIS.md), [TENNIS_DESIGN.md](../machines/070-tennis/TENNIS_DESIGN.md)

Implemented 2026-11-10, user check pending (play it, the Q/A and Up/Down keys). The phase descriptions are cut: the code is the truth.

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
| 11 | Board (user layout), per-Machine loops | `741d2f7` |
| 12 | Session test, release check, recipes, docs | this commit |

## Implementation notes

- Docs review (before phase 1): the circuit held gate by gate. Fixed: hiding the ball through `VBALL` would have stopped its top and bottom bounces during the serve (it could come back in the blanking or a wall): the ball is now hidden only in the mixer (`BALLV_N`), `H3_N` moved to `U42`; the court test expects the ball seen at dots 2–3 and 92–93; the 74LS157 folds X like the 74LS47 (Kleene OR gives X for `S` at X with both data 1); no case reads `GAME_OVER` before stage 6, seeds are searched each time (power-on bits depend on the parts), every case presses Start from stage 6; the speed-up case moved to stage 4 (only a serve clears `FAST`); slow-ball cases count from the last contact; unused gates of a stage's chips tied; `Watch` gets a step hook, `sim.wake(id)`; `LoadNetlistFile` until the Board; simulated times per case.
- Phase 4: stage 1 loads without warnings; release headless 2.12× (as 060). `tennis: court` simulates 2.3 s in 67 s of debug time (0.034×). At power-on the ball may come in across the edge dots from outside the picture (as on 060), so the edge-dot check starts once the ball has been inside the court.
- Phase 5: paddles where TENNIS.md puts them on the first run (one wiring slip, `H34_LOW` fed from `H1`, caught by the picture before the test). Release headless 1.89×; `bw_ui` 1.00×, UI 0.3 ms. `paddles follow the knobs` 12 s debug.
- Phase 6: the circuit worked as first written (`PAD_HIT` one 2-dot pulse on the ball's line 4, `SHALLOW` and `VDIR` set from the quarter); release headless 1.81×. The test player needed two fixes: aim for the frame the ball reaches the paddle, not two frames ahead, and read the new speed one frame later (the ball's reload is on its first line, before line 4). Aiming at the paddle's top line can miss near a wall, so the no-miss check is its own case aiming at the middle; the four angles are four cases. Debug: each 4 s case about 160 s, the court 195 s; the suite 4 min 40 s.
- Phase 7: the circuit worked as first written: the serve lasts 579 ms (29 frames), the scorer's count goes 0 → 1, the ball leaves the loser's edge slow; Start clears the scores; slow until two turns to the left, then fast. Release headless 1.77×. With serves in the netlist a power-on can clear `FAST`, so the stage 3 cases take a seed (searched) with the ball fast and no serve, and count slow contacts too (two frames on the column; the second decides). Debug: the speed-up case 373 s (10 s simulated), the others about 200–225 s; the suite about 10 minutes.
- Phase 8: the digits drew right first time, but release headless fell from 1.77× to 1.25×: the glyph terms took `H0` and `H1` directly and switched on every dot across the screen. `H0` and `H1` now reach them only through two gates opened by `REGION` (`QH0_N`, `QH1_N`, `COL3`, `UNITS` on spare gates), and speed is back to 1.51×. `bw_ui` dropped `--press` without a Board (buttons were handled inside the Board block): fixed, so Start can be scripted on a netlist. The new chips change every seed's power-on draws: a seed with `FAST` at power-on may still start with the ball in a goal and a serve, so the rally cases run 6 s; the court case no longer asks for the ball at both edges (the miss case shows it at the edge). The digit checks went into `a miss scores`; `score digits` reads a left-at-10 seed. Debug: 6 s cases about 320 s, the speed-up case 483 s.
- Phase 9: game over worked as first written: 11 ends the game, paddles go, the ball bounces off the goals, the scores stay; Start brings back 0 to 0 and the paddles. Release headless 1.52× (gate 1.3×). Every case presses Start first; the TV may take half a second to lock, so early-frame checks start at 550 ms, and `Read` refuses a rolling picture (ASan caught a read past the frame that release did not). The serve is 28–32 frames, not 29–32 (the first `FRAME1` fall may come at once): docs fixed. The test reads the right score at game over, since a ball starting in a goal may score at power-on. Debug: game over about 330 s, the speed-up case 450–500 s; the suite about 9 minutes.
- Phase 11: the user's layout passed `check-board` (`ok`) on the first try. The game-play cases now run on the Board's Circuit (`BoardCircuit`, as in `ball_test`); the netlist-warnings case keeps the netlist. All 13 pass with new searched seeds. Release headless 1.46× on the Board (1.52× on the netlist); `bw_ui` 1.00×, UI time 1.3–1.5 ms. In `bw_ui`, a `--press` held for 20 ms from 5 ms is missed (the first UI frame covers it): script Start at 300 ms.
- Phase 12: `session: tennis with every instrument` (two knobs, Start, 8 clips, 2 hooks, probe, meter) green. Release `bw_ui` on the netlist with every instrument attached: 1.00×, UI time 1.5–1.7 ms, during play (Start at 300 ms: paddles at 30 % and 70 %, scores 1 and 0) and without Start (random power-on 11 and 12, game over, ball alone); screenshots viewed. Recipes: ADD_PART (74LS157, 74LS48), ADD_MACHINE (070 commits, a netlist grown in stages, Board circuit in the tests, seeds, debug cost, `--press` timing).

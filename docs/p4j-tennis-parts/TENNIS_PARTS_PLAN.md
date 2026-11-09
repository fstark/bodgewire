# P4j Tennis Parts — Implementation Plan

Design: [TENNIS_PARTS_DESIGN.md](TENNIS_PARTS_DESIGN.md)
Spec: [TENNIS_PARTS.md](TENNIS_PARTS.md); Machine: [TENNIS.md](../machines/070-tennis/TENNIS.md), [TENNIS_DESIGN.md](../machines/070-tennis/TENNIS_DESIGN.md)

Each phase ends with its fence: the checks listed, `cmake --build --preset debug` and `ctest --preset debug -j 12` green, `clang-format` on the C++ files touched, then one commit with the message given (plus the attribution line). Only this step's paths are committed. The status table and the notes are updated in the phase's commit.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | 74LS157 Part, Datasheet, tests | `422d284` |
| 2 | 74LS48 Part, shared segment fold, Datasheet, tests | `e3b596b` |
| 3 | Reversed controller, two players' keys | this commit |
| 4 | Netlist stage 1: ball and box, no side walls | |
| 5 | Stage 2: paddles | |
| 6 | Stage 3: hits, angles, speed-up | |
| 7 | Stage 4: misses, serve, scores, Start | |
| 8 | Stage 5: score digits | |
| 9 | Stage 6: game over and attract | |
| 10 | Manuals | |
| 11 | Board (user layout), per-Machine loops | |
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

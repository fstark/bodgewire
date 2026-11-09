# 070 — Paddle Tennis — Machine Design

How the Machine in [TENNIS.md](TENNIS.md) is built: netlist, Board, manuals. The code it needs (74LS157, 74LS48, the reversed controller, two players' keys) comes with P4j ([TENNIS_PARTS.md](../../p4j-tennis-parts/TENNIS_PARTS.md)). Puzzles come in a later roadmap row.

## Files

Machine name `tennis`:

```
netlists/tennis.bwn                  design netlist
boards/tennis/tennis.kicad_pcb       Board (KiCad 10, laid out by hand)
manuals/tennis/user.md, service.md
```

## Netlist

`netlists/tennis.bwn` starts as a copy of `netlists/ball.bwn` (060 is left as it is) and grows in six stages, one per plan phase (§ Stages). The header comment follows [ADD_MACHINE.md](../../recipes/ADD_MACHINE.md) step 2. 58 chips, `X1`, `J1`–`J4`, `SW1`, `R1`. As on 060, every unused TTL input is tied (spare gates to `GND`) so the netlist loads without warnings; unused outputs stay unconnected.

### Parts

`U1`–`U26` keep their 060 roles ([BOUNCING_BALL_DESIGN.md](../060-bouncing-ball/BOUNCING_BALL_DESIGN.md), Netlist), with the changes listed below.

```
part J3  PADDLE min=1.248ms max=14.56ms reversed=yes   # left controller
part J4  PADDLE min=1.248ms max=14.56ms reversed=yes   # right controller
part U27 74LS74     # LWAIT, RWAIT: pulses moved to line starts
part U28 74LS93     # left paddle lines LP0-LP3
part U29 74LS93     # right paddle lines RP0-RP3
part U30 74LS107    # LDONE, RDONE
part U31 74LS157    # this half's paddle: PL2, PL3, PADV, TENS
part U32 74LS02     # LPAD_CK, RPAD_CK, LPADV, RPADV
part U33 74LS20     # COL_L_N, COL_R_N
part U34 74LS20     # PAD_N, REGION_N
part U35 74LS27     # GO_RIGHT, GO_LEFT, PAD_HIT
part U36 74LS08     # SIDES_N, TURN_N, HIT_LOW, SET_SHALLOW
part U37 74LS02     # HIT_HIGH, SET_STEEP, VLOAD_C, MID_A
part U38 74LS32     # GO_DOWN, GO_UP, BALLV_N
part U39 74LS107    # SHALLOW, GAME_OVER
part U40 74LS107    # TURN_ODD, FAST
part U41 74LS04     # H6_N, PAD_HIT_N, MOVE_N, SERVING, START, REGION
part U42 74LS00     # PCOL, MID_N, SCORE_N, H3_N
part U43 74LS27     # MISS_LEFT, MISS_RIGHT, HMOVE
part U44 74LS107    # LOST_LEFT, LOST_RIGHT
part U45 74LS107    # FRAME1
part U46 74LS93     # serve timer, SERVE_END
part U47 74LS02     # SKIP, SERVING_N, H34_LOW
part U48 74LS90     # left score units SL0-SL3
part U49 74LS90     # right score units SR0-SR3
part U50 74LS107    # TENS_L, TENS_R
part U51 74LS00     # WIN_L_N, WIN_R_N, WIN
part U52 74LS157    # this half's units SC0-SC3
part U53 74LS48     # SEG_A-SEG_G
part U54 74LS157    # this half of the cell: FE, BC, GD, A_TOP
part U55 74LS20     # T2_N, T4_N
part U56 74LS20     # T1_N, T5_N
part U57 74LS20     # T3_N
part U58 74LS30     # GLYPH
part SW1 BUTTON     # Start
part R1  R value=1k # START_N pull-up
```

The netlist lists `J3` before `J4`: the Session lists knobs in netlist order, and the UI gives the first knob (`J3`, left) Q and A, the second (`J4`, right) Up and Down (TENNIS_PARTS_DESIGN.md §5). In the file the new parts follow 060's `J2`: `J3`, `J4`, `U27`–`U58`, `SW1`, `R1`.

### Changes to the 060 chips

| Where | 060 | 070 |
|---|---|---|
| `U3.QD` | unused | `V3` |
| `U10` gates 3, 4 | spare | `SP_MID` = XOR(`SP2`, `SP3`); `HX` = XOR(`H4`, `H6`) |
| `U13` gate 4 | spare | `H12_LOW` = NOR(`H1`, `H2`) |
| `U16` `A`, `D`, `F` | `BALL_N`, `RIGHT_N`, `LEFT_N` | `BALLV_N`, `PAD_N`, `SCORE_N` (no side walls drawn; the ball hidden while serving) |
| `U17` gate 3 | spare | `SCORE_A` = NOR(`H2`, `H5`, `V6`) |
| `U18.A`, `U24.4A` | `MOVE` | `HMOVE` |
| `U20` `D C B A` | `VDIR_N`, 0 V, +5 V, +5 V | `VDIR_N`, `VLOAD_C`, `STEEP`, +5 V |
| `U22` flip-flop 2 | unused | `FRAME0`: `J`, `K`, `CLR_N` +5 V, `CLK` = `V8` |
| `U23` `QB`, `QC` | unused | `BALL_QB`, `BALL_QC` |
| `U25` gates 1, 2 | `HIT_LEFT`, `HIT_RIGHT` | `V23_LOW` = NOR(`V2`, `V3`); `H01_LOW` = NOR(`H0`, `H1`) |
| `U26` flip-flop 1 | `J` `HIT_LEFT`, `K` `HIT_RIGHT` | `J` `GO_RIGHT`, `K` `GO_LEFT`; `1Q_N` = `HDIR_N` |
| `U26` flip-flop 2 | `J` `HIT_TOP`, `K` `HIT_BOTTOM` | `J` `GO_DOWN`, `K` `GO_UP` |

`LEFT_N` and `RIGHT_N` are still decoded by `U14` and `U15` (the goals); they no longer reach the mixer.

### New circuits, gate by gate

Paddles (`J3`/`U27` flip-flop 1/`U28`/`U30` flip-flop 1 left, `J4`/flip-flop 2/`U29`/flip-flop 2 right):

| Net | Made by |
|---|---|
| `LPULSE`, `RPULSE` | `J3.OUT`, `J4.OUT`; `J3.TRIG_N`, `J4.TRIG_N` on `VSYNC_N` |
| `LWAIT`, `LWAIT_N` | `U27` flip-flop 1: `D` = `LPULSE`, `CLK` = `LINE_END_N`, `PRE_N`, `CLR_N` +5 V |
| `LPAD_CK` | `U32` gate 1: NOR(`LINE_END`, `LDONE`) |
| `LP0`–`LP3` | `U28`: `CKA` = `LPAD_CK`, `CKB` = `LP0`, `R01` = `R02` = `LWAIT` |
| `LDONE` | `U30` flip-flop 1: `CLK` = `LP3`, `J` +5 V, `K` 0 V, `CLR_N` = `LWAIT_N` |
| `LPADV` | `U32` gate 3: NOR(`LWAIT`, `LDONE`) |
| (right) | the same with `R`: `U27` flip-flop 2, `U32` gates 2 and 4, `U29`, `U30` flip-flop 2 |
| `SP2`, `SP3`, `PADV`, `TENS` | `U31` (`S` = `H6`, `G_N` 0 V): `LP2`/`RP2`, `LP3`/`RP3`, `LPADV`/`RPADV`, `TENS_L`/`TENS_R` |
| `H34_LOW` | `U47` gate 4: NOR(`H3`, `H4`) |
| `COL_L_N` | `U33` gate 1: NAND(`H1`, `H2`, `H34_LOW`, `H56_LOW`): dots 6–7 |
| `COL_R_N` | `U33` gate 2: NAND(`H3`, `H4`, `H6`, `H12_LOW`): dots 88–89 (and 120–121, blanked) |
| `PCOL` | `U42` gate 1: NAND(`COL_L_N`, `COL_R_N`) |
| `PAD_N` | `U34` gate 1: NAND(`PADV`, `PCOL`, `PLAY`, +5 V) |

Hits, angles, speed:

| Net | Made by |
|---|---|
| `H6_N` | `U41` gate 1 |
| `SIDES_N` | `U36` gate 1: AND(`LEFT_N`, `RIGHT_N`) |
| `TURN_N` | `U36` gate 2: AND(`SIDES_N`, `PAD_N`): low on a goal or a paddle |
| `GO_RIGHT` | `U35` gate 1: NOR(`BALL_N`, `TURN_N`, `H6`) |
| `GO_LEFT` | `U35` gate 2: NOR(`BALL_N`, `TURN_N`, `H6_N`) |
| `MID_A` | `U37` gate 4: NOR(`BALL_QB`, `BALL_QC`) |
| `MID_N` | `U42` gate 2: NAND(`BALL_QD`, `MID_A`): low on the ball's line 4 |
| `PAD_HIT`, `PAD_HIT_N` | `U35` gate 3: NOR(`BALL_N`, `PAD_N`, `MID_N`); `U41` gate 2 |
| `HIT_LOW` | `U36` gate 3: AND(`PAD_HIT`, `SP3`) |
| `HIT_HIGH` | `U37` gate 1: NOR(`PAD_HIT_N`, `SP3`) |
| `GO_DOWN`, `GO_UP` | `U38` gates 1, 2: OR(`HIT_TOP`, `HIT_LOW`), OR(`HIT_BOTTOM`, `HIT_HIGH`) |
| `SP_MID` | `U10` gate 3: XOR(`SP2`, `SP3`) |
| `SET_SHALLOW` | `U36` gate 4: AND(`PAD_HIT`, `SP_MID`) |
| `SET_STEEP` | `U37` gate 2: NOR(`PAD_HIT_N`, `SP_MID`) |
| `SHALLOW`, `STEEP` | `U39` flip-flop 1: `CLK` = `CLK`, `J` = `SET_SHALLOW`, `K` = `SET_STEEP`, `CLR_N` +5 V; `Q`, `Q_N` |
| `VLOAD_C` | `U37` gate 3: NOR(`VDIR_N`, `STEEP`) |
| `FRAME0`, `FRAME0_N` | `U22` flip-flop 2 |
| `TURN_ODD` | `U40` flip-flop 1: `CLK` = `HDIR`, `J` `K` +5 V, `CLR_N` = `SERVING_N` |
| `FAST` | `U40` flip-flop 2: `CLK` = `TURN_ODD`, `J` +5 V, `K` 0 V, `CLR_N` = `SERVING_N` |
| `SKIP` | `U47` gate 2: NOR(`FAST`, `FRAME0_N`): slow and an odd frame |
| `MOVE_N` | `U41` gate 3 |
| `HMOVE` | `U43` gate 3: NOR(`MOVE_N`, `SERVING`, `SKIP`) |

Misses, serve, scores, game over, Start:

| Net | Made by |
|---|---|
| `MISS_LEFT` | `U43` gate 1: NOR(`BALL_N`, `LEFT_N`, `HDIR`) |
| `MISS_RIGHT` | `U43` gate 2: NOR(`BALL_N`, `RIGHT_N`, `HDIR_N`) |
| `LOST_LEFT`, `LOST_LEFT_N` | `U44` flip-flop 1: `CLK` = `CLK`, `J` = `MISS_LEFT`, `K` = `SERVE_END`, `CLR_N` = `PLAY` |
| `LOST_RIGHT`, `LOST_RIGHT_N` | `U44` flip-flop 2, the same with `MISS_RIGHT` |
| `SERVING_N`, `SERVING` | `U47` gate 3: NOR(`LOST_LEFT`, `LOST_RIGHT`); `U41` gate 4 |
| `BALLV_N` | `U38` gate 3: OR(`BALL_N`, `SERVING`): the ball in the picture, hidden while serving (`BALL_N` itself, and so the wall bounces, are as on 060) |
| `FRAME1` | `U45` flip-flop 1: `CLK` = `FRAME0`, `J` `K` `CLR_N` +5 V (flip-flop 2 unused, inputs 0 V) |
| `ST0`–`ST2`, `SERVE_END` | `U46`: `CKA` = `FRAME1`, `CKB` = `ST0`, `R01` = `R02` = `SERVING_N`; `QD` = `SERVE_END` |
| `SL0`–`SL3` | `U48`: `CKA` = `LOST_RIGHT_N`, `CKB` = `SL0`, `R01` = `R02` = `START`, `R91` `R92` 0 V |
| `SR0`–`SR3` | `U49`: `CKA` = `LOST_LEFT_N`, the rest as `U48` |
| `TENS_L`, `TENS_R` | `U50` flip-flops 1, 2: `CLK` = `SL3` / `SR3`, `J` +5 V, `K` 0 V, `CLR_N` = `START_N` |
| `WIN_L_N`, `WIN_R_N`, `WIN` | `U51` gates 1–3: NAND(`TENS_L`, `SL0`), NAND(`TENS_R`, `SR0`), NAND of both |
| `GAME_OVER`, `PLAY` | `U39` flip-flop 2: `CLK` = `CLK`, `J` = `WIN`, `K` 0 V, `CLR_N` = `START_N`; `Q`, `Q_N` |
| `START_N` | `SW1` pin 1 (pin 2 on `GND`), `R1` from +5 V |
| `START` | `U41` gate 5 |

Score digits:

| Net | Made by |
|---|---|
| `SC0`–`SC3` | `U52` (`S` = `H6`, `G_N` 0 V): `SL0`/`SR0` … `SL3`/`SR3` |
| `SEG_A`–`SEG_G` | `U53`: `A`–`D` = `SC0`–`SC3`, `LT_N`, `RBI_N`, `BI_N` +5 V |
| `FE`, `BC`, `GD`, `A_TOP` | `U54` (`S` = `V4`, `G_N` 0 V): `SEG_F`/`SEG_E`, `SEG_B`/`SEG_C`, `SEG_G`/`SEG_D`, `SEG_A`/0 V |
| `T1_N` | `U56` gate 1: NAND(`H3`, `A_TOP`, `V23_LOW`, +5 V): a, row 0 |
| `T2_N` | `U55` gate 1: NAND(`H3`, `GD`, `V3`, `V2`): g row 3, d row 7 |
| `T3_N` | `U57` gate 1: NAND(`H3`, `FE`, `H01_LOW`, +5 V): f, e, column 0 (gate 2 unused, inputs 0 V) |
| `T4_N` | `U55` gate 2: NAND(`H3`, `BC`, `H1`, `H0`): b, c, column 3 |
| `T5_N` | `U56` gate 2: NAND(`H3_N`, `TENS`, `H1`, `H0`): the tens "1" |
| `GLYPH` | `U58`: NAND(`T1_N`–`T5_N`, +5 V × 3) |
| `REGION_N`, `REGION` | `U34` gate 2: NAND(`V5`, `TOP_B`, `SCORE_A`, `HX`); `U41` gate 6 |
| `SCORE_N` | `U42` gate 3: NAND(`GLYPH`, `REGION`) |
| `H3_N` | `U42` gate 4: NAND(`H3`, `H3`) |

Spare gates, inputs at 0 V: `U14` gate 4 (as on 060), `U38` gate 4, `U47` gate 1, `U51` gate 4, `U57` gate 2, `U45` flip-flop 2.

### Load values of the ball vertical counter

| Direction | `SHALLOW` | `U20` `D C B A` | Load | Period | Lines a frame |
|---|---|---|---|---|---|
| down | 0 | 0 0 1 1 | 195 | 316 | +4 |
| down | 1 | 0 1 0 1 | 197 | 314 | +2 |
| up | 1 | 1 0 0 1 | 201 | 310 | −2 |
| up | 0 | 1 0 1 1 | 203 | 308 | −4 |

`U21` still loads 1100 and `U22` flip-flop 1 is cleared by the wrap, as on 060.

### Timing notes

All well inside a 500 ns dot:

- `U27` is clocked by `LINE_END_N` rising (the start of dot 0); the paddle counters count on `LINE_END` rising (the start of dot 127), 127 dots after their clear is released and one dot before the next release: no race between clear and count. `LINE_END` already clocks `U23` on 060 through `U13`.
- A paddle's line k is lines L + 1 + k: `LWAIT` falls at the start of line L + 1, the counter holds 0 for that line, steps to k at dot 127 of each line, and `LDONE` is set by `LP3` falling at dot 127 of line L + 16. `U28`'s ripple (`QD` 34 ns after `CKB`) settles in dot 127, which is blanked.
- `TRIG_N` falls about 30 ns into line 308 (`VSYNC_N` through `U8`); the pulse starts 0.1 µs later. The pulse range keeps every paddle line inside lines 16–239: ⌊(0.1 µs + 1.248 ms) / 64 µs⌋ = 19 (top line 16), ⌊(0.1 µs + 14.56 ms) / 64 µs⌋ = 227 (top line 224); each end sits half a line from a boundary.
- `GO_*`, `MISS_*`, `PAD_HIT`, `SET_*` are decoded combinationally and sampled only by flip-flops clocked on the falling edge of `CLK` (mid-dot), as 060's `HIT_*`. `U26` and `U44` sample `GO_RIGHT` and `MISS_LEFT` at the same edge, so the miss and the turn happen together; from the next dot `MISS_LEFT` is 0 (`HDIR` high) and the ball is hidden.
- Apart from `LPAD_CK` and `RPAD_CK` (a NOR of `LINE_END`, as 060's `BALL_CK`), only flip-flop outputs and counter bits clock anything new: `HDIR` (`U40`), `TURN_ODD`, `FRAME0`, `FRAME1`, `LOST_*_N` (`U48`, `U49`), `SL3`, `SR3` (`U50`), `LP3`, `RP3` (`U30`).
- `GO_DOWN` and `GO_UP` are never both high: a wall hit needs the ball on lines 0–15 or 240–255, a paddle hit on lines 16–239, and one dot is one line.
- The score path from `H6` (`U52` select 18 ns, `U53` 50 ns, `U54`, `U55`–`U57`, `U58`, `U42`, `U16`, `U9`, `U17`) is under 200 ns before `U11` registers it.

### Stages

Each stage loads without warnings and is tested before the next (plan phases 4–9). A net a later stage drives is tied in the meantime as given, and every unused input of a chip a stage adds (its gates for later stages included) goes to 0 V until its own stage.

| Stage | Adds | Tied until later |
|---|---|---|
| 1 Ball and box | `tennis.bwn` from `ball.bwn`; `U16` `D`, `F` at +5 V (no side walls drawn); `U22` flip-flop 2 (`FRAME0`) | — |
| 2 Paddles | `J3`, `J4`, `U27`–`U33`, `U34` gate 1, `U42` gate 1, `U47` gate 4, `U13` gate 4; `PAD_N` on `U16.D` | `PLAY` +5 V; `U31` `4A`, `4B` (tens) 0 V |
| 3 Hits, angles, speed | `U35`–`U38`, `U39` flip-flop 1, `U40`, `U41` gates 1–3, `U42` gate 2, `U43` gate 3, `U47` gate 2, `U10` gate 3; `U20`, `U23`, `U26`, `U18`, `U24` changes; `U25` gates 1, 2 freed (inputs 0 V) | `SERVING` 0 V, `SERVING_N` +5 V |
| 4 Misses, serve, scores, Start | `U43` gates 1, 2, `U44`–`U46`, `U47` gate 3, `U41` gates 4, 5, `U38` gate 3, `U48`–`U50`, `SW1`, `R1`; `BALLV_N` on `U16.A` | `PLAY` +5 V |
| 5 Score digits | `U52`–`U58`, `U34` gate 2, `U41` gate 6, `U42` gates 3, 4, `U10` gate 4, `U17` gate 3, `U25` gates 1, 2, `U3.QD`; `SCORE_N` on `U16.F`; `TENS_L`/`TENS_R` on `U31` | `PLAY` +5 V |
| 6 Game over | `U51`, `U39` flip-flop 2; `PLAY` to `U34` and `U44` | — |

## Board

`bw export-kicad netlists/tennis.bwn -o boards/tennis/tennis.net`, then placed and routed by hand in KiCad ([KICAD_GUIDE.md](../../p2a-board-layout/KICAD_GUIDE.md)), 2 layers, no zones. About twice the 060 Board: six rows of DIP-14/16.

- Grouped by job so the Service Manual reads on the Board, as on 060: beam counters and sync (`X1`, `U1`–`U11`), with `J1` at the left and `J2` at the back edge; court and mixer (`U12`–`U17`); ball (`U18`–`U26`); paddles (`U27`–`U34`); hits, angles and speed (`U35`–`U43`); serve and score (`U44`–`U51`); score digits (`U52`–`U58`).
- `J3` near the left end of the front edge, `J4` near the right end (each player's side); `SW1` on the front edge between them, `R1` next to it.
- No Puzzle constraints yet: the Puzzles row reads the routing when it is written ([ADD_PUZZLE.md](../../recipes/ADD_PUZZLE.md)).
- `bw check-board` clean; `drc_tennis` passes (picked up by the `boards/*/*.kicad_pcb` glob).

## Manuals

- User: What it does (power on `J1`, TV on `J2`, the left player's controller on `J3` and the right player's on `J4`; Start is `SW1`; the court, the paddles following the knobs (right stop: top), the ball returned at four angles by where it hits the paddle, speeding up after a few hits; a missed ball scores for the other player and comes back from that side; first to 11, then the ball bounces alone until Start; at power-on the scores may show anything until Start).
- Service: Components, one bullet per reference with its Nets, netlist order (`X1`, `U1`–`U26`, `J1`–`J4`, `U27`–`U58`, `SW1`, `R1`). `U1`–`U26` as on 060 with the changed roles (§ Changes), for example:
  - `U16` video mixer: `MIX` high when any of `BALL_N`, `TOP_N`, `BOTTOM_N`, `PAD_N`, `NET_N`, `SCORE_N` is low
  - `U26` direction flip-flops: `HDIR` (high: right), set by `GO_RIGHT`, cleared by `GO_LEFT`; `VDIR_N` (high: up), from `GO_DOWN` and `GO_UP`
  - `U27` paddle line sync: `LWAIT`, `RWAIT`, the controllers' pulses taken at each line start
  - `U28`, `U29` paddle line counters: `LP0`–`LP3`, `RP0`–`RP3`, the paddle's line 0–15
  - `U44` lost-ball flip-flops: `LOST_LEFT`, `LOST_RIGHT`, set by a miss, cleared by `SERVE_END`
  - `U53` score decoder: `SEG_A`–`SEG_G` for the units count of the half of the line being drawn
  - `J3`, `J4` paddle controllers: `TRIG_N` on `VSYNC_N`, `OUT` `LPULSE` / `RPULSE`, 1.248 ms (right stop) to 14.56 ms (left stop)
  - `SW1` Start: pulls `START_N` low; `R1` its 1 kΩ pull-up

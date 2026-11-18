# 060 — Bouncing Ball — Machine Design

How the Machine in [BOUNCING_BALL.md](BOUNCING_BALL.md) is built: netlist, Board, Puzzles, manuals. The code it needs (74LS27, 74LS93, 74LS107) comes with P4g ([BALL_PARTS.md](../../../docs/p4g-ball-parts/BALL_PARTS.md)).

## Files

Machine name `ball`:

```
ball.bwn              design netlist
board/ball.kicad_pcb  Board (KiCad 10, laid out by hand)
manuals/user.md, service.md
puzzles/NN-*.bwp      5 Puzzles, easiest first
```

## Netlist

`machines/060-bouncing-ball/ball.bwn` (BOUNCING_BALL.md, Circuit), with the header comment of [ADD_MACHINE.md](../../../docs/recipes/ADD_MACHINE.md) step 2. 26 chips, `X1`, `J1`, `J2`; no resistors, no buttons. Every unused TTL input is tied (spare gates to `GND`, unused load inputs to `GND`), so the netlist loads without warnings; unused outputs stay unconnected (`U2.QD`, `U2.RCO`, `U3.QD` (`V3`), `U5` `QB`–`QD` and `RCO`, `U11` `1Q_N` `2Q_N`, `U18`–`U21` `QA`–`QD`, `U22` `1Q_N` and flip-flop 2, `U23` `QB` `QC`, `U26` `1Q_N`, `2Q`, the spare gates of `U10`, `U13`, `U14`, `U17`).

```
part X1  OSC freq=2MHz
part U1  74LS161    # H0-H3
part U2  74LS161    # H4-H6
part U3  74LS161    # V0-V3
part U4  74LS161    # V4-V7
part U5  74LS161    # V8
part U6  74LS20     # LINE_END_N, HSYNC_N
part U7  74LS30     # FRAME_END_N
part U8  74LS20     # VSYNC_N, BOTTOM_N
part U9  74LS04     # H4_N, LINE_END, VBALL, MR, MIX_N, HBLANK
part U10 74LS86     # composite sync
part U11 74LS74     # output registers
part U12 74LS27     # wall and net decodes
part U13 74LS02     # wall decodes, ball height clock
part U14 74LS00     # TOP_N, LEFT_N, HBLANK_N
part U15 74LS20     # RIGHT_N, NET_N
part U16 74LS30     # video mixer
part U17 74LS27     # MOVE, VIDEO_D
part U18 74LS161    # ball horizontal, low
part U19 74LS161    # ball horizontal, high
part U20 74LS161    # ball vertical, low
part U21 74LS161    # ball vertical, high
part U22 74LS107    # ball vertical, bit 8
part U23 74LS93     # ball height
part U24 74LS00     # ball loads, BALL_N, MR_N
part U25 74LS02     # bounce detectors
part U26 74LS107    # directions
part J1  POWER
part J2  VIDEO

net CLK         X1.OUT U1.CLK U2.CLK U3.CLK U4.CLK U5.CLK U11.1CLK U11.2CLK
net CLK         U18.CLK U19.CLK U20.CLK U21.CLK U26.1CLK U26.2CLK

# Horizontal counter 0..127, free-running.
net VCC         U1.CLR_N U1.ENP U1.ENT U1.LOAD_N U2.CLR_N U2.ENP U2.LOAD_N
net GND         U1.A U1.B U1.C U1.D U2.A U2.B U2.C U2.D
net H0          U1.QA U18.ENP U19.ENP U24.1B
net H1          U1.QB U12.3A
net H2          U1.QC U12.2A U12.3B U15.1D
net H3          U1.QD U6.2D U12.2B U12.3C U15.1C
net HC          U1.RCO U2.ENT U6.1A
net H4          U2.QA U6.1B U9.1A U12.2C U15.1B U15.2C
net H5          U2.QB U6.1C U6.2B U13.1A U14.3B U15.2B
net H6          U2.QC U6.1D U6.2A U13.1B U14.3A U15.1A
net H4_N        U9.1Y U6.2C
net LINE_END_N  U6.1Y U9.2A
net LINE_END    U9.2Y U3.ENP U3.ENT U4.ENP U5.ENP U7.G U20.ENP U21.ENP U13.3A
net HSYNC_N     U6.2Y U10.1A

# Vertical counter 0..311: counts at each line end, loaded with 0 at the frame end.
net VCC         U3.CLR_N U4.CLR_N U5.CLR_N U7.H
net GND         U3.A U3.B U3.C U3.D U4.A U4.B U4.C U4.D U5.A U5.B U5.C U5.D
net V0          U3.QA U7.F U17.1C
net V1          U3.QB U7.E U17.1B
net V2          U3.QC U7.D U8.1D U15.2D
net VC1         U3.RCO U4.ENT
net V4          U4.QA U7.C U8.1C U8.2D U12.1A
net V5          U4.QB U7.B U8.1B U8.2C U12.1B
net V6          U4.QC U8.2B U12.1C
net V7          U4.QD U8.2A U13.2A
net VC2         U4.RCO U5.ENT
net V8          U5.QA U7.A U8.1A U13.2B U17.2C
net FRAME_END_N U7.Y U3.LOAD_N U4.LOAD_N U5.LOAD_N
net VSYNC_N     U8.1Y U10.1B U17.1A

# Composite sync: HSYNC XOR VSYNC, then inverted.
net SYNC        U10.1Y U10.2A
net VCC         U10.2B
net SYNC_N_D    U10.2Y U11.1D

# Playfield: walls and net, active low.
net TOP_A       U12.1Y U14.1A
net TOP_B       U13.2Y U14.1B
net TOP_N       U14.1Y U16.B U25.3B
net BOTTOM_N    U8.2Y U16.C U25.4B
net LEFT_A      U12.2Y U14.2A
net H56_LOW     U13.1Y U14.2B
net LEFT_N      U14.2Y U16.F U25.1B
net RIGHT_N     U15.1Y U16.D U25.2B
net NET_A       U12.3Y U15.2A
net NET_N       U15.2Y U16.E

# Ball horizontal: steps every other dot, reloaded at 255 with 192 (191 or 193 on the MOVE line).
net VCC         U18.CLR_N U18.ENT U19.CLR_N U19.D
net MOVE        U17.1Y U18.A U24.4A
net MR          U9.4Y U18.B U18.C U18.D U19.A U19.B
net MR_N        U24.4Y U9.4A U19.C
net BHC         U18.RCO U19.ENT
net HBALL       U19.RCO U24.1A U24.3A
net HLOAD_N     U24.1Y U18.LOAD_N U19.LOAD_N

# Ball vertical: 9 bits, steps each line, reloaded on the line of 511 with 195 (down) or 203 (up).
# LINE_END on ENP, not ENT: RCO follows ENT combinationally and would glitch (VRCO clocks U22).
net VCC         U20.CLR_N U20.ENT U20.A U20.B U21.CLR_N U21.C U21.D
net GND         U20.C U21.A U21.B
net BVC         U20.RCO U21.ENT
net VRCO        U21.RCO U22.1CLK U24.2A U23.R01
net VCC         U22.1J U22.1K U22.1CLR_N
net BV8         U22.1Q U24.2B U23.R02
net VLOAD_N     U24.2Y U20.LOAD_N U21.LOAD_N
net GND         U22.2J U22.2K U22.2CLK U22.2CLR_N

# Ball height: cleared on the line of 511, counts 8 lines, then QA stops it.
net BALL_CK     U13.3Y U23.CKB
net BALL_QD     U23.QD U23.CKA
net BALL_OFF    U23.QA U13.3B U9.3A
net VBALL       U9.3Y U24.3B
net BALL_N      U24.3Y U16.A U25.1A U25.2A U25.3A U25.4A

# Bounces and directions.
net HIT_LEFT    U25.1Y U26.1J
net HIT_RIGHT   U25.2Y U26.1K
net HIT_TOP     U25.3Y U26.2J
net HIT_BOTTOM  U25.4Y U26.2K
net VCC         U26.1CLR_N U26.2CLR_N
net HDIR        U26.1Q U24.4B
net VDIR_N      U26.2Q_N U20.D

# Video: the mixer (Pong's summing resistors), blanking, output registers.
net VCC         U16.G U16.H
net MIX         U16.Y U9.5A
net MIX_N       U9.5Y U17.2A
net HBLANK_N    U14.3Y U9.6A
net HBLANK      U9.6Y U17.2B
net VIDEO_D     U17.2Y U11.2D
net SYNC_N      U11.1Q J2.SYNC_N
net VIDEO       U11.2Q J2.VIDEO
net VCC         U11.1PRE_N U11.1CLR_N U11.2PRE_N U11.2CLR_N

# Unused gate inputs.
net GND         U10.3A U10.3B U10.4A U10.4B U13.4A U13.4B U14.4A U14.4B U17.3A U17.3B U17.3C
```

Decodes, as wired:

| Net | Gate | Low (or high) for |
|---|---|---|
| `LINE_END_N` | `U6` gate 1: NAND(`HC`, `H4`, `H5`, `H6`) | dot 127 |
| `HSYNC_N` | `U6` gate 2: NAND(`H6`, `H5`, `H4_N`, `H3`) | dots 104–111 |
| `FRAME_END_N` | `U7`: NAND(`V8`, `V5`, `V4`, `V2`, `V1`, `V0`, `LINE_END`, +5 V) | line 311 at its line end |
| `VSYNC_N` | `U8` gate 1: NAND(`V8`, `V5`, `V4`, `V2`) | lines 308–311 (nothing above 311 is reached once in step) |
| `TOP_N` | `U14` gate 1: NAND(`TOP_A` = NOR(`V4`, `V5`, `V6`), `TOP_B` = NOR(`V7`, `V8`)) | lines 0–15 |
| `BOTTOM_N` | `U8` gate 2: NAND(`V7`, `V6`, `V5`, `V4`) | lines 240–255 |
| `LEFT_N` | `U14` gate 2: NAND(`LEFT_A` = NOR(`H2`, `H3`, `H4`), `H56_LOW` = NOR(`H5`, `H6`)) | dots 0–3 |
| `RIGHT_N` | `U15` gate 1: NAND(`H6`, `H4`, `H3`, `H2`) | dots 92–95, 124–127 |
| `NET_N` | `U15` gate 2: NAND(`NET_A` = NOR(`H1`, `H2`, `H3`), `H5`, `H4`, `V2`) | dots 48–49, 112–113, on lines with `V2` |
| `HBLANK` | `U14` gate 3 NAND(`H6`, `H5`), inverted by `U9` | high for dots 96–127 |
| `MOVE` | `U17` gate 1: NOR(`VSYNC_N`, `V1`, `V0`) | high on line 308 |
| `HBALL` | `U19.RCO` | high for the 2 dots of ball count 255 |
| `HLOAD_N` | `U24` gate 1: NAND(`HBALL`, `H0`) | the second dot of count 255 |
| `VLOAD_N` | `U24` gate 2: NAND(`VRCO`, `BV8`) | the whole line of ball count 511 (the load is at the end of its first dot) |
| `BALL_CK` | `U13` gate 3: NOR(`LINE_END`, `BALL_OFF`) | falls at the start of dot 127 while the ball is drawn |

Load values (`A` lowest):

| Counter | `D C B A` | Normal | Moving right / down | Moving left / up |
|---|---|---|---|---|
| `U19` (ball H high) | +5 V, `MR_N`, `MR`, `MR` | 1100 | 1011 | 1100 |
| `U18` (ball H low) | `MR`, `MR`, `MR`, `MOVE` | 0000 | 1111 | 0001 |
| `U21` (ball V high) | +5 V, +5 V, 0 V, 0 V | — | 1100 | 1100 |
| `U20` (ball V low) | `VDIR_N`, 0 V, +5 V, +5 V | — | 0011 | 1011 |

So the ball H counter loads 192 (normal), 191 (`MOVE` and `HDIR`) or 193 (`MOVE`, not `HDIR`); the ball V counter loads 195 (`VDIR`) or 203: it holds the loaded value for the rest of that line, so the period is 511 − 195 = 316 or 511 − 203 = 308 lines.

Timing notes (all well inside a 500 ns dot):

- `U18`/`U19` count on the rising edges that end a dot with `H0` high; a count lasts one `H0`-low dot and one `H0`-high dot, so `HBALL` is two dots wide and `HLOAD_N` is low only in the second, as the load must be.
- `HBALL` (`U19.RCO`) has no glitch only by a small margin: when `U18` leaves 15, its `RCO` falls (18 ns) before `U19`'s own count-to-`RCO` rise (20 ns) would show. Harmless if it ever glitched: `HBALL` reaches the flip-flops only through `HIT_*`, sampled mid-dot, and the TV only through the `U11` register.
- No `RCO` whose `ENT` is decoded combinationally clocks or clears anything: the 74LS161's `RCO` follows `ENT` at once, so with `LINE_END` on `ENT` a count reaching 15 gives a 23 ns pulse before `LINE_END` falls (measured on 050's `VC1`). So `LINE_END` goes to `ENP` of `U20` and `U21`, `U20.ENT` is +5 V, and `BVC` and `VRCO` are clean levels a whole line long.
- `VRCO` is high for the whole line of count 255 and of count 511. At 511, `VLOAD_N` loads both counters at the end of the line's first dot; `U20` leaves 15, `VRCO` falls, and `U22` toggles `BV8` back to 0. At 255 the line-end count wraps to 256, `VRCO` falls, `U22` toggles `BV8` to 1.
- `U23` is cleared during that first dot (`VRCO` and `BV8` both high on `R01`/`R02`). `BALL_CK` falls at the start of dot 127 (`LINE_END` rises) of each line while `BALL_OFF` is low: the 8th fall, on the 8th line, takes `QD` low, `QA` high, and `BALL_CK` stays low. The ball is the line of count 511 and the 7 after it.
- `U26` samples `HIT_*` on the falling edge of `CLK`, half a dot after the counters change, when every decode has settled.

## Board

`bw export-kicad machines/060-bouncing-ball/ball.bwn -o machines/060-bouncing-ball/board/ball.net`, then placed and routed by hand in KiCad ([KICAD_GUIDE.md](../../../docs/p2a-board-layout/KICAD_GUIDE.md)), 2 layers, no zones:

- Three rows of DIP-14/16, grouped by job so the Service Manual reads on the Board: the beam counters and sync (`X1`, `U1`–`U11`) at the back with `J1` at the left and `J2` at the back edge; the playfield decoders and mixer (`U12`–`U17`) in the middle; the ball counters and bounce logic (`U18`–`U26`) along the front.
- Wanted for the Puzzles (each a straight stretch away from pads, vias and corners):
  - `V2`: the pad `U15` pin 13 at an end of `V2`'s route, with a straight stretch next to it, so a crack cuts off that pad alone. (As laid out, `V2` runs `U3` pin 12 → `U15` pin 13 → `U7` pin 4 → `U8` pin 5; the user kept the layout and the Puzzle became Wide net, a crack on the two-pad `NET_A`, 2026-11-09; restored as a dry Joint in P4h, which needs no crack.)
  - `VLOAD_N`: routed `U24` pin 6 → `U20` pin 9 → `U21` pin 9 (or `U24` pin 6 → `U21` → `U20`): `U24` pin 6 at an end, a straight stretch next to it.
  - `U16` pins 4 and 5: neighbouring pins, nothing else needed (the bridge is drawn on the solder side).
- `bw check-board` clean; `drc_ball` passes (picked up by the `machines/*/board/*.kicad_pcb` glob).
- Cracks (P4e syntax), from the routed Board:
  - `VLOAD_N`: `U24.6 U21.9 50%` (routed `U24` pin 6 → `U21` pin 9 → `U20` pin 9: the 49 mm straight run on F.Cu).

## Puzzles

`machine ball` in each file (BOUNCING_BALL.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `solid_net.bwp` | `fault joint U15.13` | `V2` at `U15` |
| `wraparound.bwp` | `fault stuck U26.1Q 1` | `HDIR` |
| `stripe.bwp` | `fault stuck U23.QA 0` | `BALL_OFF` |
| `jumping_ball.bwp` | `fault crack U24.6 U21.9 50%` | `VLOAD_N` |
| `net_bounce.bwp` | `fault bridge U16.4 U16.5` | `RIGHT_N` to `NET_N` |

Titles and briefs:

- Solid net: "The net down the middle is a solid line; it used to be dashed."
- Wraparound: "The ball only ever goes right: it flies out through the right wall and comes back in through the left one."
- Stripe: "Instead of a ball there is a thin bar from top to bottom, bouncing from side to side."
- Jumping ball: "The ball flickers and jumps up and down the screen; side to side it moves as it should."
- Net bounce: "The ball bounces off the net as if it were a wall, and never goes into the right half."

## Electrical Nets on a damaged Board

- Solid net: `U15` pin 13 off `V2` (dry Joint), on a Net of its own with no Driver: floating, reads high. `NET_N` = NAND(`NET_A`, `H5`, `H4`, 1): low at the net's 2 dots on every line, a solid line. `V2` still reaches `U3`, `U7` and `U8`: its pad and a B.Cu touch on pin 13 pulse, the F.Cu touch (the leg), a Clip or a Hook on pin 13 show nothing. Swapping `U15` does not fix it. `NET_N` only feeds the mixer: the ball moves and bounces as before.
- Solid net and Jumping ball: a floating Net lights no probe lamp (the probe shows nothing), while TTL inputs on it read high.
- Wraparound: `HDIR` stuck at 1 (Strong). `MR_N` = NOT `MOVE`, so the ball H counter loads 191 on every `MOVE` line: right by 2 dots each frame. Through `HBLANK` it wraps round to dot 0 and comes in through the left wall (`HIT_LEFT`, already right). `U26`'s internal state still follows `J`/`K` (`1Q_N` toggles, unconnected).
- Stripe: `BALL_OFF` stuck at 0: `VBALL` is high on every line, so `BALL_N` is low at `HBALL` on every line: a 2-dot column across the whole active height. It overlaps the top and bottom walls every frame (`VDIR` flips back and forth, unseen) and bounces off the side walls.
- Jumping ball: `U20` and `U21` `LOAD_N` on an island without a Driver: floating, read high: never loaded. The ball V counter runs 0–511 (512 lines); the ball is drawn at each 511, every 1.64 frames, 200 lines later (mod 312) each time: 112 lines higher.
- Net bounce: `RIGHT_N` and `NET_N`, two `U15` outputs, are one Net: a Contention, 0 wins. The mixer sees the same dots low as before (they were already ORed), so the picture is unchanged; `HIT_RIGHT` = NOR(`BALL_N`, `RIGHT_N`) now fires at the net too.

## Manuals

Written to STYLE's Manuals rules (P6k):

- User: What it does (a welcome: a white box with a dashed net, a ball bouncing off the four walls, crossing in about a second; nothing to press; the ball may be out of sight for up to a second after power-on); no Using it; How it works (the goal, the clock, the beam counters as line and column numbers, sync, the dark edges, the walls and net as a stencil in a table, the ball's counters as two clocks one a little fast, across and up and down, its height, bounces as a meeting, the mixer, no reset, the unused pins in a table).
- Service: Components (every reference, netlist order); Test points (probe readings, checked with `bw run --probe` at 0.5 s, 1 s, 1.5 s and 2 s; `HDIR` and `VDIR_N` and the hits sampled every 0.2 s from 0.1 s to 3.1 s, `HDIR` low from 0.3 s to 1.3 s, high from 1.5 s to 2.1 s; four setups: a line on the analyzer and on the oscilloscope, a picture and the ball on the analyzer, each viewed in a `bw_ui --clip`/`--hook --screenshot --after 1s`); Troubleshooting (dark or rolling picture, a wall or the net wrong, the ball missing or misshapen, the ball moving wrong: ordered checks, never naming a fault).

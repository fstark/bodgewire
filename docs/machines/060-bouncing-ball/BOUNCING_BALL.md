# 060 — Bouncing Ball

Sixth rung of the tutorial ladder. A board that drives a TV: a white box with a dashed net down the middle, and a square ball that bounces off the four walls forever. It is the ball circuit of the 1972 arcade Pong on its own, without paddles or score. Its code comes with roadmap step P4g ([BALL_PARTS.md](../../p4g-ball-parts/BALL_PARTS.md)).

## Lesson

A moving picture is made by counters that run almost, but not quite, in step with the beam. The ball has its own horizontal and vertical counters; the ball is drawn wherever they overflow. Each counter's period matches the line (or the frame) exactly, except once per frame, when it is a little shorter or longer: the ball then moves by that difference. A bounce is a coincidence: the ball's video and a wall's video are high at the same dot, and a JK flip-flop turns round. The 74LS30 that mixes six video sources is the logic-only equivalent of Pong's summing resistors.

Faults show as motion going wrong (through a wall, stuck to one half, jumping about) or as shapes going wrong (a stripe instead of a ball, a net grown wide), and the player learns to tell the picture logic from the motion logic.

## Circuit

PAL-like and progressive, one dot per clock, two dots per microsecond:

| | Value |
|---|---|
| Dot clock | `X1` 2 MHz `OSC` → `CLK` (0.5 µs per dot) |
| Line | 128 dots = 64 µs (15.625 kHz) |
| Frame | 312 lines = 19.968 ms (≈ 50.08 Hz) |
| Active picture | dots 0–95, lines 0–255 |
| Horizontal sync | dots 104–111 |
| Vertical sync | lines 308–311 |
| Walls | top lines 0–15, bottom lines 240–255, left dots 0–3, right dots 92–95 |
| Net | dots 48–49, dashed: lines with `V2` high (4 on, 4 off) |
| Ball | 2 dots × 8 lines; 2 dots and 4 lines per frame |

Sync, as on the 050 test pattern but twice the dot rate:

- Horizontal counter `H0`–`H6`: `U1` 74LS161 (`H0`–`H3`, carry `HC`) and `U2` 74LS161 (`H4`–`H6`; its `QD` is unused) count freely: 128 dots per line, no reload. `U6` gate 1 decodes dot 127 into `LINE_END_N`; `U9` inverts it into `LINE_END`.
- Vertical counter `V0`–`V8`: `U3`, `U4`, `U5` 74LS161, enabled by `LINE_END`; `U7` 74LS30 decodes line 311 at the line end into `FRAME_END_N`, which reloads them with 0.
- `U6` gate 2 makes `HSYNC_N` for dots 104–111 (with `H4_N` from `U9`); `U8` gate 1 makes `VSYNC_N` for lines 308–311. `U10` 74LS86 mixes them into `SYNC_N_D`, as on 050.
- Blanking: `HBLANK` (`H6` and `H5`: dots 96–127, `U14` and `U9`) and `V8` (lines 256–311).

Playfield, decoded from the beam counters (each wall active low):

- `TOP_N` (`U12` gate 1, `U13` gate 2, `U14` gate 1): `V4`–`V8` all low, lines 0–15.
- `BOTTOM_N` (`U8` gate 2): `V4`–`V7` all high, lines 240–255.
- `LEFT_N` (`U12` gate 2, `U13` gate 1, `U14` gate 2): `H2`–`H6` all low, dots 0–3.
- `RIGHT_N` (`U15` gate 1): `H6`, `H4`, `H3`, `H2` high, dots 92–95 (and 124–127, blanked).
- `NET_N` (`U12` gate 3, `U15` gate 2): `H5`, `H4` high, `H3`–`H1` low, `V2` high: dots 48–49 (and 112–113, blanked), dashed.

Ball horizontal: `U18` and `U19` 74LS161 make an 8-bit counter that steps every other dot (`ENP` on `H0`). Its carry `HBALL` is high for the two dots of count 255: that is the ball's column. `U24` gate 1 reloads it then (`HLOAD_N`). Normally it loads 192, so it counts 64 steps = 128 dots, exactly one line: the ball stays put. On line 308 (`MOVE`, `U17` gate 1) it loads 191 when moving right (65 steps: the ball comes 2 dots later every line after) or 193 when moving left (63 steps). The load value comes from `MOVE` and `MR` (move right) and `MR_N` (`U24` gate 4, `U9`), from the direction `HDIR`.

Ball vertical: `U20` and `U21` 74LS161 and `U22` JK flip-flop 1 (bit 8, toggled each time `U21`'s carry falls) make a 9-bit counter that steps once per line (`ENP` on `LINE_END`; `ENT` only carries, so that `VRCO` is high for whole lines, without glitches). On the line of count 511 (`VRCO` and `BV8`, `U24` gate 2: `VLOAD_N`) it reloads at the end of the first dot: 195 when moving down (316 lines: the ball comes 4 lines later every frame) or 203 when moving up (308 lines). The load value's bit 3 is `VDIR_N`. There is no "still" value: the ball always moves vertically.

Ball height: `U23` 74LS93 is cleared during the first dot of the line of count 511, then counts lines on `CKB` (`BALL_CK`: `U13` gate 3 lets `LINE_END` through while `QA` is low). After 8 lines `QD` falls into `CKA`, `QA` goes high and stops the count until the next clear. `QA` is `BALL_OFF`: the ball's 8 lines are the lines while it is low: the line of count 511 and the 7 after it. `U9` inverts it into `VBALL`, and `U24` gate 3 makes `BALL_N` = NAND(`HBALL`, `VBALL`).

Bounces: `U25` 74LS02 NORs `BALL_N` with each wall into `HIT_LEFT`, `HIT_RIGHT`, `HIT_TOP`, `HIT_BOTTOM`. `U26` 74LS107 holds the directions, clocked on the falling edge of `CLK` (mid-dot, when the counters are settled): flip-flop 1 `HDIR` (set by `HIT_LEFT`: go right; cleared by `HIT_RIGHT`: go left); flip-flop 2 `VDIR` (set by `HIT_TOP`: go down; cleared by `HIT_BOTTOM`: go up). The ball steps 2 dots and 4 lines into a wall of 4 dots or 16 lines, so it can never jump over one. A turn shows one frame late: the vertical reload comes at the ball's first line, before the ball meets the wall, so the ball goes one more step (8 lines in all) into the top or bottom wall, where it is hidden for a frame; at a side wall it is hidden for a frame too (dots 2–3 or 92–93).

Video: `U16` 74LS30 is the mixer: NAND of `BALL_N`, `TOP_N`, `BOTTOM_N`, `RIGHT_N`, `NET_N`, `LEFT_N`: any source low lights the dot (`MIX`). `U17` gate 2 blanks it: `VIDEO_D` = NOR(`MIX_N`, `HBLANK`, `V8`). `U11` 74LS74 registers `SYNC_N_D` and `VIDEO_D` into `SYNC_N` and `VIDEO`, to `J2`. `J1` power connector.

No reset: from any power-on state the beam counters fall into step within 512 lines, and the ball, wherever it starts (even off screen), comes in through a wall within a second and bounces from then on. A ball overflowing more than once in the `MOVE` line (only moving left, with the reload on dot 1 of line 308: one start position in 64) steps twice in that frame, as on the real Pong; it does not show.

## When it works

The TV shows a white box with a dashed line down the middle and a small square ball moving diagonally inside. The ball bounces off each wall, crosses the screen in about a second and never stops. It locks within half a second and never rolls. At power-on the ball starts anywhere, going in any direction; it may be out of sight for up to a second.

## Puzzles

| Puzzle | Fault | Symptom | Path to the fix |
|---|---|---|---|
| Wide net | Broken Trace on `NET_A` between `U12` pin 8 and `U15` pin 9 | the net is a wide striped band (16 dots) instead of a thin dashed line; the ball is fine | `NET_A` pulses at `U12` pin 8, but the probe shows nothing at `U15` pin 9 (floating; the gate reads it high, so the net is decoded from `H4` and `H5` alone): mend the Trace |
| Wraparound | `U26` output `1Q` (`HDIR`) stuck at 1 | the ball always goes right: it flies through the right wall and comes back in through the left one; it still bounces off the top and bottom | `HIT_RIGHT` pulses at `U26` pin 4 when the ball reaches the right wall, but `HDIR` stays high: swap `U26` |
| Stripe | `U23` output `QA` (`BALL_OFF`) stuck at 0 | instead of a ball, a thin bar from the top to the bottom of the box, bouncing left and right | `BALL_CK` pulses and `QD` falls every 8 lines, but `QA` stays low: swap `U23` |
| Jumping ball | Broken Trace on `VLOAD_N` from `U24` pin 6, cutting `U20` and `U21` `LOAD_N` off | the ball flickers and jumps up and down the screen at random heights, while it moves smoothly left and right | `VLOAD_N` pulses once a frame at `U24` pin 6, but the probe shows nothing at `U20` pin 9 and `U21` pin 9 (floating, read high): the ball counter is never reloaded and runs 512 lines: mend the Trace |
| Net bounce | Solder Bridge between `U16` pin 4 (`RIGHT_N`) and pin 5 (`NET_N`) | the ball bounces off the net as if it were a wall, and never goes into the right half; the picture looks right | `HIT_RIGHT` pulses when the ball touches the net; `RIGHT_N` and `NET_N` are identical (0 wins), so the right-wall bounce also sees the net. The bridge is on the solder side of `U16`: cut it |

Wide net stands in for the Puzzle first designed, Solid net (`V2` cut off at `U15` pin 13): the Board's `V2` route has no crack that cuts that pin off alone, and pin Faults do not exist yet. Roadmap P4h adds them and restores Solid net as a dry Joint at `U15` pin 13 (`V2` pulses on the pad, the gate does not see it: resolder).

Each teaches something different: a cracked input that only changes the picture, a stuck flip-flop that only changes the motion, a ripple counter's stop flag, the reload that makes motion (a counter's period against the frame's), and a bridge between two sources that look the same on the screen.

Netlist, Board, Puzzle files and manuals: [BOUNCING_BALL_DESIGN.md](BOUNCING_BALL_DESIGN.md).

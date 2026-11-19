# Paddle Tennis — Service Manual

## Components

- `X1` dot clock: 2 MHz crystal oscillator, net `CLK`
- `U1` horizontal counter, low bits: `H0`–`H3`, carry `HC`
- `U2` horizontal counter, high bits: `H4`–`H6`; 128 dots a line
- `U3` vertical counter, bits 0–3: `V0`–`V3`; counts at each line end
- `U4` vertical counter, bits 4–7: `V4`–`V7`
- `U5` vertical counter, bit 8: `V8`; with `U3` and `U4` reloaded with 0 after line 311
- `U6` line decoder: `LINE_END_N` at dot 127, `HSYNC_N` for dots 104–111
- `U7` frame-end decoder: `FRAME_END_N` low at the end of line 311
- `U8` vertical decoders: `TICK_N` for lines 308–311, which starts the paddle timers and the ball's moves; `BOTTOM_N` for lines 240–255
- `U9` inverters: `H4_N`, `LINE_END`, `VBALL`, `MR`, `MIX_N`, `HBLANK`
- `U10` exclusive-OR gates: mixes `HSYNC_N` and `VSYNC_N` into one sync, inverted into `SYNC_N_D`; `SP_MID`, high when `SP2` and `SP3` differ; `HX`, high when `H4` and `H6` differ
- `U11` output register: re-times `SYNC_N` and `VIDEO` on the clock
- `U12` wall and net decodes: `TOP_A`, `LEFT_A`, `NET_A`
- `U13` decodes: `H56_LOW`, `TOP_B`, `H12_LOW`; `BALL_CK`, the ball height clock
- `U14` walls and goals: `TOP_N` (lines 0–15), `LEFT_N` (the left goal, dots 0–3, not drawn); `HBLANK_N`; `QH0_N`, `H0` inside the score digits
- `U15` right goal and net: `RIGHT_N` (dots 92–95, not drawn), `NET_N` (dots 48–49, dashed by `V2`)
- `U16` video mixer: `MIX` high when any of `BALLV_N`, `TOP_N`, `BOTTOM_N`, `PAD_N`, `NET_N`, `SCORE_N` is low
- `U17` move and video gates: `MOVE` on line 308; `VIDEO_D`, the mixer's output outside the dark margins; `SCORE_A`
- `U18` ball horizontal counter, low: steps every other dot, carry `BHC`
- `U19` ball horizontal counter, high: `HBALL` at its overflow; with `U18` reloaded by `HLOAD_N`, one step shorter or longer on an `HMOVE` line
- `U20` ball vertical counter, low: steps each line, carry `BVC`; loads `VDIR_N`, `VLOAD_C`, `STEEP`: 2 or 4 lines a frame, up or down
- `U21` ball vertical counter, high: `VRCO` high on count 255; with `U20` reloaded by `VLOAD_N`
- `U22` flip-flops: `BV8`, the ball vertical counter's bit 8, toggled by `VRCO`; `FRAME0`, toggled at each frame start
- `U23` ball height: cleared on the ball's top line, `BALL_OFF` low for 8 lines; `BALL_QB`–`BALL_QD` count the ball's lines
- `U24` ball gates: `HLOAD_N`, `VLOAD_N`, `BALL_N`, `MR_N`
- `U25` decodes: `HIT_TOP`, `HIT_BOTTOM` (the ball on a wall); `V23_LOW`; `UNITS`, the units digit cell
- `U26` direction flip-flops: `HDIR` (high: right), set by `GO_RIGHT`, cleared by `GO_LEFT`; `VDIR_N` (high: up), from `GO_DOWN` and `GO_UP`
- `J1` power connector: +5 V and 0 V for the whole board
- `J2` monitor connector: `VIDEO` and `SYNC_N` out
- `J3` left paddle controller connector: `TRIG_N` on `TICK_N`, its pulse back on `LPULSE`, about 1.25 ms at the right stop to about 14.6 ms at the left stop (wired the other way round); the controller runs on the board's +5 V
- `J4` right paddle controller connector: as `J3`, pulse on `RPULSE`
- `U27` paddle line sync: `LWAIT`, `RWAIT`, the controllers' pulses taken at each line start
- `U28` left paddle line counter: `LP0`–`LP3`, the paddle's line 0–15, cleared while `LWAIT` is high
- `U29` right paddle line counter: `RP0`–`RP3`, cleared while `RWAIT` is high
- `U30` paddle done flip-flops: `LDONE`, `RDONE`, set after a paddle's 16 lines
- `U31` paddle select: this half of the line's paddle, `SP2`, `SP3`, `PADV`, and tens flag `TENS`, by `H6`
- `U32` paddle gates: `LPAD_CK`, `RPAD_CK`, the line clocks; `LPADV`, `RPADV`, the paddles' lines
- `U33` paddle columns: `COL_L_N` (dots 6–7), `COL_R_N` (dots 88–89)
- `U34` gates: `PAD_N`, the paddles in the picture while `PLAY`; `REGION_N`, the score digits' area
- `U35` hit detectors: `GO_RIGHT`, `GO_LEFT` (the ball on a goal or a paddle); `PAD_HIT` (the ball's line 4 on a paddle)
- `U36` hit gates: `SIDES_N`, `TURN_N`, `HIT_LOW`, `SET_SHALLOW`
- `U37` hit gates: `HIT_HIGH`, `SET_STEEP`, `VLOAD_C`, `MID_A`
- `U38` hit gates: `GO_DOWN`, `GO_UP`; `BALLV_N`, the ball in the picture, hidden while serving; `VSYNC_N`, the frame sync, low for lines 272–287
- `U39` flip-flops: `SHALLOW` (`STEEP`), the ball's angle; `GAME_OVER` (`PLAY`), set by `WIN`, cleared by START
- `U40` speed flip-flops: `TURN_ODD`, toggled by each turn to the left; `FAST`, set at the second; both cleared while serving
- `U41` inverters: `H6_N`, `PAD_HIT_N`, `MOVE_N`, `SERVING`, `START`, `REGION`
- `U42` gates: `PCOL`, `MID_N`, `SCORE_N`, `H3_N`
- `U43` gates: `MISS_LEFT`, `MISS_RIGHT` (the ball moving into a goal); `HMOVE`, the line the ball steps sideways
- `U44` lost-ball flip-flops: `LOST_LEFT`, `LOST_RIGHT`, set by a miss, cleared by `SERVE_END`, held clear while the game is over
- `U45` frame flip-flop: `FRAME1`, toggled every 2 frames
- `U46` serve timer: counts `FRAME1` while serving, `SERVE_END` after 8
- `U47` gates: `COL3`, `SKIP`, `SERVING_N`, `H34_LOW`
- `U48` left score counter: `SL0`–`SL3`, counts when the ball is lost on the right
- `U49` right score counter: `SR0`–`SR3`, counts when the ball is lost on the left
- `U50` tens flip-flops: `TENS_L`, `TENS_R`, set when the units go from 9 to 0
- `U51` win gates: `WIN_L_N`, `WIN_R_N`, `WIN` at 11; `VSYNC_A`, the first step of the frame sync
- `U52` score select: this half of the line's units count `SC0`–`SC3`, by `H6`
- `U53` score decoder: `SEG_A`–`SEG_G`, high for a lit segment
- `U54` stroke select: `FE`, `BC`, `GD`, `A_TOP` for the top or bottom half of a digit, by `V4`
- `U55` digit terms: `T2_N` (g, d), `T4_N` (b, c)
- `U56` digit terms: `T1_N` (a), `T5_N` (the tens "1")
- `U57` digit terms: `T3_N` (f, e); `QH1_N`, `H1` inside the score digits
- `U58` glyph gate: `GLYPH` high when any digit term is low
- `SW1` START button: pulls `START_N` low while pressed
- `R1` START pull-up: holds `START_N` high, 1 kΩ

## Test points

What the logic probe shows on a working board. **P** is the pulse lamp: it stays lit while the signal keeps changing, and flashes at a single change. Click a pin below with the probe in hand to probe it.

"In play" means after START, with the ball going back and forth. Most signals here change on every line or every frame, far too fast for the eye: the probe shows P. The logic analyzer shows their shape.

| Probe | Net | A working board shows |
|---|---|---|
| `X1.OUT` | `CLK` | P lit |
| `U6.2Y` | `HSYNC_N` | P lit |
| `U6.1Y` | `LINE_END_N` | H, with P lit |
| `U8.1Y` | `TICK_N` | H, with P lit |
| `U51.4Y` | `VSYNC_A` | H, with P lit |
| `U38.4Y` | `VSYNC_N` | H, with P lit |
| `U5.QA` | `V8` | P lit |
| `U11.1Q` | `SYNC_N` | P lit |
| `U11.2Q` | `VIDEO` | P lit |
| Logic analyzer: clip `U6.2Y`, `U6.1Y`, `U11.1Q`, `U11.2Q`; trigger on `U6.2Y` falling; sample every 100 ns; 10 µs a division | | One line every 64 µs. `U6.2Y` dips low for 4 µs. `U11.1Q` copies it a fraction later. `U6.1Y` dips for half a microsecond, about 11.5 µs after the start of the sync dip. `U11.2Q` shows short white pulses where the line crosses something white |
| `J3.OUT`, `J4.OUT` | `LPULSE`, `RPULSE` | P lit |
| `U27.1Q`, `U27.2Q` | `LWAIT`, `RWAIT` | P lit |
| `U30.1Q`, `U30.2Q` | `LDONE`, `RDONE` | P lit |
| `U34.1Y` | `PAD_N` | in play: H, with P lit. Game over: H, steady |
| Logic analyzer: clip `U8.1Y`, `J3.OUT`, `J4.OUT`, `U27.1Q`, `U27.2Q`; trigger on `U8.1Y` falling; sample every 1 µs; 2 ms a division. Left knob at its left stop, right knob at its right stop | | One frame every 20 ms. `U8.1Y` dips low for a quarter of a millisecond. At that moment both controllers start a pulse. `J3.OUT` stays high for about 14.6 ms, `J4.OUT` for about 1.25 ms. Turn a knob and its pulse gets longer or shorter. `U27.1Q` and `U27.2Q` copy the pulses, each ending at the next line start |
| `U26.1Q` | `HDIR` | in play: H while the ball moves right, L while it moves left; P flashes each time it turns |
| `U26.2Q_N` | `VDIR_N` | in play: H while the ball moves up, L while it moves down; P flashes at each bounce |
| `U47.3Y` | `SERVING_N` | in play: H. After each miss, L for about half a second while the ball is hidden |
| `U40.2Q` | `FAST` | L after each serve. H once the ball has turned left twice, until the next miss |
| `U48.QA`, `U49.QA` | `SL0`, `SR0` | after START: L. Each changes when its player scores |
| `U39.2Q` | `GAME_OVER` | in play: L. H once a player reaches 11, until START |
| `SW1` pin 1 | `START_N` | H. L while START is held |
| `U53.SA` | `SEG_A` | after START, with both scores at 0: H, steady. It is H for every units digit with a top bar (0, 2, 3, 5, 7, 8, 9). P lit when only one of the two scores has one |
| `U58.Y` | `GLYPH` | L, with P lit |
| `U42.3Y` | `SCORE_N` | H, with P lit |

## Troubleshooting

**No picture, or the picture will not hold still.**

1. Probe `X1.OUT`. It must show P.
2. Probe `U6.2Y` and `U38.4Y`. Both must read H with P.
3. Probe `U11.1Q`, the sync to the TV. It must show P.
4. Set up the first analyzer row of the Test points. Each line must last 64 µs, with one sync dip.
5. Probe `U11.2Q`, the picture to the TV. It must show P.

**The court is there, but a paddle is missing or will not follow its knob.**

1. Check the game is not over: `U39.2Q` must read L. If it reads H, press START.
2. Set up the second analyzer row. Each controller's pulse must start at the `U8.1Y` dip, and its length must change as you turn the knob.
3. Probe the left paddle's chain in order: `J3.OUT`, `U27.1Q`, `U28.QA`, `U30.1Q`. Each must show P. The right paddle's chain is `J4.OUT`, `U27.2Q`, `U29.QA`, `U30.2Q`.
4. Probe `U31.3Y` and `U34.1Y`. Both must show P.

**The ball goes through a paddle, or always leaves at the same angle.**

1. Probe `U34.1Y`. It must show P.
2. Probe `U35.3Y` while the ball meets a paddle. P must flash.
3. Probe `U26.1Q`. It must change each time the ball meets a paddle.
4. Probe `U39.1Q`. It changes when a hit gives a new angle: shallow or steep.

**The ball never speeds up.**

1. Probe `U40.1Q` during a rally. It must change each time the ball turns left.
2. Probe `U40.2Q`. It must read H after the second turn.

**A miss does not score, or the ball never comes back.**

1. Probe `U47.3Y` after a miss. It must read L for about half a second, then H again.
2. Probe `U46.QA`. It must show P while serving.
3. Probe `U48.QA` and `U49.QA`. One of them must change at each miss.

**A digit is drawn wrong.**

1. Probe `U53.SA` to `U53.SG` with both scores on the same number. Each segment that should be lit reads H, the others L.
2. Press START, then probe the stroke picker `U54` before anyone scores. Both scores show 0. `U54.1Y` and `U54.2Y` read H, steady: the sides of a 0. `U54.3Y` and `U54.4Y` show P: the bottom bar is lit only in the bottom half of the digit, the top bar only in the top half.
3. Probe `U58.Y`. It must show P.

**START does nothing.**

1. Probe `SW1` pin 1. It must read L while START is held.
2. Probe `U41.5Y`. It must read H while START is held.
3. After START, `U48.QA`, `U49.QA` and `U39.2Q` must read L.

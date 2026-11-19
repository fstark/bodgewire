# Bouncing Ball — Service Manual

## Components

- `X1` dot clock: 2 MHz crystal oscillator, drives `CLK`
- `U1` dot counter, low bits: `H0`–`H3`, carry `HC`
- `U2` dot counter, high bits: `H4`–`H6`; 128 dots a line
- `U3` line counter, low bits: `V0`–`V3`; steps at each line end
- `U4` line counter, middle bits: `V4`–`V7`
- `U5` line counter, top bit: `V8`; with `U3` and `U4`, set back to 0 after line 311
- `U6` line decoder: `LINE_END_N` at dot 127, `HSYNC_N` for dots 104 to 111
- `U7` frame end: `FRAME_END_N` low at the end of line 311
- `U8` line decoders: `TICK_N` for lines 308 to 311, `BOTTOM_N` for lines 240 to 255
- `U9` inverters: `H4_N`, `LINE_END`, `VBALL`, `MR`, `MIX_N`, `HBLANK`
- `U10` sync mixer: joins `HSYNC_N` and `VSYNC_N` into `SYNC_N_D`; its spare gates turn `V3` into `V3_N` and `VSYNC` into `VSYNC_N`
- `U11` output register: sends `SYNC_N` and `VIDEO` out together, on the clock
- `U12` wall and net gates: `TOP_A`, `LEFT_A`, `NET_A`
- `U13` gates: `H56_LOW`, `TOP_B`; `BALL_CK`, which clocks the ball's height
- `U14` walls: `TOP_N` (lines 0 to 15), `LEFT_N` (dots 0 to 3); `HBLANK_N`; `VSYNC_A`, the first step of the frame sync
- `U15` right wall and net: `RIGHT_N` (dots 92 to 95), `NET_N` (dots 48 and 49, dashed by `V2`)
- `U16` video mixer: `MIX` high when any of `BALL_N`, `TOP_N`, `BOTTOM_N`, `RIGHT_N`, `NET_N`, `LEFT_N` is low
- `U17` move, video and frame sync gates: `MOVE` on line 308; `VIDEO_D`, which is `MIX` kept dark at the edges; `VSYNC`, high for lines 280 to 283
- `U18` ball across counter, low: steps every other dot, carry `BHC`
- `U19` ball across counter, high: `HBALL` at its overflow; with `U18`, loaded again by `HLOAD_N`
- `U20` ball down counter, low: steps each line, carry `BVC`
- `U21` ball down counter, high: `VRCO` high on count 255; with `U20`, loaded again by `VLOAD_N`
- `U22` ball down counter, top bit: `BV8`, which changes as `VRCO` falls
- `U23` ball height: cleared when the ball's line count overflows; `BALL_OFF` low for 8 lines
- `U24` ball gates: `HLOAD_N`, `VLOAD_N`, `BALL_N`, `MR_N`
- `U25` bounce detectors: `HIT_LEFT`, `HIT_RIGHT`, `HIT_TOP`, `HIT_BOTTOM`
- `U26` direction flip-flops: `HDIR` (high: right), `VDIR_N` (high: up)
- `J1` power connector: +5 V and 0 V for the whole board
- `J2` monitor connector: `VIDEO` and `SYNC_N` out

## Test points

What the logic probe shows on a working board. **P** is the pulse lamp. It stays lit while the signal keeps changing. Most signals here change far faster than the eye can follow, so P is lit on them all the time. Only the directions and the hits are slow enough to watch. Click a pin below with the probe in hand to probe it.

| Probe | Net | A working board shows |
|---|---|---|
| `U1.CLK` (pin 2 of every 74LS161) | `CLK` | P lit |
| `U1.QA` | `H0` | P lit |
| `U9.2Y` | `LINE_END` | mostly L, with P lit: high for one dot per line |
| `U6.2Y` | `HSYNC_N` | mostly H, with P lit |
| `U3.QC` | `V2` | P lit: it changes every 4 lines |
| `U5.QA` | `V8` | mostly L, with P lit: high for the last 56 lines of each picture |
| `U7.Y` | `FRAME_END_N` | mostly H, with P lit: low for one dot per picture |
| `U8.1Y` | `TICK_N` | mostly H, with P lit: low for 4 lines per picture |
| `U10.3Y` | `V3_N` | P lit: it changes every 8 lines |
| `U14.4Y` | `VSYNC_A` | mostly H, with P lit |
| `U17.3Y` | `VSYNC` | mostly L, with P lit: high for 4 lines per picture |
| `U10.4Y` | `VSYNC_N` | mostly H, with P lit: low for 4 lines per picture |
| `U11.1Q` (`J2`'s sync) | `SYNC_N` | mostly H, with P lit |
| `U11.2Q` (`J2`'s video) | `VIDEO` | mostly L, with P lit |
| `U14.1Y`, `U8.2Y`, `U14.2Y`, `U15.1Y`, `U15.2Y` | `TOP_N`, `BOTTOM_N`, `LEFT_N`, `RIGHT_N`, `NET_N` | mostly H, with P lit: each is low while the beam crosses its wall or the net |
| `U16.Y` | `MIX` | mostly L, with P lit |
| `U17.1Y` | `MOVE` | mostly L, with P lit: high for one line per picture |
| `U19.RCO` | `HBALL` | mostly L, with P lit: high for 2 dots per line |
| `U24.1Y` | `HLOAD_N` | mostly H, with P lit |
| `U21.RCO` | `VRCO` | P lit |
| `U24.2Y` | `VLOAD_N` | mostly H, with P lit: low for one line per picture |
| `U23.QA` | `BALL_OFF` | mostly H, with P lit: low for the ball's 8 lines |
| `U24.3Y` | `BALL_N` | mostly H, with P lit: low where the ball is drawn |
| `U25.1Y`, `U25.2Y`, `U25.3Y`, `U25.4Y` | `HIT_LEFT`, `HIT_RIGHT`, `HIT_TOP`, `HIT_BOTTOM` | L. P flashes once each time the ball meets that wall |
| `U26.1Q` | `HDIR` | H while the ball goes right, L while it goes left. It changes at each side wall, a little under a second apart; P flashes at each change |
| `U26.2Q_N` | `VDIR_N` | H while the ball goes up, L while it goes down. It changes at the top and bottom walls, about a second apart; P flashes at each change |

The logic analyzer and the oscilloscope show what the probe cannot: when each pulse comes, and how long it lasts. Four setups:

| Setup | A working board shows |
|---|---|
| A line, on the analyzer. Clip `U6.2Y` and trigger on its falling edge. Clip also `U9.6Y`, `U9.2Y`, `U11.1Q` and `U11.2Q`. Sample every 100 ns, view at 20 µs a division | `U6.2Y` is low for 4 µs, every 64 µs. `U11.1Q` is the same pulse, a moment later. `U9.6Y` is high for 16 µs around it: the dark edge. `U9.2Y` is a short spike at the end of that edge. `U11.2Q` shows short pulses between the dark edges: one at each side wall and, on the lines of a dash, one for the net |
| A line, on the oscilloscope. Hook `U11.1Q` on channel 1 and `U11.2Q` on channel 2. Trigger on channel 1, rising edge, at 10 µs a division | Channel 1: a sync pulse low for 4 µs, every 64 µs. Channel 2: a 2 µs pulse for the left wall, about 8 µs after the sync ends. A 2 µs pulse for the right wall, about 46 µs later. On some lines, 1 µs pulses between them: the net in the middle, and the ball on the lines it covers |
| A picture, on the analyzer. Clip `U8.1Y` and trigger on its falling edge. Clip also `U10.4Y`, `U5.QA`, `U17.1Y`, `U24.2Y`, `U9.3Y` and `U19.RCO`. Sample every 1 µs, view at 2 ms a division | `U8.1Y` is low for a quarter of a millisecond, every 20 ms. `U10.4Y`, the frame sync, is low for a quarter of a millisecond too, about 1.8 ms before the trigger. `U5.QA` is high for the last 3.5 ms before it. `U17.1Y` is a short high pulse as `U8.1Y` falls. `U24.2Y` is a short low pulse once a picture, and `U9.3Y` is high for half a millisecond just after it: the ball's 8 lines. That place moves a little each picture, as the ball goes up or down. `U19.RCO` pulses on every line |
| The ball, on the analyzer. Clip `U9.3Y` and trigger on its rising edge. Clip also `U19.RCO`, `U24.3Y` and `U11.2Q`. Sample every 100 ns, view at 100 µs a division | `U9.3Y` is high for 8 lines, about half a millisecond. `U19.RCO` pulses on every line, before, during and after. `U24.3Y` pulses low on the 8 lines only: the ball |

## Troubleshooting

**The screen stays dark, or the picture rolls.**

The TV is not getting a clean picture or a clean sync.

1. Probe `U1.CLK`. P must be lit.
2. Probe `U1.QA`, then `U2.QC`. Each must show P.
3. Probe `U9.2Y`, on `LINE_END`. It must show P, mostly L.
4. Probe `U3.QA`, then `U5.QA`. Each must show P.
5. Probe `U7.Y`. It must show P. If it stays H, the line count never reaches its end: check `U3.QA` to `U3.QC`, `U4.QA` to `U4.QD` and `U5.QA` in turn, each must show P.
6. Set up the line on the analyzer (Test points). `U6.2Y` must pulse low for 4 µs, every 64 µs. `U11.1Q` must pulse with it.
7. Set up the picture on the analyzer. `U8.1Y` must go low once every 20 ms. `U10.4Y` must go low once every 20 ms, about 1.8 ms earlier.
8. Probe `U11.2Q`, the video. It must show P. If it is still, go back through `U17.2Y` and `U16.Y`.

**A wall or the net is missing, too wide, or not dashed.**

The sync is fine; one shape is wrong.

1. Find the shape's output in the table of Test points: `U14.1Y` for the top wall, and so on. It must show P, mostly H.
2. Probe the same wire where it arrives at the mixer, `U16`. It must read the same.
3. If the shape's output is wrong, probe the inputs of its gate. Each must show P, and read the same as the counter output it comes from.
4. For the net's dashes, probe `U15.2D`. It must show P, like `U3.QC`.

**The ball is missing, or is the wrong shape.**

1. Probe `U19.RCO`, on `HBALL`. It must show P, mostly L.
2. Probe `U24.2Y`, on `VLOAD_N`. It must show P, mostly H.
3. Probe `U13.3Y`, the height clock, then `U23.QA`. Each must show P; `U23.QA` mostly H.
4. Probe `U24.3Y`, on `BALL_N`. It must show P, mostly H.
5. Set up the ball on the analyzer. `U9.3Y` must be high for 8 lines, once a picture. `U24.3Y` must pulse low on those 8 lines only.

**The ball moves wrong: it goes through a wall, turns where there is no wall, or never turns.**

1. Probe `U17.1Y`, on `MOVE`. It must show P, mostly L.
2. Probe `U26.1Q` while the ball crosses the box. It must change each time the ball reaches a side wall, and only then.
3. Probe `U26.2Q_N` the same way. It must change at the top and the bottom walls, and only then.
4. Probe the hit for the wall in question: `U25.1Y` for the left, `U25.2Y` for the right, `U25.3Y` for the top, `U25.4Y` for the bottom. P must flash when the ball meets that wall, and never anywhere else.
5. If a hit flashes in the wrong place, probe its wall input on `U25`: `U25.2B` for the right wall, and so on. It must read the same as the wall's own output.
6. If the hit flashes but the direction does not change, probe the same wire where it arrives at `U26`: `U26.1J`, `U26.1K`, `U26.2J` or `U26.2K`.

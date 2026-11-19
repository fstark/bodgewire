# Test Pattern Generator — Service Manual

## Components

- `X1` dot clock: 1 MHz crystal oscillator, drives `CLK`
- `U1` dot counter, low bits: `H0`–`H3`, carry `HC` to `U2`
- `U2` dot counter, high bits: `H4`, `H5`; starts again from 0 at each line end
- `U3` line counter, bits 0–3: `V0`–`V3`; steps once per line, on `LINE_END`
- `U4` line counter, bits 4–7: `V4`–`V7`
- `U5` line counter, bit 8: `V8`
- `U6` line decoder: gate 1 pulls `LINE_END_N` low at dot 63, gate 2 pulls `HSYNC_N` low for dots 52 to 55
- `U7` frame-end decoder: `FRAME_END_N` low at the end of line 311
- `U8` frame sync decoder: `VSYNC_N` low for lines 288 to 291
- `U9` inverters: `H3_N`, `V2_N`, `V3_N` and `V4_N` for the decoders, `LINE_END` for the line counter
- `U10` sync mixer: mixes `HSYNC_N` and `VSYNC_N` into one sync, `SYNC_N_D`
- `U11` blanking and video gate: `HBLANK`, `VBLANK`, and `VIDEO_D`, the grid inside the picture
- `U12` grid ORs: join counter bits for the grid, then the two grid directions into `GRID`
- `U13` grid NORs: `ACTIVE`, `GRIDH` (every 8th dot), `GRIDV` (every 16th line)
- `U14` output register: copies the sync and the video at each tick, into `SYNC_N` and `VIDEO`
- `J1` power connector: +5 V and 0 V for the whole board
- `J2` TV connector: `VIDEO` and `SYNC_N` out

## Test points

What the logic probe shows on a working board. **P** is the pulse lamp. It stays lit while the signal keeps changing. Every signal on this board changes far faster than the eye can follow, so P is lit everywhere except on the supply. Where a signal is mostly high or mostly low, that lamp is the brighter one. Click a pin below with the probe in hand to probe it.

| Probe | Net | A working board shows |
|---|---|---|
| `U6` pin 14 | `VCC` | H, steady; the meter reads 5.00 V. Every chip's supply pin reads the same |
| `U1.CLK` (pin 2 of `U1`–`U5`) | `CLK` | P lit; H and L flicker |
| `U1.QA` to `U1.QD`, `U2.QA`, `U2.QB` | `H0`–`H5` | P lit; H and L flicker |
| `U1.RCO` | `HC` | mostly L, with P lit: high for one dot in 16 |
| `U6.1Y` | `LINE_END_N` | mostly H, with P lit: low for one dot per line |
| `U9.3Y` | `LINE_END` | mostly L, with P lit: the opposite of `U6.1Y` |
| `U6.2Y` | `HSYNC_N` | mostly H, with P lit |
| `U3.QA` | `V0` | P lit: it changes at every line |
| `U5.QA` | `V8` | mostly L, with P lit: high for the last 56 lines of each frame |
| `U7.Y` | `FRAME_END_N` | mostly H, with P lit: low for one dot per frame |
| `U9.4Y`, `U9.5Y` | `V4_N`, `V2_N` | P lit; H and L flicker |
| `U8.Y` | `VSYNC_N` | mostly H, with P lit |
| `U14.1Q` (`J2`'s sync) | `SYNC_N` | mostly H, with P lit |
| `U14.2Q` (`J2`'s video) | `VIDEO` | mostly L, with P lit |
| `U1.CLR_N` | `VCC` | H, steady |

The logic analyzer and the oscilloscope show what the probe cannot: when each pulse comes, and how long it lasts. Four setups:

| Setup | A working board shows |
|---|---|
| A line, on the analyzer. Clip `U6.1Y` and trigger on its falling edge. Clip also `U1.QA`, `U1.QB`, `U1.QC`, `U1.QD`, `U2.QA`, `U2.QB`, `U6.2Y` and `U14.1Q`. Sample every 100 ns, view at 10 µs a division | The dot counter: each of `H0` to `H5` changes half as often as the one before. `U6.1Y` is low for one dot, every 64 µs, just as `H0`–`H5` are all high. `U6.2Y` is low for 4 µs, ending 7 µs before it. `U14.1Q` is the same pulse, one dot later |
| A line, on the oscilloscope. Hook `U14.1Q` on channel 1 and `U14.2Q` on channel 2. Trigger on channel 1, rising edge, at 10 µs a division | Channel 1: a sync pulse low for 4 µs, every 64 µs. Channel 2, on most lines: six short pulses, one every 8 µs, the first 8 µs after the sync ends. On a grid line, channel 2 stays high for 48 µs instead. In the dark border at the bottom it stays low |
| A frame, on the analyzer. Clip `U8.Y` and trigger on its falling edge. Clip also `U3.QA`, `U5.QA`, `U11.2Y` and `U14.1Q`. Sample every 10 µs, view at 2 ms a division | `U8.Y` is low for four lines, a quarter of a millisecond. During those lines `U14.1Q` turns over: mostly low, with short high notches. `U5.QA` goes high about 2 ms before the trigger and drops about 1.5 ms after it: the last 56 lines. `U11.2Y`, the dark border at the bottom, goes high with the trigger and drops with `U5.QA`. `U3.QA` changes at every line throughout |
| The supply, on the meter. Red lead on any chip's `VCC` pin | 5.00 V |

## Troubleshooting

**The TV shows no picture at all.**

1. Put the meter on `U6` pin 14. It must read 5.00 V. Check the other chips' supply pins the same way.
2. Probe `U1.CLK`, the clock from `X1`. P must be lit.
3. Probe `U14.2Q`, the video, and `U14.1Q`, the sync. Each must show P.
4. If one of them is still, follow it back: the video from `U11.3Y`, the sync from `U10.2Y`. Each must show P.

**The picture will not hold still.**

The TV is not getting a clean sync.

1. Set up the line on the analyzer (Test points). `U6.1Y` must pulse low once every 64 µs, as `H0`–`H5` all go high.
2. `U6.2Y` must pulse low for 4 µs, once per line. `U14.1Q` must pulse with it.
3. Set up the frame on the analyzer. `U8.Y` must go low for four lines, once per frame.
4. Probe `U7.Y`. It must show P, mostly H. If it stays H, the line count never reaches its end: probe `U3.QA` to `U3.QD`, `U4.QA`, `U4.QB` and `U5.QA` in turn. Each must show P.
5. Probe `U10.2Y`, the mixed sync. It must show P, mostly H.

**The picture holds still, but the grid is wrong.**

1. Set up the line on the analyzer. The dot counter must count: each of `H0` to `H5` changes half as often as the one before.
2. The line counter must step once per line. Probe `U9.3Y`: P lit, mostly L. Probe the same wire where it arrives, `U3.ENP`, `U3.ENT`, `U4.ENP` and `U5.ENP`: each must read the same.
3. Set up the frame on the analyzer. `U3.QA` must change once per line, every 64 µs, not faster.
4. Probe `U13.2Y`, the vertical lines, and `U13.3Y`, the horizontal lines. Each must show P, mostly L. If one is still, probe that gate's inputs and the counter outputs that feed them.
5. Probe `U12.4Y`, the two joined. It must show P, mostly L.
6. Probe `U13.1Y`, high inside the picture. It must show P, mostly H.

# Test Pattern Generator — Service Manual

## How it works

The picture is drawn one dot at a time, one dot per clock. Two counters say where the beam is: the horizontal counter counts dots along a line (0 to 63), the vertical counter counts lines down the frame (0 to 311). Small gate decoders turn the counts into sync pulses and the grid.

**Clock.** The crystal oscillator `X1` makes a 1 MHz square wave on its pin 8, net `CLK`: one dot per µs. It clocks all five counters and the output register `U14`.

**Horizontal counter.** `U1` counts dots 0 to 15 (`H0`–`H3`) on every clock. Each time it reaches 15 its carry `HC` (`U1` pin 15) lets `U2` count one step (`H4`, `H5`). Together they count 0 to 63: one line every 64 µs. Gate 1 of `U6` spots dot 63 and pulls `LINE_END_N` (`U6` pin 6) low for one dot, which reloads `U2` with 0 so the line starts again. `U9` inverts it into `LINE_END`.

**Vertical counter.** `U3`, `U4` and `U5` form a 9-bit counter (`V0`–`V8`) that counts one step per line, enabled by `LINE_END`. `U7` spots line 311 at the line end and pulls `FRAME_END_N` (`U7` pin 8) low for one dot, which reloads all three with 0: one frame every 312 lines, about 20 ms.

**Sync.** Gate 2 of `U6` makes `HSYNC_N`, low for dots 52 to 55 of every line. `U8` makes `VSYNC_N`, low for lines 308 to 311. `U10` combines the two into composite sync: XOR gate 1 mixes them, XOR gate 2 (other input tied high) inverts the result into `SYNC_N_D`.

**Picture.** `U11` gate 1 and gate 2 detect the blanking areas: dots 48 to 63 (`HBLANK`) and lines 288 to 311 (`VBLANK`). `U13` gate 1 turns them into `ACTIVE`, high inside the 48 × 288 picture. The grid: `U12` and `U13` gate 2 make `GRIDH`, high on every 8th dot (`H0`, `H1`, `H2` all low); `U12` and `U13` gate 3 make `GRIDV`, high on every 16th line (`V0`–`V3` all low). `U12` gate 4 ORs them into `GRID`, and `U11` gate 3 lets it through only while `ACTIVE`: `VIDEO_D`.

**Outputs.** `U14` re-times both signals on the clock, so they leave the board clean, one dot late: flip-flop 1 makes `SYNC_N` (`U14` pin 5), flip-flop 2 makes `VIDEO` (`U14` pin 9). Both go to the monitor through `J2`.

## Components

- `X1` dot clock: 1 MHz crystal oscillator, net `CLK`
- `U1` horizontal counter, low bits: `H0`–`H3`, carry `HC`
- `U2` horizontal counter, high bits: `H4`, `H5`; reloaded with 0 at each line end
- `U3` vertical counter, bits 0–3: `V0`–`V3`; counts at each line end
- `U4` vertical counter, bits 4–7: `V4`–`V7`
- `U5` vertical counter, bit 8: `V8`
- `U6` line decoder: gate 1 makes `LINE_END_N` at dot 63, gate 2 makes `HSYNC_N` for dots 52–55
- `U7` frame-end decoder: `FRAME_END_N` low at the end of line 311
- `U8` vertical sync decoder: `VSYNC_N` low for lines 308–311
- `U9` inverters: `H3_N`, `V3_N` for the decoders, `LINE_END` for the vertical counter
- `U10` sync mixer: composite sync from `HSYNC_N` and `VSYNC_N`, inverted into `SYNC_N_D`
- `U11` blanking and video gate: `HBLANK`, `VBLANK`, and `VIDEO_D` = grid while active
- `U12` grid ORs: combine counter bits for the grid, then the two grid directions into `GRID`
- `U13` grid NORs: `ACTIVE`, `GRIDH` (every 8th dot), `GRIDV` (every 16th line)
- `U14` output register: re-times `SYNC_N` and `VIDEO` on the clock
- `J1` power connector: +5 V and 0 V for the whole board
- `J2` monitor connector: `VIDEO` and `SYNC_N` out

## Troubleshooting

Work from the clock outwards. Every check below is on a working board; compare with what you measure.

- Check `X1` pin 8 (`CLK`): it must pulse. If not, nothing can move: check power at `X1` pin 14.
- Check `U1` pin 15 (`HC`): a short pulse every 16 µs. If missing, look at `U1` and its outputs `H0`–`H3`.
- Check `U6` pin 6 (`LINE_END_N`): a short low pulse every 64 µs. If missing, the horizontal counter or `U6` gate 1 is at fault, and the picture cannot hold still.
- Check `U7` pin 8 (`FRAME_END_N`): a short low pulse every 20 ms. If missing, the vertical counter never restarts: check that each of `V0`–`V8` pulses, each at half the rate of the one before.
- Check `U6` pin 8 (`HSYNC_N`) and `U8` pin 8 (`VSYNC_N`): both must pulse. Without them the monitor cannot lock.
- Check `U14` pin 5 (`SYNC_N`) and `U14` pin 9 (`VIDEO`): both must pulse. If the inputs `U14` pin 2 and `U14` pin 12 pulse but an output does not, suspect `U14`.
- If the picture holds still but the grid is wrong, follow the grid nets back: `GRID` at `U12` pin 11, then `GRIDH` and `GRIDV`, then the counter bits that feed them.

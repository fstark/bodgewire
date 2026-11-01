# Test Pattern Generator — Service Manual

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

# Bouncing Ball — Service Manual

## Components

- `X1` dot clock: 2 MHz crystal oscillator, net `CLK`
- `U1` horizontal counter, low bits: `H0`–`H3`, carry `HC`
- `U2` horizontal counter, high bits: `H4`–`H6`; 128 dots a line
- `U3` vertical counter, bits 0–3: `V0`–`V2` (bit 3 unused); counts at each line end
- `U4` vertical counter, bits 4–7: `V4`–`V7`
- `U5` vertical counter, bit 8: `V8`; with `U3` and `U4` reloaded with 0 after line 311
- `U6` line decoder: `LINE_END_N` at dot 127, `HSYNC_N` for dots 104–111
- `U7` frame-end decoder: `FRAME_END_N` low at the end of line 311
- `U8` vertical decoders: `VSYNC_N` for lines 308–311, `BOTTOM_N` for lines 240–255
- `U9` inverters: `H4_N`, `LINE_END`, `VBALL`, `MR`, `MIX_N`, `HBLANK`
- `U10` sync mixer: composite sync from `HSYNC_N` and `VSYNC_N`, inverted into `SYNC_N_D`
- `U11` output register: re-times `SYNC_N` and `VIDEO` on the clock
- `U12` wall and net decodes: `TOP_A`, `LEFT_A`, `NET_A`
- `U13` decodes: `H56_LOW`, `TOP_B`; `BALL_CK`, the ball height clock
- `U14` walls: `TOP_N` (lines 0–15), `LEFT_N` (dots 0–3); `HBLANK_N`
- `U15` right wall and net: `RIGHT_N` (dots 92–95), `NET_N` (dots 48–49, dashed by `V2`)
- `U16` video mixer: `MIX` high when any of `BALL_N`, `TOP_N`, `BOTTOM_N`, `RIGHT_N`, `NET_N`, `LEFT_N` is low
- `U17` move and video gates: `MOVE` on line 308; `VIDEO_D` = `MIX` outside blanking
- `U18` ball horizontal counter, low: steps every other dot, carry `BHC`
- `U19` ball horizontal counter, high: `HBALL` at its overflow; with `U18` reloaded by `HLOAD_N`
- `U20` ball vertical counter, low: steps each line, carry `BVC`
- `U21` ball vertical counter, high: `VRCO` high on count 255; with `U20` reloaded by `VLOAD_N`
- `U22` ball vertical counter, bit 8: `BV8`, toggled by `VRCO`
- `U23` ball height: cleared on the line of ball count 511, `BALL_OFF` low for 8 lines
- `U24` ball gates: `HLOAD_N`, `VLOAD_N`, `BALL_N`, `MR_N`
- `U25` bounce detectors: `HIT_LEFT`, `HIT_RIGHT`, `HIT_TOP`, `HIT_BOTTOM`
- `U26` direction flip-flops: `HDIR` (high: right), `VDIR_N` (high: up)
- `J1` power connector: +5 V and 0 V for the whole board
- `J2` monitor connector: `VIDEO` and `SYNC_N` out

# Message Display — Service Manual

## Components

- `X1` dot clock: 2 MHz crystal oscillator, net `CLK`
- `U1` dot counter: counts 10–15, `DOT_END` on a character's last dot; 6 dots a character
- `U2` column counter, low bits: `C0`–`C3`, carry `CC`
- `U3` column counter, high bit: `C4`; with `U2`, 21 characters a line, reloaded with 0 by `LINE_END_N`
- `U4` line decoders: `LINE_END_N` on the last dot of character 20, `HSYNC_N` for character 20
- `U5` inverters: `DOT_LOAD_N`, `LINE_END`, `CLK_N`
- `U6` line counter, bits 0–3: `V0` (each glyph line drawn twice), `V1`–`V3` (glyph line); counts at each line end
- `U7` line counter, bits 4–7: `V4`–`V7`, the text row
- `U8` line counter, bit 8: `V8`; with `U6` and `U7` reloaded with 0 after line 311
- `U9` frame-end decoder: `FRAME_END_N` low at the end of line 311
- `U10` vertical sync decoder: `VSYNC_N` for lines 308–311
- `U11` sync mixer: composite sync from `HSYNC_N` and `VSYNC_N`, inverted into `SYNC_N_D`
- `U12` text area: `ACTIVE` high on columns 0–15 of lines 0–255
- `U13` row gates: `R1`–`R3` = `V1`–`V3` while `ACTIVE`, else 0 (the blank line of every character)
- `U14` message ROM (2716): the page, one character code a cell; address `C0`–`C3` and `V4`–`V7`, out `CH0`–`CH5`
- `U15` character ROM (2513): address `R1`–`R3` and `CH0`–`CH5`, out the line's dots `F5` (left) to `F1`
- `U16` dot shift register: loads `F5`–`F1` on `DOT_LOAD_N`, shifts them out on `CLK_N` as `VIDEO_D`
- `U17` output register: re-times `SYNC_N` and `VIDEO` on the clock
- `J1` power connector: +5 V and 0 V for the whole board
- `J2` monitor connector: `VIDEO` and `SYNC_N` out

# Message Display — Service Manual

## Components

- `X1` dot clock: 2 MHz crystal oscillator, drives `CLK`
- `U1` dot counter: counts 10 to 15, 6 dots a character; `DOT_END` high on a character's last dot
- `U2` column counter, low bits: `C0`–`C3`, carry `CC`
- `U3` column counter, high bit: `C4`; with `U2`, 21 characters a line, back to 0 on `LINE_END_N`
- `U4` line decoders: `LINE_END_N` on the last dot of character 20, `HSYNC_N` during character 20
- `U5` inverters: `DOT_LOAD_N`, `LINE_END`, `CLK_N`
- `U6` line counter, bits 0–3: `V0` (each line of a letter drawn twice), `V1`–`V3` (the line within the letter); steps at each line end
- `U7` line counter, bits 4–7: `V4`–`V7`, the line of text
- `U8` line counter, bit 8: `V8`; with `U6` and `U7`, back to 0 after line 311
- `U9` frame-end decoder: `FRAME_END_N` low at the end of line 311
- `U10` vertical sync decoder: `VSYNC_N` during lines 308 to 311
- `U11` sync mixer: mixes `HSYNC_N` and `VSYNC_N` into one sync, `SYNC_N_D`
- `U12` page area: `ACTIVE` high in columns 0 to 15 of lines 0 to 255
- `U13` row gates: pass `V1`–`V3` on to `R1`–`R3` while `ACTIVE` is high, else hold them low (the blank line of every character)
- `U14` message ROM (2716): the page, one character code a place; address `C0`–`C3` and `V4`–`V7`, out `CH0`–`CH5`
- `U15` character ROM (2513): address `R1`–`R3` and `CH0`–`CH5`, out the line's dots `F5` (left) to `F1`
- `U16` dot shift register: loads `F5` to `F1` on `DOT_LOAD_N`, shifts them out on `CLK_N` as `VIDEO_D`
- `U17` output register: re-times `SYNC_N` and `VIDEO` on the clock
- `J1` power connector: +5 V and 0 V for the whole board
- `J2` monitor connector: `VIDEO` and `SYNC_N` out

## Test points

What the logic probe shows on a working board. **P** is the pulse lamp: it stays lit while the signal keeps changing. Almost every signal here changes thousands of times a second, so P is lit nearly everywhere. The H and L lamps then flicker; the table gives the one that shows most. Click a pin below with the probe in hand to probe it.

| Probe | Net | A working board shows |
|---|---|---|
| `X1.OUT` | `CLK` | P lit |
| `U1.RCO` | `DOT_END` | mostly L, P lit: high one dot in six |
| `U5.1Y` | `DOT_LOAD_N` | mostly H, P lit: the same pulse, upside down |
| `U16.CLK` | `CLK_N` | P lit |
| `U3.QA` | `C4` | mostly L, P lit: high for the last 5 characters of each line |
| `U4.1Y` | `LINE_END_N` | H, P lit: low for one dot a line |
| `U4.2Y` | `HSYNC_N` | H, P lit: low for one character a line |
| `U8.QA` | `V8` | mostly L, P lit: high for the last 56 lines of each picture |
| `U9.Y` | `FRAME_END_N` | H, P lit: low for one dot a picture |
| `U10.1Y` | `VSYNC_N` | H, P lit: low for 4 lines a picture |
| `U12.1Y` | `ACTIVE` | mostly H, P lit |
| `U14.A0` | `C0` | P lit |
| `U14.O5` | `CH5` | P lit |
| `U15.A1`, `U15.A2`, `U15.A3` | `R1`, `R2`, `R3` | P lit |
| `U15.O5` to `U15.O1` | `F5` to `F1` | mostly L, P lit: most of the screen is dark |
| `U16.QH` | `VIDEO_D` | mostly L, P lit |
| `U17.1Q` | `SYNC_N` | mostly H, P lit |
| `U17.2Q` | `VIDEO` | mostly L, P lit |
| `U14.CE_N`, `U14.OE_N`, `U15.CE_N` | `GND` | L, steady |
| `U16.CLK_INH`, `U16.SER` | `GND` | L, steady |
| `U1.CLR_N` | `VCC` | H, steady |

The logic analyzer shows what the probe cannot: the order of things. Four setups, each on a working board:

| Clip | Trigger, sample, view | A working board shows |
|---|---|---|
| The dots: `X1.OUT`, `U1.RCO`, `U5.1Y`, `U16.CLK`, `U16.QH`, `U17.2Q` | `U16.QH` rising; sample every 100 ns; 2 µs a division | `U1.RCO` high for one clock in six, every 3 µs; `U5.1Y` the same, upside down. `U16.QH` shows the dots of a few letters, each dot one clock long. `U17.2Q` is the same pattern, half a clock later |
| The line: `U2.QA` to `U2.QD` as one group, `U3.QA`, `U4.1Y`, `U4.2Y`, `U8.QA`, `U12.1Y`, `U17.1Q` | `U4.2Y` falling and `U8.QA` low; sample every 100 ns; 10 µs a division | `U4.2Y` low for 3 µs, every 63 µs. `U3.QA` high for the 15 µs before it. `U12.1Y` low while `U3.QA` is high. `U4.1Y` a short low spike at the end of `U4.2Y`'s low. `U17.1Q` low as `U4.2Y`, one clock later. The group counts 0 to F, then 0 to 4 while `U3.QA` is high: 21 characters a line |
| The lookups: `U14.O0` to `U14.O7` as one group, `U15.O1` to `U15.O5` as one group, `U13.1Y`, `U13.2Y`, `U13.3Y`, `U16.QH` | the `U14` group at 48 (the code of H), `U13.1Y` low, `U13.2Y` low, `U13.3Y` high; sample every 100 ns; 2 µs a division | The `U14` group spells part of the page, a new code every 3 µs. On the first line of the poem it reads 22 48 4F 50 45 22 20: `"HOPE"` and a space. The `U15` group follows each code half a microsecond later, with the dots of line 4 of that letter: H 1F (five dots), O 11, P and E 1E, the quote mark 00. `U16.QH` sends those dots out one by one |
| The picture: `U7.QA` to `U7.QD` as one group, `U8.QA`, `U10.1Y`, `U12.1Y`, `U17.1Q` | `U10.1Y` falling; sample every 1 µs; 2 ms a division | `U10.1Y` low for a quarter of a millisecond, every 20 ms. Then `U8.QA` falls and the group counts 0 to F, 1 ms each: the 16 lines of text. `U12.1Y` pulses all through them. Then `U8.QA` is high for about 3.5 ms, and `U12.1Y` stays low |

## Troubleshooting

**No picture, or the picture rolls or tears.**

1. Probe `X1.OUT`. It must show P.
2. Probe `U1.RCO`, then `U4.2Y`. Each must show P.
3. Probe `U10.1Y` and `U9.Y`. Each must read H with P lit.
4. Probe `U17.1Q`, then `J2.SYNC_N`. Both must read mostly H with P lit.
5. Use the analyzer's line and picture setups. The first one that differs from a working board shows which counter has gone wrong.

**The picture is locked, but the page stays dark.**

1. Probe `U12.1Y`. It must read mostly H with P lit.
2. Probe `U14.A0`, then `U14.O5`. Each must show P.
3. Probe `U15.O5` to `U15.O1`. Each must show P.
4. Probe `U16.CLK`, then `U16.QH`, then `U17.2Q`. Each must show P.
5. The first pin that stays still is where the dots stop.

**The page shows, but the text is wrong.**

1. Look at what is wrong. Wrong characters point to the codes between `U14` and `U15`. Wrong shapes point to the line within the letter or to the dots. Text where the border should be points to `U12` and `U13`.
2. Probe each code at both ends: `U14.O0` to `U14.O5`, and `U15.A4` to `U15.A9`. Both ends must show P.
3. Probe the line within the letter: `U6.QB`, `U6.QC`, `U6.QD`, then `U15.A1`, `U15.A2`, `U15.A3`. All must show P.
4. Probe the dots at both ends: `U15.O5` to `U15.O1`, then `U16.H`, `U16.G`, `U16.F`, `U16.E`, `U16.D`. Both ends must show P.
5. Use the analyzer's lookup setup. On a working board, the codes spell the page and the dots match the letters.

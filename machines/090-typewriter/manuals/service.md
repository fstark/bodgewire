# TV Typewriter — Service Manual

## Components

- `X1` dot clock: 4 MHz crystal oscillator, net `CLK`
- `U1` dot counter: 6 dots a character (it counts 10 to 15), `DOT_END` on the last dot; its `QB` and `QC` time the page and row buffer clocks
- `U2` column counter, low bits: `C0`–`C3`, carry `CC`
- `U3` column counter, high bits: `C4`, `C5`; with `U2`, 42 characters a line, reloaded with 0 by `LINE_END_N`
- `U4` line decoders: `LINE_END_N` on the last dot of character 41, `HSYNC_N` for characters 40–41
- `U5` inverters: `DOT_LOAD_N`, `CLK_N`; `QB_N` for the page clocks; `D1_N`, `D4_N` for the Return decoder
- `U6` line counter, bits 0–3: `V0` (each glyph line drawn twice), `V1`–`V3` (glyph line); clocked by `LINE_END`
- `U7` line counter, bits 4–7: `V4`–`V7`, the text row
- `U8` line counter, bit 8: `V8`; with `U6` and `U7` reloaded with 0 after line 311
- `U9` frame-end decoder: `FRAME_END_N` low during line 311
- `U10` vertical sync decoder: `VSYNC_N` low for lines 308–311; `ROW_CLK`, the row buffer's clock, once a character in the text
- `U11` sync mixer: composite sync from `HSYNC_N` and `VSYNC_N` into `SYNC_N_D`; two gates compare `V6`, `V7` with `Y2`, `Y3` (`NE7`, `NE8`)
- `U12` text area `ACTIVE` (columns 0–31 of lines 0–255); `CTL`, high for control codes; `CUR_SHOW`, the blinking cursor; `MATCH`
- `U13` row gates: `GL1`–`GL3`, the copy of `V1`–`V3` while `ACTIVE` is high, else 0; `ROW_TOP`, the first line of a text row
- `U14` row top gates: `V01_ZERO` (`V0` and `V1` low), `V23_ZERO` (`V2` and `V3` low); `CLEAR`; `GO`
- `U15` page clock gates: `PHI2_N` on dots 12–13, `PHI1_N` on dot 15 of characters 0–31 of a row's top line
- `U16` page (2524), bit 0 of each character: 512 cells, round once a frame, out on `P0`, written from `W0` while `WRITE` is high
- `U17` page (2524), bit 1: `P1`, `W1`
- `U18` page (2524), bit 2: `P2`, `W2`
- `U19` page (2524), bit 3: `P3`, `W3`
- `U20` page (2524), bit 4: `P4`, `W4`
- `U21` page (2524), bit 5: `P5`, `W5`
- `U22` row buffer (2518): the 32 characters of the row being drawn on `B0`–`B5`, round once a line on `ROW_CLK`; takes them from `P0`–`P5` while `ROW_LOAD_N` is low
- `U23` character ROM (2513): address `GL1`–`GL3`, `B0`–`B5`; out the line's dots `F5` (left) to `F1`
- `U24` dot shift register: loads `F5`–`F1` on `DOT_LOAD_N`, shifts them out on `CLK_N` as `VIDEO_D`
- `U25` output register: re-times `SYNC_N`, `VIDEO` and `LINE_END` (the first dot of each line) on the clock
- `U26` keyboard buffer: drives `D0`–`D5` onto the write bus `W0`–`W5` except during Clear
- `U27` space buffer: drives the code of a space onto `W0`–`W5` while `CLEAR_N` is low
- `U28` key flip-flops: `KEY` set by `STROBE`; `WR` after the key's cell is written, clears `KEY`
- `U29` write gates: `WRK_N`, `WRITE` (a printable key's cell, or Clear); `ROW_LOAD_N`, low on a row's top line
- `U30` Return decoder: `CR_N` low while the last key was Return
- `U31` key gates: `PRINT`, `ADV` (printable or Return), `XC_N` (column 31), `YEN` (step the row)
- `U32` cursor column, bits 0–3: `X0`–`X3`, carry `XRCO`; loaded with 0 by `CR_N`
- `U33` cursor column, bit 4: `X4`
- `U34` cursor row: `Y0`–`Y3`; all three cursor counters step when `WR` rises, cleared by `CLEAR_N`
- `U35` cursor comparator: `NE0`–`NE3` high where `C0`–`C3` and `X0`–`X3` differ
- `U36` cursor comparator: `NE4`–`NE6` for `C4`, `V4`, `V5` against `X4`, `Y0`, `Y1`; gate 4 swaps black and white in the cursor's cell while `CUR_SHOW` is high (`VIDEO_M`)
- `U37` cursor comparator: `EQ0`–`EQ2`, each high where three bits agree
- `U38` cursor match: `MATCH_N` low while the beam is on the cursor's cell; `GO_N` low while a key waits and its cell is at the page's output
- `U39` cursor delay: `CURSOR_N`, `MATCH_N` one character late, clocked by `DOT_END`
- `U40` blink counter: counts frames on `V8`; `BLINK` changes every 8 frames
- `SW1` Clear button: pulls `CLEAR_N` low while pressed
- `R1` 4.7 kΩ pull-up on `CLEAR_N`
- `J1` power connector: +5 V and 0 V for the whole board
- `J2` monitor connector: `VIDEO` and `SYNC_N` out
- `J3` keyboard connector: the key's code on `D0`–`D6`, `STROBE` high for 25 ms after each key; the keyboard is powered from the board

## Test points

What the logic probe shows on a working board. **P** is the pulse lamp: it stays lit while the signal keeps changing. Click a pin below with the probe in hand to probe it.

Most signals here change thousands of times a second: the probe shows only P. The logic analyzer shows their shape. "Idle" means no key typed and CLEAR not pressed. "After CLEAR" means CLEAR pressed and let go, nothing typed since.

| Probe | Net | A working board shows |
|---|---|---|
| `U1.CLK` | `CLK` | P lit, always |
| `U1.RCO` | `DOT_END` | P lit, always |
| `U25.4Q_N` | `LINE_END` | P lit, always |
| `U4.2Y` | `HSYNC_N` | P lit, always |
| `U10.1Y` | `VSYNC_N` | P lit, always |
| `U25.1Q` | `SYNC_N` | P lit, always |
| `U8.QA` | `V8` | P lit: it changes 100 times a second |
| `U12.1Y` | `ACTIVE` | P lit, always |
| `U13.4Y` | `ROW_TOP` | P lit, always |
| `U15.1Y`, `U15.2Y` | `PHI1_N`, `PHI2_N` | P lit, always. The same on the PHI1 and PHI2 pins of each page chip, `U16`–`U21` |
| `U16.OUT` to `U21.OUT` | `P0`–`P5` | idle: P lit, the random page going round. After CLEAR: `U16.OUT` to `U20.OUT` L, `U21.OUT` H, steady: every cell holds a space |
| `U10.2Y` | `ROW_CLK` | P lit, always |
| `U29.3Y` | `ROW_LOAD_N` | P lit, always |
| `U22.OUT1`, `U22.OUT6` | `B0`, `B5` | idle: P lit. After CLEAR: `U22.OUT1` L, `U22.OUT6` H, steady |
| `U13.1Y` | `GL1` | P lit, always |
| `U23.O1` | `F1` | idle: P lit. After CLEAR: L, steady: a blank page has no dots |
| `U25.2Q` | `VIDEO` | P lit; after CLEAR only the blinking cursor keeps it changing |
| `U40.QD` | `BLINK` | P flashes about six times a second |
| `U12.3Y` | `CUR_SHOW` | P flashes with the blinking cursor |
| `U29.2B` | `CLEAR_N` | H; L while CLEAR is held |
| `U29.2Y` | `WRITE` | L idle; H while CLEAR is held |
| `U21.IN` | `W5` | H while CLEAR is held (the space's one high bit). `U16.IN` (`W0`) L then |
| `U28.1CLK` | `STROBE` | L; H for 25 thousandths of a second after each key: P flashes |
| `U28.1Q` | `KEY` | L idle; after a key, H until the cursor's cell comes round, at most a fiftieth of a second |
| `U28.2Q` | `WR` | L; a very short pulse after each key: P flashes |
| `U32.QA` | `X0` | after CLEAR: L. Changes at each printable key: H after one key, L after two |
| `U34.QA` | `Y0` | after CLEAR: L. H after one Return |
| `U30.Y` | `CR_N` | H; L after Return, until the next key |
| `U12.2Y` | `CTL` | L after a letter; H after Return |

**Logic analyzer setups.** Each one starts from a working board, switched on.

| Clip | Trigger and timing | A working board shows |
|---|---|---|
| `U12.1Y` (`ACTIVE`), `U6.QB` (`V1`), `U25.1Q` (`SYNC_N`), `U25.2Q` (`VIDEO`), `U10.2Y` (`ROW_CLK`) | trigger on `U12.1Y` rising, with `U6.QB` at 1; sample every 100 ns; 10 µs a division | one line of text, 63 µs long. `ACTIVE` is high for the first 48 µs: the 32 characters. `ROW_CLK` pulses once a character while `ACTIVE` is high. `VIDEO` shows dots in the same stretch. `SYNC_N` is low for 3 µs near the end of the line |
| `U15.1Y` (`PHI1_N`), `U15.2Y` (`PHI2_N`), `U13.4Y` (`ROW_TOP`), `U10.2Y` (`ROW_CLK`), `U29.3Y` (`ROW_LOAD_N`), `U16.OUT` (`P0`) | trigger on `U15.1Y` falling; sample every 100 ns; 10 µs a division | the top line of a text row. `ROW_TOP` is high for the whole line, and `ROW_LOAD_N` low. `PHI2_N` and then `PHI1_N` pulse low 32 times, once a character. `P0` changes from cell to cell. On the next line `ROW_TOP` falls and only `ROW_CLK` goes on |
| `U10.1Y` (`VSYNC_N`), `U8.QA` (`V8`), `U13.4Y` (`ROW_TOP`), `U12.1Y` (`ACTIVE`) | trigger on `U10.1Y` falling; sample every 1 µs; 2 ms a division | one picture, about 20 ms. `VSYNC_N` is a short low pulse once a picture. `V8` is low for the first 16 ms, the text, and high for the rest. `ROW_TOP` pulses about once a millisecond. `ACTIVE` pulses only while `V8` is low |
| `U28.1CLK` (`STROBE`), `U28.1Q` (`KEY`), `U29.2Y` (`WRITE`), `U28.2Q` (`WR`), `U32.QA` (`X0`) | after CLEAR; trigger on `U28.1CLK` rising, then type a letter; sample every 1 µs; 5 ms a division | `STROBE` high for 25 ms. `KEY` rises with it and falls when the cursor's cell comes round, within 20 ms. At that moment `WRITE` and `WR` give one very short pulse each, and `X0` changes: the cursor has moved |

## Troubleshooting

**No picture, or the picture does not lock.**

1. Probe `U1.CLK`. It must show P.
2. Probe `U1.RCO` and `U25.4Q_N`. Each must show P. If one is dark, the beam counters stop there.
3. Probe `U4.2Y` and `U10.1Y`, the two syncs. Each must show P.
4. Probe `U25.1Q`, the sync to the TV. It must show P.
5. Set up the first and third analyzer setups. The line must be 63 µs long, with `SYNC_N` low for 3 µs. The picture must be about 20 ms long.

**The picture locks, but the characters are wrong.**

Press CLEAR first: a blank page is the easiest to check.

1. Probe the page clocks, `U15.1Y` and `U15.2Y`. Each must show P.
2. Probe the two clock pins of each page chip: `U16.PHI1`, `U16.PHI2`, and so on to `U21.PHI2`. Each must show P. A page chip whose clock stops loses its bit in every character.
3. Probe the page's outputs, `U16.OUT` to `U21.OUT`. `U16.OUT` to `U20.OUT` must read L, and `U21.OUT` H: a space in every cell.
4. Probe `U29.3Y` (`ROW_LOAD_N`) and `U10.2Y` (`ROW_CLK`). Each must show P. Set up the second analyzer setup: `ROW_LOAD_N` must be low for the whole top line of a row.
5. Probe the row buffer's outputs, `U22.OUT1` to `U22.OUT6`. They must read the same as the page's outputs, in order.
6. Probe `U13.1Y` (`GL1`). It must show P.
7. Probe `U23.O1`. It must read L, steady: a blank page has no dots.

**CLEAR does not blank the page.**

1. Probe `U29.2B`. It must read L while CLEAR is held.
2. While CLEAR is held, probe `U29.2Y` (`WRITE`): H. Probe `U21.IN`: H. Probe `U16.IN`: L.
3. After CLEAR, the cursor must be at the top left. Probe `U32.QA` and `U34.QA`: both L.

**Typing goes wrong: a key does nothing, lands in the wrong place, or fills the page.**

1. Probe `U29.2Y` (`WRITE`) with no key typed. It must read L. Its inputs, `U29.2A` and `U29.2B`, must both read H.
2. Press CLEAR, then type a letter. Probe `U28.1CLK`. P must flash at the key.
3. Probe `U28.1Q`. It must read L again within a fiftieth of a second. If it stays H, the key never finds its cell: probe `U38.1Y` (`MATCH_N`), which must show P.
4. Set up the fourth analyzer setup and type a letter. `WRITE` and `WR` must each pulse once, and `X0` must change.
5. Type Return. Probe `U30.Y`: it must read L. Probe `U34.QA`: it must change.
6. Type a letter. Probe `U30.Y` again: it must read H.

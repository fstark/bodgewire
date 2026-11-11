# 090 — TV Typewriter

Ninth rung of the tutorial ladder. A board between an ASCII keyboard and a TV: what is typed shows on the screen, 16 lines of 32 characters, white on black, with a blinking block where the next character goes. Return starts a new line; after the last line the cursor goes back to the top and types over the page (no scrolling). A Clear button blanks the page and sends the cursor home. Inspired by the 1973 TV Typewriter (half of the Apple 1): the video chain of 080 with RAM where the message ROM was, and the keyboard writing into that RAM at a cursor. Its code comes with roadmap step P4p ([TV_TYPEWRITER.md](../../p4p-tv-typewriter/TV_TYPEWRITER.md)); the RAM, the bus buffer and the keyboard with P4n ([RAM_BUS.md](../../p4n-ram-bus/RAM_BUS.md)).

## Lesson

One memory, two users taking turns. The beam reads the page RAM all through the text area, one character code a cell, as 080 read its ROM. The keyboard writes into it only while the beam is in the border to the right of the text. An address multiplexer chooses whose address the RAM sees; tri-state buffers choose who drives the data bus, so the RAM, the keyboard and the Clear spaces never drive it at once. The cursor is a counter, stepped once a character is written; Return is a decoded code that loads the column with 0 and steps the line. The cursor shows on the screen by comparing that counter with the beam's own counters, one character late as the picture is.

Faults show as typing gone wrong in ways that point to one link: a key lands in the wrong place (an address line through the multiplexer), every key prints the wrong character (a bus line), keys never show (the write strobe), letters show but the cursor does not move (the cursor counter's enable), the cursor sits in the wrong place or not at all (the comparator), Clear leaves part of the page (the space buffer).

## Circuit

PAL-like and progressive, one dot per clock, four dots per microsecond: 080's timing at twice the dot rate, so a line holds 32 characters.

| | Value |
|---|---|
| Dot clock | `X1` 4 MHz `OSC` → `CLK` (0.25 µs per dot) |
| Character | 6 dots = 1.5 µs: 5 dots of the glyph and 1 dark dot |
| Line | 42 characters = 252 dots = 63 µs (15.87 kHz) |
| Frame | 312 lines = 19.656 ms (≈ 50.9 Hz) |
| Text | 32 columns × 16 rows; a row is 16 lines (8 glyph lines, each drawn twice) |
| Active picture | line characters 1–32 (the text, one character late), lines 0–255 |
| Horizontal sync | characters 40–41 (3 µs) |
| Vertical sync | lines 308–311 |
| Write window | characters 32–41 (the right border): the cursor's address on the RAM |

### Beam counters (080's, at 4 MHz)

- Dot counter `U1` 74LS161 counts 10 to 15 and reloads 10: 6 dots a character, `DOT_END` on the last dot; `U5` inverts it into `DOT_LOAD_N`.
- Column counter `C0`–`C5`: `U2` (`C0`–`C3`, carry `CC`) and `U3` (`C4`, `C5`) 74LS161, stepped by `DOT_END`, reloaded with 0 after character 41: `U4` gate 1 decodes `LINE_END_N` = NAND(`DOT_END`, `C5`, `C3`, `C0`), 41 being the first count with those three bits high. `U21`'s fourth flip-flop re-times it on the clock: its `4Q_N` is `LINE_END`, high for the first dot of each line, free of the decoder's glitches (it clocks counters).
- `U4` gate 2 makes `HSYNC_N` = NAND(`C5`, `C3`): characters 40 and 41.
- Line counter `V0`–`V8`: `U6`, `U7`, `U8` 74LS161 clocked by `LINE_END` (not by the dot clock with an enable as on 080: three fewer chips on the 4 MHz clock), reloaded with 0 after line 311 by `U9` 74LS30 (`FRAME_END_N`, low through line 311). `U10` gate 1 decodes `VSYNC_N` for lines 308–311; `U11` mixes the syncs into `SYNC_N_D` (as 060 and 080).
- Within a text row, `V0` repeats each glyph line twice, `V1`–`V3` are the glyph line, `V4`–`V7` the text row.

### Page RAM and the picture

- Address multiplexer `U14`–`U16` 74LS157, select `C5`: during characters 0–31 the RAM address `RA0`–`RA8` is the beam's (`C0`–`C4` the column, `V4`–`V7` the row); during characters 32–41 it is the cursor's (`X0`–`X4`, `Y0`–`Y3`). Cell address = row × 32 + column.
- Page RAM `U17`, `U18` 2114 (1K × 4, the lower 512 words used; `A9` low, always selected): `U17` holds bits 0–3 of each code, `U18` bits 4–5 on `IO1`–`IO2`. Their data pins are the bus `B0`–`B5`.
- The bus is the character address of the character ROM `U19` 2513 (`A4`–`A9`), as `CH0`–`CH5` were on 080. Blanking, the 2513, the shift register `U20` 74LS165 and the inverted shift clock are 080's: `U12` gate 1 makes `ACTIVE` = NOR(`C5`, `V8`), `U13` gives the 2513 `R1`–`R3` = `V1`–`V3` AND `ACTIVE`, so outside the text it reads line 0, blank in every character. The cursor's cell, read during the write window, never shows.
- Output register `U21` 74LS175 re-times `SYNC_N` and `VIDEO` on the clock (080's 74LS74), and with them the write strobe (below) and `LINE_END`.

Timing: the column counter steps at the start of a character; the address passes the multiplexer in 9–18 ns, the 2114 answers 450 ns later and the 2513 500 ns after that, under four dots; the shift register loads on the sixth. A character addressed during line character k is shown during k + 1, as on 080.

### Writing a key

- Keyboard `J3` (`KEYBOARD strobe=100us`): a key's code on `D0`–`D6`, then `STROBE` high for 100 µs; the code stays until the next key, which comes at least 1 µs after `STROBE` falls.
- `U24` 74LS74 half 1 (`KEY`) is set by the rise of `STROBE`. Half 2 (`WR`) takes `KEY` at the start of character 32 (clocked by `C5`): `WR` high means "this line writes".
- Write strobe: `U25` 74LS30 makes `WE_D` = NAND(`WR`, `C5`, `C1`, `C0`, `PRINT`), low during characters 35 and 39. `U21` re-times it on the dot clock into `WEK_N`, so the decoder's glitches at character boundaries never reach the RAM: `WEK_N` is low for 6 dots, one dot after each of those characters starts, well inside the write window.
- `WEK_N` enables `U22` 74LS244, which drives the key's bits 0–5 (`D0`–`D5`) onto the bus, and through `U13` gate 4 (`WE_N` = `WEK_N` AND `CLEAR_N`) writes the RAMs at the cursor's address. The RAMs let go of the bus 100 ns after `WE_N` falls; until then the bus fights (X), as on the P4n bench, long before the write ends.
- Printable codes only: `U12` gate 2 makes `CTL` = NOR(`D6`, `D5`), high for control codes 0–31; `U27` gate 1 inverts it into `PRINT`. The code's bit 6 is not stored: the 2513 takes the low 6 bits of an upper-case code.
- At the line's end (`LINE_END` rising, at the start of the next line's character 0) the cursor steps (below), and `U10` gate 2 clears `KEY` (`KEY_CLR_N` = NAND(`WR`, `LINE_END`)). At the next start of character 32, `WR` takes `KEY` again: low unless a new key came.
- A key is written and the cursor stepped at most 79 µs after its `STROBE` rises (up to one line waiting for character 32, then that line's write window and its end). The next key comes at least 101 µs after the rise. Hence the 100 µs strobe: a key's code must stay on `D0`–`D6` until its write and step are over, and the next `STROBE` must rise after `KEY` is cleared.

### Cursor

- Cursor counter: `U28` 74LS161 (`X0`–`X3`), `U29` (`X4`, its `QA`; `QB`–`QD` unused), `U30` (`Y0`–`Y3`), clocked by `LINE_END`, counting when `WR` (`ENP`) and their carry (`ENT`) are high, so at most once a line, at the line's end, after the write window and while no write is on.
- `ADV` (`U27` gate 2) = `PRINT` OR `CR`: the cursor moves for a printable key or Return, not for other control codes. `U28` counts on `ADV`; its carry `XRCO` (`X0`–`X3` = 15) steps `U29`; `XC_N` (`U27` gate 3) = NAND(`XRCO`, `X4`) marks column 31; `YEN` (`U27` gate 4) = `XC` OR `CR` steps `U30`. So column 31 wraps to column 0 of the next row, and row 15 to row 0: the page wraps.
- Return: `U26` 74LS30 decodes `CR_N` (code 13 = `0001101`) from `CTL` (bits 6 and 5 low), `D4_N`, `D3`, `D2`, `D1_N`, `D0` (`D1_N` and `D4_N` from `U5`). `CR_N` loads 0 into `U28` and `U29` at every line end while 13 is the last code sent, and `YEN` steps the row once, with `WR`. Nothing is written.
- Clear: `SW1` pulls `CLEAR_N` low (`R1` pulls it up). While held: `U23` 74LS244 drives the space code 0x20 onto the bus, `WE_N` is held low so the RAMs write the bus at every address the beam goes past, and the cursor counters are cleared to 0. Every cell is written within one frame (20 ms); the press of a button lasts longer.
- Cursor on the screen: `U31`, `U32` and two gates of `U11` (74LS86) compare `X0`–`X4` with `C0`–`C4` and `Y0`–`Y3` with `V4`–`V7` (`NE0`–`NE8`, high where they differ); `U33` 74LS27 NORs them in threes (`EQ0`–`EQ2`); `U34` gate 1 makes `MATCH_N` = NAND(`EQ0`, `EQ1`, `EQ2`, `ACTIVE`), low while the beam addresses the cursor's cell. `U35` half 1, clocked by `DOT_END`, delays it to the character that cell is shown in (`CURSOR_N`, from the last dot of the cell's character to the last dot of the next). `U36` 74LS93 counts frames (each fall of `V8`, at line 0): `BLINK` (its `QD`) changes every 8 frames, 3.2 blinks a second. `U12` gate 3 makes `CUR_SHOW` = NOR(`CURSOR_N`, `BLINK`); `U32` gate 4 XORs it into the video (`VIDEO_M` into `U21`): the cursor's cell inverted, all 6 dots and 16 lines, every other 8 frames.

No reset: the beam counters fall into step within a frame as on 080. The RAMs, the cursor counter, `KEY` and `WR` power on at random: the page shows random characters and the cursor sits anywhere until Clear is pressed, like the original.

## When it works

The TV shows a page of random characters at power-on, locked within half a second, with a blinking block somewhere. Pressing Clear blanks the page and puts the block at the top left. Each key typed appears where the block was and the block moves one place right; after 32 characters it goes on at the start of the next line, Return starts the next line at once, other control keys (Backspace, Esc, CTRL) do nothing, and after the 16th line it goes back to the top line and types over what is there. Letters show in upper case, white on black, with a dark border all round.

## Puzzles

None in this step: P4q writes the 090 Puzzles from the finished Board. Links worth a Fault: an address line between the multiplexer and the RAMs (keys land in pairs of cells, or a key typed shows in two places), a bus line (every key a different character, the cursor fine), the write strobe re-timing (`WEK_N` stuck: nothing typed shows, or each key smeared over the window's neighbours), the cursor counter's carry (lines never change), the comparator (the cursor shows on the wrong row), the space buffer (Clear leaves `@`s).

Netlist, Board and manuals: [TYPEWRITER_DESIGN.md](TYPEWRITER_DESIGN.md).

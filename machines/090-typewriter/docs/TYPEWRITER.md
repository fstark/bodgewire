# 090 — TV Typewriter

Ninth rung of the tutorial ladder. A board between an ASCII keyboard and a TV: what is typed shows on the screen, 16 lines of 32 characters, white on black, with a blinking block where the next character goes. Return starts a new line; after the last line the cursor goes back to the top and types over the page (no scrolling). A Clear button blanks the page and sends the cursor home. Built as the 1973 TV Typewriter (half of the Apple 1) was: the page is held in MOS shift registers, memory without addresses, going round once a frame in step with the beam. The video chain is 080's. Its code comes with roadmap step P4q ([SHIFT_TYPEWRITER.md](../../../docs/p4q-shift-typewriter/SHIFT_TYPEWRITER.md)); the keyboard with P4n ([RAM_BUS.md](../../../docs/p4n-ram-bus/RAM_BUS.md)). P4p built it first on two 2114 RAMs ([TV_TYPEWRITER.md](../../../docs/p4p-tv-typewriter/TV_TYPEWRITER.md)).

## Lesson

Memory without addresses. The page is six long shift registers, one per bit of the character code, 512 cells each: a cell can only be read or written when it comes round to the register's output, and the page comes round exactly once a frame because the registers are clocked 512 times a frame, in step with the beam. A short register, the row buffer, holds the 32 characters of the text row being drawn and brings them back on each of the row's 16 lines. To type, the board waits for the cursor's cell to come round and switches the page from recirculate to write for that one cell. To clear, it writes every cell for a whole frame.

The page registers are dynamic: they keep their bits only while both their clocks keep pulsing. Stop one and that register's bits all fade to one level: one bit of every character is lost.

Faults show as typing gone wrong in ways that point to one link: a page clock lost (one bit of every character fades to 0: each letter becomes a lookalike), the write control (every cell fills with the key), the row buffer's load control (every row shows the same text), the cursor comparator (keys land on the wrong line), a page output held (every character shifted along the font, Clear leaves zeros), the Return decoder (a letter acts as Return).

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
| Page shifts | characters 0–31 of the first line of each text row: 32 a row, 512 a frame |

### Beam counters (080's, at 4 MHz)

- Dot counter `U1` 74LS161 counts 10 to 15 and reloads 10: 6 dots a character, `DOT_END` on the last dot; `U5` inverts it into `DOT_LOAD_N`. Its outputs `QB`, `QC` and `DOT_END` also time the memory clocks within a character (dots 10 = `1010` to 15 = `1111` on `QD`–`QA`).
- Column counter `C0`–`C5`: `U2` (`C0`–`C3`, carry `CC`) and `U3` (`C4`, `C5`) 74LS161, stepped by `DOT_END`, reloaded with 0 after character 41: `U4` gate 1 decodes `LINE_END_N` = NAND(`DOT_END`, `C5`, `C3`, `C0`). `U25`'s fourth flip-flop re-times it on the clock: its `4Q_N` is `LINE_END`, high for the first dot of each line, free of the decoder's glitches (it clocks counters).
- `U4` gate 2 makes `HSYNC_N` = NAND(`C5`, `C3`): characters 40 and 41.
- Line counter `V0`–`V8`: `U6`, `U7`, `U8` 74LS161 clocked by `LINE_END`, reloaded with 0 after line 311 by `U9` 74LS30 (`FRAME_END_N`). `U10` gate 1 decodes `VSYNC_N` for lines 308–311; `U11` mixes the syncs into `SYNC_N_D`.
- Within a text row, `V0` repeats each glyph line twice, `V1`–`V3` are the glyph line, `V4`–`V7` the text row. `U12` gate 1 makes `ACTIVE` = NOR(`C5`, `V8`): characters 0–31 of lines 0–255.

### Page and row buffer

- Page `U16`–`U21`: six Signetics 2524 512-bit dynamic recirculating shift registers, `U16` bit 0 of every code to `U21` bit 5. Each has two clock inputs, `PHI1` (output clock) and `PHI2` (input clock), pulsed low in turn. On the rise of `PHI1` the next cell comes to the output (`P0`–`P5`, 100 ns later); on the rise of `PHI2` the cell at the output is written from `IN` (the write bus `W0`–`W5`) if `WRITE` is high, or kept (recirculated) if it is low. `READ` is tied high (output enabled).
- Row top: `U14` gates 1–2 make `V01_ZERO` = NOR(`V0`, `V1`) and `V23_ZERO` = NOR(`V2`, `V3`); `U13` gate 4 ANDs them into `ROW_TOP`, high on the first line of each row (and on four lines in the blanking, which `ACTIVE` keeps out). That line shows glyph line 0, blank in every character, so what the row buffer does on it never shows.
- Page clocks: `U15` 74LS20 makes `PHI2_N` = NAND(`ROW_TOP`, `ACTIVE`, `QC`, `QB_N`), low during dots 12–13 (`QB_N` from `U5`), and `PHI1_N` = NAND(`ROW_TOP`, `ACTIVE`, `DOT_END`), low during dot 15, in each of characters 0–31 of a row's top line. One signal makes each edge, so the pulses are clean. 32 pairs a row, 16 rows: the page goes round exactly once a frame, and cell `r × 32 + k` is at the output through character `k` on the top line of row `r` (it came out as character `k` − 1 ended). The longest pause between pulses, from line 240 to line 0 of the next frame, is 4.5 ms.
- Row buffer `U22`: a Signetics 2518 hex 32-bit static shift register, its inputs `IN1`–`IN6` on `P0`–`P5`, its outputs `OUT1`–`OUT6` the bus `B0`–`B5`. On each rise of `CLK` it puts the word at its output back in (`REC` high) or takes its inputs instead (`REC` low), then shows the next word 300 ns later. `U10` gate 2 makes its clock `ROW_CLK` = NAND(`QC`, `ACTIVE`), low during dots 12–15 of characters 0–31, rising as the next character starts (as `QC` falls; `ACTIVE` changes 10 ns after `QC`, so no glitch at characters 0 and 32): 32 clocks a text line, so the buffer goes round once a line. `U29` gate 3 makes `ROW_LOAD_N` = NAND(`V01_ZERO`, `V23_ZERO`), into `REC`: on a row's top line the buffer takes the 32 cells of that row from the page, one at the end of each character, while the page shows them.
- The bus `B0`–`B5` is the character address of the character ROM `U23` 2513 (`A4`–`A9`), as on 080. `U13` gates 1–3 give the 2513 `R1`–`R3` = `V1`–`V3` AND `ACTIVE`, so outside the text it reads line 0, blank. The 2513, the shift register `U24` 74LS165 and the inverted shift clock are 080's; `U25` 74LS175 re-times `SYNC_N`, `VIDEO` and `LINE_END` on the clock.

Timing within character `k` (dots 10–15): the column steps at the start of dot 10, and the row buffer has just shifted (its clock rose at the same moment), so word `k` is on `B0`–`B5` 300 ns later; the 2513 answers 500 ns after that, during dot 13; the 74LS165 loads on dot 15. Character `k` is shown during `k` + 1, as on 080. On a row's top line, cell `k` is on `P0`–`P5` from the start of character `k`, a write happens at the end of dot 13, and at the end of dot 15 the row buffer takes the cell, into the word it shows in column `k`, as the page shifts the next cell out.

### Writing a key

- Keyboard `J3` (`KEYBOARD strobe=25ms`): a key's code on `D0`–`D6`, then `STROBE` high for 25 ms; the code stays until the next key, which comes at least 1 µs after `STROBE` falls. A key waits at most one frame (19.7 ms) for its cell, so its code is still there when the cell comes round. Keys come at most 40 a second (the original managed about 30); faster ones wait in the keyboard.
- `U28` 74LS74 half 1 (`KEY`) is set by the rise of `STROBE`.
- Cursor match: `U35`, `U36` and two gates of `U11` (74LS86) compare `X0`–`X4` with `C0`–`C4` and `Y0`–`Y3` with `V4`–`V7` (`NE0`–`NE8`); `U37` 74LS27 NORs them in threes (`EQ0`–`EQ2`); `U38` gate 1 makes `MATCH_N` = NAND(`EQ0`, `EQ1`, `EQ2`, `ACTIVE`), low while the beam is on the cursor's cell; `U12` gate 4 inverts it into `MATCH`.
- `U38` gate 2 makes `GO_N` = NAND(`KEY`, `ROW_TOP`, `MATCH`): low while a key waits and the cursor's cell is at the page's output. `U14` gate 4 inverts it into `GO`.
- Printable codes only are written: `U12` gate 2 makes `CTL` = NOR(`D6`, `D5`), high for control codes 0–31; `U31` gate 1 inverts it into `PRINT`. `U29` gate 1 makes `WRK_N` = NAND(`GO`, `PRINT`), gate 2 `WRITE` = NAND(`WRK_N`, `CLEAR_N`): high while a printable key's cell is at the output, or while Clear is held. It goes to the six `WRITE` pins, which act only at the rise of `PHI2_N`.
- The write bus: `U26` 74LS244 drives the key's bits 0–5 (`D0`–`D5`) onto `W0`–`W5` except while Clear is held; then `U27` drives 0x20 instead. The code's bit 6 is not stored: the 2513 takes the low 6 bits of an upper-case code.
- `U28` half 2 (`WR`) takes `GO` at each rise of `PHI2_N`: it rises at the end of the dot that wrote the key's cell. `WR` clocks the cursor counters (below) and, through its `Q_N`, holds `KEY` cleared; at the next rise of `PHI2_N` it takes `GO` again, now low. Return and the other control codes go the same way, waiting for the cursor's cell, with nothing written.

### Cursor

- Cursor counter: `U32` 74LS161 (`X0`–`X3`), `U33` (`X4`, its `QA`), `U34` (`Y0`–`Y3`), clocked by `WR`, counting when their `ENT` is high (`ENP` tied high), so once a key.
- `ADV` (`U31` gate 2) = `PRINT` OR `CR`: the cursor moves for a printable key or Return, not for other control codes. `U32` counts on `ADV`; its carry `XRCO` steps `U33`; `XC_N` (`U31` gate 3) = NAND(`XRCO`, `X4`) marks column 31; `YEN` (`U31` gate 4) = `XC` OR `CR` steps `U34`. Column 31 wraps to column 0 of the next row, and row 15 to row 0: the page wraps.
- Return: `U30` 74LS30 decodes `CR_N` (code 13) from `CTL`, `D4_N`, `D3`, `D2`, `D1_N`, `D0` (`D1_N` and `D4_N` from `U5`). `CR_N` loads 0 into `U32` and `U33` and `YEN` steps the row, at `WR`'s rise.
- Clear: `SW1` pulls `CLEAR_N` low (`R1` pulls it up). While held: `U14` gate 3 makes `CLEAR`, which turns `U26` off; `U27` drives 0x20 onto `W0`–`W5`; `WRITE` is high, so every cell is written with a space as it comes round; the cursor counters are cleared to 0. Every cell is written within one frame (20 ms); the press of a button lasts longer.
- Cursor on the screen (P4p's): `U39` half 1, clocked by `DOT_END`, delays `MATCH_N` to the character that cell is shown in (`CURSOR_N`). `U40` 74LS93 counts frames (each fall of `V8`): `BLINK` (its `QD`) changes every 8 frames. `U12` gate 3 makes `CUR_SHOW` = NOR(`CURSOR_N`, `BLINK`); `U36` gate 4 XORs it into the video (`VIDEO_M` into `U25`): the cursor's cell inverted, every other 8 frames.

No reset: the beam counters fall into step within a frame as on 080. The page, the row buffer, the cursor counter, `KEY` and `WR` power on at random: the page shows random characters and the cursor sits anywhere until Clear is pressed, like the original.

## When it works

The TV shows a page of random characters at power-on, locked within half a second, with a blinking block somewhere. Pressing Clear blanks the page and puts the block at the top left. Each key typed appears where the block was and the block moves one place right; after 32 characters it goes on at the start of the next line, Return starts the next line, other control keys (Backspace, Esc, CTRL) do nothing, and after the 16th line it goes back to the top line and types over what is there. Letters show in upper case, white on black, with a dark border all round. Keys typed faster than about 40 a second appear in turn, a little behind.

## Puzzles

Six Puzzles (P4r, 2026-11-12), each a different Fault on a different link, each seen once Clear is pressed and a few keys typed:

| Puzzle | Fault | Symptom | Path to the fix |
|---|---|---|---|
| Lookalikes | Broken Trace on `PHI2_N` from `U17` pin 1 to `U18` pin 1 | bit 2 of every character is 0: `HELLO` shows as `HAHHK`; Clear still blanks | `P2` stays at 0; a new `U18` fades the same way; `PHI2_N` pulses at `U17` pin 1, nothing at `U18` pin 1: mend the Trace |
| Wallpaper | `U29` output `2Y` (`WRITE`) stuck at 1 | the whole screen is one character; each key fills it with that key; Clear blanks it only while held; the cursor moves as it should | `WRITE` high at `U29` pin 6 while `WRK_N` pulses and `CLEAR_N` is high: swap `U29` |
| Copies | `U22` input `REC` (`ROW_LOAD_N`) stuck at 1 | all 16 lines show the same 32 characters; typing and Clear change nothing on the screen, though the block moves | the page's outputs `P0`–`P5` change on the top lines and `ROW_LOAD_N` pulses on `U22` pin 4, yet `B0`–`B5` repeat every line: swap `U22` |
| Every other line | Dry Joint at `U36` pin 5 (`2B`, `Y0`) | after Clear the block is on the second line; Return goes down a line every second time, the next line typing over the one before; the first line is never used | `Y0` changes on pin 5's pad (B.Cu) but the leg (F.Cu) stays high: resolder `U36` pin 5 |
| Zeros | Solder Bridge between `U20` pin 2 (`OUT`, `P4`) and pin 3 (`READ`, tied to VCC) | Clear fills the screen with `0`s; `A`–`O` show as `P`–`_` (`HELLO` as `XU\\_`) | `P4` reads 1 at both ends, and a new `U20` changes nothing; with the power off the meter finds `P4` connected to VCC: cut the bridge on the solder side of `U20` |
| M is Return | Broken Trace on `CTL` from `U12` pin 4 to `U30` pin 1 | every `M` (and `-`) is printed, then ends its line | `CR_N` pulses low for `M`; `CTL` pulses at `U12` pin 4, `U30` pin 1 floats: mend the Trace |

Each teaches something different: dynamic memory losing its clock (the obvious swap fails), a write enable stuck on (every cell takes the bus), a register that never loads (the page is right, the picture is not), a dry Joint on a comparator input (rows paired), a bridge to a tie (the obvious swap fails again), and a decoder input cut (one key decoded as another).

Netlist, Board and manuals: [TYPEWRITER_DESIGN.md](TYPEWRITER_DESIGN.md).

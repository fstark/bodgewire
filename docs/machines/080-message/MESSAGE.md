# 080 — Message Display

Eighth rung of the tutorial ladder. A board that drives a TV and shows a page of fixed text: a short poem, 16 lines of 16 characters, white on black, still. It is the video chain of a home computer of the late 1970s (the PET 2001's, BW-2001 later) with a ROM where the video RAM would be: beam counters address a message ROM, its character code and the beam's line inside the character address a character ROM, and a shift register turns the character ROM's dots into pixels. Its code comes with roadmap step P4l ([ROM_PARTS.md](../../p4l-rom-parts/ROM_PARTS.md)).

## Lesson

Text on a screen is a lookup done again for every line of every character. The beam counters say where the beam is; part of that is an address in the message ROM, which holds one character code a cell; the code and the line inside the character are an address in the character ROM, which holds the dots of that line of that character; the shift register sends them out one dot a clock. Each lookup takes time, so the chain works one character ahead: what the counters address now is shown one character later. The character ROM's first line is blank in every character, and the board blanks the picture by asking the character ROM for that line outside the text area: one trick, and the borders and the pipeline stay in step.

Faults show as text going wrong in ways that point to one place in the chain: the same letter everywhere (a code line stuck), every letter missing a column of dots (a font output), letters cut in half or repeated rows (a row address line), the page repeated in the borders (blanking), columns of text swapped or doubled (a column address line).

## Circuit

PAL-like and progressive, one dot per clock, two dots per microsecond:

| | Value |
|---|---|
| Dot clock | `X1` 2 MHz `OSC` → `CLK` (0.5 µs per dot) |
| Character | 6 dots = 3 µs: 5 dots of the glyph and 1 dark dot |
| Line | 21 characters = 126 dots = 63 µs (15.87 kHz) |
| Frame | 312 lines = 19.656 ms (≈ 50.9 Hz) |
| Text | 16 columns × 16 rows; a row is 16 lines (8 glyph lines, each drawn twice) |
| Active picture | line characters 1–16 (the text, one character late), lines 0–255 |
| Horizontal sync | character 20 (3 µs) |
| Vertical sync | lines 308–311 |

Beam counters:

- Dot counter `U1` 74LS161 counts 10 to 15 and reloads 10 (`LOAD_N` on `DOT_LOAD_N`): 6 dots a character. Its carry `DOT_END` is high on the last dot of each character; `U5` inverts it into `DOT_LOAD_N`.
- Column counter `C0`–`C4`: `U2` 74LS161 (`C0`–`C3`, carry `CC`) and `U3` 74LS161 (`C4`; `QB`–`QD` unused), stepped by `DOT_END`, reloaded with 0 after character 20: `U4` gate 1 decodes `LINE_END_N` = NAND(`DOT_END`, `C4`, `C2`) (20 is the first count with `C4` and `C2` high), `U5` inverts it into `LINE_END`.
- `U4` gate 2 makes `HSYNC_N` = NAND(`C4`, `C2`): character 20.
- Line counter `V0`–`V8`: `U6`, `U7`, `U8` 74LS161, enabled by `LINE_END`, reloaded with 0 after line 311 by `U9` 74LS30 (`FRAME_END_N`); `U10` gate 1 decodes `VSYNC_N` for lines 308–311; `U11` 74LS86 mixes the two syncs into `SYNC_N_D`. All four are the 060 circuits ([BOUNCING_BALL.md](../060-bouncing-ball/BOUNCING_BALL.md), Circuit) under new references.
- Within a text row, `V0` repeats each glyph line twice, `V1`–`V3` are the glyph line (0–7), `V4`–`V7` the text row (0–15).

The chain:

- Message ROM `U14` 2716: address `A0`–`A3` = `C0`–`C3` (column), `A4`–`A7` = `V4`–`V7` (row), `A8`–`A10` tied low; always enabled. Its contents (`roms/message.rom`) are the page, one ASCII code a cell, rows of 16 at 0x00, 0x10, … 0xF0. `O0`–`O5` are the character code `CH0`–`CH5` (the 2513 takes the low 6 bits of an upper-case ASCII code); `O6` and `O7` are unused.
- Blanking: `U12` 74LS02 gate 1 makes `ACTIVE` = NOR(`C4`, `V8`): columns 0–15 on lines 0–255. `U13` 74LS08 lets the glyph line through only while active: `R1`–`R3` = `V1`–`V3` AND `ACTIVE`. Outside the text area the character ROM is asked for line 0, blank in every character.
- Character ROM `U15` 2513: row address `A1`–`A3` = `R1`–`R3`, character address `A4`–`A9` = `CH0`–`CH5`, `CE_N` tied low. Its contents (`roms/2513.rom`) are 64 characters of 8 lines: line 0 blank, a 5 × 7 glyph on lines 1–7 (upper-case letters, digits, punctuation: ASCII 0x20–0x5F). `O5` is the leftmost dot, `O1` the rightmost: `F5`–`F1`, high for a lit dot. The −5 V and −12 V supplies of the real chip are not modelled: its `VGG` and `VDD` pins are left unconnected.
- Shift register `U16` 74LS165, clocked by `CLK_N` (`U5`), so it shifts in the middle of each dot: while `DOT_LOAD_N` is low (the last dot of a character) it loads `H` = `F5`, `G` = `F4`, `F` = `F3`, `E` = `F2`, `D` = `F1`, `C`, `B`, `A` = 0; then it shifts one stage each half-way clock, serial input 0. `QH` is `VIDEO_D`.
- `U17` 74LS74 re-times `SYNC_N_D` and `VIDEO_D` on the clock into `SYNC_N` and `VIDEO`, to `J2`. Sampled at the end of each dot, `QH` gives the six dots `F5`, `F4`, `F3`, `F2`, `F1`, 0 of each character in turn: the asynchronous load at the start of the last dot puts `F5` on `QH` just in time for that dot's end, and no shift falls inside the load.
- `J1` power connector.

Timing: the column counter steps at the start of a character; the 2716 answers 450 ns later and the 2513 500 ns after that, about two dots into the character; the shift register loads on its sixth dot. A character addressed during line character k is shown during character k + 1, so the text sits on line characters 1–16, and the blanking, which goes through the same ROM, moves with it.

No reset: from any power-on state the counters fall into step within a line (the dot and column counters) and a frame (the line counter); the shift register empties in a character. The picture locks within half a second and the page is the same from the first locked frame.

## When it works

The TV shows the poem, 16 lines of 16 characters, upper case, white on black, still, with a dark border all round: the text a little left of the middle, starting near the top. It locks within half a second and never rolls. The same page at every power-on.

The page (`roms/message.rom`), Emily Dickinson's "Hope" is the thing with feathers (1891, public domain), first stanza, rows 2 to 13, the other rows blank:

```
"HOPE" IS THE
THING WITH
FEATHERS -
THAT PERCHES
IN THE SOUL -
AND SINGS THE
TUNE WITHOUT
THE WORDS -
AND NEVER STOPS
- AT ALL -

EMILY DICKINSON
```

## Puzzles

None yet: they come with their own roadmap row (P4m), from the finished Board.

Netlist, Board and manuals: [MESSAGE_DESIGN.md](MESSAGE_DESIGN.md).

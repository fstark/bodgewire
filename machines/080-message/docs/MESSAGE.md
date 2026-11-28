# 080 — Message Display

Eighth rung of the tutorial ladder. A board that drives a TV and shows a page of fixed text: a short poem, 16 lines of 16 characters, white on black, still. It is the video chain of a home computer of the late 1970s (the PET 2001's, BW-2001 later) with a ROM where the video RAM would be: beam counters address a message ROM, its character code and the beam's line inside the character address a character ROM, and a shift register turns the character ROM's dots into pixels. Its code comes with roadmap step P4l ([ROM_PARTS.md](../../../docs/p4l-rom-parts/ROM_PARTS.md)).

## Lesson

Text on a screen is a lookup done again for every line of every character. The beam counters say where the beam is; part of that is an address in the message ROM, which holds one character code a cell; the code and the line inside the character are an address in the character ROM, which holds the dots of that line of that character; the shift register sends them out one dot a clock. Each lookup takes time, so the chain works one character ahead: what the counters address now is shown one character later. The character ROM's first line is blank in every character, and the board blanks the picture by asking the character ROM for that line outside the text area: one trick, and the borders and the pipeline stay in step.

Faults show as text going wrong in ways that point to one place in the chain: letters turned into other characters (a code line), letters cut in half (a row address line), letters doubled (a column address line), text in the border (blanking), every letter missing a column of dots (a dot line), letters smeared into bars (the shift clock).

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
| Horizontal sync | characters 18–19 (6 µs) |
| Vertical sync | lines 280–283 |

Beam counters:

- Dot counter `U1` 74LS161 counts 10 to 15 and reloads 10 (`LOAD_N` on `DOT_LOAD_N`): 6 dots a character. Its carry `DOT_END` is high on the last dot of each character; `U5` inverts it into `DOT_LOAD_N`.
- Column counter `C0`–`C4`: `U2` 74LS161 (`C0`–`C3`, carry `CC`) and `U3` 74LS161 (`C4`; `QB`–`QD` unused), stepped by `DOT_END`, reloaded with 0 after character 20: `U4` gate 1 decodes `LINE_END_N` = NAND(`DOT_END`, `C4`, `C2`) (20 is the first count with `C4` and `C2` high), `U5` inverts it into `LINE_END`.
- `U4` gate 2 makes `HSYNC_N` = NAND(`C4`, `C1`): characters 18–19 (the counter stops at 20, so they are the only counts with `C4` and `C1` high).
- Line counter `V0`–`V8`: `U6`, `U7`, `U8` 74LS161, enabled by `LINE_END`, reloaded with 0 after line 311 by `U9` 74LS30 (`FRAME_END_N`); `U10` gate 1 decodes `VSYNC_N` = NAND(`V8`, `V4`, `V3`, `V2_N`) for lines 280–283, `U5` gate 4 inverting `V2` into `V2_N`; `U11` 74LS86 mixes the two syncs into `SYNC_N_D`. All four are the 060 circuits ([BOUNCING_BALL.md](../../060-bouncing-ball/docs/BOUNCING_BALL.md), Circuit) under new references.
- Both syncs sit early in the blanking (P6n, [PICTURE_MARGINS.md](../../../docs/p6n-picture-margins/PICTURE_MARGINS.md)): the text starts 11.5 µs after the line sync's edge, and line 0 comes 32 lines after the vertical sync's start, so the window of a real monitor (10 µs and 28 lines of flyback) cuts nothing.
- Within a text row, `V0` repeats each glyph line twice, `V1`–`V3` are the glyph line (0–7), `V4`–`V7` the text row (0–15).

The chain:

- Message ROM `U14` 2716: address `A0`–`A3` = `C0`–`C3` (column), `A4`–`A7` = `V4`–`V7` (row), `A8`–`A10` tied low; always enabled. Its contents (`machines/080-message/roms/message.rom`) are the page, one ASCII code a cell, rows of 16 at 0x00, 0x10, … 0xF0. `O0`–`O5` are the character code `CH0`–`CH5` (the 2513 takes the low 6 bits of an upper-case ASCII code); `O6` and `O7` are unused.
- Blanking: `U12` 74LS02 gate 1 makes `ACTIVE` = NOR(`C4`, `V8`): columns 0–15 on lines 0–255. `U13` 74LS08 lets the glyph line through only while active: `R1`–`R3` = `V1`–`V3` AND `ACTIVE`. Outside the text area the character ROM is asked for line 0, blank in every character.
- Character ROM `U15` 2513: row address `A1`–`A3` = `R1`–`R3`, character address `A4`–`A9` = `CH0`–`CH5`, `CE_N` tied low. Its contents (`machines/080-message/roms/2513.rom`) are 64 characters of 8 lines: line 0 blank, a 5 × 7 glyph on lines 1–7 (upper-case letters, digits, punctuation: ASCII 0x20–0x5F). `O5` is the leftmost dot, `O1` the rightmost: `F5`–`F1`, high for a lit dot. The −5 V and −12 V supplies of the real chip are not modelled: its `VGG` and `VDD` pins are left unconnected.
- Shift register `U16` 74LS165, clocked by `CLK_N` (`U5`), so it shifts in the middle of each dot: while `DOT_LOAD_N` is low (the last dot of a character) it loads `H` = `F5`, `G` = `F4`, `F` = `F3`, `E` = `F2`, `D` = `F1`, `C`, `B`, `A` = 0; then it shifts one stage each half-way clock, serial input 0. `QH` is `VIDEO_D`.
- `U17` 74LS74 re-times `SYNC_N_D` and `VIDEO_D` on the clock into `SYNC_N` and `VIDEO`, to `J2`. Sampled at the end of each dot, `QH` gives the six dots `F5`, `F4`, `F3`, `F2`, `F1`, 0 of each character in turn: the asynchronous load at the start of the last dot puts `F5` on `QH` just in time for that dot's end, and no shift falls inside the load.
- `J1` power connector.

Timing: the column counter steps at the start of a character; the 2716 answers 450 ns later and the 2513 500 ns after that, about two dots into the character; the shift register loads on its sixth dot. A character addressed during line character k is shown during character k + 1, so the text sits on line characters 1–16, and the blanking, which goes through the same ROM, moves with it.

No reset: from any power-on state the counters fall into step within a line (the dot and column counters) and a frame (the line counter); the shift register empties in a character. The picture locks within half a second and the page is the same from the first locked frame.

## When it works

The TV shows the poem, 16 lines of 16 characters, upper case, white on black, still, with a dark border all round: the text close to the left edge (TV column 92 of a window from 80), on TV rows 64–255, a band of dark above it and a wider one below. It locks within half a second and never rolls. The same page at every power-on.

The page (`machines/080-message/roms/message.rom`), Emily Dickinson's "Hope" is the thing with feathers (1891, public domain), first stanza, rows 2 to 13, the other rows blank:

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

Seven Puzzles (six from P4m, 2026-11-11; Slanted page from P8k, 2026-11-28), each a different Fault on a different link of the chain, each seen on the first locked frame:

| Puzzle | Fault | Symptom | Path to the fix |
|---|---|---|---|
| Half letters | `U13` output `3Y` (`R3`) stuck at 0 | each line of the poem shows only the tops of its letters (glyph lines 1–3), twice, one above the other | `R3` stays low at `U13` pin 8 while `V3` pulses on pin 9: swap `U13` |
| Echo | `U12` input `1B` (`V8`) stuck at 0 | the poem whole, and a row of broken dots along the very top of the screen (the tops of the poem's second line) | `V8` pulses on `U12` pin 3, leg and pad, yet `ACTIVE` (pin 1) goes on pulsing on lines 256–311: the chip reads its input wrong: swap `U12` |
| Gibberish | Broken Trace on `CH5` from `U14` pin 15 to `U15` pin 22 | the letters are digits and punctuation (`"(/0%"` for `"HOPE"`); spaces and punctuation stay where they were | `CH5` pulses at `U14` pin 15, nothing at `U15` pin 22: mend the Trace |
| Stammer | Dry Joint at `U14` pin 8 (`A0`, `C0`) | every other character shown twice, the others gone (`HHPP""II  HH`) | `C0` pulses on pin 8's pad (B.Cu) but not on the leg (F.Cu): resolder `U14` pin 8 |
| Slanted page | `X1` runs at 1.9 MHz, though marked 2 MHz (a Wrong Clock) | the TV never holds the page: the words slide across the screen in slanting streaks | every counter counts right, but each line lasts 67 µs instead of 64, past the TV's hold; `CLK` reads 1.9 MHz on the scope against the Service Manual's 2 MHz: swap `X1` |
| Thin letters | Solder Bridge between `U16` pin 13 (`C`, tied to GND) and pin 14 (`D`, `F1`) | every letter without its right-hand column of dots | `F1` reads 0 at both ends, and a new `U15` changes nothing; with the power off the meter finds `F1` connected to GND: cut the bridge on the solder side of `U16` |
| Block letters | Broken Trace on `CLK_N` from `U5` pin 6 to `U16` pin 2 | the text is solid bars and blocks: each letter's left column of dots drawn six dots wide | `CLK_N` pulses at `U5` pin 6, nothing at `U16` pin 2; `VIDEO_D` changes at most once a character: mend the Trace |

Each teaches something different: an output stuck (half the address space never asked for), an input stuck (a border that is not blanked although its signal is right), a data line cut between two ROMs (every lookup shifted by the same amount), a dry Joint on an address input (two cells read as one), a bridge to a tie (the obvious chip swap fails), and a clock cut (a register that loads but never shifts).

Memory Fault candidates (P5g, tested in `message_test`, no Puzzle file yet): a stuck cell in the character ROM (`fault cell U15.O3 $29 0`: every `E` loses the middle dot of its top bar, the only Fault that touches one letter alone) and a stuck column (`fault column U15.O1 0`: Thin letters' picture made inside `U15`: `F1` is low at both ends but nothing ties it to GND, so the swap that fails there is the fix here).

Netlist, Board and manuals: [MESSAGE_DESIGN.md](MESSAGE_DESIGN.md).

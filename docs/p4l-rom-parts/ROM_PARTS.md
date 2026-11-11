# P4l — ROM Parts

Roadmap step P4l ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the code that the 080 message display needs, and the tests of that Machine. The Machine itself is specified in [MESSAGE.md](../machines/080-message/MESSAGE.md).

Decided with the user (2026-11-11):

- 16 columns × 16 rows of text at a 2 MHz dot clock, as 060 and 070: 6-dot character cells (a mod-6 dot counter), 21 characters a line, each glyph line drawn twice. 32 × 16 at 4 MHz was measured at about half the speed, under 1× with instruments.
- The message in a 2716 EPROM; the font in a ROM with the real Signetics 2513's pinout; one ROM model behind both. Pixels through a 74LS165.
- The page is a short poem.
- ROM contents in our own text format, readable as text and as pictures, no generator script.
- No Puzzles in this step: a later roadmap row (P4m), from the finished Board.

Decided while designing (2026-11-11), for the user to overrule:

- The poem: the first stanza of Emily Dickinson's "Hope" is the thing with feathers (1891, public domain), 12 rows centred on the page.
- The 2513's pinout from the 1972 Signetics MOS data book (pp. 65–67): `A1`–`A3` the glyph line, `A4`–`A9` the character, `O5` the leftmost dot, `CE` low to enable. Its −5 V and −12 V supplies are not modelled: `VGG` and `VDD` are no-connect pins, and its Datasheet says so. Its font is our own drawing in the 2513's layout (64 characters, line 0 blank, 5 × 7 glyphs on lines 1–7).
- ROM contents files live in `roms/`; a netlist names one with `contents=`, relative to the netlist's directory.
- Blanking goes through the character ROM: outside the text area its row address is forced to 0, the blank line of every character. It stays in step with the one-character pipeline by itself.
- The shift register shifts on the inverted clock and the output is re-timed by a 74LS74, so the 74LS165's asynchronous load gives six clean dots a character.
- The ROMs are 600-mil DIP-24s; the chip grid's slots (16 × 27 mm) are too small, so the Board's `.place` puts them on an edge by a `row` rule. Grid slots for big chips are later, with functional groups.

## New code

1. **ROM contents format.** A `.rom` text file, read when a ROM Part is built:
   - `;` outside a string starts a comment to the end of the line;
   - `@HHH` sets the address of the next word (hex);
   - `HH` is one word (hex, 1–2 digits), `HH*N` N copies (N decimal);
   - `"text"` is one word per character, its ASCII code (`\"` and `\\` escaped);
   - a run of `.` and `#` is one word drawn as dots: `#` = 1, the last character bit 0 (`.##.#` = 0x0D);
   - words go to consecutive addresses. A word wider than the ROM's data width, an address past its end, or one written twice is an error naming the file and line. Unwritten words read all ones (an erased EPROM).
2. **ROM Parts.** One ROM model, a Component configured by a spec (pins, address and data pins in bit order, enables, delays), with the contents read at build time from `contents=<file>` (required):
   - `2716`: 2K × 8 EPROM, DIP-24: `A0`–`A10`, `O0`–`O7`, `CE_N`, `OE_N` (both low to read; else the outputs float), `VPP` (an input, tied to `VCC` to read), `VCC` pin 24, `GND` pin 12. Address access 450 ns, `CE_N` 450 ns, `OE_N` 120 ns, outputs off 100 ns (Intel's maximums, the only figures given).
   - `2513`: 64 × 8 × 5 character generator, DIP-24: `A1`–`A9` (address bits 0–8), `O1`–`O5` (data bits 0–4), `CE_N` (low enables, else the outputs float), `VCC` pin 24, `GND` pin 10, `VGG`, `VDD` and `NC` no-connect. Character address (`A4`–`A9`) 500 ns, row address (`A1`–`A3`) 450 ns, `CE_N` 150 ns (typicals).
   - No state; an X address bit gives X only on the data bits whose words differ.
   - Datasheets `2716.md` and `2513.md`.
3. **74LS165 parallel-in serial-out shift register.** `part U16 74LS165`; DIP-16, the datasheet pinout (`SH_LD_N` `CLK` `E` `F` `G` `H` `QH_N` `GND` `QH` `SER` `A` `B` `C` `D` `CLK_INH` `VCC`). `SH_LD_N` low loads `A`–`H` at once (asynchronous; `QH` follows `H`); while high, each rising edge of `CLK` OR `CLK_INH` shifts `SER` → `A` → … → `H`. Datasheet `74LS165.md`.
4. **DIP-24 package.** The KiCad `DIP-24_W15.24mm` footprint vendored in `boards/footprints/`; `bw place`, the Board reader and `check-board` take it like any other package. The chip panel draws it as any DIP.
5. **Contents paths.** A netlist loaded from a file resolves `contents=` against its directory (`bw run`, `bw_ui`, `bw place`, `check-board`, the Machine and the Puzzles).

Nothing else is new: the TV, the counters and gates, the Board pipeline, the Session and the instruments are unchanged. The watch budget is unchanged (the TV watches two Nets).

## Machine

The message display is built as in [MESSAGE_DESIGN.md](../machines/080-message/MESSAGE_DESIGN.md): netlist, ROM contents, Board (placed by `bw place`, routed by the user), manuals. No Puzzles.

## Tests

- Contents format: each kind of line and word; comments; `@` jumps; repeats; escapes; unwritten words all ones; errors with file and line (bad token, word too wide, past the end, written twice, unterminated string).
- ROM Parts: catalogue rows and pins (`VGG`, `VDD`, `NC` no-connect on the 2513, power off the corners); each output follows the word at the address after the access delay (2716 one delay, 2513 row and character delays); `CE_N`/`OE_N` float the outputs and give them back with their delays; X address bits; `contents=` missing, unreadable or bad, refused with the netlist line.
- 74LS165: parallel load while `SH_LD_N` is low (`QH` follows `H`), eight shifts with `SER`, `CLK_INH` holds, no shift while loading, delays, X on `SH_LD_N` and `CLK_INH`.
- DIP-24: the footprint loads with 24 pads and a courtyard; `KicadFootprint("DIP-24")`.
- A netlist from a file finds `contents=` beside it; from text, relative to the working directory.
- `tests/machines/message_test.cpp`, on the TV picture: the netlist loads without warnings; for several seeds, once locked, the picture is the page: each of the 256 cells reads as its character in the font (the expected dots rendered from the two `.rom` files, found at one place on the picture), the dots within a cell exact, and nothing lit outside the text area.
- The message display joins the per-Machine checks: Board against netlist, KiCad DRC, Board scene, manuals; a Session test with every instrument attached.
- Release build: 1.00× with every instrument attached, screenshot of the page.

Design: [ROM_PARTS_DESIGN.md](ROM_PARTS_DESIGN.md). Plan: [ROM_PARTS_PLAN.md](ROM_PARTS_PLAN.md).

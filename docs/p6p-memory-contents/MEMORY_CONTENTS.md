# P6p — Memory contents

Roadmap step P6p ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): what a ROM holds, one click from the chip, for questions like "what is at `$0038`?". Design: [MEMORY_CONTENTS_DESIGN.md](MEMORY_CONTENTS_DESIGN.md). Plan: [MEMORY_CONTENTS_PLAN.md](MEMORY_CONTENTS_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

A repairer of a computer Machine (the LED Computer, BW-1, BW-2001, BW-80) asks what the program does at an address: where the reset vector points, what the routine the analyzer caught is, which glyph a character code draws. Today the player can only watch the bus. A real bench has an EPROM reader and a printed listing; the game gives the reading in the binder, next to the manuals.

The look is done ahead of the step (`c9511f2`, `99004fa`): the 2716 shows its quartz window, the mask ROMs (2332, 2316B, 2513, the 6530's ROM) the same window with the die only while x-rayed, their part number under it. The window is where the player clicks.

## User decisions (2026-11-20)

From the roadmap row (the user, 2026-11-20):

| Question | Decision |
|---|---|
| Where | a click on a memory chip's window opens the binder at the chip's contents: an optional divider, only for a memory chip, only once opened |
| Views | three, the same on every Machine, each a display choice the player changes: hex with a text column in a character set (ASCII, PETSCII, the ZX80's: any on any ROM); a disassembly in a CPU's code (6502, Z80) from an offset, for a length, at the address the chip is mapped at; a graphical view to find the characters (offset, bytes per glyph, bits per row) |
| Defaults | a layer on top gives each chip its defaults (the Machine's character set and CPU, the whole chip at its address on the Machine), never more than defaults |
| A fault | the views show what the chip holds now, a fault included: anything else would break the player's trust in what they see. Acknowledged as not exactly real, since a bench cannot read a chip in its socket |
| Not in it | RAM, editing a ROM, debug ROMs; a published source listing with its comments (a tab of its own, later if ever) |

Settled at the step's start (the user, 2026-11-20):

| Question | Decision |
|---|---|
| Where a chip's defaults come from | a file per Machine, `machines/NNN-name/roms/contents.txt`, beside the ROM files it describes: line-based like a `.bwp`, the CPU stated (not derived), the character set, and per chip its address and where its glyphs start. A Machine without memory chips has none. The netlist stays hardware only |
| A click on the window with a tool held | always opens the contents, whatever is held: the window is a control, like the flip corner. The puller pulls from the body off the window |
| A mask ROM's window while one of its pins is hovered | kept: any x-ray shows the die (the body, a pin or the selection) |
| The ROM-fault Puzzles | all three kept: none is given away (below) |
| The text column's codes with no letter | a dot: letters, digits and punctuation in the UI font, everything else `.`; an inverse ZX80 letter as its letter |
| Bits per row (after the implementation) | none: a glyph is always 8 dots wide, bit 7 the leftmost. Which bits reach the screen is the circuit's business, not the ROM's |

The ROM-fault Puzzles, checked against the views (the user agreed, 2026-11-20):

- **BW-1 Odd seven** (`fault cell U2.D3 rom $3EE 1`): U2's segment table byte for 7 (`TABLE + 7`, `$1FEE`) reads `$8F`, not `$87`. Found only by locating the table and decoding segments, which is the repair skill the views are for.
- **BW-1 ROM test fails** (`fault cell U3.D0 rom $380 0`): a lone `$FE` in U3's `$FF` fill; but the self-test already says U3's ROM sum is wrong. The view confirms.
- **BW-2001 Odd E** (`fault cell UA2.O2 $2A 1`): the glyph view shows the E with its dot, as the TV does; one glyph wrong can only come from UA2. The view confirms.

Derived in the design (2026-11-20), reported to the user:

- **What "holds" means**: the cells, a stuck cell or column included. A pin Fault on a ROM (the LED Computer's `blackout`, `U2.CE_N` stuck) is in the pins, not the cells: the views do not show it; the probe finds it. A swap shows at once: the page reads the design's words through the Machine's current cell Faults, not the last power-on's build, so a swapped chip reads clean before the next power-on.
- **The Z80 disassembler** exists already: the test-only one of P5t (`tests/asm/z80_disassembler.cpp`, 305 lines, decoding by opcode fields, apart from the assembler's table) moves into `src/asm`. The 6502 one decodes with the core's own `Mos6502Decode`. The row's estimate (half of 1,000 lines) drops to about 100 lines.
- **Undocumented opcodes** read as data (`.byte $FF`): the `$FF` fill of a 6502 ROM would otherwise read as `ISC $FFFF` all the way down.
- **The window click** neither selects the chip nor acts with the tool, and wins over a Trace or bodge wire under the window (no Trace runs under a chip's body on our Boards, but a wire may cross it). A right-click on the window does what it does on the body.
- **Only ROMs**: the page reads a ROM chip (a 2716, 2332, 2316B, 2513, or the 6530's ROM), never a RAM: a RAM's cells are written by the running Machine on another thread.
- **Phases**: the disassemblers come before the page, so the page arrives with its three views in one commit, never with a placeholder (the row said "the page, hex and graphics; then the disassembly": the same split, in the other order).
- **Settings** are kept per chip for the run, not saved.

## What the player sees

### The window and the click

1. A click on a memory chip's window (its seal included) opens the binder, if away, at a new divider **Contents** showing that chip, and brings the binder to the front. Whatever is held: probe, pencil, puller, iron, cutters, wire or an instrument. The chip is not selected; the tool does nothing.
2. Hovering the window shows the tooltip "Read U2's contents" (the chip's reference) in place of the chip's own; the x-ray stays as today.
3. The window is clickable where it is drawn: on a 2716 always, on a mask ROM whenever the mouse is over it (the hover x-rays it). In the plain look (Tab) no window is drawn and no click opens one; the Contents divider stays reachable by its tab once opened.

### The Contents divider

4. A fifth tab, **Contents**, after Repairs, appears the first time a window is clicked in the run and stays for the run. Its page shows the last chip opened. Clicking another chip's window turns to it; a chip selected on the Board turns the binder to Datasheets, as today.
5. The page's head: the reference (a link: hover highlights the chip, a click frames it on the Board without selecting it, so the page stays), the Part, its size (`2K x 8`, `512 x 5`), the address range when the chip has an address (`$C000-$C7FF`); `2K x 8` with a plain `x`. Under it, three buttons pick the view: **Hex**, **Code**, **Glyphs**. Then the view's settings on one line, then the view, scrolling.
6. **Address** (`$` and four hex digits): where the CPU sees the chip's first word; the hex and code views number their rows from it. Default: the chip's address in `contents.txt`, else `$0000`.
7. **Hex**: rows of 8 words at the binder's default width, 16 when the player widens it (4 if ever narrower), the row's address, each word in two hex digits, then the text column. Setting: **Characters** (ASCII, PETSCII, ZX80). A **Go to** field takes an address and scrolls to its row, the word outlined; an address outside the chip does nothing.
   - ASCII: `$20`–`$7E` as themselves.
   - PETSCII: `$20`–`$5D` as themselves (`$5C` `£`); everything else a dot (graphics, controls, shifted letters).
   - `£` is drawn if the UI font has the glyph, else a dot.
   - ZX80: `$00` space, `$01` `"`, `$0C` `£`, `$0D` `$`, `$0E` `:`, `$0F` `?`, `$10`–`$1B` `( ) - + * / = > < ; , .`, `$1C`–`$25` digits, `$26`–`$3F` letters; bit 7 (inverse) as the same character; the block graphics `$02`–`$0B` and the tokens (`$40`–`$7F`, `$C0`–`$FF`) a dot.
8. **Code**: one line per instruction: address, its bytes in hex, the instruction (upper case, `$` hex, MOS and Zilog syntax: `LDA ($FA),Y`, `LDA $0034` for an absolute operand, always four digits, `LD HL,($4004)`, an index displacement in decimal as Zilog writes it, `LD B,(IX-3)`, branch targets as addresses). Settings: **CPU** (6502, Z80), **From** (an offset into the chip, hex), **Length** (words, hex), and the Go to field. An undocumented opcode, or an instruction running past the length, is a `.byte $XX` line. Default: the Machine's CPU (6502 on a Machine without one), the whole chip.
9. **Glyphs**: the words from **From**, cut into glyphs of **Rows** words each, one word a row, always 8 dots wide, bit 7 the leftmost (a 2513's 5-bit glyph in the right five columns); drawn as lit dots on dark squares, 16, 8 or 4 glyphs to a row (the most that keep a dot at least 3 px), each with its index under it (`$05`). Hovering a glyph tells its index and its words in the chip (`$05: words $028-$02F`). Default: from the chip's `glyphs` in `contents.txt` (else 0), 8 rows.
10. Which view a chip opens at, the first time in the run: Code when the chip has an address and the Machine a CPU; else Glyphs when it has `glyphs`; else Hex. After that the page keeps each chip's view and settings for the run.

### What the chips show

| Machine | Chip | Opens at |
|---|---|---|
| Message | U14 (message 2716), U15 (2513) | Hex ASCII; Glyphs |
| Typewriter | U23 (2513) | Glyphs |
| LED Computer | U2 (2716) | Code 6502 at `$F800` |
| BW-1 | U2 (6530-002's ROM), U3 (6530-003's) | Code 6502 at `$1C00`, `$1800` |
| BW-2001 | UH1, UH5, UH2, UH6, UH3, UH4, UH7; UA2 | Code 6502 at `$C000`, `$C800`, `$D000`, `$D800`, `$E000`, `$F000`, `$F800`, PETSCII; Glyphs 8 × 8 (901447-08) |
| BW-80 | IC2 (2332) | Code Z80 at `$0000`, ZX80 characters, its glyphs at `$0E00` |

11. A stuck cell or column shows in every view, as the chip holds it now; a swap clears it at once.

## What is new

To be settled file by file in the design:

1. **The read**: `MemoryCells::read` and `wordBits`; `Machine::contents` (a ROM's design words through the current cell Faults).
2. **The defaults**: `roms/contents.txt` on six Machines, its parser, a test that each names real memory chips.
3. **The window**: its geometry and kind in `src/view` (shared by the drawing and the click), the click in `BoardView`, its tooltip.
4. **The binder**: the Contents divider in `Binder`, its tab, the page and its three views (`src/ui`), their ImGui-free logic (`src/view`).
5. **Disassembly**: the Z80 disassembler moved from the tests to `src/asm`; a 6502 one beside it.
6. **Flags**: `--contents REF[:hex|code|glyphs]` opens the binder at a chip's contents for screenshots.
7. **Docs**: the User Manuals of the six Machines (one line: the window, the divider), GLOSSARY, recipes (ADD_MACHINE, ADD_PUZZLE), ARCHITECTURE.

The kernel, Parts' behaviour, the builder, devices and Puzzles are untouched.

## Tests

- **Unit tests**: `Rom`, `Mos6530`, `Ram2114`, `Ram6102` `read` and `wordBits`, a stuck cell read through; `Machine::contents` with Odd E's cell, and clean after `swap`, nullopt for a RAM; the `contents.txt` parser (each line, each error with its line); every Machine's `contents.txt` parses and names ROM chips of its netlist, its addresses on 16 bits; the character sets' mapping; the hex row width; the 6502 disassembler (every documented opcode assembled back by `bw_asm` to the same bytes, undocumented as `.byte`, a branch target, the end of the length); the Z80 tests as today from the moved file; the glyph dots (the 2513's `@`, the 901447-08's `E`, the ZX80's `K` at `$0E00 + $30 × 8`); the window's geometry and hit; the binder's Contents divider (hidden until opened, opened while away and out, a selection still turning to Datasheets).
- **Screenshots** (release, viewed): the BW-80's IC2 in Code and in Glyphs at `$0E00`; the BW-2001's UH4 in Hex PETSCII and UA2 in Glyphs on Odd E; the Message's U15 and the Typewriter's U23 (5-bit glyphs); the BW-1's U2 on Odd seven in Hex.
- **By reading and by the user**: the click with each tool; the tooltip; Go to; the settings kept per chip; 16 words a row in a widened binder.
- **Release**: the full release suite passes.

## Later

- A published source listing with its comments, a tab of its own, if ever.
- RAM contents (racy with the running Machine; not wanted now).

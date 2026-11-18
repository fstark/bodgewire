# BW-80

## What it does

Welcome to the BW-80, a copy of the Sinclair ZX80 of 1980, chip for chip. It is a home computer you program in BASIC, on a TV.

- The processor is `IC1`, a Z80, ticking 3¼ million times a second.
- The program is the ZX80's own, 4096 bytes in the ROM `IC2`. It holds BASIC, the keyboard routine, and the shapes of the 64 characters.
- The memory holds 1024 bytes, in two chips, `IC3` and `IC4`. It keeps your program, the BASIC's notes, and the text on the screen.
- The keyboard is the window beside the Board, `J3`.
- The picture goes to the TV through `J2`.

There is no video chip. The processor draws the picture itself, with the help of a handful of plain logic chips. How it works tells how.

At power-on, the TV shows nothing steady for about a fifth of a second: the BW-80 clears and checks its memory. Then the picture locks: white, with a black square near the bottom left holding a white K. That square is the cursor. The BW-80 waits for a command.

## Using it

**The keyboard.** It has 40 keys, as the ZX80's. Click a key to press it. Or click the keyboard window, or press F2, and type on the PC keyboard: each PC key presses the BW-80 key with the same character. The BW-80 adds SHIFT where it needs it: a `"` presses SHIFT and Y.

On the PC keyboard, Enter is NEW LINE and Backspace is RUBOUT. The arrows are the cursor keys, Home is HOME, and Escape is BREAK. Shift+Enter is EDIT. Cmd+V (Ctrl+V) pastes a listing: each keyword is typed as its key, so a program copied from a book runs.

**One key, one keyword.** The ZX80 saves typing: one key types a whole word of BASIC. The word is written above the key.

- While the cursor shows K, the BW-80 expects a keyword. O types PRINT, K types LET, A types LIST, R types RUN.
- After the keyword, the cursor shows L. Now the keys type their letters and digits.
- SHIFT with a letter types the symbol printed in gold on the key: SHIFT and K is +. THEN, TO, AND, OR and NOT are SHIFT with a digit or B.

**A command.** Type PRINT 2+2 and press NEW LINE. The answer, 4, shows at the top of the screen. At the bottom, a report shows 0/-2. The 0 means no error. The -2 is the line it ran: -2 is the line you type on, where a command runs at once. While a report shows, the first key you press only clears it.

**A program.** A line that starts with a number is kept, not run. Type these three lines, each ended with NEW LINE:

- 10 LET A=6
- 20 PRINT A*7
- RUN

The screen shows 42, and the report 0/20: no error, last line 20. LIST shows the program. NEW clears it.

**The blank screen.** The picture goes blank whenever the BW-80 is busy: for a moment after each key, and while a program runs. Every ZX80 did this. It is not a fault: How it works tells why.

**No cassette.** The BW-80 has no tape recorder. SAVE writes nothing. LOAD waits for ever, until BREAK (the SPACE key) stops it.

**The edge connector.** `J4`, at the back, is the ZX80's expansion connector. Nothing is plugged in.

## How it works

**The goal.** A home computer must run BASIC, read the keys, and draw a page of text on a TV. Most computers have a video chip for the picture. In 1980 that was too dear for Sinclair. So the ZX80 lets the processor draw the picture itself. The price: while the processor draws, it can do nothing else.

**The clock.** `X1` ticks 6½ million times a second, on `DOT`. That is the speed of the dots on the screen. `IC18` halves it to 3¼ million, on `PHI`. Two gates of `IC15` pass it on to the processor, as `CLK`.

**The reset.** At power-on, `RST1` holds `RESET_N` low for a thousandth of a second. Then the processor starts the program in the ROM from the beginning. The program first clears the memory and checks it. That takes about a fifth of a second. Only then does the picture start.

**The processor.** `IC1` works like a cook following a recipe. It reads an instruction, does it, then reads the next. It puts a number on the address lines, `A0`–`A15`, to say where. The byte travels on the data lines, `D0`–`D7`. `MREQ_N` goes low when it talks to memory. `RD_N` and `WR_N` say which way. `M1_N` goes low each time it reads an instruction.

After each instruction read, the Z80 does one more thing. It puts a counter on the address lines, and `RFSH_N` goes low. This is the refresh. It was made for a kind of memory this board does not have. The ZX80 uses it for the picture.

**The memory map.** Only one chip may answer at a time. The address picks it, like the house numbers on a street. `IC12` looks at `A14`: low picks the ROM, high picks the memory.

| Addresses | What answers |
|---|---|
| $0000–$0FFF | the ROM, `IC2`: the program, and the character shapes at $0E00 |
| $4000–$43FF | the memory, `IC3` and `IC4` |
| $C000–$C3FF | the same memory again: the picture is drawn from here |

Fewer address lines are looked at than a full decoder would use. So the ROM and the memory also answer at other addresses: the memory, for example, again every 1024 bytes up to $7FFF. The program uses $C000 on purpose, as we will see.

Each memory chip holds four bits of every byte. `IC3` holds bits 7, 1, 0 and 2. `IC4` holds bits 4, 3, 5 and 6.

**Two data buses.** The memories do not sit on the processor's data lines. They have their own, `MD0`–`MD7`. Eight resistors, `R4`–`R11`, join each `MD` line to its `D` line. Normally a byte just passes through. But a gate can pull a `D` line low without touching the `MD` line behind its resistor. Then the processor reads one thing, while the rest of the board reads another. This is the trick the whole picture rests on.

**The screen in memory.** The text on the screen is kept in memory as a list of codes, one byte per character: the display file. Each row of text ends with the code $76. To the Z80, $76 is the instruction HALT.

**The processor draws the picture.** Think of a child reading aloud with a finger under each word. The finger sets the pace. On the ZX80, the processor is the finger. The rest of the board does the reading.

To draw a row, the program jumps into the display file, at its copy above $C000. The processor now takes each character code for an instruction, and tries to run it:

1. The processor reads the code. `A15` is high, and the code is a character. So `IC16` and `IC17` raise `NOP_EN`.
2. The gates of `IC14` and `IC15` pull the processor's data lines low. The processor reads 0. To a Z80, 0 is NOP: do nothing. So it does nothing, and moves on to the next code.
3. Meanwhile, the real code stays on the memory's side, on `MD0`–`MD7`. The latch `IC5` keeps it, on `CH0`–`CH7`.
4. Then comes the refresh. Now the multiplexers `IC6`–`IC8` give the ROM a different address. In place of the processor's address, they give it the kept code and the line counter.
5. The ROM answers with one line of the character's shape, eight dots.
6. The shift register `IC9` takes the eight dots, at the next tick of `PHI`. It sends them to the TV one by one, at 6½ million a second.

Each code takes four ticks of the processor. That is just the time for eight dots. So the processor walks along the row, one character at a time, at the speed of the beam.

**The character shapes.** The shapes sit in the ROM from $0E00 on, like a book of stencils. Each character has a page of eight lines, one byte per line. The code picks the page. The line counter `IC21` picks the line, on `LC0`–`LC2`. The processor walks the same row of codes eight times, once for each line of the characters.

A code with bit 6 set is not a character. $76, the HALT, is one of those. It is not turned into a NOP, so the processor really halts. That ends the row.

**Inverse characters.** A code with bit 7 set is drawn white on black, like the cursor. `IC12` keeps that bit while the character is drawn, on `INV`. `IC20` turns the dots over when it is set. The result goes to the TV on `VIDEO`: high for white, low for black.

**The line sync.** The TV must know when each line starts. That is the line sync: `SYNC` low for about 6 millionths of a second, every 64.

The halted processor still refreshes, and its refresh counter still counts. The counter is wired to the interrupt input: `IC1.INT_N` is on `A6`. The program sets the counter so `A6` goes low just as the line ends. It works like an egg timer.

The interrupt wakes the processor. To answer it, the processor pulls `IORQ_N` low. `IC18` notes that. `IC19` then counts two instruction reads, on `M1_CLK`, and takes `SYNC` low. Two more, and `SYNC` goes high again. Each fall of `SYNC` also steps the line counter `IC21`. Then the program draws the next line.

Blank rows at the top and bottom of the picture are just lines the program counts without a row of text.

**The frame sync.** Fifty times a second, the TV must also go back to the top. That is the frame sync: `SYNC` held low for about 400 millionths of a second.

It comes from the keyboard. Any read from the keyboard sets the latch in `IC11` low, on `VS_N`. That holds `SYNC` low and clears the line counter. The program reads the keys at that moment, so the same read starts the frame sync. Any write from the processor, an OUT, ends it.

**Why the screen goes blank.** The processor makes every line of the picture, and every sync. When it has a key to work on, or a program to run, it stops drawing. The TV then gets no sync at all, and shows nothing.

**The keyboard.** The 40 keys sit in a grid: 8 rows of 5. Think of a roll call. The program calls one row at a time, and the keys in that row answer.

- The rows are the address lines `A8`–`A15`. To read the keys, the program reads from the keyboard with one of these lines low: the row it calls.
- The columns are `KB0`–`KB4`. `R13`–`R17` hold them high.
- A held key joins its column to its row's line, through a diode inside the keyboard. While its row is called, the key pulls its column low.
- `IC10` puts the five columns on `D0`–`D4` during the read. `KBD_N` opens it: any read from an even port.

The program calls all eight rows in turn, once a frame:

| Row | Keys, from the outside in |
|---|---|
| `A8` | SHIFT, Z, X, C, V |
| `A9` | A, S, D, F, G |
| `A10` | Q, W, E, R, T |
| `A11` | 1, 2, 3, 4, 5 |
| `A12` | 0, 9, 8, 7, 6 |
| `A13` | P, O, I, U, Y |
| `A14` | NEW LINE, L, K, J, H |
| `A15` | SPACE, the full stop, M, N, B |

The outermost key of each row is on `KB0`, the next on `KB1`, and so on. So O is row `A13`, column `KB1`.

The same read also gives two more bits. `IC10` puts the cassette input on `D7`; with no cassette, `R1` holds it low. `R28` holds bit 6 high. That tells the program to draw the picture for a 50 Hz TV.

**Pins with nothing to do.** Some features are not used on this board:

| Part | What it can do | Here |
|---|---|---|
| `IC1.NMI_N`, `IC1.WAIT_N`, `IC1.BUSRQ_N` | interrupt, slow down or stop the processor | held high by `R12`, `R23`, `R22`: never used |
| `IC10.A1`, on `EAR` | read a cassette | held low by `R1`: no cassette |
| `IC20` | four gates | two make the picture; the other two drove the ZX80's crystal, replaced by `X1` |
| `IC9.SER`, `IC9.CLK_INH` | chain shift registers, pause the shifting | tied low: never used |
| `J4` | expansion bus | nothing plugged in |

The Service Manual lists what the probe and the logic analyzer show on a working board.

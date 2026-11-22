# BW-2001

## What it does

Welcome to the BW-2001, the first computer on the bench with a screen. Its board copies the Commodore PET 2001 of 1977, chip for chip and wire for wire. The TV beside it is the screen, and a keyboard comes with it in its own window.

- The processor is a 6502, `UF3`, running a million steps a second.
- It has 8 KB of memory for programs, and 1 KB more for the screen.
- Its programs sit in seven ROM chips, Commodore's own: BASIC, the screen editor and the operating system of the first PET.
- The screen shows 25 rows of 40 characters. A separate group of chips draws it, sixty times a second.

At power-on the screen fills with random characters for a moment. That is the screen memory as it wakes up. Then the BW-2001 checks its memory, clears the screen and prints:

`*** COMMODORE BASIC ***`

`7167 BYTES FREE`

`READY.`

A blinking square under `READY.` is the cursor. The BW-2001 is waiting for you to type.

## Using it

**The keyboard.** It is the PET 2001's keyboard. The letters and the top row's symbols are on the left. The digits and `/ * + - = .` are on the pad at the right. Click a key to press it. A click on SHIFT keeps it down until the next key you click is up, or until you click SHIFT again. A right-click keeps any key down until you right-click it again.

On a PC keyboard, each key types the BW-2001 key with the same character, with the keyboard's window out or put away. Return is RETURN, Backspace is DEL, Escape is STOP and Home is HOME. The arrows are the cursor keys. Cmd+V (Ctrl+V) pastes text, typed one key at a time.

**BASIC.** It is Commodore's BASIC of 1977. Type a line and press RETURN.

- A line without a number runs at once: `PRINT 2+2` prints 4.
- A line with a number is kept as part of the program.
- `LIST` shows the program, `RUN` runs it and `NEW` clears it.
- STOP stops a running program.
- `PEEK` reads a byte of memory and `POKE` writes one: `PRINT PEEK(32768)` reads the screen's first character.

**The screen editor.** The cursor can go anywhere on the screen.

- The cursor keys move it.
- HOME puts it at the top left. SHIFT and HOME (CLR) clears the screen.
- DEL deletes a character. SHIFT and DEL (INST) inserts a space.
- RETURN on a line already on the screen enters it again. So to change a listed line, type over it and press RETURN.

**The killer poke.** On the PET 2001, `POKE 59458,62` made printing much faster, and filled the screen with white specks while it printed. It does the same here. How it works explains why.

To read a ROM, point at its chip and click the round window that shows on it (`UH1`, `UH5`, `UH2` and `UH6` hold BASIC, `UH3` the screen editor, `UH4` and `UH7` the operating system; `UA2` the character set): the binder opens at Contents, as 6502 code, in hex with the text in PETSCII, or as glyphs.

## How it works

**The goal.** The board holds two machines that share one memory. The first draws the picture: it reads the screen memory sixty times a second and turns each byte into a character on the TV. The second is the computer: it runs the programs and writes into the screen memory what they print. Neither waits for the other, so the board must keep them out of each other's way.

**The clock.** `X1` ticks eight million times a second, on `CLK8`. Each tick is one dot on the screen. `UC9` divides it by eight, into `CLK1M`: one tick per character. `UC8` turns that into `PHI0`, the 6502's clock. So the picture and the computer keep the same beat.

**Part one: the picture.** The blocks below draw the screen, with no help from the processor.

**How a TV draws.** Think of reading a page. The eye moves along a line, jumps back to the left and down, and at the bottom of the page jumps back to the top. The TV's beam does the same. Here a line lasts 64 millionths of a second. A frame has 260 lines and lasts about a sixtieth of a second.

**The beam counters.** The board must know where the beam is.

- `UC9`, `UB5` and `UC7` count the characters along a line, 64 of them. The first 40 are the text. `UC5` holds `DIS_ON` high during those 40.
- `UC5` also makes `HORZ_DRIVE`, the TV's line signal. It is high 24 millionths of a second in every line.
- `UA1` counts the scan lines inside a character row, 0 to 7, on `RA0` to `RA2`. A character is 8 dots wide and 8 lines tall.
- `UB6` steps through the four parts of a frame: 20 lines of vertical drive, 20 blank, 200 of text and 20 blank.
- `UD8` holds `VERT_DRIVE` low during the vertical drive. That tells the TV to go back to the top. `UC6` holds `VIDEO_ON` high during the 200 lines of text.

**The video address.** Think of a finger running under the text as you read. `UD5`, `UD6` and `UD7` count from 0 to 999, one character after another. That count is the address in the screen memory. Each character row is drawn 8 times, once per scan line. So `UE6` remembers where the row starts, and the counters go back there at each line. After the row's eighth line, they go on to the next 40 characters.

**The screen memory.** `UC3` and `UC4` hold 1 KB, one byte per character. `UC3` keeps the low 4 bits of each byte, `UC4` the high 4. The byte is a screen code: 1 is A, 2 is B, and so on.

**The character ROM.** `UA2` holds the shape of each character, 8 rows of 8 dots. It gets the screen code from the screen memory, on `SD0` to `SD6`, and the scan line from `UA1`. It answers with the 8 dots of that line of that character, on `CD0` to `CD7`.

**The shift register.** `UB2` takes the 8 dots at each character, then sends them out one per tick on `DOT`, leftmost first. That is how 8 dots fit in one character's time.

**Reverse video.** The top bit of the screen code, `SD7`, asks for the character in reverse: black on white. `UC1` delays that bit so it arrives with its own dots, on `REV`. A gate of `UE2` then flips the dot when `REV` is high. The blinking cursor is a reversed character.

**The output.** `UE9` lets a dot through only inside the text: while `VIDEO_ON`, `DIS_ON` and `BLANK_TV` are all high. Its output, `TV_VIDEO`, is low for a lit dot. `J7` takes `TV_VIDEO`, `HORZ_DRIVE` and `VERT_DRIVE` to the TV.

**Part two: the computer.** The blocks below run the programs.

**The processor.** `UF3` is a 6502. It reads its program from the ROMs one byte at a time, at a million steps a second. It puts an address on `A0`–`A15` and reads or writes a byte on `D0`–`D7`. `RW` says which: high for a read, low for a write.

**The buffers.** One processor cannot drive the pins of some forty chips. So `UE3` and `UE4` repeat its address onto the board's address bus, `BA0`–`BA10`. `UG5` and `UG6` repeat its data onto the board's data bus, `BD0`–`BD7`, in either direction.

**The address decoder.** Each chip must answer only at its own addresses. `UG2` splits the 64 KB the 6502 can reach into 16 blocks of 4 KB. It pulls one select line low per block, `SEL0_N` to `SELF_N`. `SEL8_N` is the screen; `UE5` turns it into `SEL8`, high while the 6502 touches the screen memory.

| Addresses | What answers | Chips |
|---|---|---|
| $0000–$1FFF | memory, 8 KB | `UI1`–`UI8`, `UJ1`–`UJ8` |
| $8000–$83FF | screen memory, 1 KB (seen four times, up to $8FFF) | `UC3`, `UC4` |
| $C000–$DFFF | BASIC | `UH1`, `UH5`, `UH2`, `UH6` |
| $E000–$E7FF | the screen editor and the keyboard | `UH3` |
| $E810 | PIA1: the keyboard, the blanking, the retrace | `UG8` |
| $E820 | PIA2: the IEEE-488 port | `UB8` |
| $E840 | VIA: the user port, the retrace wait | `UA5` |
| $F000–$F7FF | the operating system: cassettes and the IEEE-488 port | `UH4` |
| $F800–$FFFF | the operating system: start-up and interrupts | `UH7` |

Nothing answers at the other addresses.

**The memory.** Sixteen 2114 chips hold 1 KB of 4 bits each. They work in pairs: `UI8` keeps the low 4 bits of the first kilobyte, `UJ8` the high 4 bits, and so on. `UI9` picks the pair from the address. `UH8` and `UH9` link the memory's own data bus, `BBD0`–`BBD7`, to the board's. `UJ9` makes `RW_RAM`, which tells the memory when to write.

**The ROMs.** Seven sockets hold 2 KB each, Commodore's chips 901447-01 to 901447-07.

**Start-up.** `UDE9` holds `RES_N` low for a moment at power-on. Then the 6502 reads where to start from the top of `UH7`: $FD38. The start-up code there finds how much memory works, clears the screen and prints the banner.

**The keyboard.** The keys sit on a grid of 10 rows and 8 columns, like the squares of a chessboard. PIA1, `UG8`, picks a row with a number on `KEY_A` to `KEY_D`. `UG9` pulls that row low, through `J5`. A key held in that row pulls its column low too. The columns come back on `KIN0` to `KIN7`, each held high by a resistor when no key is pressed. The program checks the 10 rows one after the other, sixty times a second.

**The retrace interrupt.** At the end of the text of each frame, `VIDEO_ON` falls. PIA1 sees it on `UG8.CB1` and pulls `IRQ_N` low. The 6502 stops what it is doing, reads the keyboard, blinks the cursor, and goes back.

**The diagnostic LED.** `DS1` lights only if a program selects keyboard row 11, which does not exist. Programs can use it as a signal. BASIC leaves it dark.

**Part three: sharing the screen memory.** The two machines meet here.

**Two readers, one memory.** The picture reads the screen memory all the time. The 6502 reads and writes it too. `UD2`, `UD3` and `UD4` choose who gives the address. Most of the time it is the video counters. While `SEL8` is high, it is the 6502's `BA0`–`BA9`. `UB3` and `UB4` then link the screen memory's data, `SD0`–`SD7`, to the board's data bus.

**Snow.** The picture never waits. If the 6502 touches the screen while the beam draws text, the beam draws the byte the 6502 touched, in the wrong place. That shows as a brief white speck. Many of them look like snow.

**The wait.** So the editor writes a character to the screen only while the beam is outside the text. The VIA, `UA5`, reads `VIDEO_ON` on `UA5.PB5`. The editor waits for it to go low before each character. That is why printing is slow, and the picture stays clean. The killer poke turns that pin of the VIA into an output, so the editor never waits.

**The blanking.** Scrolling moves the whole screen up a row: 1000 bytes, far too many to wait for the beam each time. So the editor blanks the picture instead. PIA1 pulls `BLANK_TV` low on `UG8.CA2`, and `UE9` lets no dot through. The screen goes dark for a moment at each scroll.

**Features the board does not use.** Some of the PET's features have nothing connected here:

| Feature | What it can do | Here |
|---|---|---|
| `SEL2_N`–`SEL7_N`, `SEL9_N`–`SEL11_N` | select memory added on an expansion board | no expansion connector: nothing answers |
| `GRAPHIC` from `UA5.CA2` | choose the second character set | held low: the first set |
| `J1`, PIA2 `UB8`, `UA7`–`UA9` | IEEE-488 printers and disk drives | nothing plugged in |
| `J3`, `J6` | two cassette recorders | nothing plugged in |
| `J2` | the user port | nothing plugged in. Its DIAG pin starts the self-test of the debug ROMs (Service Manual) |

The Service Manual lists what the probe and the logic analyzer show on a working board.

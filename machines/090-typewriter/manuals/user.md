# TV Typewriter

## What it does

Welcome to the TV Typewriter. It turns a keyboard and a TV into a typewriter: what you type on the keyboard `J3` shows on the screen.

The page holds 16 lines of 32 characters. The letters are upper case, white on black, with a dark border all round. A blinking white block, the cursor, shows where the next character goes.

The board has no computer and no memory with addresses. It is built like the TV Typewriter of 1973: the page is kept in shift registers, a kind of memory that goes round and round in step with the picture.

## Using it

- When the board is switched on, the page is full of random characters. The cursor sits anywhere on it.
- Press CLEAR (`SW1`) to blank the page. The cursor goes to the top left.
- Type on the keyboard. Each key shows where the cursor was, and the cursor moves one place to the right.
- After 32 characters, typing goes on at the start of the next line.
- Return starts the next line at once.
- The other control keys do nothing: Backspace, Esc, and CTRL with a key.
- After the last line, typing goes on at the top line, over what is already there. The page never scrolls.
- Keys typed faster than about 40 a second still appear, in turn, a little behind.

## How it works

**The goal.** The TV draws the picture as a beam sweeping across the screen, line after line, 50 times a second. The board must say, for every dot of every line, whether it is white or black. And it must remember the 512 characters of the page while it does.

The board does it in four parts, each told below:

- the beam counters and the sync follow the beam and lock the TV;
- the page, going round, holds the characters;
- the row buffer and the character ROM turn a row of characters into dots;
- the keyboard and the cursor write each key in its place.

**Part 1: the beam counters and the sync.**

**The clock.** `X1` ticks four million times a second, on `CLK`. Each tick is one dot on the screen.

**Counting dots, characters and lines.** Three counters follow the beam, like the hands of a clock.

- `U1` counts the 6 dots of a character: 5 for the letter and 1 dark gap. At the last dot it raises `DOT_END`.
- `U2` and `U3` count the 42 characters of a line, on `C0`–`C5`. Only the first 32 hold text. The rest is the border, and the time the beam needs to fly back.
- `U6`, `U7` and `U8` count the 312 lines of a picture, on `V0`–`V8`. They step once a line, on `LINE_END`.

**Reading the line counter.** The line number says where the beam is in the text.

- `V0` changes every line, so each line of a letter is drawn twice. That makes the letters tall enough.
- `V1`–`V3` say which of the letter's 8 lines is being drawn.
- `V4`–`V7` say which of the 16 text rows it is.

**The text area.** `U12` raises `ACTIVE` while the beam is on the text: the first 32 characters of the first 256 lines.

**The sync.** The TV needs a signal to start each line and each picture. `U4` makes `HSYNC_N`, low for characters 40 and 41 of every line. `U10` makes `VSYNC_N`, low for the last 4 lines of the picture. `U11` mixes them into one sync signal, and `U25` sends it to the TV on `SYNC_N`.

**Part 2: the page, going round.**

**Memory without addresses.** Think of a merry-go-round with 512 seats. You can only reach the seat in front of you. To read or change a seat, you wait until it comes round.

**Six rings, one per bit.** A character's code is 6 bits. `U16`–`U21` each hold one bit of every character, in a ring of 512. `U16` holds bit 0, `U21` bit 5. Their outputs are `P0` to `P5`.

**Two clocks move the ring.** `U15` makes the page clocks, `PHI1_N` and `PHI2_N`. Each pair of pulses moves the ring one place.

- `PHI2_N` comes first. As it ends, the cell at the output is either kept, or written with a new character.
- `PHI1_N` comes next. As it ends, the next cell comes to the output.

**In step with the beam.** The page clocks pulse only on the top line of each text row, once in each of its 32 characters. `U13` marks that line with `ROW_TOP`. So the ring moves 32 places a row and 512 places a picture. It comes back to the same place at the start of every picture.

**The memory must keep moving.** These chips hold their bits as tiny charges. If either clock stops for long, every bit fades to the same value.

**Part 3: the row buffer and the character ROM.**

**Why a second memory.** The page shows each row's characters only once, on the row's top line. But a row of text is 16 lines tall. Every one of those lines needs the same 32 characters.

**The row buffer.** `U22` is a short ring of 32 characters. It goes round once every line, clocked by `ROW_CLK` from `U10`. Its output is the code of the character under the beam, on `B0`–`B5`.

**Filling it.** On a row's top line, `U29` pulls `ROW_LOAD_N` low. Then the buffer takes each character from the page as it comes past, in place of its old one. On the other 15 lines it just goes round, showing the same row again.

**The character ROM.** `U23` holds the shape of every letter. It gets the character's code on `B0`–`B5`, and the letter's line on `GL1`–`GL3`. It answers with the 5 dots of that line, on `F1` to `F5`. Outside the text, `U13` holds `GL1`–`GL3` at 0. Line 0 of every letter is blank, so the border stays dark.

**The dots.** `U24` takes the 5 dots at the end of each character. Then it sends them out one by one, a dot per tick, on `VIDEO_D`. `U25` cleans up the timing and sends the picture to the TV on `VIDEO`.

**Part 4: the keyboard and the cursor.**

**The cursor's address.** Three counters hold where the cursor is. `U32` and `U33` hold its column, on `X0`–`X4`. `U34` holds its row, on `Y0`–`Y3`.

**Finding the cursor's cell.** `U35`, `U36`, `U11` and `U37` compare the cursor's address with the beam's. `U38` pulls `MATCH_N` low while they agree.

**Showing the cursor.** `U39` delays `MATCH_N` by one character, to where the character is drawn. `U40` counts pictures: its output `BLINK` changes every 8 pictures. While the beam is on the cursor's cell and `BLINK` is low, `U12` raises `CUR_SHOW`. Then `U36` swaps black and white in that cell. So the block blinks about three times a second, and a letter under it shows dark on white.

**A key arrives.** The keyboard puts the key's code on `D0`–`D6`. Then it raises `STROBE` for 25 thousandths of a second. `U28` remembers the key: it raises `KEY`.

**Waiting for the cell.** The key must wait until the cursor's cell comes round to the page's output. That takes at most one picture. Then `U38` pulls `GO_N` low.

**Writing the key.** `U29` raises `WRITE` for that one cell. `U26` puts the key's code on the write bus, `W0`–`W5`. At the end of `PHI2_N`, the page takes it. Bit 6 of the code is not stored: upper-case letters need only 6 bits.

**Moving on.** Just after, `U28` raises `WR` for a moment. That steps the cursor one place and clears `KEY`. After column 31, the cursor goes to the start of the next row. After the last row, it goes back to the top.

**Control keys.** `U12` raises `CTL` for a control code. `U31` turns it into `PRINT`, low. Then `U29` keeps `WRITE` low, so nothing is written, and the cursor stays put. Return is the exception. `U30` spots its code and pulls `CR_N` low. Then the cursor goes back to column 0 and steps down a row.

**CLEAR.** The button pulls `CLEAR_N` low. `R1` holds it high the rest of the time. While it is held:

- `U26` lets go of the write bus, and `U27` puts the code of a space on it.
- `WRITE` stays high, so every cell is written with a space as it comes round. The whole page takes a fiftieth of a second.
- The cursor counters are held at 0: the top left.

**No reset.** The beam counters fall into step on their own within one picture. The page, the row buffer and the cursor start at random. That is why the page is full of random characters until CLEAR is pressed.

**Pins with nothing to do.** These chips have features this board does not use. Their pins are tied so they stay out of the way:

| Chip | Pin | What it can do | Here |
|---|---|---|---|
| 2524 | `READ` | show 0 on `OUT` instead of the bit | tied high: always shows the bit |
| 74LS161 | `CLR_N` on the beam counters | reset the count to 0 | tied high: they reload from `LOAD_N` instead |
| 74LS161 | `A`–`D` | the number loaded | tied low: the dot counter `U1` loads 10, every other counter loads 0 |
| 74LS93 | R01, R02 | reset the count | tied low: `U40` never resets |
| 74LS244 | the last two buffers of each chip | two more bits | inputs tied low, outputs unused |
| 74LS74 | second flip-flop of `U39` | a second memory | inputs tied, outputs unused |

The Service Manual lists what the probe shows on a working board.

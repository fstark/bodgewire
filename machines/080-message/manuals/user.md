# Message Display

## What it does

Welcome to the Message Display. It shows a page of text on the TV: the first verse of a short poem by Emily Dickinson, "Hope" is the thing with feathers. The letters are white on black, in upper case, 16 lines of up to 16 characters. A dark border runs all round the page. The text starts close to the left edge of the screen, with a band of dark above it and a wider one below.

Nothing moves. The picture settles within half a second of power-on, and the page is the same every time.

There is no computer on this board. Two memory chips hold the page and the shapes of the letters. A handful of counters read them out, dot by dot, fifty times a second.

To read what a ROM holds, click the round window on its chip (`U14`, the message; `U15`, the letters' shapes, whose window shows while you point at it): the binder opens at Contents, in hex with the text beside it, or as glyphs.

## How it works

**The goal.** The TV draws its picture as a beam sweeping across the screen, line after line, top to bottom. It knows nothing about letters. At every moment, the board must say one thing: is this dot white or black? So the board has to know, at every moment, which letter the beam is on, which line of that letter, and which dot of that line.

**Reading a page.** Think of reading a book aloud with your finger on the text. Your finger says where you are. Here, counters play the finger. A first memory chip holds the page, one character at each place. A second memory chip is like a font book: for each character, it holds a drawing of its dots. The board looks up the character under the finger, then looks up the dots of that character.

**The clock.** `X1` ticks two million times a second, on `CLK`. Each tick is one dot on the screen.

**The dot counter.** `U1` counts the six dots of one character: five for the letter and one dark dot as a gap. It counts from 10 to 15, then goes back to 10. On the last dot, its output `DOT_END` goes high. `U5` turns it upside down into `DOT_LOAD_N`.

**The column counter.** `U2` and `U3` count the characters along a line, on `C0`–`C4`. They step once a character, at each `DOT_END`. A line holds 21 characters. The first 16 carry the text. After character 20, `U4` pulls `LINE_END_N` low, and the count starts again at 0. A line lasts 63 millionths of a second.

**The line counter.** `U6`, `U7` and `U8` count the lines down the screen, on `V0`–`V8`. They step once a line, on `LINE_END`. After line 311, `U9` pulls `FRAME_END_N` low, and the count starts again at 0. A whole picture takes about a fiftieth of a second.

The line count has three parts:

- `V0` changes every line. It is not used to pick anything, so each line of a letter is drawn twice. That makes the letters tall enough to read.
- `V1`, `V2` and `V3` pick the line within the letter, 0 to 7.
- `V4` to `V7` pick the line of text, 0 to 15.

**The sync.** The TV must know where each line and each picture starts. `U4` pulls `HSYNC_N` low during characters 18 and 19 of every line. `U10` pulls `VSYNC_N` low during lines 280 to 283, with `U5` turning `V2` upside down for it. `U11` mixes the two into one sync signal. Both pulses come early in the dark part of the line and of the picture, well before the text: after each sync the TV's beam flies back to the left, or to the top, and whatever is drawn while it flies is lost.

**The page.** The message ROM, `U14`, holds the page. It has one character code for each of the 16 × 16 places. Its address is the column, `C0` to `C3`, and the line of text, `V4` to `V7`. Out come the character codes, on `CH0`–`CH5`.

**The font.** The character ROM, `U15`, holds the shapes of 64 characters: the capital letters, the digits and punctuation. Each shape is 8 lines of 5 dots. Its address is the character code and the line within the letter. Out come the five dots of that line, on `F5`, `F4`, `F3`, `F2` and `F1`, `F5` the leftmost. A lit dot is high.

**The shift register.** The five dots arrive side by side, but the TV wants them one after the other. `U16` does the change. On the last dot of each character, `DOT_LOAD_N` goes low, and `U16` grabs all five dots at once. Then, at each tick, it pushes them out one by one on `VIDEO_D`, leftmost first. The sixth dot is always dark: the gap between letters. `U16` is clocked on `CLK_N`, the clock turned upside down by `U5`. So it moves in the middle of each dot, well away from the other chips' ticks.

**The output.** `U17` holds the picture and the sync for one tick. That lines them up on the same clock before they leave on `J2`, as `VIDEO` and `SYNC_N`.

**Working one character ahead.** Each lookup takes time. The message ROM answers about half a microsecond after its address changes. The character ROM takes another half microsecond. So the board looks up one character while it shows the one before. What the counters point at now appears on the screen one character later.

**The border.** Outside the page, the screen must stay dark. `U12` raises `ACTIVE` only in columns 0 to 15 of lines 0 to 255. `U13` lets the line within the letter, `V1` to `V3`, through to the character ROM only while `ACTIVE` is high. Those gated lines are `R1`–`R3`. Outside the page, `R1`–`R3` are all low. That asks the character ROM for line 0, and line 0 is blank in every character. So the border is dark. Because the border goes through the same lookups as the text, it moves one character late with it, and the two stay in step.

**No reset.** Nothing on the board is reset at power-on. The counters start anywhere, but they fall into step within one picture. The shift register empties within one character.

**Pins with nothing to do.** The chips have features this board does not use. Their pins are tied so they stay out of the way:

| Pin | What it can do | Here |
|---|---|---|
| `U1.CLR_N`, `U2.CLR_N`, `U3.CLR_N`, `U6.CLR_N`, `U7.CLR_N`, `U8.CLR_N` | reset a count to 0 | tied high: never resets |
| `A`–`D` of each counter | the number it loads | wired to 10 on `U1`, to 0 on the others |
| `U14.A8`, `U14.A9`, `U14.A10` | the rest of the message ROM | tied low: only the first 256 places are used |
| `U14.O6`, `U14.O7` | the top two bits of each code | not connected: the character ROM needs only six |
| `U14.CE_N`, `U14.OE_N`, `U15.CE_N` | switch a ROM off | tied low: always on |
| `U16.A`, `U16.B`, `U16.C`, `U16.SER` | more dots into the shift register | tied low: the gap after each letter stays dark |
| `U16.CLK_INH` | stop the shift register | tied low: never stops |
| `U17.1PRE_N`, `U17.1CLR_N`, `U17.2PRE_N`, `U17.2CLR_N` | force an output high or low | tied high: never used |

The Service Manual lists what the probe and the logic analyzer show on a working board.

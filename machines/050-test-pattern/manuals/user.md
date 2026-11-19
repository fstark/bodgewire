# Test Pattern Generator

## What it does

Welcome to the Test Pattern Generator, the first board on the bench that drives a TV. The TV sits beside it on the bench. After power-up it shows a white grid on black that fills the screen. The board draws 6 vertical lines and 18 horizontal ones. The top horizontal line sits just above the screen's edge, hidden like the edges of any TV picture, so 17 of them show. The picture locks within half a second and never rolls.

There is no processor and no memory on this board. A clock and a row of counter chips draw the whole picture. A few gates turn the counts into the grid and into the timing the TV needs.

## How it works

**The goal.** A TV draws its picture one dot at a time. The board must say, for every dot, whether it is white or black. It must also tell the TV when to start a new line and when to start a new picture. Everything else on the board serves those two jobs.

**How a TV draws.** Think of reading a page of a book. Your eye moves along a line from left to right. At the end it jumps back to the left and down one line. At the bottom of the page it jumps back to the top. The TV's beam does the same. Each line has 64 dots. Each picture, called a frame, has 312 lines. The beam draws about 50 frames a second, so the eye sees a steady picture.

**The clock.** `X1` ticks one million times a second. Each tick is one dot. So a line lasts 64 millionths of a second, and a frame about a fiftieth of a second.

**The dot counter.** The board must know which dot the beam is on. `U1` and `U2` count the dots of a line, like the Counter's chips: `U1` counts every tick, and warns `U2` on `HC` when it rolls over. Together their six outputs, `H0` to `H5`, count from 0 to 63. At dot 63, `U6` sees the count and pulls `LINE_END_N` low. That makes `U2` start again from 0 at the next tick. The line starts over.

**The line counter.** The board must also know which line the beam is on. `U3`, `U4` and `U5` count lines, on `V0` to `V8`. They count only once per line: `U9` turns `LINE_END_N` into `LINE_END`, which lets them step at the line end. At the end of line 311, `U7` pulls `FRAME_END_N` low. All three start again from 0. The frame starts over.

**The sync.** The TV needs a signal to know where a line and a frame begin. Without it, the picture slides sideways or rolls. That signal is the sync.

- `U6` pulls `HSYNC_N` low for dots 52 to 55. That is the line sync: it tells the TV to jump back to the left.
- `U8` pulls `VSYNC_N` low for lines 288 to 291, just after the picture's last line. That is the frame sync: it tells the TV to jump back to the top.
- The sync comes early in the dark border on purpose. The beam takes a while to fly back, and the dark lines after the sync give it that time before line 0.
- `U10` mixes the two into one wire, `SYNC_N_D`. The TV tells them apart by their length: a line sync is short, a frame sync lasts four lines.

**The dark border.** Dots 48 to 63 and lines 288 to 311 are never lit. The beam is on its way back there, or off the edge of the screen. `U11` marks those dots on `HBLANK` and those lines on `VBLANK`. `U13` combines them into `ACTIVE`, high only inside the picture.

**The grid.** A grid line is where the dot count, or the line count, is a round number.

- `GRIDH` is high on every 8th dot: 0, 8, 16 and so on. Those dots make the 6 vertical lines.
- `GRIDV` is high on every 16th line. Those lines make the 18 horizontal lines.
- `U12` joins them into `GRID`. `U11` lets `GRID` through only while `ACTIVE` is high. The result is `VIDEO_D`.

**The output.** The gates take a moment to settle after each tick, and their outputs can twitch meanwhile. `U14` copies `SYNC_N_D` and `VIDEO_D` at each tick, so the TV gets clean `SYNC_N` and `VIDEO` signals through `J2`. The picture moves one dot to the right; the TV does not mind.

**No reset button.** The counters start from any number at power-up. Within one frame or two they reach their ends and start over from 0. From then on, the picture is steady.

**Pins with nothing to do.** The board leaves some features of its chips unused:

| Pin | What it can do | Here |
|---|---|---|
| `CLR_N` of `U1`–`U5` | reset the count to 0 | tied high: never resets |
| `A`–`D` of `U1`–`U5` | load a starting number | tied low: a load always starts from 0 |
| `LOAD_N` of `U1` | load a starting number | tied high: `U1` never loads |
| `V6`, `V7` on `U4` | two more line bits | not used: the count stops at 311 first |
| `PRE_N`, `CLR_N` of `U14` | force an output high or low | tied high: never used |

The Service Manual lists what the probe, the logic analyzer and the oscilloscope show on a working board.

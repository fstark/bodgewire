# Bouncing Ball

## What it does

Welcome to the Bouncing Ball, the first board on the bench that moves a picture. The TV shows a white box with a dashed line down the middle. A small square ball flies across it on a slant. It bounces off each of the four walls and never stops. It crosses the box in about a second.

This is the ball of the first arcade tennis games, on its own: no paddles, no score. There is nothing to press. The picture locks within half a second of power-on. The ball starts anywhere, and may be out of sight for up to a second before it comes in through a wall.

## How it works

**The goal.** A TV draws its picture one line at a time, left to right, top to bottom, like a person reading a page. The board must say, for every dot the beam passes, whether that dot is white. It must also tell the TV when to start a new line and a new picture. Moving the ball is a trick on top of that: the ball's own counters run almost, but not quite, in step with the beam.

**The clock.** `X1` ticks two million times a second. Each tick is one dot on the screen.

**Where the beam is.** Two counters follow the beam, like the line and column numbers in a text editor.

- `U1` and `U2` count the dots of a line, from 0 to 127, then start again. A line lasts 64 millionths of a second.
- At dot 127, `U6` says the line is ending, on `LINE_END_N`. `U9` turns it over into `LINE_END`.
- `U3`, `U4` and `U5` count the lines. They step once at each line end.
- After line 311, `U7` sets them back to 0, on `FRAME_END_N`. So a picture has 312 lines, and the TV gets 50 pictures a second.

**Sync.** The TV must know when each line and each picture starts.

- `U6` makes `HSYNC_N` low for dots 104 to 111 of each line.
- `U8` makes `VSYNC_N` low for lines 308 to 311.
- `U10` mixes the two into one wire, `SYNC_N_D`.
- `U11` passes the sync and the video on together, at the clock, to `J2`: `SYNC_N` and `VIDEO`.

**The dark edges.** Only dots 0 to 95 and lines 0 to 255 are on the screen. The rest is time for the beam to fly back. `HBLANK` is high for dots 96 to 127. `V8` is high for lines 256 and after. Either one keeps the dot dark.

**The box and the net.** Each wall is a gate that watches the beam counters. It goes low while the beam is over that wall. Think of a stencil laid on the screen.

| Shape | Made by | Low while the beam is on |
|---|---|---|
| top wall, `TOP_N` | `U12`, `U13`, `U14` | lines 0 to 15 |
| bottom wall, `BOTTOM_N` | `U8` | lines 240 to 255 |
| left wall, `LEFT_N` | `U12`, `U13`, `U14` | dots 0 to 3 |
| right wall, `RIGHT_N` | `U15` | dots 92 to 95 |
| net, `NET_N` | `U12`, `U15` | dots 48 and 49, on lines where `V2` is high |

`V2` is high for 4 lines, then low for 4. That cuts the net into dashes.

**The ball across.** Think of two clocks, one a little fast. They tick together for a while, then one pulls ahead. The ball works the same way. It has its own counters, which run almost in step with the beam.

- `U18` and `U19` count every other dot. When they overflow, the ball's column is under the beam. That moment is `HBALL`.
- At once, `U24` loads them again, on `HLOAD_N`. Normally they then take exactly one line to overflow again. So the ball stays in the same column, line after line.
- Once a picture, on line 308, `MOVE` is high. On that line the counters load one step more or one step less. The ball's column then comes one step later or earlier: 2 dots right or left.
- `HDIR` chooses. High: the ball goes right. Low: it goes left.

**The ball up and down.** `U20`, `U21` and `U22` count lines the same way. Their round is a little longer or shorter than a picture.

- When they overflow, `VLOAD_N` loads them again.
- Going down, their round is 316 lines: 4 lines more than a picture. So the ball comes 4 lines lower every picture.
- Going up, it is 308 lines: 4 lines less. The ball comes 4 lines higher.
- `VDIR_N` chooses. High: up. Low: down. The ball never stops moving up or down.

**The ball's height.** The ball is 8 lines tall. `U23` counts them. It starts when the line counters overflow, and stops itself after 8 lines. While it counts, `BALL_OFF` is low and `VBALL` is high. `U24` lights the ball where `HBALL` and `VBALL` are both high: `BALL_N` goes low. So the ball is 2 dots wide and 8 lines tall.

**Bounces.** A bounce is a meeting: the ball's dot and a wall's dot under the beam at the same time.

- `U25` watches the ball and each wall. When they meet, it pulses `HIT_LEFT`, `HIT_RIGHT`, `HIT_TOP` or `HIT_BOTTOM`.
- `U26` remembers the two directions. A hit on the left wall sets `HDIR`: go right. A hit on the right wall clears it: go left. The top and bottom walls do the same with `VDIR_N`.
- The ball steps 2 dots and 4 lines at a time. The walls are thicker than that, so the ball can never jump over one.
- A turn shows one picture late. So for one picture the ball hides inside the wall. That is normal.

**The mixer.** `U16` lights a dot when any of the six shapes asks for it: the ball, the four walls or the net. Its output is `MIX`. `U17` keeps it dark at the edges, and `U11` sends it to the TV as `VIDEO`.

**No reset.** There is no reset button, and none is needed. At power-on the counters may hold anything. They fall into step with each other within a fraction of a second. The ball, wherever it starts, comes in through a wall and bounces from then on.

**Pins with nothing to do.** Some chip features are not used here. Their pins are tied so they stay out of the way:

| Pin | What it can do | Here |
|---|---|---|
| `CLR_N` on the 74LS161 counters | reset the count to 0 | tied high: never resets |
| `LOAD_N` on `U1` and `U2` | load a starting number | tied high: the dot counter never loads |
| `U2.QD`, `U3.QD`, `U5.QB` to `U5.QD` | count higher | unused: the counts never get there |
| `U22`'s second flip-flop | a second flip-flop | inputs tied to 0 V |
| `U23.QB`, `U23.QC` | middle bits of the count | unused: only the last bit and the stop matter |

The Service Manual lists what the probe shows on a working board.

# Paddle Tennis

## What it does

Welcome to Paddle Tennis, a game for two players on the TV. The picture is a court seen from above. A wall runs along the top and the bottom. A dashed net runs down the middle. Each player has a paddle near their own side edge, and their score at the top, on their side of the net.

The ball bounces off the walls and off the paddles. A ball that gets past a paddle scores a point for the other player. The first player to 11 wins.

There is no computer on this board. Everything you see is made by counters, gates and flip-flops, the way the first arcade tennis games were built.

## Using it

**The knobs.** The left player turns the knob of `J3`, the right player the knob of `J4`. Turn a knob towards its right stop and the paddle goes up. Turn it towards its left stop and the paddle goes down. The paddle follows within a frame or two.

Drag a knob up or down, or use the mouse wheel over it. The keys are printed under each knob:

| Knob | Paddle up | Paddle down |
|---|---|---|
| `J3`, left player | Q | A |
| `J4`, right player | Up arrow | Down arrow |

The keys work once you have clicked on the knob box.

**The game.** A rally starts slow: the ball takes about a second and a half to cross the court. After three or four hits it goes twice as fast.

Where the ball meets the paddle sets its new angle. The top quarter of the paddle sends it steeply up, the bottom quarter steeply down. The two middle quarters send it off at a shallow angle, up from the upper half and down from the lower half.

A missed ball disappears. About half a second later it comes back out of the same edge, slow, heading for the player who just scored.

**The end of a game.** At 11 the paddles disappear. The scores stay. The ball bounces fast around all four edges.

**START** (`SW1`) starts a new game. It sets both scores to 0 and brings the paddles back.

**At power-up** the picture settles within half a second. The scores may show any number, or odd shapes, and the game may already be over. Press START.

## How it works

**The goal.** A TV draws its picture the way you read a page: dot by dot along a line, then line by line down the screen, 50 times a second. The board does not store the picture. Instead it watches where the beam is. When the beam reaches a wall, a paddle, the ball or a stroke of a digit, the board says "white" for that moment.

So most of the board is counters that follow the beam, and gates that notice when the beam is somewhere special.

**The clock.** `X1` ticks two million times a second. Each tick is one dot on the screen.

**The beam counters.** `U1` and `U2` count the dots along a line, 0 to 127. That takes 64 millionths of a second. At the end of each line, `U6` and `U9` raise `LINE_END`, and `U3`–`U5` count one more line. After line 311, `U7` sends them back to line 0. That is a frame: 312 lines, about 50 frames a second.

The visible court is dots 0 to 95 and lines 0 to 255. The rest of each line and each frame is dark. It gives the TV's beam time to fly back.

**The sync.** The TV must know when a line starts and when a frame starts. `U6` makes `HSYNC_N`, a short dip near the end of every line. `U8` makes `VSYNC_N`, a longer dip of four lines near the end of every frame. `U10` mixes the two into one signal, and `U11` sends it to the TV on `SYNC_N`.

**The court.** Gates watch the beam counters. `U12`–`U14` notice the top wall, lines 0 to 15. `U8` notices the bottom wall, lines 240 to 255. `U15` notices the net, dots 48 and 49, and lets it through only on every other group of four lines, so it is dashed.

`U14` and `U15` also notice the side edges, dots 0 to 3 and 92 to 95. These are the goals. They are not drawn.

**The picture.** Every white thing has its own wire, low while the beam is on it. `U16` gathers them. If any one of them is low, the dot is white. `U17` blacks out the margins, and `U11` sends the result to the TV on `VIDEO`.

**The ball.** The ball has its own pair of counters. Think of two clocks: one is the beam, the other is the ball. The ball is drawn where the two agree.

- `U18` and `U19` count across the line. When they roll over, the beam is at the ball's column.
- `U20`–`U22` count down the frame. When they roll over, the beam is at the ball's top line. `U23` then counts the ball's eight lines.
- `U24` lights the ball, `BALL_N`, where both agree.

To move the ball, the board makes its counters run a little short or a little long, once a frame. Then the ball is drawn a little further along. `U26` remembers the ball's two directions: `HDIR` is high while it moves right, `VDIR_N` is high while it moves up. A wall flips `VDIR_N`; a paddle or a goal flips `HDIR`.

**The paddles.** Each controller is a kitchen timer. At the end of every frame, `VSYNC_N` winds both timers. The knob sets how long each one runs: from about one millisecond to about fifteen. While a timer runs, its paddle waits. When it rings, the paddle is drawn on the next 16 lines.

- `U27` lines each timer up with the start of a line. Its outputs, `LWAIT` and `RWAIT`, are high while the paddle waits.
- `U28` and `U29` then count the paddle's 16 lines, `LP0`–`LP3` and `RP0`–`RP3`.
- After line 15, `U30` sets `LDONE` or `RDONE`, and the paddle stops.
- `U31` picks the left paddle on the left half of the screen and the right paddle on the right half. `U33` notices the paddle columns, dots 6 and 7 and dots 88 and 89. `U34` lights the paddle, `PAD_N`.

A short timer makes a high paddle. A long one makes a low paddle. That is why turning the knob moves the paddle.

**Hits and angles.** A hit is the ball and a paddle lit at the same dot.

- `U35` sees it. The ball turns right on the left half of the screen, and left on the right half. A goal turns it the same way.
- The paddle's own line counter says where on the paddle the ball landed. `U35`–`U38` read it on the ball's middle line and set the new angle. The bottom half of the paddle sends the ball down, the top half up.
- `U39` remembers the angle in `SHALLOW`. A shallow ball moves two lines up or down each frame, a steep one four.

**The speed.** At first the ball steps sideways only every other frame. `U22` and `U45` count the frames. `U40` counts the ball's turns to the left. At the second turn it sets `FAST`, and the ball then steps sideways every frame.

**A miss and the serve.** A miss is the ball reaching a goal while it is still moving into it.

- `U43` sees it, and `U44` sets `LOST_LEFT` or `LOST_RIGHT`. Either one means "serving".
- While serving, `U38` hides the ball and `U43` stops its sideways steps. The ball still moves up and down, unseen.
- `U46` counts frames. After about half a second it raises `SERVE_END`, and the serve is over. The ball reappears at the goal it went into, already turned around.

**The scores.** `U48` counts the left player's points, `U49` the right player's. Each one is a units counter, 0 to 9. When the units go from 9 back to 0, `U50` sets the tens: `TENS_L` or `TENS_R`.

`U51` notices 11: a ten and a units count of 1. It sets `GAME_OVER` in `U39`. Then `PLAY` goes low, which hides the paddles and stops the misses. The goals become walls.

START pulls `START_N` low. That clears both scores, both tens and `GAME_OVER`.

**The score digits.** There is no character memory. Each digit is drawn with five stencils, like a digital clock's segments.

- `U52` picks the units count of the half of the screen the beam is in. `U53` turns it into the seven segments, `SEG_A`–`SEG_G`.
- Each digit is a box 4 dots wide and 32 lines tall. `U54` picks the segments for the top half of the box or the bottom half.
- `U55`–`U57` light each segment only in its own strip of the box: the top bar on the top row, the sides on the outer columns, and so on. The tens digit is only ever a "1": a single column.
- `U58` gathers the strips into `GLYPH`, and `U42` sends it to the picture as `SCORE_N`.

**Pins with nothing to do.** Some chip features are not used here. Their pins are tied so they stay out of the way:

| Pin | What it can do | Here |
|---|---|---|
| `U48.R91`, `U48.R92` (and `U49`'s) | set the score to 9 | tied low: never used |
| `U53.LT_N`, `U53.BI_N`, `U53.RBI_N` | test or blank the segments | tied high: segments always follow the count |
| `U31.G_N`, `U52.G_N`, `U54.G_N` | switch the outputs off | tied low: always on |
| `U45`'s second flip-flop, a gate in `U38` and in `U51` | spare | inputs tied low |

The Service Manual lists what the probe shows on a working board.

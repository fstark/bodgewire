# 070 — Paddle Tennis

Seventh rung of the tutorial ladder, after the bouncing ball (060) and the knob meter (045), which it joins. Two players turn a knob each to move a paddle up and down the left and right edges of the TV. They hit the ball back and forth; a ball that gets past a paddle scores a point for the other player, and the first to 11 wins. Then the paddles disappear and the ball bounces around the empty court until someone presses Start. It is the game of the 1972 arcade Pong, built our own way on top of the 060 ball circuit. Its code comes with roadmap step P4j ([TENNIS_PARTS.md](../../p4j-tennis-parts/TENNIS_PARTS.md)).

## Lesson

Everything on the screen is a coincidence between counters and the beam. A paddle is where its own line counter runs; the controller sets when that counter starts, so the knob's position becomes a height (the knob meter turned a time into a number, here it becomes a place). A hit is the ball and a paddle lit at the same dot. Where on the paddle it lands is the paddle counter's value at that moment, and that value sets the ball's new vertical speed: the angle comes from the counter, not from geometry. A miss is the ball meeting the edge instead; it starts a timer that hides the ball, and counts a point. The score digits are drawn without any character memory: a decoder's segments are each let through a gate only in their own rectangle of the beam.

Faults show as rules going wrong (the ball goes through a paddle, always leaves at the same angle, never speeds up, scores the wrong side, never ends) as well as pictures going wrong (a paddle stuck, a digit missing a stroke), and the player learns to follow a rule back to the counter that makes it.

## Circuit

The beam counters, sync, net and ball of the 060 bouncing ball ([BOUNCING_BALL.md](../060-bouncing-ball/BOUNCING_BALL.md), Circuit), with the same timing (2 MHz dot clock, 128 dots × 312 lines, active picture dots 0–95, lines 0–255). What changes and what is new:

| | Value |
|---|---|
| Court | top wall lines 0–15, bottom wall lines 240–255, dashed net at dots 48–49; no side walls drawn |
| Goals | dots 0–3 (left) and 92–95 (right): the 060 side walls, still decoded, not drawn |
| Paddles | 2 dots × 16 lines; left at dots 6–7, right at dots 88–89; top line 16 to 224 |
| Controllers | `J3` (left), `J4` (right), triggered by `VSYNC_N`; the knob's right stop puts the paddle at the top |
| Ball | 2 dots × 8 lines, as on 060; 1 dot a frame across after a serve, 2 dots a frame after the ball has turned left twice; 2 or 4 lines a frame up or down |
| Angles | the ball's 5th line (line 4 of 0–7) on paddle lines 0–3: steep up; 4–7: shallow up; 8–11: shallow down; 12–15: steep down |
| Serve | a miss hides the ball at that edge for 28–32 frames (about 0.6 s), then it leaves that edge towards the scorer, slow |
| Scores | 0 to 11 each, drawn on lines 32–63: left tens at dots 16–19 and units at 24–27, right tens at 64–67 and units at 72–75 |
| Game over | at 11: paddles hidden, the ball bounces off all four edges, scores kept, until Start (`SW1`) |

### Paddles

Each controller is a `PADDLE` behind a 4-pin Connector ([PADDLE.md](../../../datasheets/PADDLE.md)): each fall of `TRIG_N` starts a high pulse on `OUT` whose length the knob sets. Both are triggered by `VSYNC_N`, at the start of line 308. They are wired the other way round (`reversed=yes`): the left stop gives the longest pulse, 14.56 ms, the right stop the shortest, 1.248 ms.

- `U27` 74LS74 samples each pulse at every line start (`LINE_END_N` rises): `LWAIT` and `RWAIT` are the pulses moved to line boundaries.
- `U28` (left) and `U29` (right) 74LS93 count the paddle's lines. Each is cleared while its `WAIT` is high, then counts on the start of dot 127 of each line (`LPAD_CK` = NOR(`LINE_END`, `LDONE`)): during the k-th line after the pulse it holds k. `LP0`–`LP3` and `RP0`–`RP3` are the paddle line, 0–15.
- `U30` 74LS107 holds `LDONE` and `RDONE`: set when the counter's `QD` falls after line 15, cleared while `WAIT` is high. A paddle is lit on its lines (`LPADV` = NOR(`LWAIT`, `LDONE`)) at its column.
- The pulse ends during line L = 308 + ⌊(0.1 µs + pulse) / 64 µs⌋ (mod 312); the paddle is lines L + 1 to L + 16. With the knob at k (0 left stop, 1 right stop) its top line is ⌊224.5 − 208 k⌋: 224 at the left stop, 120 half way, 16 at the right stop.
- `U31` 74LS157 picks the left or the right paddle's `PADV`, line bits 2 and 3 (`SP2`, `SP3`) and tens flag by `H6` (left half of the line, right half). `U33` decodes the columns (`COL_L_N` dots 6–7, `COL_R_N` dots 88–89); `PAD_N` = NAND(`PADV`, `PCOL`, `PLAY`) goes to the mixer.

### Hits and angles

- The ball turns right on meeting the left goal or a paddle in the left half (`GO_RIGHT`), and left on meeting the right goal or a paddle in the right half (`GO_LEFT`). These replace 060's `HIT_LEFT` and `HIT_RIGHT` at `U26` flip-flop 1 (`HDIR`).
- `PAD_HIT` = ball, paddle and the ball's line 4 (`U23` `QD` high, `QB` and `QC` low) at the same dot. On it, `U26` flip-flop 2 (`VDIR`) takes the paddle line's bit 3 (`SP3`: lower half goes down), alongside the 060 top and bottom bounces; `U39` flip-flop 1 (`SHALLOW`) takes `SP2` XOR `SP3` (the two middle quarters are shallow).
- When slow, the ball sits on the paddle's column for two frames; the second contact decides the angle.
- The ball vertical counter's load (060: 195 down, 203 up) becomes 195 + 2 × (`SHALLOW`) when going down and 203 − 2 × (`SHALLOW`) when going up: periods 316, 314, 310, 308 lines, the ball moving 4, 2, −2, −4 lines a frame. `U20` loads `D` = `VDIR_N`, `C` = `VDIR` and `SHALLOW`, `B` = `STEEP`, `A` = +5 V.
- A ball whose line 4 misses the paddle while another line overlaps it still turns (any overlap gives `GO_RIGHT` or `GO_LEFT`) and keeps its vertical speed.

### Speed and serve

- `U22` flip-flop 2 toggles at each frame start (`V8` falls): `FRAME0`. `U45` flip-flop 1 toggles on its falls: `FRAME1`, every 4 frames.
- `U40` counts the ball's turns to the left (`HDIR` falls): flip-flop 1 toggles, flip-flop 2 (`FAST`) is set at its second fall. Both are cleared while serving.
- The 060 `MOVE` (line 308) becomes `HMOVE` = `MOVE` and not `SERVING` and not (`FRAME0` and not `FAST`): the ball's horizontal counter steps on every frame when fast, every other frame when slow, never while serving. The vertical motion is unchanged.
- A miss is the ball meeting a goal while moving into it: `MISS_LEFT` = ball, left goal, `HDIR` low. `U44` flip-flops `LOST_LEFT` and `LOST_RIGHT` are set by it (sampled mid-dot like `HDIR`, which turns at the same edge), cleared by `SERVE_END`, and held clear while the game is over. `SERVING` is either.
- While serving the ball is hidden from the picture only (`BALLV_N` = OR(`BALL_N`, `SERVING`) into the mixer) and does not move sideways; it keeps moving and bouncing off the top and bottom walls, unseen. At its goal `GO_*` only repeats the turn already taken, and `MISS_*` stays 0. `U46` 74LS93, cleared while not serving, counts `FRAME1` falls; its `QD` (`SERVE_END`) rises after 8, 28 to 32 frames into the serve, and ends it. The ball reappears at the goal it went into (dots 2–3 or 92–93), already turned, and leaves slow. On the way out it passes the loser's own paddle from behind, which may set its angle.

### Scores, game over and Start

- `U48` (left) and `U49` (right) 74LS90, as ÷10 (`QA` → `CKB`): the left player scores when the ball is lost on the right (`CKA` = `LOST_RIGHT_N`, which falls when `LOST_RIGHT` is set), and the other way round. `U50` 74LS107 `TENS_L` and `TENS_R` are set when the units go from 9 to 0 (`QD` falls).
- `WIN` = (`TENS_L` and `SL0`) or (`TENS_R` and `SR0`): 11, the first odd score past 10. `U39` flip-flop 2 `GAME_OVER` is set by it; `PLAY` is its `Q_N`. While the game is over: `PAD_N` is high (paddles hidden, no paddle hits), `U44` is held clear (no misses: the ball bounces off the goals as walls), the scores stay.
- `SW1` Start, pulled up by `R1`: `START_N` low clears both units counters (`R01`, `R02` through `START`), `TENS_L`, `TENS_R` and `GAME_OVER`.
- No reset: at power-on the scores, the game state and the ball hold random values (odd shapes for units counts 10–15); press Start for a new game.

### Score digits

- `U52` 74LS157 picks the left (`H6` low) or right units count into `U53` 74LS48, whose active-high segments `SEG_A`–`SEG_G` light the units digit of whichever half the beam is in. The tens digit is a "1" drawn from `TENS` (picked by `U31`), or nothing.
- A digit cell is 4 dots × 32 lines on lines 32–63; its rows are `V4`–`V2` (eight 4-line rows), its columns `H1`–`H0`. `U54` 74LS157 picks by `V4` (top half, bottom half): `FE` (f, then e), `BC` (b, then c), `GD` (g, then d), `A_TOP` (a, then nothing).
- `REGION` is lines 32–63 at dots with `H5` and `H2` low and `H4` ≠ `H6`. The glyph is five terms, each a NAND into `U58` 74LS30: a on row 0; g on row 3 and d on row 7 (`GD` on rows with `V3` and `V2` high); f and e on column 0 (`FE`); b and c on column 3 (`BC`), in the units cell (`UNITS`: `REGION` and `H3` high); the tens "1" on column 3 of the tens cell (`H3` low). The column bits `H0` and `H1` reach the terms only through gates opened by `REGION`, so the terms stay still outside the digits (otherwise the simulation slows to 1.25× real time).

Mixer (`U16`): `BALLV_N`, `TOP_N`, `BOTTOM_N`, `PAD_N`, `NET_N`, `SCORE_N`; two inputs at +5 V.

## When it works

- The TV shows the top and bottom walls, the dashed net, a paddle near each side edge and the two scores at the top. Turning the left knob towards its right stop (Q) moves the left paddle up, towards its left stop (A) down; the right knob (Up and Down) the same for the right paddle. The paddle follows the knob within two frames.
- The ball bounces off the top and bottom walls and off the paddles. Where no paddle stops it, it reaches the edge (dots 2–3 or 92–93, the goals are not drawn). Hitting it with the top or bottom quarter of the paddle sends it off steeply, with the middle quarters at a shallow angle, up from the upper half and down from the lower half.
- A rally starts slow (the ball crosses in about 1.6 s) and speeds up (about 0.8 s) after the ball has turned left twice: three or four hits.
- A ball that gets past a paddle disappears; about 0.6 s later it comes back out of that edge, slow, heading for the player who just scored, whose score has gone up by one.
- At 11 the paddles disappear and the ball bounces around all four edges at speed; the scores stay. Start (`SW1`) clears both scores to 0 and the paddles come back.
- At power-on the scores may show any number or odd shapes, and the game may start over or in play: press Start.
- The 74LS48 draws 6 and 9 without their tails.

## Puzzles

Faults that change the picture or what is on it, each seen within a few seconds of Start (none needs a game played to 11):

| Puzzle | Fault | Symptom | Path to the fix |
|---|---|---|---|
| Mirror paddle | `U31` input `S` (`H6`) stuck at 0 | the right paddle follows the left knob; the right knob does nothing (the right tens digit is the left one's) | `H6` pulses on `U31` pin 1, leg and pad, yet `PADV` (pin 9) follows `LPADV` only: the chip reads its select wrong: swap `U31` |
| Shy paddles | Solder Bridge between `U31` pin 10 (`RPADV`) and pin 11 (`LPADV`) | each paddle shows only on the lines it shares with the other; knobs apart, both vanish and the ball goes through | `LPADV` and `RPADV`, two `U32` outputs, read the same on both pins: cut the bridge on the solder side of `U31` |
| Paddle fence | Dry Joint at `U33` pin 4 (`H34_LOW`) | four left paddles at dots 6, 14, 22 and 30, moving together; the ball turns at whichever it meets first | `COL_L_N` pulses four times a line; `H34_LOW` pulses on pin 4's pad (B.Cu) but not on the leg (F.Cu): resolder `U33` pin 4 |
| Long paddle | Broken Trace on `LPAD_CK` from `U32` pin 1 to `U28` pin 14 | the left paddle runs from the knob's height down to the bottom wall; every return off it leaves steeply up | `LPAD_CK` pulses at `U32` pin 1, nothing at `U28` `CKA`: the counter stays at line 0 and `LDONE` is never set: mend the Trace |
| Invisible ball | Broken Trace on `SERVING` between `U43` pin 10 and `U38` pin 10 | no ball, yet the game goes on: the scores go up | `BALL_N` pulses at `U38` pin 9; `SERVING` is driven at `U41` and `U43` but nothing at `U38` pin 10 (floating, read high, hides the ball): mend the Trace |
| Double scores | `U10` output `4Y` (`HX`) stuck at 1 | each score drawn twice, 8 dots further out (left: dots 0–11 and 16–27; right: 64–75 and 80–91) | `HX` stays high at `U10` pin 11 while `H4` and `H6` pulse on its inputs: swap `U10` |
| Folded digits | `U54` input `S` (`V4`) stuck at 1 | the digits are drawn as their bottom halves twice: no top bar, odd shapes that change with the score | `V4` pulses at `U54` pin 1, yet `A_TOP` (pin 12) never lights: swap `U54` |
| Rolling picture | Dry Joint at `U10` pin 2 (`VSYNC_N`) | the picture rolls up the screen without stopping; the paddles and the game work | the paddles prove `VSYNC_N` is good (it triggers `J3` and `J4`): it pulses on `U10` pin 2's pad but not on the leg, so `SYNC_N` carries only the line syncs: resolder `U10` pin 2 |
| Lost paddles | Dry Joint at `U7` pin 4 (`V2`) | the picture rolls, the paddles are gone, the ball never crosses the court | `FRAME_END_N` comes at line 307: the frame is 308 lines and line 308 never comes, so no `VSYNC_N` (no sync, no trigger) and no `MOVE`; `V2` pulses on pin 4's pad, not on the leg: resolder `U7` pin 4 |

Each teaches something different: a select input stuck (a multiplexer shows the wrong source), a bridge between two outputs (wired-AND of two pictures), a decoder input left open (one column becomes four), a counter clock cut (a stop flag never comes), a hidden signal that is still there, a decoder output stuck (a picture region repeated), a stroke selector stuck (a glyph folded), sync lost on the way out while the Machine is fine, and a frame counter cut short (everything timed by the frame's end goes at once).

Netlist, Board and manuals: [TENNIS_DESIGN.md](TENNIS_DESIGN.md).

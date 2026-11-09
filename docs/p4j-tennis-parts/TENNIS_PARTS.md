# P4j — Tennis Parts

Roadmap step P4j ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the code that the 070 paddle tennis needs, and the tests of that Machine. The Machine itself is specified in [TENNIS.md](../machines/070-tennis/TENNIS.md).

Decided with the user (2026-11-10):

- Faults may go anywhere on the Board, old circuits included; the "only in the new circuits" rule is gone from the roadmap.
- The netlist is built in stages, each tested before the next (ball and box, paddles, hits, misses and scores, digits, game over), with one Board layout at the end.
- Winning score 11, like the arcade game: units from a 74LS90 per player, the tens a flip-flop drawn as a "1".
- After a miss the ball is served from the loser's edge, towards the scorer.
- Four angles (steep and shallow, up and down, by the quarter of the paddle hit) and a speed-up during a rally.
- Keys: Q and A turn the left player's knob, Up and Down the right player's.
- Score chips: 74LS157 multiplexers and a 74LS48 decoder (active-high outputs).
- A Start button; at power-on the scores and game state are random until Start, as on real chips.
- The scores at the top, either side of the net.
- No Puzzles in this step: a later roadmap row, once the user has played it.

Decided while designing (2026-11-10), for the user to overrule:

- The Machine is called Paddle Tennis (`tennis`), not Pong: the circuit is our own and the game's name belongs to someone else. The docs cite Pong as the inspiration, as 060 does.
- The controllers are wired the other way round, so the knob's right stop (and the Up key) puts the paddle at the top.
- Knob keys turn at 100 % a second (was 50 %), for every Machine: at 50 % a paddle needs 2 s to cross the court, longer than the fast ball takes.
- The serve waits 29–32 frames, about 0.6 s; the ball crosses in about 1.6 s slow and 0.8 s fast; it speeds up after turning left twice.
- 58 chips (060 had 26).

## New code

1. **74LS157 quad 2-to-1 multiplexer.** `part U31 74LS157`; DIP-16, the datasheet pinout (`S` `1A` `1B` `1Y` `2A` `2B` `2Y` `GND` `3Y` `3B` `3A` `4Y` `4B` `4A` `G_N` `VCC`). Each `Y` is its `A` while `S` is low, its `B` while `S` is high; `G_N` high forces every `Y` low. No state. Datasheet `74LS157.md`.
2. **74LS48 BCD to 7-segment decoder.** `part U53 74LS48`; the 74LS47's pinout, function table and glyphs, but the segment outputs `SA`–`SG` are high for a lit segment (the 74LS48's 2 kΩ pull-ups give the high). Datasheet `74LS48.md`.
3. **Reversed controller.** `PADDLE … reversed=yes` (or `no`, the default): the pot wired the other way round, the longest pulse at the left stop and the shortest at the right. The knob, the Session and the command line are unchanged. Datasheet `PADDLE.md` says so.
4. **Two players' keys.** With one knob, Up and Down turn it, as on 045. With two or more, Q and A turn the first knob in the netlist (`J3`) and Up and Down the second (`J4`); Q and Up turn towards the right stop. Keys turn at 100 % a second. The slider tooltips name each knob's keys.

Nothing else is new: the 74LS74, 74LS93, 74LS90, 74LS107, the gate packages, the controller, the button, the TV and the Faults already exist. The watch budget (48) is unchanged: the Machine has no lamps, the TV watches two Nets.

## Machine

The paddle tennis is built as in [TENNIS_DESIGN.md](../machines/070-tennis/TENNIS_DESIGN.md): netlist in six stages, Board (laid out by the user in KiCad), manuals. No Puzzles.

## Tests

- Unit tests for the 74LS157 (each `Y` follows `A` or `B` by `S`, `G_N` high forces 0, data, select and strobe delays, X on `S` with `A` = `B`, X on `G_N`) and the 74LS48 (glyphs 0–15 high-true, blanking and lamp test as the 74LS47, X inputs), and `PADDLE` `reversed` (pulse lengths at 0, 0.5 and 1, the parameter refused unless `yes` or `no`).
- `tests/machines/tennis_test.cpp`, on the TV picture, one group per stage:
  - court: the top and bottom walls, the dashed net, no side walls; the ball turns at dots 2–3 and 92–93 and never lights dots 0–1 or 94–95;
  - paddles: at the left stop, half way and the right stop, each paddle's 16 lines start where TENNIS.md says, at its columns;
  - hits: a test player follows the ball; the ball turns at each paddle and leaves at the angle of the quarter it hit (4, 2, −2, −4 lines a frame); after a serve slow, fast after two turns to the left;
  - misses: after Start, with a paddle out of the way, the ball disappears at that edge for about 0.6 s, the other player's score goes from 0 to 1, the ball comes back out of that edge slow;
  - digits: after Start both scores read 0 on the TV, and the scorer's digit changes after a point; 10 shows as "1" and "0";
  - from the game over stage on, every case presses Start first (a power-on may be game over);
  - game over: from a power-on state at 10 (seed picked by the test), one more point ends the game: paddles gone, the ball bounces off the side edges, scores kept; Start brings back 0 to 0 and the paddles.
- The paddle tennis joins the per-Machine checks: Board against netlist, KiCad DRC, Board scene, manuals; a Session test with every instrument attached.
- Release build: 1.00× with every instrument attached; screenshots of play and of the game over picture.

Design: [TENNIS_PARTS_DESIGN.md](TENNIS_PARTS_DESIGN.md). Plan: [TENNIS_PARTS_PLAN.md](TENNIS_PARTS_PLAN.md).

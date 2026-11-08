# P4g — Ball Parts

Roadmap step P4g ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the code that the 060 bouncing ball needs, and the tests of that Machine. The Machine itself is specified in [BOUNCING_BALL.md](../machines/060-bouncing-ball/BOUNCING_BALL.md).

Decided with the user (2026-11-08):

- Playfield: a box of four walls and a dashed net down the middle; the ball bounces off all four walls. The detailed design came to 26 chips (the first estimate was about 23).
- Timing: a 2 MHz dot clock (96 × 256 visible dots and lines), a 2-dot × 8-line ball moving 2 dots and 4 lines per frame; the TV is unchanged.
- New Parts: 74LS107, 74LS27 and 74LS93, each used where Pong uses its kind of part.
- Five Puzzles.

## New code

1. **74LS27 triple 3-input NOR.** `part U12 74LS27`; a gate package row (DIP-14, the datasheet pinout: `1A` `1B` `2A` `2B` `2C` `2Y` `GND` `3Y` `3A` `3B` `3C` `1Y` `1C` `VCC`), datasheet typical delays. Datasheet `74LS27.md`.
2. **74LS93 4-bit binary counter.** `part U23 74LS93`; DIP-14 with `VCC` on pin 5 and `GND` on pin 10, like the 74LS90. A ÷2 (`QA`, clocked by `CKA`) and a ÷8 (`QB`–`QD`, clocked by `CKB`), both on the falling edge; wired `QA` → `CKB` it counts 0–15. `R01` and `R02` both high clear it, asynchronously. Power-on: random state. The ÷8 outputs change with their ripple delays (`QD` last). Datasheet `74LS93.md`.
3. **74LS107 dual JK flip-flop with clear.** `part U26 74LS107`; DIP-14, the datasheet pinout (`1J` `1Q_N` `1Q` `1K` `2Q` `2Q_N` `GND` `2J` `2CLK` `2CLR_N` `2K` `1CLK` `1CLR_N` `VCC`). Negative-edge clocked: `J` and `K` are sampled just before the clock falls; 0 0 holds, 1 0 sets, 0 1 clears, 1 1 toggles. `CLR_N` low clears it, asynchronously. Power-on: random state. Datasheet `74LS107.md`.

Nothing else is new: the bouncing ball's 74LS161, 74LS20, 74LS30, 74LS04, 74LS00, 74LS02, 74LS86, 74LS74, oscillator, connectors, TV and Faults (stuck outputs, Broken Traces, Solder Bridges) already exist. The watch budget (48) is unchanged: the Machine has no lamps, the TV watches two Nets.

## Machine

The bouncing ball is built as in [BOUNCING_BALL_DESIGN.md](../machines/060-bouncing-ball/BOUNCING_BALL_DESIGN.md): netlist, Board (laid out by the user in KiCad), manuals, five Puzzles.

## Tests

- Unit tests for the 74LS27 (truth table of each gate, delays), the 74LS93 (÷2 and ÷8 on falling edges, 0–15 with the outside link, ripple order of `QB`–`QD`, asynchronous clear, X) and the 74LS107 (hold, set, clear, toggle on the falling edge only, `J`/`K` sampled before the edge, asynchronous clear, X).
- `tests/machines/ball_test.cpp`, on the TV picture: fault-free (locked; the walls and the dashed net where the spec puts them; one 2-dot × 8-line ball, moving 2 dots and 4 lines per frame; the ball bounces off the walls and stays in the box); each Puzzle shows its Symptom and is fixed by its Repair (BOUNCING_BALL.md, Puzzles).
- The bouncing ball joins the per-Machine checks: Board against netlist, KiCad DRC, Board scene, manuals; a Session test with every instrument attached.
- Release build: 1.00× with every instrument attached; a screenshot of the ball in its box.

Design: [BALL_PARTS_DESIGN.md](BALL_PARTS_DESIGN.md). Plan: [BALL_PARTS_PLAN.md](BALL_PARTS_PLAN.md).

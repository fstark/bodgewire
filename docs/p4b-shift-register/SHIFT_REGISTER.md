# P4b — Shift Register

Roadmap step P4b ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the code that the 020 LED chaser needs, and the tests of that Machine. The Machine itself is specified in [LED_CHASER.md](../machines/020-led-chaser/LED_CHASER.md) and built as in [LED_CHASER_DESIGN.md](../machines/020-led-chaser/LED_CHASER_DESIGN.md).

## New code

1. **74LS164 Part.** 8-bit serial-in, parallel-out shift register, DIP-14. On each rising `CLK` edge, `QA` takes `A` AND `B` and every other output takes the one before it (`QB` ← `QA`, … `QH` ← `QG`). `CLR_N` low clears all outputs to 0 at once, without the clock. Power-on contents are seeded random, like the other registers. Datasheet `74LS164.md`.
2. **Watch budget.** Eight LEDs watch 16 Nets. With every instrument attached as well (8 Clips, 2 Hooks, probe, meter: 12) the worst case is 28, plus one while a lead moves. The limit goes from 16 to 32 watched Nets. This costs about 1 MB of memory per extra slot and a little more merging on the devices thread, not on the simulation ([ARCHITECTURE.md](../ARCHITECTURE.md#performance)).

Nothing else is new: the LED, its glow and its drawing (P4a) already handle any number of LEDs; Solder Bridges, cracks and stuck outputs already exist.

## Machine

The LED chaser is built as in LED_CHASER_DESIGN.md: netlist, Board (laid out by the user in KiCad), manuals, four Puzzles.

## Tests

- Unit tests for the 74LS164: shift, `A` AND `B`, clear, power-on.
- `tests/machines/chaser_test.cpp`: fault-free, one low bit runs `S1` → `S8` once settled; each Puzzle shows its Symptom and is fixed by its Repair (LED_CHASER.md, Puzzles).
- The chaser joins the existing per-Machine checks: Board against netlist, KiCad DRC, Board scene, manuals.
- Every instrument attached to the chaser at once, with all eight LEDs watched: no "too many watched nets".
- Release build: 1.00× with every instrument attached; one LED seen lit in a screenshot.

Design: [SHIFT_REGISTER_DESIGN.md](SHIFT_REGISTER_DESIGN.md).

# P4a — Counter LED

Roadmap step P4a ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the code that the 010 counter needs, and the tests of that Machine. The Machine itself is specified in [COUNTER.md](../machines/010-counter/COUNTER.md) and built as in [COUNTER_DESIGN.md](../machines/010-counter/COUNTER_DESIGN.md).

## New code

1. **LED Part.** `part D1 LED` in a netlist; Pins `K` (pad 1) and `A` (pad 2); no power Pins; it never pulls a Net. KiCad footprint `LED_D5.0mm`; Datasheet `LED.md`.
2. **Glow.** An LED is lit while current flows: anode driven high and cathode driven low. A floating end leaves it dark. Brightness is the share of each 20 ms it was lit, so an LED switching fast looks half lit.
3. **Drawn on the Board.** A red disc in the LED's outline, under its pads, from dark to bright with a halo. It follows the electrical Nets (a Broken Trace darkens it) and is dark with the power off.
4. **Watch budget.** Each LED watches its two Nets. The counter's worst case (every instrument attached, plus `D1`) fits in the 16 watched Nets.

## Machine

The counter is rebuilt as in COUNTER_DESIGN.md: netlist, Board, manuals, three Puzzles.

## Tests

- Unit tests for the Part, the glow and its Consumer.
- `tests/machines/counter_test.cpp`: each Puzzle shows its Symptom and is fixed by its Repair (COUNTER.md, Puzzles); fault-free, `D1` blinks once a second.
- Existing Board, netlist and manual tests follow the new counter.
- Release build: 1.00× with every instrument attached; `D1` seen lit and dark in screenshots.

Design: [COUNTER_LED_DESIGN.md](COUNTER_LED_DESIGN.md). Plan: [COUNTER_LED_PLAN.md](COUNTER_LED_PLAN.md).

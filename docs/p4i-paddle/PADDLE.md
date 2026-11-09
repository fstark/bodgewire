# P4i — Paddle Controller

Roadmap step P4i ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the code that the 045 knob meter needs, and the tests of that Machine. The Machine itself is specified in [KNOB_METER.md](../machines/045-knob-meter/KNOB_METER.md). Pong (070, P4j) reuses the controller, two of them, triggered from `VSYNC`.

Decided with the user (2026-11-09):

- Latch: the 74LS175 (edge-triggered: the count is taken when the pulse ends, so the digits never show the counting).
- The controller sits behind a 4-pin Connector (`TRIG_N`, `OUT`, `VCC`, `GND`) and runs on the Board's power: it works only while its `VCC` and `GND` pins are on +5 V and 0 V, like a chip.
- Knob input in `bw_ui`: a Knob section in the side panel with a slider (mouse drag, mouse wheel), the Up and Down arrow keys turning the first knob while held; `--knob J2:40%` on the command line.
- Five Puzzles.

## New code

1. **74LS175 quad D flip-flop with clear.** `part U9 74LS175`; DIP-16, the datasheet pinout (`CLR_N` `1Q` `1Q_N` `1D` `2D` `2Q_N` `2Q` `GND` `CLK` `3Q` `3Q_N` `3D` `4D` `4Q_N` `4Q` `VCC`). On each rising edge of `CLK` every `Q` takes its `D` (as it was just before the edge); `Q_N` is the opposite. `CLR_N` low clears all four at once and holds them. Power-on: random state. Datasheet `74LS175.md`.
2. **Paddle controller Part.** `part J2 PADDLE min=30us max=9.93ms`: a Connector (KK-4 header, pins 1 `TRIG_N`, 2 `OUT`, 3 `VCC`, 4 `GND`) with the controller behind it, the logic-only equivalent of a pot and a 555 one-shot. `OUT` is low while idle. A falling edge on `TRIG_N` starts one high pulse on `OUT`, 0.1 µs later (the 555's delay); its length is `min` + knob × (`max` − `min`), the knob as a fraction 0–1 read when the pulse starts. A fall during a pulse is ignored; nothing else on `TRIG_N` matters. `min` and `max` are durations, `max` > `min` ≥ 1 µs, both required: each Machine picks the range its circuit counts. Datasheet `PADDLE.md`.
3. **Powered through its Connector.** The controller is Powered only while `J2`'s `VCC` pin is on a +5 V island and its `GND` pin on a 0 V island, by the rule chips follow (P3f). Unpowered, its Pins are off every Net: `OUT` drives nothing and the Net it feeds floats. Its `VCC` and `GND` join the rails implicitly in the netlist, as a chip's do. They do not supply the Board.
4. **The knob.** Each `PADDLE` has a knob, 0 (left stop) to 1 (right stop), 0 until set. It is held by the Session, so it can be turned with the power off, and keeps its position across power-off, power-on and the rebuild after a Repair. A turn takes effect at the next trigger, with no wake: the simulation reads it when a pulse starts.
5. **Turning it in the UI.** A `Knob` section in the side panel, shown when the Machine has a `PADDLE`: one slider per controller, named by its reference (`J2`), 0–100 %; drag it, or roll the mouse wheel over it (1 % a notch). The Up and Down arrow keys turn the first controller's knob while held, 50 % a second, unless a text field has the keyboard. Pong's second controller gets its own keys in P4j.
6. **Command line.** `bw run x.bwp --knob J2:40%` and `bw_ui … --knob J2:40%` set a knob before power-on; several separated by commas. In `bw_ui` the player can turn it afterwards.
7. **Watch budget.** Unchanged: the knob meter's two digits watch 16 Nets; the controller needs no watch (it is an input).

Nothing else is new: the knob meter's 74LS90, 74LS00, 74LS47, digits, resistors and its Faults (Broken Traces, stuck outputs, dry Joints, Solder Bridges) already exist.

## Machine

The knob meter is built as in [KNOB_METER_DESIGN.md](../machines/045-knob-meter/KNOB_METER_DESIGN.md): netlist, Board (laid out by the user in KiCad), manuals, five Puzzles.

## Tests

- Unit tests for the 74LS175 (each flip-flop takes `D` on the rising edge only, `D` changed at the edge is not seen, asynchronous clear wins over the clock, delays, X on `CLK`, `D` or `CLR_N`) and the `PADDLE` (pins and package, parameters refused when missing or `max` ≤ `min`, idle low, a fall starts a pulse of `min`, `max` and half way for the knob at 0, 1 and 0.5, a fall during a pulse ignored, a rise or an X does nothing, the knob turned during a pulse does not change it).
- Power: a Board whose `VCC` to the controller is cracked lists `J2` as not Powered, and its `OUT` Net floats.
- `--knob` parsing; the Session keeps a knob across power-off, power-on and a rebuild, and refuses an unknown reference.
- `tests/machines/knob_test.cpp`: fault-free, the display reads 00 at the left stop, 99 at the right, and 20, 50, 79 between (KNOB_METER.md, Circuit), steady over cycles; a knob turned while running shows within two cycles; each Puzzle shows its Symptom and is fixed by its Repair (KNOB_METER_DESIGN.md, Puzzles).
- The knob meter joins the per-Machine checks: Board against netlist, KiCad DRC, Board scene, manuals; a Session test with every instrument attached.
- Release build: 1.00× with every instrument attached; a screenshot after `--knob J2:42%` shows 42 and the Knob section.

Design: [PADDLE_DESIGN.md](PADDLE_DESIGN.md). Plan: [PADDLE_PLAN.md](PADDLE_PLAN.md).

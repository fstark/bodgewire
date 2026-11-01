# P4d — Push Button

Roadmap step P4d ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the code that the 030 dice needs, and the tests of that Machine. The Machine itself is specified in [DICE.md](../machines/030-dice/DICE.md).

## New code

1. **Button Part.** `part SW1 BUTTON` in a netlist; Pins `1` and `2`; no power Pins. KiCad footprint `Button_Switch_THT:SW_PUSH_1P1T_6x3.5mm_H5.0_APEM_MJTP1250`: a 6 × 3.5 mm tactile switch with two pads. Its courtyard is drawn with lines, which the Board reader now reads too. Datasheet `BUTTON.md`.
2. **Behaviour.** Like a resistor, one end goes on `GND` or `VCC` and the other on a signal; the netlist builder refuses anything else. On a damaged Board, a button whose rail end is cut off does nothing, like a resistor. Released, it drives nothing. Pressed, it drives the signal to its rail as strongly as a chip output: it beats a pull-up, and fights a chip output like a second output would. No bounce (later).
3. **Pressing in the UI.** Only while powered. Holding the left mouse button on the button's body (not on a pad) presses it, in any tool; letting go anywhere releases it. Right-click on the body latches it pressed until the next click on it, so both hands are free for the instruments. A pressed button is drawn pushed in. Power off releases every button.
4. **Through the Session.** The Session holds which buttons are pressed and hands each press and release to the simulation, which applies it at the start of the next slice (within 1 ms of the click). A rebuild after a Repair keeps nothing pressed.
5. **Headless and command line.** `bw run x.bwp --press SW1:200ms:700ms` presses `SW1` at 200 ms and releases it at 700 ms of simulated time, exactly; several presses separated by commas. `bw_ui` accepts the same option, for screenshots (applied at the first frame at or after each time).
6. **Watch budget.** Unchanged: seven LEDs watch 11 distinct Nets; with every instrument attached the worst case is 23, plus one while a lead moves, within the 32 of P4b.

Nothing else is new: the dice's chips (74LS161, 74LS04, 74LS00, 74LS02), its LEDs, and its Faults (Broken Traces, stuck outputs) already exist.

## Machine

The dice is built as in [DICE_DESIGN.md](../machines/030-dice/DICE_DESIGN.md): netlist, Board (laid out by the user in KiCad), manuals, four Puzzles.

## Tests

- Unit tests for the Button: pressed and released against a pull-up, against a chip output, a builder refusal when neither end is on a rail, left out of a damaged Board's build when its rail end is cut off; a courtyard read from lines.
- `--press` parsing, and press timing exact to the nanosecond headless.
- `tests/machines/dice_test.cpp`: fault-free, rolls of different lengths stop on every face from 1 to 6, and the LEDs match the face; each Puzzle shows its Symptom and is fixed by its Repair (DICE.md, Puzzles).
- The dice joins the existing per-Machine checks: Board against netlist, KiCad DRC, Board scene, manuals.
- Release build: 1.00× with every instrument attached; a face seen lit in a screenshot after `--press`.

Design: [PUSH_BUTTON_DESIGN.md](PUSH_BUTTON_DESIGN.md). Plan: [PUSH_BUTTON_PLAN.md](PUSH_BUTTON_PLAN.md).

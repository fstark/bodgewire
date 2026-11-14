# P5i — Switches on the Board

Roadmap step P5i ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): one switch Part for buttons and keys, so the BW-1's hex keypad (P5j) can sit on the Board as keys wired by copper to a scanned matrix, and fail like any other copper.

Until now a button (P4d) was a Driver: pressed, its signal pin drove the rail's value; it needed one leg on `GND` or `VCC`. A key in a matrix joins two signals (a row and a column) and neither is a rail. A switch now joins its two Nets while closed, whatever they are.

## Decided

With the roadmap row (2026-11-14):

- **One switch Part** with two legs on two Nets; closed, it joins them. A button is a switch with one leg on a rail. The P4d button is folded in, and its Machines (030, 040, 070, 090, 100, 110) are retested.
- **Nets stay fixed while running** (no merging or splitting). One Board-wide switch component has a Pin on every Net a switch touches. When a switch changes, it rebuilds the groups of Nets joined by closed switches (only those). When a Net's Drivers change, it re-resolves that Net's group only (a Net in no group: it returns at once). It combines what the real Drivers push, read by the kernel without the component's own drive (so a switch never hears its own echo), and drives the result on every Net of the group.
- **Ghosting, cracks and bridges follow from the copper.** A rail is a Net like any other.
- **Switch Faults** on every button and key: stuck closed, never closes.
- Keys pressed from the PC; keycaps plain (legend, pressed), drawn with the ASCII keyboard's cap code.
- **74145** Part (open-collector rows).
- Tested on a bench netlist in `tests/data/`, no test Machine.
- No contact bounce. No speed work: a switch costs a wake per change of its Nets, about one gate input. Waking its Pins only while closed is the next step if a measurement asks for it.
- Floating devices behind a Connector stay faultless. The ASCII keyboard is unchanged.

Asked at the start of the step (2026-11-14):

- **Key footprint: Cherry MX 1u** (`Button_Switch_Keyboard:SW_Cherry_MX_1.00u_PCB`, 19.05 mm pitch). A 6 × 4 keypad is about 115 × 76 mm.
- **Parameters: `legend=` and `pc=`.** `part SW5 KEY legend=GO pc=G`. `pc=` defaults to the legend when the legend is one character (`KEY legend=7`). A `BUTTON` takes an optional `pc=` too.
- **PC keys through Typing (F2).** On a Machine with Board keys and no ASCII keyboard, F2 turns Typing on and off. While it is on, a PC key held down holds its Board switch down, several at once (so the player can make a ghost), and the tool shortcuts are off.
- **Fault syntax: `fault switch SW1 closed`** (stuck closed) and **`fault switch SW1 open`** (never closes). Repaired by swapping the switch (Swap tool on its body, `--repair swap:SW1`), like a chip.

## New

1. **The kernel: a switched Pin.** A new Pin kind for the switch component. Its drive is at Supply strength, so it sets the whole group's value on each of its Nets. The component is woken when the Drivers of one of its Nets change, not when the Net's value changes. It reads a Net's drives without its own.
2. **Switch and switch network Parts.** `BUTTON` and `KEY` are one class, `Switch`. The builder adds one `SwitchNetwork` per Circuit with a Pin on each Net a switch leg is on. A leg on no Net (a pad cut off every Net) is open. A switch with both legs on one Net does nothing. A press reaches the simulation as before: the Session wakes the switch, which reads the press at that instant and wakes the network, so presses stay deterministic and every existing `wake(button->id())` keeps working. The switch's legs draw the same random bits at power-on as the button's did, so every seed's power-on state (the Blackout brief) is unchanged.
3. **The rail rule goes.** A switch joins any two Nets. A button cut off its rail by a crack now joins its signal to a dead island, which does nothing. Before, the electrical netlist left it out.
4. **A pressed button forces its rail's value** (Supply strength through the switch), so it beats a TTL output instead of fighting it to a contention 0. The 030–110 Machines do not press against an output, so their tests are unchanged.
5. **74145** BCD-to-decimal decoder/driver, outputs `Y0_N`–`Y9_N` open collector.
6. **Puzzle line** `fault switch REF closed|open`, refused on a Part that is not a `BUTTON` or `KEY`, repaired by a swap.
7. **`bw_ui`**: keys drawn as keycaps on their footprint (legend, pressed), clicked like buttons (left: held, right: latched; the Swap tool swaps them instead); PC keys through Typing. `--press` works for keys too. A Machine with an ASCII keyboard does not type on its Board keys (none has both yet).

## Tests

- Kernel: two Nets joined by a closed switch take the strongest drive of the pair. A pull-up reaches the other Net, a rail beats a TTL output, two TTL outputs give a contention 0, and an X gives X. A group with nothing driving floats, and each Net reads as it would alone. Opening the switch gives each Net back its own value. Chains of three Nets join through two switches. The component is never woken by its own drive (a wake count).
- 74145: truth table for 0–15, X folding, open-collector release, delays.
- Bench netlist `tests/data/switch_bench.bwn`: a 74LS93 counts the 74145 through rows 0–3 of a 4 × 4 key matrix, the columns pulled up and read by a 74LS04, and a button to `GND` on a pulled-up line:
  - no key: every column high;
  - one key: its column low only while its row is scanned;
  - three keys at the corners of a rectangle: the fourth corner's column goes low on its row (a ghost);
  - a cracked row (the key's leg on a Net of its own, as a crack leaves it): the key does nothing;
  - two columns bridged: a key on either pulls both low;
  - stuck closed: low on every scan without a press; never closes: a press does nothing;
  - the button pulls its line to 0 and the inverter's output follows.
- Builder: a key between two signals builds; an unlisted leg is open; duplicate PC keys are refused; `SwitchOf`.
- Puzzle parsing and Machine loading of `fault switch` (errors: unknown reference, not a switch, bad state); a swap removes the Fault; on the dice, `closed` rolls without a press and `open` never rolls.
- Session: switches listed with their PC keys; a key pressed through the Session pulls its column.
- The six button Machines' tests pass unchanged (they wake the switch as they woke the button), and the 110's Blackout seed still freezes as its brief says.
- Release: `bw_ui` screenshot of a keypad (a scratch Machine, not committed) with keys up and down, viewed; the 070 and `bus_load` headless ratios within noise of before the kernel change.

Design: [SWITCHES_DESIGN.md](SWITCHES_DESIGN.md). Plan: [SWITCHES_PLAN.md](SWITCHES_PLAN.md).

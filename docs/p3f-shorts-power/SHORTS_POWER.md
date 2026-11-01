# Solder Bridges and Real Power — Specification (P3f)

Last P3 milestone (see [ARCHITECTURE.md](../ARCHITECTURE.md), Pins, nets, drivers and Roadmap). So far every chip works whatever its power Pins touch, and the only Faults are Stuck-at and Broken Traces. P3f adds Solder Bridges (the first Short) and real power: a chip works only while its power Pins sit on the right Rails. Four new Puzzles on the test-pattern board use them. Builds on the Repairs of [BOARD_REPAIR.md](../p2h-board-repair/BOARD_REPAIR.md) (P2h), the Cut tool of [BODGE_WIRES.md](../p3e-bodge-wires/BODGE_WIRES.md) (P3e) and the Multimeter of [SCOPE_METER.md](../p3d-scope-meter/SCOPE_METER.md) (P3d).

Terminology: [GLOSSARY.md](../GLOSSARY.md) (adds **Solder Bridge**, **Rail**, **Powered**, **Dead Rail**).

## Decisions

- **No Repair Validation.** A golden comparison can't be trusted on larger Machines (a fault seen only under one specific use, such as a program touching one bad RAM chip). There is no win condition for now: the player decides when the Machine is fixed. Revisit later.
- **Solder Bridge = Short between two neighbouring pins of one chip**, on the solder side (B.Cu). Pin n and n+1 on the same row of a DIP. Pad-to-Trace, Trace-to-Trace and under-chip Shorts are later.
- **A Solder Bridge joins the two pads' islands**, exactly like a Bodge Wire (P3e): the Nets merge at build, and the existing electrical rules give the Symptom (Contention, Tie to a Rail).
- **Drawn subtle**: a solder blob between the two pads on B.Cu, visible when zoomed in. Net highlighting and the Net panel ignore it (design Netlist, ARCHITECTURE Connectivity and visibility).
- **The Cut tool removes a Solder Bridge**, as it snips a Bodge Wire.
- **Real power, in volts.** Each power Pin of a Part declares the voltage it expects (74LS: VCC 5 V, GND 0 V). A Rail is a Net holding a `POWER` connector supply pin; its voltage is that pin's. A chip is Powered when every power Pin is on a Rail of exactly its voltage, worked out at each build (fixed topology), never simulated. Today only 5 V and 0 V exist; more Rails later add data, not code.
- **Unpowered chip**: all its outputs are Z; its inputs have no effect on their Nets. Power Pins on the wrong Rail are just Unpowered (no damage).
- **Dead Rail**: a Net holding supply pins of different voltages. It is no valid Rail, so every chip on it is Unpowered; the Multimeter reads 0 V on it. The bench supply limits its current: nothing is damaged, and removing the Short brings the board back.
- **Current-limit LED** next to the power control, lit while powered with a Dead Rail.
- **The Logic Probe stays independent of board power.**
- Out, later: Wrong Part (with chip removal and sockets, only if it can be more than "click and check every IC"), chip damage, near-voltage tolerance, intermittent power contacts.

## Scope

In: Solder Bridge Faults in `.bwp`, their drawing, Cut on a bridge, Part power-Pin voltages, Powered check per build, Dead Rail, Multimeter 0 V on a Dead Rail, current-limit LED, four Puzzles, headless tests.

Out: Repair Validation, Wrong Part, sockets, chip removal, other Shorts, damage, new Rails.

## Behaviour

### S1 Solder Bridge Fault

- Puzzle line `fault bridge U1.7 U1.8` (pins by number or name). Error naming it when the two pins are not neighbours on the same chip, or the Reference or pin is unknown.
- The two pads' islands are joined in every build until the bridge is cut.
- Drawn as a thin tin blob joining the two pads on B.Cu (seen zoomed in).

### S2 Cut a Solder Bridge

- `Cut (K)` with the power off: a click on a bridge removes it (`unbridge U1.7 U1.8`), before Traces, as for Bodge Wires.
- The blob is no longer drawn; continuity (P3d) stops beeping across it at once.
- `--repair` accepts `unbridge:U1.7:U1.8`.

### S3 Real power

- At each build every chip is checked: each power Pin on a Rail of its declared voltage → Powered; otherwise Unpowered (outputs Z, inputs ignored).
- Connectors are always Powered.
- The fault-free test pattern, `broken_trace`, `dead_counter` and `stuck_gate` behave exactly as before.

### S4 Dead Rail

- A Net holding `POWER` supply pins of different voltages: chips with a power Pin on it are Unpowered, the Multimeter reads 0 V on it.
- While powered with a Dead Rail, the current-limit LED next to the power control is lit.

### S5 Puzzles (test-pattern board)

- **Signal bridge**: two neighbouring signal pins bridged (Contention, 0 wins).
- **Bridge to a Tie**: a signal pin bridged to a neighbouring pin tied to GND or VCC (looks stuck, but swapping the chip doesn't fix it).
- **Unpowered chip**: a crack in the VCC Trace feeding one chip.
- **Dead Rail**: U1 pins 7 (ENP, VCC) and 8 (GND) bridged.
- Each with a title and Brief; pins and crack points chosen in the design.

### S6 Checks

- Headless:
  - parse of `fault bridge` and `unbridge:`; errors for non-neighbours, unknown pin;
  - a bridge joins two islands; unbridge restores them;
  - Powered: fault-free chips all Powered; a cracked VCC Trace leaves only its chip Unpowered with Z outputs; a GND pin wired to VCC is Unpowered;
  - Dead Rail: all chips Unpowered, Multimeter 0 V, LED state set;
  - each new Puzzle shows its Symptom on the TV and is fixed by its intended Repair (and the Dead Rail also by a Cut of the bridge only);
  - the three existing Puzzles and the fault-free frame unchanged.
- `--screenshot`, read by the agent: the Dead Rail Puzzle powered (LED lit). A bridge zoomed in on B.Cu: user check.
- Release: 1.00× running, rebuild still under 100 ms.

## Done when

- S1–S6 implemented; tests pass in debug and release.
- The user plays the four new Puzzles to the end.

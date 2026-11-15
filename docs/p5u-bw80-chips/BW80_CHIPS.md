# P5u — BW-80 chips

Roadmap step P5u ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), the first of the BW-80's (P5u chips, P5v video spike, P5w ROM, P5x Machine, P5y Puzzles). It brings the parts of the Sinclair ZX80 that the catalogue lacks. It also adds the resistor between two signal Nets that isolates the ZX80's data bus, and tests both on a bench before any Machine exists.

## The ZX80's chips

The inventory comes from Grant Searle's redrawn ZX80 schematic (issue 2 board, `zx80.pdf`) and its parts list, from his ZX80 page (searle.x10host.com/zx80/zx80.html):

| IC | Part | Function | In the catalogue |
|---|---|---|---|
| IC1 | Z80A ("780-1") | CPU at 3.25 MHz | `Z80` |
| IC2 | 4K mask ROM ("8332") | program and character set, `CS1` pin 20, `CS2` pin 21 on A12 | `2332` new |
| IC3, IC4 | 2114, 2114-1 | 1K RAM, a nibble each | `2114` |
| IC5 | 74LS373 | RAM data latch: character code, NOP bit (D6), inverse bit (D7) | new |
| IC6–IC8 | 74LS157 | ROM address: CPU address or character code and line counter, switched by `RFSH_N` | `74LS157` |
| IC9 | 74LS165 | dots out at 6.5 MHz | `74LS165` |
| IC10 | 74LS365 | keyboard and cassette onto the data bus at `KBD_N` | new |
| IC11, IC12 | 74LS00 | RAM select, VSYNC latch, data latch enable | `74LS00` |
| IC13 | 74LS04 | inverters | `74LS04` |
| IC14, IC15 | 74LS05 | open collector: eight force a NOP onto `D0`–`D7`, two paralleled drive the CPU clock | new |
| IC16 | 74LS10 | NOP condition, latch enable | new |
| IC17 | 74LS32 | I/O decoding (`KBD_N`) | `74LS32` |
| IC18, IC19 | 74LS74 | clock ÷ 2, HSYNC timing | `74LS74` |
| IC20 | 74LS86 | 6.5 MHz oscillator, inverse video | `74LS86` |
| IC21 | 74LS93 | character line counter | `74LS93` |
| IC22 | 7805 | regulator | `POWER` stands in |

Eight 1K resistors, R4–R11, join each CPU-side data line `D0`–`D7` to its memory-side line `D0'`–`D7'`. The CPU, the 74LS165, the 74LS365 and the 74LS05 NOP gates are on the CPU side; the ROM, both RAMs and the 74LS373 are on the memory side. During a display fetch the RAM drives `D'` with a character code while the 74LS05s pull `D` low, so the CPU executes a NOP and the latch still sees the character (Searle, "Data bus isolator"). The same holds for a 2.2K resistor R19 between the RAM select gate and the RAMs' `CS_N`, and for R24 in the HSYNC circuit: a gate's output on one side, inputs (and the edge connector) on the other.

Today the builder accepts a resistor only as a pull-up or pull-down. P5u adds the **series resistor**: either end on a signal Net. A Net that nothing else drives takes the other Net's value, weakly; a Net driven by a chip keeps its own value. When two weak values meet on a Net, the one through less resistance wins: on `D6'` a 47K pull-up (R28) loses to the CPU side's 0 through R7's 1K. Equal resistances that disagree give X.

Left for the steps that use them, as on the BW-1 and BW-2001: the analog edges of the circuit (the 74LS86 ceramic-resonator oscillator, `OSC` standing in; the RC reset, `POR`; the C11/R25 and C9/R2 pulse shapers on the 74LS165's load and clock, P5v; the modulator and cassette, P5x). The keyboard's diodes D3–D10 belong to the keyboard, decided at P5x (on the Board key by key, or a floating device). D11, the 60 Hz link, is not fitted: the BW-80 is the 50 Hz machine, R28 pulling `D6` high.

## Decided

With the roadmap row (2026-11-15): the BW-80 runs our own ROM (P5w); the real ZX80 ROM is a local reference outside the repo (P5v).

Asked at the start of the step (2026-11-15):

- **The `2332` mask ROM** for IC2: Sinclair's socket as drawn, 24 pins, `A11` on pin 18, `CS1` on pin 20, `CS2` on pin 21 (A12 on the ZX80), each select's polarity set by `select=` as on the 2316B. Offered instead: the 2532 EPROM of Searle's build (one select, A12 lost) or the 2732 of his parts list (two pins swapped against the socket).
- **A small Z80 bench** proves the chips, as P5j and P5n did: the ZX80's CPU, ROM, RAM, data bus resistors, NOP gates, data latch and keyboard buffer at their reference designators, running a `bw asm` program. No video: that is P5v.
- **No diodes in P5u** (the user, 2026-11-15): the keyboard's form is P5x's choice, and a floating keyboard reads the address lines itself.

Decided in this step's design (not the user's): any resistor with both ends on signal Nets is a series resistor; its `value=` and the pull-ups' decide a weak-against-weak fight, as above. For P5x: the builder refuses a switch on a joined Net and a pull of a joined Net without `value=`, and a repaired Machine that does not build stops the Session, so the BW-80's Board must keep its key Nets away from the data bus (or ElectricalNetlist must leave such a resistor out); decided there.

## New

1. **Series resistor**: an `R` Part between two signal Nets. One `ResistorNetwork` on the Circuit resolves every group of Nets joined by series resistors, as the P5i switch network resolves closed switches: a Net with a driver of its own (any strength) keeps it; any other Net in the group gets the value of the nearest driven Net or pull-up, counted in ohms, at Weak strength, 1 ps later. Pull-ups on a joined Net become part of its group. `value=` is required on series resistors and on pull-ups of joined Nets.
2. **`2332`**: 4K × 8 mask ROM, `select=` for `CS1`, `CS2`. On the ZX80: `part IC2 2332 select=LL contents=…`.
3. **TTL**: `74LS373` (octal transparent latch, three-state outputs), `74LS365` (hex buffer, three-state, two enables), `74LS05` (hex inverter, open collector), `74LS10` (triple 3-input NAND).
4. **A bench** `tests/data/bw80_bench.bwn` at the ZX80's designators: 6.5 MHz halved by IC18 and buffered by two paralleled 74LS05s with R20, the Z80, the 2332 at `$0000`, two 2114s at `$4000` (mirrored at `$C000`), R4–R11 and R28, R19 on the RAM select, the 74LS373 and the NOP gates as the ZX80 wires them, the 74LS365 at even I/O reads with two keys. Its program `roms/bw80_bench.asm` tests the RAM, runs a line of character codes in the `$C000` mirror as NOPs, and reads the keys, writing its results to RAM.

## Tests

- Series resistor: a Net with nothing else takes the other side's 0, 1 and X; a driven Net keeps its own; an open-collector 0 on one side and a strong 1 on the other give 0 and 1; a pull-up beyond a 1K loses to a driven value and wins over nothing; equal paths that disagree give X; a chain of three Nets; no driver anywhere: both float; each side follows 1 ps after the other changes; the builder's errors (no or bad `value=`, both ends on one Net or on rails, a switch on a joined Net).
- Board: a series resistor stays between its two islands; with its ends bridged or both on rails it is left out; the Net panel marks it on both Nets.
- `2332`: pin names; each `select=` combination selects only at its levels; access, select and float delays; parameter errors.
- TTL: truth tables, X handling and data sheet delays; the 74LS373 transparent while `LE` is high, holding on its fall, outputs off at `OE_N` high; the 74LS365 off unless both enables are low; the 74LS05 releases its 1.
- Bench, seeds 1–3: the program's result byte `$55` (RAM test, NOP line, latch, keys); the CPU's `D` never X during a write into the ROM's space while the ROM drives `D'`; the 74LS373 holds the last character code; a key on `A8` read as bit 0 low, `D6` read high through R7 and R28, `D7` low; a stuck RAM cell fails the RAM test.
- Release: the bench's headless ratio recorded; `bus_load`, `bus_load_z80`, 110 and BW-1 ratios unchanged.

Design: [BW80_CHIPS_DESIGN.md](BW80_CHIPS_DESIGN.md). Plan: [BW80_CHIPS_PLAN.md](BW80_CHIPS_PLAN.md).

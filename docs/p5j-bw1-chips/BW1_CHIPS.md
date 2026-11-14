# P5j — BW-1 chips

Roadmap step P5j ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), the first of four building the BW-1 on the KIM-1's own design (P5j chips, P5k monitor, P5l Machine, P5m Puzzles). This step brings the chips the KIM has and the catalogue lacks, tested on a bench before any Machine exists:

- the **6530** (MOS "ROM-RAM-I/O-timer" array): 1024 × 8 mask ROM, 64 × 8 RAM, two 8-bit ports with their data direction registers, an interval timer whose interrupt comes out on `PB7`. On the KIM, two of them hold the monitor (6530-002 and 6530-003), drive the display, read the keypad and time the cassette;
- the **6102**, MOS's 1K × 1 static RAM (an Intel 2102 equivalent): eight of them are the KIM's 1 KB at `$0000`;
- the **74125**, quad tri-state buffer: two of them put the 6102s' `DOUT` on the data bus (KIM Fig. 3.4).

The 2716, 2513 and 2114 keep their contents in code written for each; the new chips would add three more copies. Their storage becomes two shared blocks, a ROM block and a RAM block, with P5g's stuck cells working inside them, so a stuck cell is the same Fault in every memory chip.

## Decided

With the roadmap row (2026-11-14): the KIM-1's own design; our own monitor, not MOS's (P5k); the mask as a parameter; LED drivers as logic-only equivalents (P5l).

Asked at the start of the step (2026-11-14):

- **RAM name `6102`**, as on the KIM's chips and schematic. Its datasheet file says it is MOS's second source of the Intel 2102. Timing: the 500 ns grade of the KIM parts list.
- **6530 mask as `mask=` plus `contents=`**: `part U2 6530 mask=002 contents=../roms/bw1_002.rom`. One catalogue Part. `mask=002|003` picks the KIM decode; the ROM bytes come from a contents file like a 2716's, because they are our monitor.
- **Memory keyword in cell Faults**: `fault cell U2.D3 ram $3F 0`, `fault column U2.D0 rom 1`. A chip with two memories (the 6530) needs the name (`rom` or `ram`); a chip with one (2114, 2716, 2513, 6102) takes none, as today.
- **The 74125 in this step**, so the bench wires the RAM exactly as the KIM's Fig. 3.4. The other glue (7400, 7404, 7406, 7438, the 556) is P5l's.

## The 6530 as the KIM uses it

From the MCS6530 preliminary data sheet (MOS, August 1975) and the KIM-1 User Manual (Figs. 3.2–3.5, 3.11–3.13, Appendix A):

- **Pins** (data sheet p. 12): 1 `VSS`, 2 `PA0`, 3 `φ2`, 4 `RS0`, 5–8 `A9`–`A6`, 9 `R/W`, 10–15 `A5`–`A0`, 16 `RES`, 17 `PB7`/`IRQ`, 18 `CS1`/`PB6`, 19 `CS2`/`PB5`, 20 `VCC`, 21–25 `PB4`–`PB0`, 26–33 `D7`–`D0`, 34–40 `PA7`–`PA1`.
- **Selection**: the mask decides which of `RS0`, `CS1`, `CS2`, `A6`–`A9` select the ROM, the RAM and the I/O-timer, and with which polarity (data sheet Figs. 6, 7). On the KIM, `K5` (`$1400`–`$17FF`) goes to `CS1` of both chips, `K7` to `RS0` of the 002 and `K6` to `RS0` of the 003 (Fig. 3.2). The 74LS145's outputs are active low, so: ROM when `RS0` is low; RAM and I/O when `RS0` is high and `CS1` low, and `A9`–`A6` match the mask: 002 RAM `$17C0` (`1111`), I/O `$1740` (`1101`); 003 RAM `$1780` (`1110`), I/O `$1700` (`1100`) (Figs. 3.11, 3.12). `PB6` is a chip select on both masks; `PB5` stays a port pin (the 002's goes to the TTY, Fig. 3.7).
- **Registers** (Fig. 8): `A2` low: `A1 A0` select port A data, port A direction, port B data, port B direction. `A2` high: a write loads the timer, `A1 A0` choosing ÷1, ÷8, ÷64, ÷1024; a read with `A0` low reads the timer, with `A0` high the interrupt flag (on `D7`). `A3` on a timer write or timer read enables (1) or disables (0) the interrupt on `PB7`.
- **Ports**: a direction bit of 1 makes the pin an output. An output pin drives its data bit; a 0 sinks, a 1 is a pull-up. An input pin is held high by its pull-up ("less than one TTL load"). A read gives the data bit for an output and the pin for an input. The KIM's keypad needs nothing more: a key joins a 74145 row (open collector, low while scanned) to a `PA` pin, whose own pull-up holds it high otherwise (Fig. 3.5).
- **Timer** (Fig. 5): `N` written with ÷`P` at pulse 0 reads `N − 1 − ⌊k/P⌋` at pulse `k`; at pulse `N·P` the interrupt flag sets and it reads `$FF`, then counts down once a pulse. `PB7` goes low at least 200 ns (TIC) before that pulse's `φ2` rises, if enabled. A timer write, or a timer read after the flag's pulse, clears the flag; a read in the flag's own pulse does not. The data sheet is off by one in its own example (it reads `$AC` 84 pulses after `$FF`): we follow the rule, `$FF` then one less a pulse.
- **`RES` low** clears the four port registers (every pin an input) and disables the interrupt; the data bus floats.
- **Timing at 1 MHz** (data sheet p. 3): data valid 395 ns after `φ2` rises (TCDR, maximum), held 10 ns after it falls (THR); write data latched as `φ2` falls; port outputs valid 1 µs after `φ2` falls (TCPW, maximum). Maximums only, as for the 2716.

## New

1. **ROM and RAM blocks** (`src/parts/memory_block.{hpp,cpp}`): the words of a ROM (from a contents file, a stuck bit changes them) and of a static RAM (random at power-on, the stuck bits forced on every store). The 2716, 2513 and 2114 use them, with the same random draws, so every seed behaves as before. `MemoryCells` names a chip's memories: one unnamed, or the 6530's `rom` and `ram`.
2. **Memory keyword** in `fault cell` and `fault column`, checked by the Machine (a 6530 needs `rom` or `ram`; the others take none).
3. **A pull-up Pin kind** in the kernel: a port pin that drives 0 strongly and 1 weakly (its own pull-up), and reads its Net. A key, a stronger output or a stuck Fault pulls it low; nothing on it reads 1, not floating.
4. **`6530`**: the Part above, woken only by `φ2` (both edges), every other input read at those edges. `mask=002|003` and `contents=` required.
5. **`6102`**: 1K × 1 static RAM: `CE_N` low and `RW` high drive `DOUT`; `RW` low writes `DIN` (the address and data held just before the write ends are kept, as the 2114); `DOUT` floats while writing or deselected.
6. **`74125`**: four buffers, each with its own active-low enable.
7. **A bench** `tests/data/bw1_bench.bwn` with its program `roms/bw1_bench.asm`: the KIM's processor, decoder, 6530-002 and RAM wired as Figs. 3.2–3.4, with LS gates for the glue, a button on `PA7` and `PB7` on `IRQ_N`.

## Tests

- Blocks: the 2114's random contents are the same words for a seed as before; stuck bits hold through stores and power-on; the ROM block changes its words. The 110 and 080 tests pass unchanged (including the 110's Blackout seed and P5g's memory Faults).
- Kernel: a pull-up Pin alone reads 1, not floating; against a strong 0 it reads 0; it starts released at power-on; a stuck pull-up Pin holds its stuck value.
- 6102: write then read, deselected and writing float `DOUT`, the held write, access delays, stuck cells, random power-on.
- 74125: each buffer's truth table with X, enable and disable delays.
- 6530: ROM reads at `RS0` low with the TCDR delay and THR release; RAM and I/O at each mask's addresses and nowhere else; `CS1` high deselects them; ports (direction, output reads the register, input reads the pin, pull-up alone reads 1, a 0 from outside wins, output delay); `RES`; the timer at Fig. 5's pulses (51 at pulse 1, 25 at 213, 0 at 415, `$FF` at 416), the four rates, `PB7` low 300 ns after the `φ2` fall before pulse 416 (enabled only), cleared by a later read, not by a read in pulse 416, the flag on `D7`; stuck cells in `rom` and `ram`.
- Puzzle and Machine: the memory keyword parsed; a 6530 cell without a name, with an unknown name, a 2114 cell with a name: refused with their messages.
- Bench, seeds 1–3: the program passes its 6102 and 6530 RAM tests (`$55` on `PA0`–`PA6`); timer interrupts every `N·8` = 800 µs plus the handler's latency (810–830 µs), counted on `PB0`–`PB4`; the button on `PA7` shows on `PB5`. A 6102 stuck cell fails the first test (`$01`), a stuck cell in the 6530's `ram` the second (`$02`).
- Release: the bench's headless ratio; the 110, `bus_load` and 080 ratios within noise of before the block change.

Design: [BW1_CHIPS_DESIGN.md](BW1_CHIPS_DESIGN.md). Plan: [BW1_CHIPS_PLAN.md](BW1_CHIPS_PLAN.md).

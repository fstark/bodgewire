# Parts and Netlist — Specification (P1b)

Second milestone of P1 (see [ARCHITECTURE.md](ARCHITECTURE.md), Roadmap). A catalogue of real Parts (74LS chips, oscillator, resistor) with datasheet pinouts and delays, a text netlist format that describes a circuit with them, and a CLI that runs a netlist and writes a VCD. Builds on the kernel ([KERNEL.md](KERNEL.md)). Design: [PARTS_DESIGN.md](PARTS_DESIGN.md).

Terminology: [GLOSSARY.md](GLOSSARY.md).

## Scope

In:

- Part catalogue: name, description, package, factory.
- Behavioural models, one Component per chip (not built from P1a primitives).
- Netlist format, parser and circuit builder, with electronics-aware errors.
- `bw run` CLI: load a netlist, run, record Nets, write VCD.

Out (later):

- Test-pattern generator netlist, sync checks, ≥ 10× benchmark: P1c.
- Copper, Board layout, KiCad export: P2.
- Per-instance delay overrides (Slow Part), Wrong Part: with Timing faults.
- Resistors between two signal Nets: with the BW-80.
- Other families (74HC, 74S…), CPUs, RAM, ROM: later machines.

## Behaviour

### S1 Parts

| Part | Contents | Package |
|---|---|---|
| 74LS00 | quad 2-input NAND | DIP-14 |
| 74LS02 | quad 2-input NOR | DIP-14 |
| 74LS04 | hex inverter | DIP-14 |
| 74LS08 | quad 2-input AND | DIP-14 |
| 74LS20 | dual 4-input NAND | DIP-14 |
| 74LS30 | 8-input NAND | DIP-14 |
| 74LS32 | quad 2-input OR | DIP-14 |
| 74LS86 | quad 2-input XOR | DIP-14 |
| 74LS74 | dual D flip-flop, preset and clear | DIP-14 |
| 74LS161 | synchronous 4-bit binary counter, asynchronous clear | DIP-16 |
| OSC | crystal oscillator can, parameter `freq` | DIP-14 footprint, 4 pins |
| R | resistor, parameter `value` (optional, informational) | axial |

- Pins are numbered as in the datasheet (package pin 1 = Component pin 1) and named as in the datasheet (`1A`, `1Y`, `CLR_N`, `QA`, `RCO`). GND and VCC pins are power pins; unused positions are no-connect.
- All chip pins are TTL family. Outputs are totem-pole.
- Delays are datasheet typical values, one rise/fall pair per path class (e.g. CLK→Q, ENT→RCO).
- 74LS74: asynchronous active-low preset/clear override the clock; both low → Q and Q_N both high. Clock: rising edge, D sampled as before the edge.
- 74LS161: asynchronous active-low clear; on the rising clock edge: LOAD_N low → load A–D; else ENP and ENT high → count; RCO = ENT and count 15. Inputs sampled as before the edge. `X` on a control input at the edge makes the count unknown (`X` outputs) until the next clear or load.
- Power-on: flip-flop and counter state seeded random, as in the kernel.

### S2 Netlist format

Text, UTF-8, one statement per line, `#` starts a comment, tokens separated by whitespace. File extension `.bwn`.

```
# 4-bit counter at 1 MHz
part X1 OSC freq=1MHz
part U1 74LS161
part R1 R value=4k7

net CLK   X1.OUT U1.CLK
net CLR_N U1.1 R1.1
net VCC   R1.2 U1.ENP U1.ENT U1.LOAD_N
```

- `part <ref> <part> [<key>=<value> …]`: places a Component. `ref` is uppercase letters then digits (`U12`, `X1`, `R3`), unique.
- `net <name> <ref>.<pin> …`: puts pins on a Net. `pin` is a package pin number or a datasheet pin name. A Net may span several `net` lines with the same name.
- `GND` and `VCC` are the power rails (supply 0 / supply 1). Power pins connect to them implicitly; listing a power pin on its own rail is allowed, on any other Net is an error.
- A resistor with one pin on a rail is a pull-up / pull-down (weak drive) on the Net at its other pin. Any other resistor wiring is an error for now.
- Frequencies: `Hz`, `kHz`, `MHz`, decimals allowed (`14.31818MHz`). The oscillator period is rounded to the nearest picosecond.

### S3 Errors and warnings

- Every error and warning carries `file:line:` and names the object: `board.bwn:12: error: U3 (74LS00) has no pin 15`.
- All errors in a file are reported, not just the first. Any error → no circuit.
- Errors: syntax, unknown part, unknown or missing parameter, bad parameter value, duplicate reference, unknown reference, unknown pin, pin on two Nets, no-connect pin used, power pin on the wrong Net, unsupported resistor wiring.
- Warnings: input pin left unconnected (it floats and reads 1, as on real TTL).

### S4 CLI

```
bw run <file.bwn> [--time 10ms] [--seed 1] [--record NET,NET… | --record-all] [--vcd out.vcd]
```

- Loads, prints warnings, powers on with the seed, runs, writes the VCD of the recorded Nets.
- Prints simulated time, wall time and kernel stats.
- Exit codes: 0 success, 1 netlist errors, 2 usage error.

## Done when

- Every Part has tests for pinout, behaviour (truth table / sequence) and delays.
- Parser and builder tests cover every S3 error and warning.
- A sample netlist (`netlists/counter.bwn`: oscillator, two cascaded 74LS161, a 74LS74 divider, gate decode) loads, counts correctly in a test, and gives the same recording as the same circuit built through the C++ API.
- `bw run netlists/counter.bwn --vcd counter.vcd` produces a VCD that looks right in a viewer.

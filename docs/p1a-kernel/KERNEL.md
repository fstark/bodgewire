# Kernel — Specification (P1a)

First milestone of P1 (see [ARCHITECTURE.md](../ARCHITECTURE.md), Roadmap). A headless digital simulation kernel: Logic Values, Drivers, Nets, event-driven scheduling with inertial delays, a few primitive Components built in C++, recording of Nets and VCD export. Design: [KERNEL_DESIGN.md](KERNEL_DESIGN.md).

Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Scope

In:

- Logic Values, Drive Strengths, pin families (TTL, MOS) and Net resolution, including the Floating flag.
- Ties on a Net: supply (GND/VCC) and weak (pull-up / pull-down resistor to a rail).
- Discrete-event kernel: picosecond time, inertial delays, update/evaluate phases.
- Circuit building in C++: Nets, Components, Pin connections.
- Primitive Components: logic gates, tri-state buffer, D flip-flop, clock source.
- Power-on with a seed; deterministic runs.
- Recording of chosen Nets; VCD export (the dev logic analyzer for P1).
- A kernel benchmark.

Out (later milestones):

- Real Parts (74LS…) and datasheet delays, netlist files: P1b.
- Test-pattern generator, sync checks, ≥ 10× real-time benchmark: P1c.
- Resistors between two signal Nets (resistor-isolated buses, BW-80): with the BW-80.
- Copper, Joints, Sockets, Faults, Repairs: P2+.
- Timing checks (setup/hold): later.

## Behaviour

### S1 Logic Values and Drives

- A Net's value is `0`, `1` or `X`. `Z` exists only as a Driver output (nothing driven).
- Drive Strengths, strongest first: supply, strong, weak, none (`Z`).
- Output Pin kinds: totem-pole (drives strong 0/1, or `Z` when tri-stated) and open-collector (strong 0, otherwise `Z`).
- Every Pin has a family, TTL or MOS, which selects the resolution rules below.

### S2 Net resolution

Applied whenever a Driver of the Net changes:

| Situation | Value | Floating |
|---|---|---|
| Strongest Drivers agree | their value | no |
| Strongest Drivers disagree, all strong, all TTL | `0` | no |
| Strongest Drivers disagree, any other case | `X` | no |
| Any strongest Driver drives `X` | `X` | no |
| No Driver above `Z`, a TTL input on the Net | `1` | yes |
| No Driver above `Z`, no TTL input | last driven value | yes |

### S3 Scheduling

- Time is a signed 64-bit count of picoseconds.
- A Component changes its outputs only by scheduling a change at `now + delay`, with `delay ≥ 1 ps`. It never writes a Net or another Component.
- Inertial delay: scheduling compares with the output's *pending* value; a different pending change is cancelled; a new change is scheduled only if the value differs from the current output. Pulses shorter than a delay are swallowed.
- Rise and fall delays may differ.
- Each timestamp: first all due output changes are applied and their Nets resolved (update), then each Component with a changed input Net is evaluated once (evaluate).
- A Component can read a Net's value *before the current timestamp*, so a flip-flop clocked at the same instant its D input changes samples the old D.
- Order of events at the same timestamp never affects the result.

### S4 Power-on and determinism

- `powerOn(seed)` resets time to 0, gives every output and every internal state bit a seeded random `0`/`1` (never `X`), resolves all Nets, then evaluates every Component once.
- Same circuit + same seed → identical event sequence and recordings, on every platform.
- Components needing a random bit (`X` read by a MOS input) draw it from the same seeded generator.

### S5 Primitive Components

- Gates: AND, OR, NAND, NOR, XOR, XNOR (2+ inputs), NOT, BUFFER, with rise/fall delays. `X` inputs propagate per 3-valued logic (a controlling `0` into AND still gives `0`).
- Tri-state buffer: `A`, `EN` → `Y`; `Y` is `Z` when `EN` is `0`.
- D flip-flop: `D`, `CLK` → `Q`, `Q_N`; samples D on the rising edge of CLK; seeded random power-on state.
- Clock source: square wave of a given period on one output.

### S6 Recording and VCD

- Any Net can be recorded, starting right after power-on or later: its value at that moment, then every value change with its timestamp.
- Recordings export to a VCD file (timescale 1 ps) viewable in GTKWave or Surfer.

### S7 Benchmark

- A release-build benchmark executable runs a synthetic circuit: a 16-bit synchronous counter (D flip-flops plus carry gates) clocked at 14.318 MHz, for 1 simulated second.
- It prints simulated/wall-clock ratio, applied events per second and evaluations per second.
- Pass: ≥ 20 M applied events per wall-clock second. Below that, optimise the kernel before P1b.

## Done when

- All S1–S6 behaviours covered by unit tests, passing in debug (with sanitizers) and release.
- A ring oscillator and a 4-bit ripple counter, recorded to VCD, look right in a viewer.
- The S7 benchmark passes.

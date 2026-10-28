# Faults, Repairs, Logic Probe — Specification (P2d)

Fourth milestone of P2 (see [ARCHITECTURE.md](../ARCHITECTURE.md), Faults, Connectivity and visibility, Roadmap). Puts hand-placed Faults on the test-pattern Machine, lets them be found with a logic probe and fixed by a Repair, all headless. Symptoms are seen on the TV ([TV.md](../p2c-tv/TV.md)). Builds on the Board reader of [BOARD_LAYOUT.md](../p2a-board-layout/BOARD_LAYOUT.md).

Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Decisions

- **Nets come from the Copper.** A Machine with a Board runs on electrical Nets: the islands of its Copper, after Faults and Repairs. A netlist without a Board still runs on its design Nets, as today. Faults and Repairs edit the Board, never the Nets.
- **Power stays ideal.** The island holding J1's VCC Pin is the VCC Tie, J1's GND Pin the GND Tie. Chips' supply Pins are always powered. An island cut off from J1 is an ordinary Net (a tied-high input that loses its Trace floats).
- **Two Fault kinds in P2**: Stuck-at Fault (a Component's output Pin always drives 0 or 1) and Broken Trace (a crack at one point of a Trace). Shorts, bad Joints, Intermittent and Timing Faults come later.
- **A Fault belongs to its object.** A Stuck-at Fault moves with its Component: swapping the chip removes it. A Broken Trace stays on the Board until mended.
- **Two Repairs in P2**: chip swap (replace a Component with a new one of the same Part) and Trace mend (solder across a crack). Repairs happen with the power off; Nets are recomputed at the next power-on.
- **Puzzles are files**, hand-written; nothing is generated (that is P4).
- **The logic probe is a period TTL probe**: HIGH and LOW lamps plus a PULSE lamp, not a voltmeter.

## Scope

In: electrical Nets from the Board; the two Fault kinds; the two Repairs; Puzzle files; the logic probe model; `bw run` on a Puzzle with Repairs and probes; three Puzzles on the test pattern.

Out: UI (P2e, P2f), Fault appearance on the Board view (P2f), golden comparison as a game rule and repair validation (P3), Bodge Wires (P3), other Fault kinds, wrong or slow Parts.

## Behaviour

### S1 Electrical Nets

- Each copper island (pads, Traces, vias connected by geometry, as `bw check-board` computes them) is one Net; a Pin joins the Net of its pad.
- Ties per Decisions; a Pin on no copper at all is unconnected (floats, TTL input reads 1).
- Each Net is named after the design Net of its Pins; a design Net split in several islands gives `NAME`, `NAME~2`, `NAME~3`… ordered by their first Pin (by reference, then pin number).
- Without Faults, electrical Nets equal the design Nets (already guaranteed by `check-board`), so the Machine behaves exactly as with the netlist alone.

### S2 Faults

- **Stuck-at**: `U5.QA` stuck at 0 or 1. The Pin must be an output; it drives that value at full strength from power-on, whatever the chip computes. Its other outputs are unaffected.
- **Broken Trace**: a crack at a point (Layer, x, y in mm, KiCad coordinates) on a Trace. The Trace is split there with a gap; Nets follow from the new islands. The point must lie on a Trace of that Layer (within its width); a crack on a VCC or GND Trace is allowed and simply disconnects what lies beyond it.

### S3 Repairs

- **Chip swap** `U5`: the Component is replaced by a new one of the same Part; Faults on the old one are gone. Fresh chips are unlimited in P2.
- **Trace mend** at a point (Layer, x, y): removes a crack within 1 mm of the point on that Layer; elsewhere it changes nothing.
- A repaired Machine with the same seed behaves exactly like the fault-free one.

### S4 Puzzle file

```
# puzzles/example.bwp
machine test_pattern            # netlists/test_pattern.bwn + boards/test_pattern/test_pattern.kicad_pcb
fault stuck U1.QC 0
fault crack F.Cu 52.30 41.00
```

- One directive per line, `#` comments, errors name file and line like `.bwn` errors (unknown Machine, Pin, non-output, point off any Trace).

### S5 Logic probe

- Touches one Net (by Pin, e.g. `U5.QA`; touching a Trace point comes with the Board view).
- HIGH lit when the Net is 1, LOW when 0. Neither when the Net is floating (a floating TTL input reads 1, but the probe sees ≈ 1.4 V) or X.
- PULSE lit for 100 ms after any change of the Net (pulse stretcher), so a 10 ns glitch is visible as on a real probe.
- Reads the same recorded changes as the TV; costs nothing when not touching a Net.

### S6 CLI

```
bw run puzzles/example.bwp --time 500ms --tv out.bmp --probe U1.QC,U12.1A --repair swap:U1,mend:F.Cu:52.30:41.00
```

- A `.bwp` file runs the Puzzle on its Board; `--repair` applies Repairs before power-on.
- `--probe` prints each probe's lamps at the end: `probe U1.QC LOW` / `HIGH PULSE` / `-` (none lit).

### S7 Puzzles and checks

- Three Puzzles in `puzzles/`, from the roadmap: dead counter output (Stuck-at on a 74LS161 output), stuck gate output (Stuck-at on a gate), broken Trace (a crack on a signal Trace).
- For each, several seeds: the locked TV picture differs from the fault-free one (a Symptom, or the TV does not lock); probing the faulty Pin or the far side of the crack shows the expected lamps (stuck: steady HIGH or LOW, no PULSE; crack on a driven input: neither lamp); the right Repair gives a picture identical to the fault-free one; a wrong Repair (another chip swapped) does not.
- Fault-free Machine on its Board: picture identical to the netlist-only run.

## Done when

- S1–S7 implemented; tests pass in debug and release.
- `bw run puzzles/<each>.bwp --tv` shows the Symptom; with the right `--repair` the grid is back.

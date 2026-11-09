# P4h — Pin Faults

Roadmap step P4h ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): two Faults of one IC pin, the Copper untouched and nothing drawn on the Board. Until now every Fault but a stuck output could be found by looking: a crack or a bridge is drawn. These two are found only by measuring.

Decided with the user (2026-11-09):

- No Fault is drawn on the Board: a Fault must not be found by looking.
- Leg or pad: a probe or meter touch on a chip pad with the component side (F.Cu) active reads the leg, with the solder side (B.Cu) active the pad.
- Stuck-input Puzzle: the stopwatch, `U8` input `D` (`S3`) stuck at 0.
- The new repair tool is **Resolder (R)**; resoldering a good Joint is accepted and logged like any other, so the tool never gives the Fault away.
- The leg/pad rule is told by the toolbar's Probe hint, not in a Machine's User Manual (a User Manual describes its Machine only). A manual of the bench and its tools does not exist yet; UI and Puzzle discoverability get their own pass later.
- Intermittent Joints stay Later.

## New code

1. **Stuck input.** `fault stuck REF.PIN 0|1` also takes a chip input (until now outputs only). The chip reads the stuck value on that pin; the pin's Net and every other pin on it are unchanged, and a Clip, a Hook or a probe on the leg reads the Net. Repaired by swapping the chip.
2. **Dry Joint.** `fault joint REF.PIN` on a chip signal pin: the leg is not soldered to its pad. The pin is left off its pad's Net and gets a Net of its own: an input floats (a TTL input reads high), an output drives only its own leg. The pad and its Trace still carry the signal of the other pins. Repaired by resoldering: `Machine::resolder`, a Session Repair, `--repair resolder:REF.PIN` in `bw run` and `bw_ui`, and the Resolder tool (click a chip pin with the power off). A swap does not fix it: the game's swap replaces the chip, not its Joints.
3. **Leg or pad.** On a chip pad, the probe and the meter's Volts lead read the leg with F.Cu active and the pad (the Copper island, as today) with B.Cu active. Only a dry Joint makes the two differ. Continuity from a dry leg is open to everything, including its own pad. The probe panel and the meter's lead labels say "leg" or "pad"; the Probe hint says "Probe: click a pin (F.Cu: leg, B.Cu: pad) or trace". Clips and Hooks clip onto the leg, as today.

Stuck inputs and dry Joints are both chip-pin Faults; the player tells them apart with the leg: a stuck input's leg reads the right signal and the chip still acts wrong (swap), a dry Joint's leg differs from its pad (resolder).

## Puzzles

- **Solid net** (060 bouncing ball), as first designed, in place of Wide net, the crack that stood in for it: a dry Joint at `U15` pin 13 (`V2`). The net down the middle is a solid line instead of dashes; `V2` pulses on the pad and on the leg's neighbours, the leg floats: resolder. `puzzles/wide_net.bwp` is removed.
- **Missing eight** (040 stopwatch): `U8` input `D` (`S3`) stuck at 0. The seconds digit shows 0 for 8 and 1 for 9; the probe sees `S3` high on `U8` pin 6, leg and pad, so the decoder ignores a good input: swap `U8`.

[ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md) gains both Faults and the Resolder repair.

## Tests

- Puzzle syntax: `fault joint REF.PIN` parsed; errors for a bad line.
- Loading: a stuck input accepted; a stuck power pin or connector input (`J1.1`, `J2.VIDEO`) refused; a dry Joint on an unknown reference, an unknown pin, a non-chip or a power pin refused.
- Builder: a stuck input reads its value while its Net keeps its Driver and its other Loads.
- Electrical netlist: a dry pin is left off its Net and reads high.
- Machine: `resolder` removes a dry Joint, accepts a good one, refuses an unknown or non-chip pin; a swap keeps dry Joints and removes stuck inputs.
- Repairs: `resolder:U15.13` parsed and logged as `resolder U15.13`; through the Session, refused while powered.
- Leg or pad: on a dry Joint, the F.Cu touch reads the leg's own Net, the B.Cu touch the pad's; continuity from the dry leg is open, from the pad closed; elsewhere both sides read the same Net.
- Board scene: lead labels and the probe panel's side word.
- Solid net and Missing eight show their Symptom and are fixed by their Repair and not by the other one.
- Release build: screenshots of both Puzzles viewed; 1.00× unchanged.

Design: [PIN_FAULTS_DESIGN.md](PIN_FAULTS_DESIGN.md). Plan: [PIN_FAULTS_PLAN.md](PIN_FAULTS_PLAN.md).

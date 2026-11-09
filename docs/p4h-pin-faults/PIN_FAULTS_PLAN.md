# P4h Pin Faults — Implementation Plan

Design: [PIN_FAULTS_DESIGN.md](PIN_FAULTS_DESIGN.md)
Spec: [PIN_FAULTS.md](PIN_FAULTS.md)

Status: implemented 2026-11-09, user check pending.

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Stuck inputs: loader and builder | `9a08b3b` |
| 2 | Dry Joints: syntax, loader, electrical netlist, leg/pad, `resolder` | `75a436a` |
| 3 | Resolder Repair: parse, Session, `bw run`, `bw_ui --repair` | `f0c61d4` |
| 4 | UI: Resolder tool, leg/pad words, Probe hint | `ae5a8b1` |
| 5 | Puzzles: Solid net, Missing eight; Machine docs | `98b070a` |
| 6 | Release check, recipes, glossary, roadmap | this commit |

## Implementation notes

- A stuck input gets its own Net, named `REF.PIN stuck` (not in `LoadedCircuit::nets`), tied Strong; its Connection stays on the listed Net, so `PinNet`, Clips, Hooks and the leg read the Net.
- The leg rule only acts on dry pins (`Machine::dryLegAt`): elsewhere leg and pad are one Net, and power pins keep reading the pad (P3f Unpowered chip).
- `bw run --probe` and `bw_ui --probe/--meter` touch the leg: `--probe U15.13` on Solid net shows nothing, `U3.QC` pulses.
- Missing eight is tested from power-on counts (seed scan for 8 or 9) instead of 8 simulated seconds.
- Release: Solid net and Missing eight both 1.00× at 1 s, ui 0.6–0.7 ms; screenshots viewed (solid net, "· leg" in the Probe panel, meter "U8.6 D leg").
- No bench/tool manual exists; the leg/pad rule is in the Probe hint only. The user plans a UI and Puzzle discoverability pass.

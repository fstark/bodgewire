# P4i Paddle Controller — Implementation Plan

Design: [PADDLE_DESIGN.md](PADDLE_DESIGN.md)
Spec: [PADDLE.md](PADDLE.md); Machine: [KNOB_METER.md](../machines/045-knob-meter/KNOB_METER.md), [KNOB_METER_DESIGN.md](../machines/045-knob-meter/KNOB_METER_DESIGN.md)

Status: implemented 2026-11-09, user check pending.

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | 74LS175 Part, Datasheet, tests | `2bb1a10` |
| 2 | PADDLE Part, powered through its Connector | `8bfbcc6` |
| 3 | Knob: parse, Session, `bw run`, `bw_ui` side panel and keys | `da76381` |
| 4 | Knob meter netlist, manuals, headless fault-free tests | `d24f490` |
| 5 | Knob meter Board (user layout), per-Machine loops | `73063c0` |
| 6 | Knob meter Puzzles + Symptom/Repair tests | `e1468a0` |
| 7 | Session test, release check, recipes, docs | this commit |

## Implementation notes

- Docs review (before phase 1): the circuit and every predicted reading held. Fixed: the Datasheet check builds Parts without parameters (`PartPinNames` gives `PADDLE` its own); `--knob` checked in `bw_ui`'s `main()` with the other options; debug runs at 0.12× here, the long cases are `[slow]` and step the knob within one power-on; comments and docs saying only chips are Powered; the count formula's 0.2 µs (half way sits on a boundary); Dead knob compares segment masks; instrument Clips on pads.
- Phase 4: the netlist worked as first written, no warnings; every reading exact (00, 20, 50, 79, 99; 30 → 59 after a turn). Release headless: 14× real time. `knob: reads the knob` (three seeds, one case) takes 19 s in debug.
- Phase 5: the user's layout keeps both Puzzle constraints. `VCC`: `J2` pin 3 is a leaf, a 23 mm straight run on B.Cu to `U6` pin 14 (crack `J2.3 U6.14`). `C3`: `U10` pin 13 → `U8` pin 11 → `U7` pin 14 on F.Cu, with a long straight stretch on the last leg (crack `U8.11 U7.14`). `J2` ended up on the right edge, which no doc depends on.
- Phase 6: every Puzzle as predicted, first time, on seeds 1–3 where tested: Dead knob holds the same segments at 20 % and 80 %; Runaway adds 30 each cycle; 47 % → 07; 0 % → 10, 25 % → 35; 0 % → 04, 9 % → `0` and the shape of 13. On a damaged Board the Nets are named from the Copper, so the bridge test compares `U12` `C` and `LT_N` pin Nets rather than `L2` and `VCC`. `knob:` cases take 55 s in debug.
- Phase 7: `session: knob meter with every instrument` lights 4 and 2 at 42 % (8 Clips, 2 Hooks, probe and meter). Release `bw_ui`: netlist with `--knob J2:42%` and every instrument reads 42, 1.00×, UI 1.1 ms; Odd tens at 42 % reads 52, 1.00×, UI 0.6 ms; screenshots viewed. New recipe ADD_CONNECTOR_DEVICE.md; ADD_PUZZLE gains the pin-Net comparison on a damaged Board.

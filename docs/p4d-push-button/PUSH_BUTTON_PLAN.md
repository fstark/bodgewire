# P4d Push Button — Implementation Plan

Design: [PUSH_BUTTON_DESIGN.md](PUSH_BUTTON_DESIGN.md)
Spec: [PUSH_BUTTON.md](PUSH_BUTTON.md); Machine: [DICE.md](../machines/030-dice/DICE.md), [DICE_DESIGN.md](../machines/030-dice/DICE_DESIGN.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1–8 | Button Part, builder, presses, dice netlist/Board/manuals/Puzzles, Session, Board view | done 28ecaea..(phase 7 test) |
| 9 | User check, ARCHITECTURE P4d row | waiting for the user |

## Implemented

2026-11-02, 28ecaea onwards: 498 tests pass in debug and release. Release `bw_ui` on the dice and each Puzzle with 8 Clips at 100 MHz, 2 Hooks, probe and meter while rolling: 1.00×, ui 0.5–0.9 ms. Screenshots: all seven LEDs glowing at different brightness while `SW1` is held (cap drawn pushed in), one face (2) after release.

## Implementation notes

- The button footprint's `F.CrtYd` lines stop at pad 2's centre; a courtyard read from lines also covers the pads.
- User layout: 167 segments, 2 vias. `N2` runs `U3` pin 2 → `U2` pin 3 → `U4` pin 6 → `U1` pin 12 in one chain, so no crack isolates `U2` alone: the planned Corners Puzzle became Loaded dice (crack between `U2` and `U4`; `U2` and `U3` read `N2` high, `U1` alternates 1 and 2, shown as a 3 and a 6). DICE.md and DICE_DESIGN.md updated.
- Crack points (`test_points.hpp`): `kBtnNCrack` F.Cu 100.03 87.00, `kN2Crack` B.Cu 58.86 71.17, `kSw1GndCracks` F.Cu 109.23 89.77.
- All Puzzle tests passed as first written; debug dice tests take under 1 s each (the counter cycles every 6 µs).
- `bw_ui --press` applies only with a Board (the frame loop drives presses through the Board view's Button list).
- User check 1: the dice only rolled even. UI presses were applied at 1 ms slice boundaries, so every hold lasted whole milliseconds (1000 clocks ≡ 4 mod 6) and the face kept its parity. Fix 836dd59: `Session::setPressed` wakes the Button at a wall-clock-derived offset within 1 ms (`Simulator::wake`/`Runner::wake` take a delay); headless `--press` stays exact.

## Phase 9 — User check, docs

1. **User check**: hold to roll, right-click latch, the four Puzzles.
2. ARCHITECTURE P4d row → "done" with links; this plan pruned to Implemented + notes.

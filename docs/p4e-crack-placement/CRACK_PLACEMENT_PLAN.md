# P4e Crack Placement — Implementation Plan

Design: [CRACK_PLACEMENT_DESIGN.md](CRACK_PLACEMENT_DESIGN.md)
Spec: [CRACK_PLACEMENT.md](CRACK_PLACEMENT.md)

## Implemented

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Copper graph: `BuildCopperGraph`, `PadItemAt`, `Touches`; `FindIslands` on it | c9e4d86 |
| 2 | `Crack` (placed) split from `CrackDecl`; `CrackBoard`, Machine, UI on `Crack` | 2e651c3 |
| 3 | `PlaceCrack`, `Machine::placeCrack`, `Machine::cracks()`, crack tests | c9e0381 |
| 4 | Conversion table for the seven Puzzles and two test-only cracks | a257c50 |
| 5 | New `fault crack` syntax, Puzzles rewritten, tests off hard-coded points | f6111c0 |
| 6 | Docs, release check | this commit |

## Implementation notes

### Conversion table (phase 4)

All on the old crack's Trace and Layer, whole percents.

| Crack | New line | Old point | New point | Distance |
|---|---|---|---|---|
| `broken_trace` | `fault crack U1.QA U13.2B 79%` | F.Cu 137.59 99.17 | 137.676 99.169 | 0.086 mm |
| `unpowered_chip` | `fault crack U6.VCC U10.VCC 24%` | F.Cu 164.85 51.05 | 164.880 51.047 | 0.030 mm |
| `dark_led` | `fault crack U5.QD D1.1 51%` | F.Cu 192.52 122 | 192.513 122.000 | 0.007 mm |
| `gap` | `fault crack R6.2 D5.2 51%` | F.Cu 73.46 101.05 | 73.460 101.041 | 0.009 mm |
| `dark_chaser` | `fault crack U6.8 U5.1 50%` | B.Cu 70.17 73.69 | 70.280 73.690 | 0.110 mm |
| `dead_button` | `fault crack SW1.1 R9.2 48%` | F.Cu 100.03 87.00 | 100.010 87.000 | 0.020 mm |
| `loaded_dice` | `fault crack U2.3 U4.6 52%` | B.Cu 58.86 71.17 | 58.858 71.206 | 0.036 mm |
| `kR1VccCracks` (counter) | `R1.1 U1.16 58%` | B.Cu 128.31 106.30 | 128.310 106.304 | 0.004 mm |
| `kSw1GndCracks` (dice) | `SW1.2 J1.2 7%` | F.Cu 109.23 89.77 | 109.120 89.880 | 0.156 mm |

`Machine::padOf` only knows chips, so `placeCrack` resolves pads with `padPosition` (chip pins by name or number, passives by pad number) and compares positions for "two different pads". Fixed in phase 5.

Phase 3: `placeCrack(U1.QA, U13.2B, 50)` lands on B.Cu, so the machine_test checks separation, not the Layer.

Phase 5:
- `LoadMachine` checks crack and bridge pins with one `known` lambda: reference from `loaded.parts` (so resistors count), pin by `padPosition`. A bridge pin with no chip pin (a resistor) now says `pins of one chip only`.
- A 5th token without `%` gives the `expected:` error, so the old `F.Cu X Y` form always does; `bad position` is for a malformed `N%`.
- Test helpers are cached functions (`H0Crack()`, `R1VccCrack()`, …) built on `PuzzleCrack` / `BoardCrack`, instead of a local per test.
- `view: highlight crosses the broken_trace crack`: the new point sits over a B.Cu Trace, and `pick` falls through to the other Layer; the test now checks no F.Cu Trace is hit.

Phase 6: release tests pass; `bw_ui puzzles/broken_trace.bwp --screenshot` shows the same Symptom (vertical grid lines gone). Older phase docs (P2d, P3f) keep the old syntax as history.

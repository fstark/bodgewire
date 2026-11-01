# P4b Shift Register — Implementation Plan

Design: [SHIFT_REGISTER_DESIGN.md](SHIFT_REGISTER_DESIGN.md)
Spec: [SHIFT_REGISTER.md](SHIFT_REGISTER.md); Machine: [LED_CHASER.md](../machines/020-led-chaser/LED_CHASER.md), [LED_CHASER_DESIGN.md](../machines/020-led-chaser/LED_CHASER_DESIGN.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | 74LS164 Part: `Ls164`, delays, catalogue, Datasheet, unit tests | done 7fb9693 |
| 2 | Chaser netlist, manuals, headless fault-free test | done a9b1f20 |
| 3 | Chaser Board (user layout), per-Machine test loops | done be36d29 |
| 4 | Chaser Puzzles + headless Symptom/Repair tests | done cf595a6 |
| 5 | `kMaxWatched` 32, Session test with every instrument, screenshot | done 9ba532d |
| 6 | Release check, user check, docs | done |

## Implemented

P4b done 2026-11-01: the 74LS164 Part, the LED chaser Machine (netlist, Board, manuals, four Puzzles) and a watch budget of 32 Nets. 466 tests pass in debug and release. The user solved all four Puzzles.

## Implementation notes

- Crack points on the user's layout: `LED_A5` at F.Cu 73.46 101.05 (10.6 mm segment), `SER` at B.Cu 70.17 73.69 (42 mm segment); `kLedA5Crack` and `kSerCrack` in [test_points.hpp](../../tests/machine/test_points.hpp).
- Twins: TTL contention resolves to 0, so the bridged `S1`/`S2` Net is low while either stage is low; all four Puzzle tests passed as first written.
- `session: chaser with every instrument` aborts with "too many watched nets" at 16 slots (18 distinct Nets) and passes at 32.
- Debug test times: fault-free chaser and gap ≈ 8 s, the other three Puzzles ≈ 15 s each (two recordings), session test ≈ 6 s.
- Release, 8 Clips at 100 MHz + 2 Hooks + probe + meter: 1.00×, ui 0.7 ms.

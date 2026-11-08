# P4f Seven-Segment Digits — Implementation Plan

Design: [SEVEN_SEGMENT_DESIGN.md](SEVEN_SEGMENT_DESIGN.md)
Spec: [SEVEN_SEGMENT.md](SEVEN_SEGMENT.md); Machine: [STOPWATCH.md](../machines/040-stopwatch/STOPWATCH.md), [STOPWATCH_DESIGN.md](../machines/040-stopwatch/STOPWATCH_DESIGN.md)

## Implemented

User check pending: play the six stopwatch Puzzles in `bw_ui`.

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | `EvalContext::fell`; 74LS90 Part, Datasheet, tests | 5b5d52e |
| 2 | 74LS47 Part, Datasheet, tests | f8a69d7 |
| 3 | Lamps in the catalogue, DIGIT Part; `LedGlow` anodes, `LedConsumer`, Session lamps; TTL-only warning; `kMaxWatched` 48 | 1419047 |
| 4 | Digit geometry (`digit_shape`) and drawing in the Board view | a2deab7 |
| 5 | Stopwatch netlist, manuals, headless fault-free test | 8a18984 |
| 6 | Stopwatch Board (user layout), per-Machine loops | 8d3644a |
| 7 | Stopwatch Puzzles + Symptom/Repair tests | ae506f8 |
| 8 | Session test, release check, recipes, docs | this commit |

## Implementation notes

- Phase 3: `probe consumer: touching many Nets in turn` went round the test pattern's Nets once, which is fewer than 48: it now loops until it passes `kMaxWatched`.
- Phase 5: every test passed as first written; the stopwatch tests take ~14 s in debug (the `[slow]` seconds case most of it).
- Phase 6: the first screenshot read 0.7x with every digit upright and `DS1`'s point lit; no geometry change.
- Phase 7: the routed `T0` runs `U9` pin 7 → `U6` pin 1 → `U6` pin 12, so no crack cuts `U6` pin 1 off alone. Instead of a reroute, Nineteen cracks `U6.1 U6.12`: `U6` `CKB` and `U9` `A` float together, the watch reads 0.10 after Reset and runs 0.10–0.19 round again (STOPWATCH.md updated). The four cracks are on long straight stretches; every Puzzle test passed as first written.
- Phase 8: `session: stopwatch with every instrument` passes (24 lamps, 8 Clips, 2 Hooks, probe, meter). Release `bw_ui` on each of the six Puzzles with every instrument attached and Stop, Reset, Start pressed: 1.00×, UI 0.6–1.0 ms. Debug 548 tests and release 540 pass.

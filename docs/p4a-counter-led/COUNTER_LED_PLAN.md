# P4a Counter LED — Implementation Plan

Design: [COUNTER_LED_DESIGN.md](COUNTER_LED_DESIGN.md)
Spec: [COUNTER_LED.md](COUNTER_LED.md); Machine: [COUNTER.md](../machines/010-counter/COUNTER.md), [COUNTER_DESIGN.md](../machines/010-counter/COUNTER_DESIGN.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | LED Part: `PartKind::Led`, `Led` Component, catalogue, KiCad footprint, Datasheet | done ab92f30 |
| 2 | `LedGlow` (bw_devices) with unit tests | done 22f5a90 |
| 3 | `LedConsumer` (bw_runner) with Runner tests | done b8eb2c2 |
| 4 | Counter rework: netlist, KiCad board (**user layout**), manuals, existing tests | done 440a19d |
| 5 | Counter Puzzles + headless Symptom/Repair tests | done e660a69 |
| 6 | Session `leds()`, Board view drawing, screenshots | done 8049a68 |
| 7 | Release check, user check, docs | done |

Implemented 2026-11-01 (ab92f30..8049a68, 451 tests). Release: all tests pass; counter and each new Puzzle in `bw_ui` 1.00×, ui 0.2 ms. Screenshots: `D1` lit on the fault-free counter, dark on `dark_led`. User check: "all works!".

## Implementation notes

- New board: user layout in KiCad 10 (140 segments, no vias); `bw check-board` clean, `drc_counter` passes.
- `kR1VccCracks` is now one crack on the long B.Cu segment (128.31 106.30) of R1.1's only VCC Trace: the 1.2 mm stub at the pad can't be cracked (its far piece still overlaps the 1.6 mm pad).
- `dark_led` crack: F.Cu 192.52 122, midpoint of the straight `C19` segment to `D1` pad 1 (`kC19Crack` in test_points.hpp).
- `counter: LED blinks once a second` (`[slow]`) takes ≈ 11 s in debug.
- `Runner::powerOff()` is queued: tests call `waitIdle()` before checking its effects.

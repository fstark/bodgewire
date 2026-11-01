# Solder Bridges and Real Power — Implementation Plan (P3f)

Design: [SHORTS_POWER_DESIGN.md](SHORTS_POWER_DESIGN.md)
Spec: [SHORTS_POWER.md](SHORTS_POWER.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Solder Bridges in Puzzle and Machine: parse, validate, join islands, remove | done ec317a2 |
| 2 | `unbridge:` Repair through Session, `bw run` and `bw_ui --repair` | done 9fd621e |
| 3 | Real power: `PowerPinVolts`, `FindPower`, unpowered chips left out of the Circuit | done 737a706 |
| 4 | Dead Rail LED state + four Puzzles with headless Symptom/Repair tests | done 43c4f0b |
| 5 | UI: bridge drawing, Cut on a bridge, `LIMIT` LED, screenshots | done 153df9b |
| 6 | Release check, user check, docs | user check open |

Implemented 2026-11-01 (ec317a2..153df9b, 429 tests). Release: all tests pass; rebuild mean 0.8 ms; each new Puzzle in `bw_ui` 1.00×, ui ≤ 0.7 ms. Screenshots: `dead_rail` LIMIT lit, white TV; with `--repair unbridge:U1.7:U1.8` LIMIT dark, picture back.

## Remaining: user check

The user plays `sync_bridge`, `line_bridge`, `unpowered_chip` and `dead_rail` to the end, and looks at a bridge zoomed in on B.Cu (spec S6). Then: ARCHITECTURE P3f row `done <date>`, prune this section.

## Implementation notes

- Actual Symptoms (Briefs follow them): `sync_bridge` and `unpowered_chip` both no lock, grid torn into slanted dashes; `line_bridge` vertical lines only (no line count, no horizontal lines, no roll); `dead_rail` blank **white** screen, not black: with every chip Unpowered, J2's VIDEO and SYNC_N float and the TV reads a floating input as high. Left as is; revisit if the user finds it misleading (a monitor input termination would read it as black).
- The meter on a cut-off VCC stub reads 0.00 V (`Session::volts()` gives 0 V on bare Copper), not `- - -` as the design guessed.
- An Unpowered oscillator still runs on its private Net (≈ 2 evaluations per µs, invisible); harmless.
- `NearestWire()` in board_view.cpp serves both `wireAt()` and `bridgeAt()`.

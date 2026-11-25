# P8g — The Multimeter held — Implementation Plan

Spec: [MULTIMETER.md](MULTIMETER.md). Design: [MULTIMETER_DESIGN.md](MULTIMETER_DESIGN.md).

## Status

| Phase | Description | Commit |
|---|---|---|
| 1 | The held meter: session, view, UI | `ec36255` |
| 2 | Docs, README, release check | (this commit) |

## Implementation notes

- Review (2026-11-25, a fresh agent on the three docs): must-fixes applied to the docs before phase 1. `bodge wire` re-clips after its power cycle; paused shows dashes in `main.cpp`, a rail and a reading from before the pause included; a meter press is one that starts on copper (else the drag pans), `MeterPress::start` not optional, `meterAt_` added; the chip panel's Function section merges `held`; phase 1's fence runs the full release suite and the web build; the debug filter leaves out the BW-2001, BW-80 and tennis cases. Also taken: anchored ctest filters, the stopwatch's pairs for the screenshots, `powerChanged` and `unclip` refresh continuity, `hold` called only on a new Hit, `PinHits`' hold as strict as a link's, held links before `route()`'s early return, focus tests off `out(Owner::Meter)`, `LeadsAcrossH0`, `--pop`'s help, `--meter` then `--focus`, the LCD's anchor on the Shelf's top edge, the docs list.
- Phase 1 (2026-11-25): as designed. The Shelf's hints shortened to fit about 22 characters at 1280 px ("Meter: hold on copper", "Meter: clip black lead", "Meter: hold red lead"); "a click on it lifts it" moved to the clip's line in the meter's tooltip (spec and design updated). `main.cpp` calls `hold` only when the pick's Hit or Layer changes, and lifts the red Lead at once when the press ends. Checks: the full release suite (1696 passed); debug `-R "^(meter reading|session|bodge wire|shorts power|focus): " -E "bw2001|bw80|tennis"` (74 passed); the web build; screenshots viewed on 040-stopwatch: Volts held (`0.20 V`), paused (dashes), Continuity U1.12–U1.1 (`0.0`, green, •))) ) and U1.12–U2.12 (`OL`), the meter in hand with no LCD and its hint, a finger's hold (the loupe on U1's QA, `0.20 V`), and the Service Manual's `U12.1A` link held by a finger (`1.85 V`).
- Phase 2 (2026-11-25): GLOSSARY (Instrument, bench objects, Multimeter, Test Lead, View buttons), ADD_INSTRUMENT.md (the held shape, `--meter`), ARCHITECTURE (held tips, this row), README_SPEC and README (the continuity story: black clipped on the LED, red held on U5 pin 11), `tools/readme_shots.py` (`--meter D1.1,U5.11`), `docs/readme/continuity.png` regenerated and viewed (`OL`). GAME_DESIGN.md names the Multimeter without a window or a mode: unchanged. Speed: the 110 with the meter held 1.00×, UI 0.2 ms; the 140 0.24× with or without it (its known speed). The full release suite not rerun: no code changed since phase 1's (1696 passed).

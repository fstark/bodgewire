# P8g — The Multimeter held — Implementation Plan

Spec: [MULTIMETER.md](MULTIMETER.md). Design: [MULTIMETER_DESIGN.md](MULTIMETER_DESIGN.md).

## Status

| Phase | Description | Commit |
|---|---|---|
| 1 | The held meter: session, view, UI | (this commit) |
| 2 | Docs, README, release check | |

## Phase 1 — The held meter

1. `MeterSetup` rewritten, `Session` power calls (design §1); the session and machine tests moved to `hold` / `clip`.
2. `src/view/meter_reading` and its tests (design §2).
3. The meter out of `BenchObject` and the Equipment; `Measures`; bench options (design §3) and their tests.
4. `BoardView::meterPress`, held links, `OnClip` and the clip in `ToolClick` (design §4).
5. `DrawMeterLcd`, the Shelf, the tags, `bench_setup`, `main.cpp`; `meter_window` deleted (design §5).
6. Screenshots viewed (design, Checks); the LCD's look tuned on crops.

Fence: the full release suite (shared code); debug `ctest --preset debug -j -R "^(meter reading|session|bodge wire|shorts power|focus): " -E "bw2001|bw80|tennis"`; the web build (`bw_ui` draws differently); the screenshots viewed. Commit: `ui: the Multimeter held, its LCD over the Shelf while it measures (P8g phase 1)`.

## Phase 2 — Docs, README, release check

1. GLOSSARY, ADD_INSTRUMENT.md, GAME_DESIGN.md (design §6).
2. README_SPEC, `tools/readme_shots.py`, README.md, `docs/readme/continuity.png` regenerated and viewed.
3. The 110 with the meter held: speed and UI ms. The web build.
4. The full release suite. ARCHITECTURE row done; this plan cut to the table and the notes.

Fence: the full release suite; the web build; screenshots viewed. Commit: `docs: P8g done, the Multimeter held (P8g phase 2)`.

## Implementation notes

- Review (2026-11-25, a fresh agent on the three docs): must-fixes applied to the docs before phase 1. `bodge wire` re-clips after its power cycle; paused shows dashes in `main.cpp`, a rail and a reading from before the pause included; a meter press is one that starts on copper (else the drag pans), `MeterPress::start` not optional, `meterAt_` added; the chip panel's Function section merges `held`; phase 1's fence runs the full release suite and the web build; the debug filter leaves out the BW-2001, BW-80 and tennis cases. Also taken: anchored ctest filters, the stopwatch's pairs for the screenshots, `powerChanged` and `unclip` refresh continuity, `hold` called only on a new Hit, `PinHits`' hold as strict as a link's, held links before `route()`'s early return, focus tests off `out(Owner::Meter)`, `LeadsAcrossH0`, `--pop`'s help, `--meter` then `--focus`, the LCD's anchor on the Shelf's top edge, the docs list.
- Phase 1 (2026-11-25): as designed. The Shelf's hints shortened to fit about 22 characters at 1280 px ("Meter: hold on copper", "Meter: clip black lead", "Meter: hold red lead"); "a click on it lifts it" moved to the clip's line in the meter's tooltip (spec and design updated). `main.cpp` calls `hold` only when the pick's Hit or Layer changes, and lifts the red Lead at once when the press ends. Checks: the full release suite (1696 passed); debug `-R "^(meter reading|session|bodge wire|shorts power|focus): " -E "bw2001|bw80|tennis"` (74 passed); the web build; screenshots viewed on 040-stopwatch: Volts held (`0.20 V`), paused (dashes), Continuity U1.12–U1.1 (`0.0`, green, •))) ) and U1.12–U2.12 (`OL`), the meter in hand with no LCD and its hint, a finger's hold (the loupe on U1's QA, `0.20 V`), and the Service Manual's `U12.1A` link held by a finger (`1.85 V`).

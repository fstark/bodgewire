# P4k Board Placement — Implementation Plan

Done 2026-11-11, user check pending.

Design: [BOARD_PLACEMENT_DESIGN.md](BOARD_PLACEMENT_DESIGN.md)
Spec: [BOARD_PLACEMENT.md](BOARD_PLACEMENT.md)

Each phase ends with its fence: the checks listed, `cmake --build --preset debug` and `ctest --preset debug -j 12` green, `clang-format` on the C++ files touched, then one commit with the message given (plus the attribution line). Only this step's paths are committed. The status table and the notes are updated in the phase's commit.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Vendored footprints, `LoadFootprints` | `61cd611` |
| 2 | Rules file parser | `f232392` |
| 3 | Placer: panel rules, grid, annealing | `df925e4` |
| 4 | Writer, `bw place`, 070 rules, DRC ctest | `eb98630` |
| 5 | 070 Board regenerated (user routing), Puzzles checked | `8a8e489` |
| 6 | Release check, recipe, docs | this commit |

## Implementation notes

- Docs review (before phase 1): fixed the 070 pad-net check (a writer test on 070), the uuids in the button and digit footprints (dropped by the writer), an empty grid and over-long groups defined, `Side` renamed `Edge` (GLOSSARY: a Side is a Layer), origins snapped to 1.27 mm, `at` defined on the courtyard-box centre, the DRC script without `--exit-code-violations` (it exits 5 on unrouted pads).
- Phase 1: the nine footprints from KiCad 10.0.7; `ReadFootprint` is public so the error cases need no files.
- Phase 3: 070 with the front-edge rules, seed 1, one try: 12 × 5 grid, 11.32 m (prototype 10.5–11.4 m with the controllers elsewhere); `placer: tennis` takes 5 s in debug. Two gaps found by the tests and closed in the design: a front row wider than a narrow grid widens the Board (grid centred) instead of being refused, and a snapped origin could leave the margin by up to 0.6 mm (now stepped back).
- Phase 4: `bw place` on 070 (seed 1, 4 tries): 11.07 m, 0.4 s release, 19 s debug; debug and release write the same bytes. KiCad 10.0.7's DRC on it with the written `.kicad_pro`: 0 violations, 0 footprint errors, 499 unconnected pads. Viewed as an SVG: `J1` left, `J2` back right, `J3`/`J4` at the front corners, `SW1` and `R1` front middle, one empty slot. The DRC ctest runs one `place_drc_<m>` per `boards/*/*.place`.
- Phase 5: the user routed the generated Board (2,361 segments, 102 vias), keeping `SERVING` as `U41` → `U43` → `U38`; `bw check-board` `ok`. Both crack Puzzles hold without a change: `LPAD_CK` has two pads, and Invisible ball's test checks `SERVING` driven at `U43` and floating at `U38`. The 23 070 cases pass, the nine Puzzles included. TENNIS_DESIGN.md § Board now points at `tennis.place`.
- Phase 6: release `bw_ui puzzles/invisible_ball.bwp` with probe, clip, hook and meter on `SERVING`: 0.96×, UI 1.8–2.1 ms, on the old hand-placed Board and the new one alike (the machine was under load: load average 3–4); `U38.3B` floats at 1.40 V, no ball. ADD_MACHINE.md § 3 now places with `bw place` and leaves only routing to the user; ARCHITECTURE's Layout paragraph says the same.

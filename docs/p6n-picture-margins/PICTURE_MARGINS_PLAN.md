# P6n — Picture margins — Implementation Plan

Spec: [PICTURE_MARGINS.md](PICTURE_MARGINS.md). Design: [PICTURE_MARGINS_DESIGN.md](PICTURE_MARGINS_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Five netlists (no fence of its own: every picture case loads the Board) | with `dae282f` |
| 2 | Boards rebuilt unrouted (`.place` for 050, 060; `renet_board.py` for 070–090), routed whole by the user, every test updated, full release suite | `dae282f` |
| 3 | Puzzle files re-read | `b3265f1` |
| 4 | Manuals and docs: Test Pattern, Bouncing Ball, Tennis | `a71c0f2` |
| 5 | Manuals and docs: Message, Typewriter | `5418035` |
| 6 | Recipes, ARCHITECTURE, release check | this commit |

## Implementation notes

- Design (2026-11-19): every decode prototyped on scratch copies of the netlists and measured with `bw run --tv` (the spec's What is new numbers); Echo and Lost paddles prototyped with the fault wired into the netlist.
- Review (2026-11-19), fixed: the Ball's missing `V3` Net and `U3.QD` pad; every picture case loads the Board, so phase 1 has no fence and phase 2 updates every test and runs the full suite (the Test Pattern is a shared fixture); the Test Pattern's Catch names; `tennis: rolling_picture` probes the new `VSYNC_N`; the Test Pattern's decode-sanity and grid cases; the 050 Torn picture on `VSYNC_N`; `TICK_N` tracks re-netted; part comments; citations.
- Phase 2 (2026-11-19): the user asked for whole Boards to route rather than a netlist import and stubs to delete, and for no hand placement: `at` takes an angle (050's two turned connectors), 050 and 060 got `.place` files from their Boards, `tools/renet_board.py` (checked byte for byte against the Boards handed over) for 070–090. Freerouting left one connection (080 `CH0`), routed by `route_missing.py`. Every crack still resolves and its symptom case passes; only `shorts power`'s typed `mend:` moved (`B.Cu:110.66:35.78`). The window check went to its own `tv_window` (name clash in the Message's and Typewriter's tests). ADD_MACHINE §3's routed-Board bullet and the `at` angle written now, not in phase 6. Full release suite 1460/1460.
- Phases 4–5 (2026-11-19): manuals and Machine docs written by two agents in parallel, each reading checked (`bw run --probe`, `bw_ui --clip`, screenshots viewed); reviewed. The rerouted 050 Board's Zips crack also cuts `U7` pin 11 off: the counters reload every 312 dots (measured), the symptom unchanged.
- Phase 6 (2026-11-19): ADD_MACHINE §2's margins rule; the five fault-free Machines and every Puzzle screenshot at the knobs' marks and viewed; full release suite.

# P6n — Picture margins — Implementation Plan

Spec: [PICTURE_MARGINS.md](PICTURE_MARGINS.md). Design: [PICTURE_MARGINS_DESIGN.md](PICTURE_MARGINS_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Five netlists (no fence of its own: every picture case loads the Board) | with phase 2 |
| 2 | Boards rebuilt unrouted (`.place` for 050, 060; `renet_board.py` for 070–090), routed whole by the user, every test updated, full release suite | see git log |
| 3 | Puzzle files re-read | see git log |
| 4 | Manuals and docs: Test Pattern, Bouncing Ball, Tennis | |
| 5 | Manuals and docs: Message, Typewriter | |
| 6 | Recipes, ARCHITECTURE, release check | |

Catch names: the Test Pattern's cases are "test pattern: …" and "faults: …", its Board's "boards: test pattern …"; ctest patterns below use them.

## Phase 1 — Netlists

1. The five netlists as the design's Netlists section, part comments and headers included.
2. `bw run machines/NNN-name/<m>.bwn --time 1s --tv` for each (the netlist alone): the rows and columns of the spec's What is new table.
3. The Boards made ready for routing (design, Boards, steps 1–2).

Check: step 2's numbers. No fence, no commit: the netlists go in with their Boards. **Stop: the user routes the five Boards** (design, Boards, step 3).

## Phase 2 — Boards and tests

1. `bw check-board` on each prints `ok` (open connections: `tools/route_missing.py`); every crack Puzzle checked again (design, Boards, step 4).
2. `LitBounds` and `InsideWindow` in `tests/machines/tv_window`; every test change of the design's Tests section, `message: echo`, `tennis: short_frames` and `tennis: rolling_picture` included.
3. The Test Pattern is the shared fixture (about 15 test files load it): the full release suite.

Fence: `ctest --preset release -j` (full). Commit:

```
machines: the early TV Machines' syncs before their pictures (P6n phases 1-2)

The Test Pattern, Bouncing Ball, Tennis, Message and Typewriter decoded
their syncs on the counters' last values: the picture started on the line
after the vertical sync, and the text Machines' a dot after the line sync,
so a monitor's window cut them. The syncs move into the blanking, on spare
gates (no chip added, no footprint moved). Ball and Tennis keep line 308
as TICK_N for MOVE and the paddles. Boards rerouted by the user
(Freerouting) on the changed Nets only. A shared window check in the TV
tests.

Tests: ctest --preset release -j, N/N.
```

## Phase 3 — Puzzles

1. Puzzle files: Lost paddles and Rolling picture (070) comments and briefs; Echo's new brief (spec, decisions).
2. Each of the five Machines' Puzzles at 1 s in `bw_ui --screenshot`, viewed: each brief matches.

Fence: `ctest --preset release -j -R "test pattern|faults|ball|tennis|message|typewriter|puzzle"`. Commit: `puzzles: … after the sync move (P6n phase 3)`.

## Phase 4 — Manuals and docs: 050, 060, 070

Design, Manuals and Machine docs. Readings with `bw run --probe`; analyzer setups and the TV with `bw_ui --clip … --screenshot`, viewed.

Fence: `ctest --preset release -j -R "manual|test pattern|ball|tennis"`. Commit: `manuals: Test Pattern, Bouncing Ball, Tennis after the sync move (P6n phase 4)`.

## Phase 5 — Manuals and docs: 080, 090

As phase 4.

Fence: `ctest --preset release -j -R "manual|message|typewriter"`. Commit: `manuals: Message, Typewriter after the sync move (P6n phase 5)`.

## Phase 6 — Recipes and release

1. ADD_MACHINE §2 and §3 (design, Recipes).
2. Screenshots at the knobs' marks of each fault-free Machine (`bw_ui machines/NNN-name/<m>.bwn --screenshot … --after 1s`) and its first Puzzle, viewed.
3. ARCHITECTURE's P6n row: done, date, links; this plan cut to the table and notes.

Fence: `ctest --preset release -j` (full). Commit: `docs: P6n done (P6n phase 6)`.

## Implementation notes

- Design (2026-11-19): every decode prototyped on scratch copies of the netlists and measured with `bw run --tv` (the spec's What is new numbers); Echo and Lost paddles prototyped with the fault wired into the netlist.
- Review (2026-11-19), fixed: the Ball's missing `V3` Net and `U3.QD` pad; every picture case loads the Board, so phase 1 has no fence and phase 2 updates every test and runs the full suite (the Test Pattern is a shared fixture); the Test Pattern's Catch names; `tennis: rolling_picture` probes the new `VSYNC_N`; the Test Pattern's decode-sanity and grid cases; the 050 Torn picture on `VSYNC_N`; `TICK_N` tracks re-netted; part comments; citations.
- Phase 2 (2026-11-19): the user asked for whole Boards to route rather than a netlist import and stubs to delete, and for no hand placement: `at` takes an angle (050's two turned connectors), 050 and 060 got `.place` files from their Boards, `tools/renet_board.py` (checked byte for byte against the Boards handed over) for 070–090. Freerouting left one connection (080 `CH0`), routed by `route_missing.py`. Every crack still resolves and its symptom case passes; only `shorts power`'s typed `mend:` moved (`B.Cu:110.66:35.78`). The window check went to its own `tv_window` (name clash in the Message's and Typewriter's tests). ADD_MACHINE §3's routed-Board bullet and the `at` angle written now, not in phase 6. Full release suite 1460/1460.

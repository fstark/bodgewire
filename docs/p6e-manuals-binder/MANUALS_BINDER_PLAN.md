# P6e — Manuals binder — Implementation Plan

Spec: [MANUALS_BINDER.md](MANUALS_BINDER.md). Design: [MANUALS_BINDER_DESIGN.md](MANUALS_BINDER_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Binder state, datasheet index, the binder's first size | b9ae7e2 |
| 2 | The binder drawn; shelf drawing; `--manual` dividers | cd505c2 |
| 3 | Glossary, recipes, ARCHITECTURE | this commit |

P6e implemented 2026-11-17, user check pending.

## Implementation notes

- Docs review (before the docs commit), fixed: a pin click selects its chip whatever the focus, so it turns the binder (stated, for the user); `bw1_bench.bwn` has no manuals; plain references without a Board; `All datasheets` a button in `manual_window`; a `PushID` per Datasheets mode; the colours `TextFlow` needs; the rings an ellipse; the minimum width 500 for a BW-2001 pinout; `--manual repairs` without a Board; the scrollbar's hover colours; ADD_PUZZLE, `BW1_DESIGN.md` and the ARCHITECTURE wording.
- Written while the review ran: phase 1, most of phase 2.
- Phase 1: release and debug `-R "binder|manual: datasheet index|bench places"`.
- Phase 2: full release suite 1424 tests in 69 s. Screenshots viewed: the Stopwatch at each divider (`--manual user`, `datasheet --probe U1.1`, `datasheet`), the BW-2001's index (33 Parts, wrapped references) and its 6502 page, `bw1_bench.bwn` (`no user manual`, the index with plain references, Repairs grayed), the shelf drawing out and away, the 110 with every instrument. The datasheets' `→` drew as `?` (the font lacks it): `Displayable` gives `->`. The set-back tabs at 80 % read muddy: 88 %.
- Release gate: the 110 with every instrument and the binder out: **1.00×, UI 0.8 ms**.
- After the user check (2026-11-17): the objects came out at their default place every time (P6d's `ImGuiCond_Appearing`, from UX_DESIGN's muscle-memory rule); the user chose to keep the player's place for the run and send an object home by double-clicking its shelf drawing (`SendHome` in `src/ui/bench`; the second click's `MouseClickedLastCount` is 2: out, in front, home; an instrument also takes the focus). Tooltip `double-click: back in its place`.
- Clicks (the tabs, `All datasheets`, a Part's name, a reference in the index, the jump on a selection, the hover on Datasheets, dragging and resizing the binder) are checked by reading the code only: the agent cannot drive the mouse. User check.

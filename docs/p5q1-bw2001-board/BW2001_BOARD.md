# P5q1 — BW-2001 Board as the PET's

Roadmap step P5q1 ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), after P5q ([BW2001_MACHINE.md](../p5q-bw2001/BW2001_MACHINE.md)): the BW-2001's Board laid out as the PET 2001's own board, not only named after it. P5q put each chip in the cell its reference names (`UF3`: row F, column 3), but guessed the grid's direction, the chips' orientation and the connectors' places. The user found the real orientation differs.

## The source

PET-Shack's "PET CPU BOARD" component layout, pages 34–35 of the "PET-Shack PET 2001 Bundle" (zimmers.net, `pub/cbm/schematics/computers/pet/2001/`). It is of an earlier board (6550 RAMs, 6540 ROMs) whose locations differ from the 320137's in places (its 74157s at D1–D3, the 320137's at D2–D4), but the board's geometry is the same family's:

- **Letters are columns, numbers are rows.** Column J at the left to A at the right; row 1 at the bottom (the expansion edge, J4) to 8–9 at the top (the power regulators).
- **Most chips lie horizontally**, notch at the left (pin 1 bottom left). **Eight stand upright**: the 6502, PIA2, the VIA, the 74100 and the character ROM with the notch at the bottom; PIA1 (the drawing's G6) and the 74LS145 ("GH") with it at the top; the 74154 (G1) with no notch drawn. The DIP-40s and the 24-pin chips span more than one row; the 74LS145 fits its own.
- **Edges:** IEEE-488 (`J1`), user port (`J2`) and cassette #2 (`J3`) on the right edge, top to bottom, beside column A; the expansion connector `J4` along the bottom; power (`J8`) at the top over columns I–H (the rectifiers in the top-left corner); cassette #1 (`J6`) at the top; the monitor (`J7`) at the top over column D; the keyboard header (`J5`) standing inside the board between columns H and G at rows 7–9, beside the 74LS145 (here on the top edge by columns H–G, an approximation: `bw place` puts headers on edges); the diagnostic LED at the top by column E.

## Decided

Asked at the start of the step (2026-11-15):

- **The drawing's geometry**, each of our 320137 chips at its own reference's cell (letter the column, number the row). Offered instead: the user's 320137 photos.
- **Chips oriented as the real board**: the chips the drawing shows upright stand upright (`UF3`, `UB8`, `UA5`, `UE6`, `UA2` notch at the bottom; `UG8`, `UG9` notch at the top; `UG2`, no notch drawn, at the bottom), the rest horizontal (the review found `UG8`'s notch and `UG9` upright in the drawing after the question was asked: the drawing is what was chosen); an exception to "every chip the same way" (Board layout preferences), chosen because the Board is a copy of a real one. Offered instead: all horizontal.
- **Connectors where the drawing has them**. Offered instead: today's edges.
- **The 2114s stay the 300 ns grade** (P5q). The TV's picture stays where it is (right of centre); not in this step.

## New

1. `grid pet` follows the PET: columns `J`–`A` left to right, rows 1–9 bottom to top.
2. An `upright REF up|down|in [0|180]` rule: the chip stands upright (180, the default: notch at the bottom; 0: at the top) in its cell's column, reaching into the cells above (`up`) or below (`down`), which must hold no other chip, or within its own row (`in`).
3. `boards/bw2001/bw2001.place` rewritten to the drawing; the Board placed again; **the user routes it again**.

## Tests

- `grid pet`: 10 columns × 9 rows; `UC9` in column C (third from the right), row 9 (the top); `UJ1` at the bottom left; an upright chip at 180° spanning its cell and the one it names; an upright chip reaching into a filled cell refused; a header on the right edge stands along it.
- The Board: `place_drc_bw2001`; after the routing, `check-board`, `drc_bw2001`, the Board loops and the Machine's cases (they run on the Board's circuit) unchanged.
- Release: `bw_ui` on the new Board with every instrument; a screenshot of the Board viewed against the drawing.

Design: [BW2001_BOARD_DESIGN.md](BW2001_BOARD_DESIGN.md). Plan: [BW2001_BOARD_PLAN.md](BW2001_BOARD_PLAN.md).

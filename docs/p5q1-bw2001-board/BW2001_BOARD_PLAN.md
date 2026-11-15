# P5q1 BW-2001 Board as the PET's — Plan

Spec: [BW2001_BOARD.md](BW2001_BOARD.md). Design: [BW2001_BOARD_DESIGN.md](BW2001_BOARD_DESIGN.md).

Each phase ends with its fence: `cmake --build --preset debug` clean, `clang-format` on the C++ touched, the named tests green, one commit.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | `grid pet` as the PET's, `upright`, angle 180 | `5f34a54` |
| 2 | `bw2001.place`, the Board placed, routed by the user, checks, docs | |

### 1. `grid pet` as the PET's, `upright` (design §1, §2, §5)

Fence: `-R "placer|place|kicad pcb writer|board"`, `place_drc_bw2001` included: it places the committed `.place` (P5q's rules, no `upright`) under the new direction and must stay green (if it does not, the phase-1 commit carries the minimal `.place` change that makes it so, and the notes say which). Commit `place: grid pet as the PET's board, upright chips, angle 180 (P5q1)`.

### 2. `bw2001.place`, the Board placed, routed by the user, checks, docs (design §4, §6)

`bw2001.place` rewritten; `bw place … --force` (the routed Board is replaced); `place_drc_bw2001` green; the Board's picture compared with the drawing. **Stop for the user**: route it (two layers, no zones, DRC 0 errors). Nothing is committed in between (`drc_bw2001`, `check-board` and the Board loops are red until the routing). Then `check-board`; `-R "bw2001|boards|electrical|scene|session|drc"`; release `bw_ui` with every instrument, the Board's screenshot; ADD_MACHINE, BW2001_DESIGN, ARCHITECTURE. Commit `bw2001: the Board as the PET's, routed by the user (P5q1)`.

## Implementation notes
- Docs review, fixed: angle 180 was new in the placer's courtyard and pads and the writer's `(point)`, not only an angle the reader takes; ten columns (`I`), nine rows; P5q's resistor chains would hit `UE6` and `UA2` (re-planned); "flush with the cell's edge" defined by the chip band and the row; the drawing has PIA1's notch at the top and the 74LS145 upright (`upright … 0`, `in`); `UA5` covers `UA4` only; `J5` stands inside the board on the PET; `J8` over columns I–H; `R48` left of `R10`; `J1` at 17 %; the phase-1 fence and the routing phase made consistent.
- Phase 1: green first time but the P5q tests, rewritten for the new direction (the header test now compares with row 9's `UC9`); `place_drc_bw2001` stayed green on the old `.place`. An `upright` naming no PET chip is refused by the placer.
- Phase 2: the first `bw place` refused every upright chip: snapping its origin to the 1.27 mm grid pushed its courtyard a fraction of a millimetre into the next row, which counted as reaching it; a row is now entered only past the 2 mm part gap. The back edge spread out (`J8` 10 %, `J6` 20 %, `J5` 40 %, `DS1` 55 %, `J7` 66 %: the 20-pin keyboard header is 51 mm long). Placed: 35.2 m of wire (P5q's 51.4 m), 293 × 269 mm (P5q's 396 × 300); KiCad DRC two silkscreen warnings, no errors. Waiting for the user's routing.

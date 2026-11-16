# P4k — Board Placement

Roadmap step P4k ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): `bw place` writes a `.kicad_pcb` from a netlist and a short rules file, with every footprint placed, every pad on its Net, the outline drawn and no tracks. The user only routes it. Until now the user placed every chip by hand in KiCad (KICAD_GUIDE steps 3–6), which is the hard part of a Board; on 070 the prototype in [prototype/](prototype/README.md) cut the wire length from 15.1 m (hand, chips grouped by U-number) to about 10.7 m.

Decided with the user (2026-11-10):

- Rules live in `boards/<m>/<m>.place`, a small text file next to the Board; the netlist stays circuit-only.
- The footprints `bw place` writes are copied into the repository (`boards/footprints/`), so the output does not depend on the installed KiCad and the tests run without it.
- The 070 Board is regenerated with `bw place` and routed again by the user; it replaces the committed Board, and the nine Puzzles are checked on it.
- Only 070 gets a rules file in this step. The rules for LEDs, digits and resistors are tested on small netlists; the other Boards stay as they are.
- Kept from the prototype (user, 2026-11-10): panel parts come from the Machine design, not the optimiser; every chip has one orientation (rotation measured at about 3 % less wire, not worth it).
- Out of scope, later or its own row: functional groups (chips by job, placed as blocks with space between them and labels on the Board).

## What is new

1. **Rules file.** One rule a line, `#` comments:
   - `edge REF SIDE AT`: `REF` against the `SIDE` edge (`left`, `right`, `back`, `front`; the front is the player's side, at the bottom of the Board view). `AT` is where along that edge: one of its ends (`left` or `right` on the back and front edges, `back` or `front` on the left and right edges), `middle`, or a percentage from the left or back end (`30%`).
   - `row SIDE AT REF REF …`: several parts side by side along an edge, in this order from the left (or back) end, the row as a whole at `AT`.
   - `next REF OTHER SIDE`: `REF` just beside `OTHER` on its `SIDE`, centred on it; `OTHER` is placed by an earlier line.
   - `grid COLUMNS ROWS`: the chip grid's size, when the computed one does not suit.
   - Later: `grid pet` and `upright` (P5q1), `size W H` and `at REF X Y` for a layout copied from a real board (P5x: [BW80_MACHINE_DESIGN.md](../p5x-bw80/BW80_MACHINE_DESIGN.md) §5).
2. **Panel parts and chips.** A part with a rule is a panel part: placed by its rule, upright (0°), its courtyard inside a band along its edge. Every other part goes into the chip grid: DIP chips (the oscillator too) and resistors. A part of any other package without a rule is an error.
3. **Chip grid.** Columns 16 mm apart, rows 27 mm apart (070's spacing, which the user routed), every chip upright with pin 1 at the top left. Its size is computed from the chip count (the grid near 1.6:1; 5 rows of 12 on 070) unless `grid` says otherwise. The chips are placed by simulated annealing on the half-perimeter wire length of every Net but `VCC` and `GND`, the panel parts fixed, as in the prototype; several seeds, the shortest kept. The same input gives the same file.
4. **Board.** The outline is a rectangle round the grid and the bands; edges without panel parts keep a 5 mm margin. Panel parts that overlap (two rules sending parts to the same place) are an error naming both. The `.kicad_pcb` is KiCad 10's format, opened by KiCad as any Board: the ratsnest shows what to route. A `.kicad_pro` with the design rules every Board uses (KiCad's defaults) is written next to it if there is none.
5. **Command.** `bw place netlists/<m>.bwn boards/<m>/<m>.place -o boards/<m>/<m>.kicad_pcb [--seed 1] [--tries 4] [--force]`. It prints the wire length and the grid. It refuses to overwrite a Board that has tracks unless `--force`, so a routed Board is not lost by mistake.
6. **The 070 Board.** `boards/tennis/tennis.place`, from TENNIS_DESIGN.md: `J1` at the left, `J2` on the back edge, `J3` and `J4` at the two ends of the front edge, `SW1` between them with `R1` beside it. The user routes the generated Board; it replaces the committed one.
7. **Recipe.** ADD_MACHINE.md § Board: the agent writes the `.place` from the Machine design and runs `bw place`; the user routes.

## Tests

- Footprints: each of the nine vendored footprints loads, with its pads where KiCad has them and a courtyard round them.
- Rules: each rule parses; errors name the file and line (unknown word, side or position, a reference given twice, `next` to a part not yet placed).
- Placement on small netlists: an `edge` part lies against its edge at its position; a `row` keeps its order with gaps between; `next` puts a resistor beside its LED; parts falling off the Board or overlapping are refused; a digit without a rule is refused; annealing puts two chips joined by many Nets next to each other; the same seed gives the same placement.
- 070: the generated Board reads back with our reader (every Part present), every pad on its netlist Net, every courtyard inside the outline and apart from the others, the rules held (`J3` left of `J4` on the front edge, `R1` right of `SW1`), wire length under 12 m; KiCad's DRC on it finds no violation, only unrouted connections.
- After the user's routing: `bw check-board` prints `ok`, `drc_tennis` passes, the nine Puzzles and every 070 test pass on the new Board.

Design: [BOARD_PLACEMENT_DESIGN.md](BOARD_PLACEMENT_DESIGN.md). Plan: [BOARD_PLACEMENT_PLAN.md](BOARD_PLACEMENT_PLAN.md).

# P5q1 BW-2001 Board as the PET's — Design

Implements [BW2001_BOARD.md](BW2001_BOARD.md). Recipe: [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §3 (Board). P5q's `grid pet`: [BW2001_MACHINE_DESIGN.md](../p5q-bw2001/BW2001_MACHINE_DESIGN.md) §7.

## 1. `grid pet` — `src/board/placer.cpp`

- `PetCell(reference)` returns `(row, column)` with the column from the letter, `J` = 0 (left) to `A` = 9 (right; ten letters with `I`), and the row from the number, `9` = 0 (top, the back) to `1` = 8 (bottom, the front). `kPetRows` = 9, `kPetColumns` = 10 (were 10 and 9): `Placement.rows`/`columns`, the grid strings and the free cells follow; the rest works on the pair.
- A row is `rowPitch_` tall: its chip band (`cellBox`, `chipHeight_`) on top, its resistor band below. Rows run top (9) to bottom (1).
- Column widths stay "the longest chip lying in it"; an upright chip counts by its width across (a DIP-40's 17.8 mm), so columns holding one get no wider than their horizontal chips need.
- Placement messages and the printed grid name cells by their PET reference, so the printed grid reads as the board: top row 9, `UJ9 … UA9`.

## 2. `upright` — `src/board/place_rules.{hpp,cpp}`, `placer.cpp`

- Rule `upright REF up|down|in [0|180]` (after `grid pet`; `REF` a PET cell chip; angle default 180): `PlaceRules::upright` a map reference → `{direction, angle}`.
- Placer: the chip at that angle (180: notch at the bottom, pin 1 bottom right; 0: notch at the top, pin 1 top left), its courtyard centred across its column. `down`: its top at its row's top (the chip band's top), reaching down; `up`: its bottom at its row's bottom (the resistor band's bottom), reaching up; `in`: centred in its row, which must hold it (`rowPitch_`). Every row its courtyard enters is covered in its column: another PET chip there is refused ("'UG8' upright reaches 'UG9''s cell"); free parts never go to a covered cell. `next` beside it uses its turned courtyard.
- **Angle 180 is new everywhere a footprint is turned** (P5q handled only 90): `Courtyard` (`placer.cpp:34`: at 180 (x, y) → (−x, −y), the box's min and max swapped and negated), `Rotated` (pads, for the wire length), the writer's `(point)` turn (`kicad_pcb_writer.cpp:117`: (−x, −y)) and its pads' angles (added, as for 90); `PlacedPart::angle`'s comment ("0, 90 or 180"). The reader takes any angle (`kicad_board.cpp:177`).

## 3. Edges

Headers on the left and right edges already stand along them (a 2 × 12 header's pads run down the edge at 0°); headers on the back and front edges lie along them (P5q). No code.

## 4. `boards/bw2001/bw2001.place`

```
grid pet
upright UF3 down            # notch at the bottom (the drawing's F1)
upright UG8 down 0          # notch at the top (G6)
upright UG9 in 0            # the 74LS145 ("GH"), pin 1 top left; fits its row
upright UB8 down
upright UA5 down
upright UG2 down            # no notch drawn: at the bottom
upright UE6 up
upright UA2 up
edge J1 right 17%           # IEEE-488 beside UA7-UA9 (rows 9-7)
edge J2 right 50%           # user port beside the VIA (UA5)
edge J3 right 90%           # cassette #2 by UA1
edge J8 back 15%            # power over columns I-H (the corner held rectifiers)
edge J6 back 25%
edge J5 back 35%            # keyboard: inside the board by H-G on the PET, here on the edge
edge DS1 back 50%           # the LED by column E
edge J7 back 62%            # monitor by column D
```

Each upright chip's cells, from the netlist's free cells (checked by the review): `UF3` rows 3–2 of column F (`UF2`, `UF1` empty), `UG8` rows 8–7 (`UG7`), `UB8` 8–7 (`UB7`), `UA5` 5–4 (`UA4`; it ends inside A4), `UG2` 2–1 (`UG1`), `UE6` 6 and 7's resistor band (`UE7` empty; `UE5` filled), `UA2` 2 and 3's resistor band (`UA3` empty; `UA1` filled), `UG9` its own row.

The resistors, re-planned around the upright chips (the review found P5q's chains would hit `UE6` and `UA2`):

- `R29 UE3 front`, `R30`–`R33` right (row 3's band, columns E–C: `UF3` fills column F there); `R34 UE4 front`, `R35`–`R38` right (row 4's band, E–C);
- `R25 UG8 front`, `R24`–`R18` left (row 7's band, columns G–J: `UE6` reaches row 7's band in column E);
- `R28 UE2 front`, `R46 R28 right` (`UE2 back` would share row 3's band with `R29`);
- `R13 UF3 front`, `R14`, `R17` right (row 1, columns F–D: `UG2` covers G1);
- `R10 UA9 front`, `R48 R10 left` (right of it, `R48` would reach the right band by `J1`);
- `R26 UG3 front`, `R12 UC7 front`, `R39 UC5 front`, `R40 UD2 front`, `R41 UB6 front`, `R43 UA1 front`, `R44 UC1 front`, `R45 R44 right`, `R27 UE8 front` as P5q's.

`bw place` refuses any overlap; a rule it refuses is moved and the notes say so.

## 5. Tests

`tests/board/placer_test.cpp`: the P5q `grid pet` cases rewritten for the new direction: `p.columns == 10`, `p.rows == 9`, `UC9` at `grid[0 * 10 + 7]` (top row, third column from the right), `UA5` right of `UB5` and below `UB8`, row 9 above row 1; `upright`: a DIP-40 `down` at 180° in its column reaching the row below, `up`, `in` with angle 0; refused into a cell holding a chip; refused before `grid pet` (parser); `kicad_pcb_writer_test.cpp`: the turned-pads case extended to 180° (`(−x, −y)`), and a part's `(point)` at 180°.

## 6. Docs

ADD_MACHINE §3: `grid pet`'s direction, `upright`. BW2001_DESIGN.md's Board section. ARCHITECTURE row P5q1.

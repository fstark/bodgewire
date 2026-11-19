# P6n — Picture margins

Roadmap step P6n ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the early TV Machines put their sync pulses at the end of the blanking, so a real monitor's window (P6f) cuts the top of every picture and the left of the text Machines'. The circuits are fixed, as their designers would have fixed them. Design: [PICTURE_MARGINS_DESIGN.md](PICTURE_MARGINS_DESIGN.md). Plan: [PICTURE_MARGINS_PLAN.md](PICTURE_MARGINS_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

A line or a frame is picture, then blanking: a front porch, the sync, a back porch. The monitor's beam is still flying back during the back porch, so a picture there is lost. P6f's window ([TV_CONTROLS.md](../p6f-tv-controls/TV_CONTROLS.md)) hides what a real monitor hides: TV columns before 80 (the line's first 10 µs from the sync edge) and from 496 (its last 2 µs), and the rows before 28 (the vertical retrace, counted from the frame start 16 µs into the vertical sync).

The five early TV Machines have blanking of the right length but the sync at its end. They decode the sync on the counter's last values, so the counter wraps to 0 and the picture starts at once (rows and columns: the lit cells measured with `bw run --tv` at 1 s):

| Machine | Horizontal: front porch, sync, back porch | Vertical sync | Picture rows | Picture columns | Cut by the window |
|---|---|---|---|---|---|
| 050 Test Pattern | 4, 4, 8 µs | lines 308–311 | 4–291 | 96–480 | top 24 lines |
| 060 Bouncing Ball | 4, 4, 8 µs | 308–311 | 4–259 | 96–480 | top 24 lines (the top wall) |
| 070 Tennis | 4, 4, 8 µs | 308–311 | 4–259 | 96–480 | top 24 lines |
| 080 Message | 9, 3, under 3 µs | 308–311 | 4–259 | 44–400 | top 24 lines (blank text rows), left 36 columns |
| 090 Typewriter | about 10, 3, 1.25 µs | 308–311 | 4–259 | 34–415 | the first text row, left 46 columns |

Horizontally, 050–070 are right; 080 and 090 have their porches the wrong way round. Vertically none has a back porch: line 0 follows the sync's last line. (The roadmap row's "Ball's and Tennis's left 16 columns" does not hold: their picture starts at column 96.)

Side effect of the fix: the frame's first line is drawn 32 columns left (it starts on the end of the last inverted sync pulse, the XOR mixing). Today that line is the picture's line 0 (the Ball's top wall); afterwards it is a blank line in the back porch.

## User decisions (2026-11-19)

| Question | Decision |
|---|---|
| Fix the TV or the circuits | the circuits: a bug in the Machines, fixed as their designers would have. The TV and its window stay |
| Must the picture fit at the knobs' marks | the aim, not a must: a player turning H-POS/V-POS for a Machine is period (the user did, on real computers) |
| Board work | the user reroutes in KiCad (Freerouting) the connections that change; no Board is laid out again (050 and 060 were placed by hand, 070–090 by `bw place` then tidied) |
| New chips | none: spare gates only, so every footprint stays where it is |
| The Test Pattern's 288 lines | kept: the sync moves to lines 288–291, right after the grid; its lines 0–3 (the top grid line) stay hidden at any knob setting, in the overscan as on a real monitor (V-POS moves the cropped picture, it shows no hidden row; only the underscan trimmer does) |
| Ball and Tennis: the paddles' trigger and `MOVE`, worked out from line 308 | stay on line 308: the old decode keeps them, renamed `TICK_N`; a new `VSYNC_N` decode drives the sync mixer only |
| Vertical sync length | 4 lines where the spare gates allow (Test Pattern, Ball, Message, Typewriter); Tennis, with only a NAND and an OR spare, 16 lines (272–287) |
| Puzzles whose symptom changes | kept, their brief and explanation re-read; replaced only if they stop being interesting |
| Echo (080, `fault stuck U12.1B 0`) | kept with the move: its echo (text rows 0–3 drawn again on lines 256–311) now follows the vertical sync, so the poem's first line lands above the window and what shows is the tops of the second line's letters (lines 304–311) along the screen's top edge, rows 28–31; "good enough". New brief for that strip |

Decided while designing (2026-11-19), for the user to overrule:

- **The sync moves, the picture stays.** Every picture keeps its counter values (dots 0–95, lines 0–255…): the manuals, test points and Puzzle explanations that count in them stay true; only the sync decodes change.
- **Message's horizontal sync becomes 6 µs** (characters 18–19): one pin moved, no gate added. 3 µs (one character) cannot give a back porch of 10 µs with the text still clear of column 496.
- **New Net names**: `TICK_N` (line 308 on Ball and Tennis), `VSYNC_A` (a decode's first gate, as `TOP_A`), `VSYNC` (Ball's active-high pulse before its inverter), `V2_N`, `V3_N`, `V4_N`, `C2_N`.

## What is new

The five netlists' sync decodes (prototyped on scratch copies; rows and the 050–070 columns measured with `bw run --tv`, the 080 and 090 columns the text's first and last dot computed from the timing and matching the measured lit cells):

| Machine | Horizontal sync | Vertical sync | Gates used | Picture rows | Picture columns |
|---|---|---|---|---|---|
| 050 Test Pattern | unchanged | 288–291 | 2 spare inverters | 24–311 (lines 0–3 hidden) | 96–480 |
| 060 Bouncing Ball | unchanged | 280–283 | spare NAND, NOR3, 2 XORs; line 308 kept as `TICK_N` | 32–287 | 96–480 |
| 070 Tennis | unchanged | 272–287 | spare NAND, OR; line 308 kept as `TICK_N` | 40–295 | 96–480 |
| 080 Message | characters 18–19 (6 µs) | 280–283 | 1 spare inverter | 32–287 (text 64–255) | 92–476 |
| 090 Typewriter | characters 34–35 (3 µs) | 280–283 | 1 spare inverter, 1 spare NAND as inverter | 32–287 | 106–490 |

Then, for each Machine: its Board rerouted where its Nets change, its tests, its Puzzles re-read, its manuals and docs. And a rule in [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) so a new TV Machine leaves room for the monitor: sync plus back porch of 10–14 µs before the picture, the picture over by 62 µs after the sync edge, and the vertical sync at least 28 lines before the picture's first.

The kernel, Parts, the builder, devices, the TV, the UI and Session are untouched.

## Puzzles

Checked against the prototypes:

- **Lost paddles (070, `fault joint U7.4`)**: the frame is 308 lines, so line 308 never comes: no `TICK_N`, no paddle triggers, no `MOVE`. The new sync still comes, so the picture now holds (prototype: locked). The new symptom, "a steady picture, no paddles, the ball never comes across", is distinct from Rolling picture's (rolls, the game plays). Brief and explanation rewritten.
- **Rolling picture (070, `fault joint U10.2`)**: unchanged (the sync never reaches the mixer). Its explanation's proof changes: the working paddles prove `TICK_N`, no longer `VSYNC_N`.
- **Rolling picture (050, `fault stuck U3.QA 0`)**: still no lock: the frame end is never decoded and the counter runs to 512 lines; the sync decode (which reads neither `V0` nor `V6`, `V7`) pulses on lines 288, 352, 416 and 480 of each 512, as the old one did on its own lines. Its case checks the picture differs from the good one.
- **Torn picture (050, `fault bridge U10.1 U10.2`)**: `HSYNC_N` and `VSYNC_N` bridged; the decode behind `VSYNC_N` moves, the symptom does not (the XOR sees equal inputs, `SYNC` stays low).
- **Echo (080)**: decided above; brief and explanation rewritten, its case checks the strip on rows 28–31 (line l ≥ 280 on row l − 280).
- Every other Puzzle of the five: no fault on a changed pin or Net (checked against the design's pad lists), the symptom checked by its test after the change. The crack Puzzles' Traces (050 `H0`, `VCC` with the mend `shorts power` makes at `F.Cu:164.85:51.05`, `LINE_END`; 060 `VLOAD_N`; 070 `LPAD_CK`, `SERVING`; 080 `CH5`, `CLK_N`; 090 `PHI2_N`, the Return decoder's input from `U12` pin 4) are not on changed Nets and must keep their routes.

## Tests

- A shared check: the lit part of a frame lies inside the window (columns 80–496, rows from 28), used by each of the five Machines' fault-free cases. The Test Pattern's case allows its lines 0–3 above the window, and only those.
- Each Machine's cases that find the picture's position: the new offsets (the Message's and the Typewriter's left and top bounds, the Test Pattern's sync lines).
- The Message's Echo case and the Tennis's Lost paddles case: their new symptoms.
- `boards: … matches its netlist` and the DRC case for each rerouted Board.
- The full release suite at the end; screenshots of each Machine at the knobs' marks, viewed.

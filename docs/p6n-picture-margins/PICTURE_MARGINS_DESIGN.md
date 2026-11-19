# P6n — Picture margins — Design

How [PICTURE_MARGINS.md](PICTURE_MARGINS.md) is built. Plan: [PICTURE_MARGINS_PLAN.md](PICTURE_MARGINS_PLAN.md).

## Files

| File | Change |
|---|---|
| `machines/050-test-pattern/test_pattern.bwn` | vertical sync on lines 288–291 |
| `machines/060-bouncing-ball/ball.bwn` | `TICK_N`; vertical sync on lines 280–283 |
| `machines/070-tennis/tennis.bwn` | `TICK_N`; vertical sync on lines 272–287 |
| `machines/080-message/message.bwn` | horizontal sync on characters 18–19; vertical sync on lines 280–283 |
| `machines/090-typewriter/typewriter.bwn` | horizontal sync on characters 34–35; vertical sync on lines 280–283 |
| `machines/*/board/*.kicad_pcb` (the five) | pads moved to their new Nets, those connections rerouted (the user) |
| `tests/machines/tv_page.hpp`, `.cpp` | `LitBounds`, the shared window check |
| `tests/machines/{test_pattern,ball,tennis,message,typewriter}_test.cpp` | new positions, the window check, Echo and Lost paddles |
| `machines/070-tennis/puzzles/09-short-frames.bwp`, `machines/080-message/puzzles/02-echo.bwp`, `machines/070-tennis/puzzles/08-rolling-picture.bwp` | briefs and comments (Echo's new brief) |
| each Machine's `manuals/user.md`, `manuals/service.md`, `docs/NAME.md`, `docs/NAME_DESIGN.md` | the sync, the new gates, readings re-checked |
| `docs/recipes/ADD_MACHINE.md` | the monitor's margins (§2); changing a routed Board's Nets (§3) |
| `docs/ARCHITECTURE.md` | the P6n row |

## Netlists

Every change below rewires pins of chips already on the Board; a pin leaving the unused-gate `GND` (or `VCC`) line is removed from it. Net names follow `docs/NAMING.md`; lines counted from 0, `V8` the line counter's top bit. Within the frame (lines 0–311) `V8` high means lines 256–311, offsets 0–55 in `V5`–`V0`, so a decode with `V8` need not read `V6` and `V7`; each decode below was checked to match its lines only (a pattern matching offsets 52–55 too would make a second pulse on 308–311).

### 050 Test Pattern

`U8` (74LS30) decodes `VSYNC_N` = NAND(`V8`, `V5`, `V4`, `V2`, `V3_N`, +5 V ×3): lines 308–311. It becomes NAND(`V8`, `V5`, `V4_N`, `V3_N`, `V2_N`, +5 V ×3): lines 288–291 (1 0010 00xx).

- `U9` gate 4 (spare): `4A` = `V4`, `4Y` = `V4_N` → `U8.C`.
- `U9` gate 5 (spare): `5A` = `V2`, `5Y` = `V2_N` → `U8.D`.
- `U9.6A` stays on `GND`. Part comment `U9`: "H3_N, V3_N, LINE_END, V4_N, V2_N".

`VBLANK` (lines ≥ 288) and the grid unchanged. Line 0 lands on row 24, so lines 0–3 (the top grid line) stay above the window: the user's decision.

### 060 Bouncing Ball

`U8` gate 1's output, NAND(`V8`, `V5`, `V4`, `V2`) = line 308–311, keeps feeding `U17` gate 1 (`MOVE` = NOR(it, `V1`, `V0`): line 308) and is renamed `TICK_N`. A new decode makes `VSYNC_N` for `U10.1B` alone, lines 280–283 (1 0001 10xx: `V8`, `V4`, `V3` high, `V2` low):

- `U14` gate 4 (spare NAND): `VSYNC_A` = NAND(`V8`, `V4`).
- `U10` gate 3 (spare XOR, `3B` on +5 V): `V3_N` = NOT `V3`. The Ball has no `V3` Net (`U3.QD` unconnected): a new `net V3 U3.QD U10.3A`.
- `U17` gate 3 (spare NOR3): `VSYNC` = NOR(`VSYNC_A`, `V3_N`, `V2`).
- `U10` gate 4 (spare XOR, `4B` on +5 V): `VSYNC_N` = NOT `VSYNC` → `U10.1B`.

`U13` gate 4 stays spare. Part comments: `U8` "TICK_N, BOTTOM_N" (as on 070), `U10` "composite sync, V3_N, VSYNC_N", `U14` "TOP_N, LEFT_N, HBLANK_N, VSYNC_A", `U17` "MOVE, VIDEO_D, VSYNC". The header's "make sync as on 050" stays true in kind; the netlist's sync comment names the lines.

### 070 Tennis

As 060, `U8` gate 1's output becomes `TICK_N` and keeps `U17.1A` (`MOVE`), `J3.TRIG_N` and `J4.TRIG_N`: the paddle maths of [TENNIS.md](../../machines/070-tennis/docs/TENNIS.md) (top line ⌊224.5 − 208 k⌋ from line 308) is unchanged. Tennis has no spare inverter, NOR3 or XOR (`U10` gates 3–4, `U14` gate 4 and `U17` gate 3 serve the paddles and scores), so its new decode is the two gates it has, lines 272–287 (1 0001 xxxx: `V8`, `V4` high, `V5` low):

- `U51` gate 4 (spare NAND): `VSYNC_A` = NAND(`V8`, `V4`).
- `U38` gate 4 (spare OR): `VSYNC_N` = OR(`VSYNC_A`, `V5`) → `U10.1B`.

The 16-line pulse: the TV starts its frame 16 µs into it ([TV_DESIGN.md](../p2c-tv/TV_DESIGN.md) §5.2, Line and frame starts) and takes the line syncs through it (the XOR mixing inverts them, as on the 4-line pulse); the other 15 lines are blank. Part comments: `U8` "TICK_N, BOTTOM_N", `U38` "GO_DOWN, GO_UP, BALLV_N, VSYNC_N", `U51` "WIN_L_N, WIN_R_N, WIN, VSYNC_A"; the header's "triggered by VSYNC_N" becomes `TICK_N`.

### 080 Message

Horizontal: `U4` gate 2 decodes `HSYNC_N` = NAND(`C2`, `C4`, +5 V, +5 V): character 20. `2A` moves from `C2` to `C1`: NAND(`C1`, `C4`): characters 18–19 (1 0010, 1 0011; the counter stops at 20). The text, shown one character after it is addressed (MESSAGE_DESIGN, Timing), starts 4 characters less a dot after the sync edge: 11.5 µs, column 92, and ends at column 476.

Vertical: `U10` gate 1 decodes NAND(`V8`, `V5`, `V4`, `V2`). `1B` moves from `V5` to `V3`, `1D` from `V2` to `V2_N`: NAND(`V8`, `V3`, `V4`, `V2_N`), lines 280–283.

- `U5` gate 4 (spare inverter): `4A` = `V2`, `4Y` = `V2_N` → `U10.1D`.

Part comment `U5`: "DOT_LOAD_N, LINE_END, CLK_N, V2_N"; `U4` and `U10` unchanged; the header says where the syncs are.

### 090 Typewriter

Horizontal: `U4` gate 2 decodes NAND(`C5`, `C3`, +5 V, +5 V): characters 40–41. It becomes NAND(`C5`, `C1`, `C2_N`, +5 V): characters 34–35 (10 0010, 10 0011; 38–39 have `C2` high, 40–41 `C1` low). The text starts 9 characters less a dot after the sync edge: 13.25 µs, column 106; it ends at column 490.

- `U4.2B` from `C3` to `C1`; `U4.2C` from +5 V to `C2_N`.
- `U5` gate 6 (spare inverter): `6A` = `C2`, `6Y` = `C2_N`.

Vertical: `U10` gate 1 as 080's: `1B` from `V5` to `V3`, `1D` from `V2` to `V2_N`, lines 280–283.

- `U29` gate 4 (spare NAND): `4A` = `4B` = `V2`, `4Y` = `V2_N` (the only spare inverter, `U5` gate 6, now makes `C2_N`).

The page clocks, the row buffer, the cursor and the blink read `V0`–`V8`, `C0`–`C5` and `ACTIVE`, never the syncs: unchanged. Part comments: `U5` "…, C2_N", `U29` "WRK_N, WRITE, ROW_LOAD_N, V2_N".

## Boards

No footprint moves. For each Machine the pads that change Net (by reference and pin name, as above):

| Machine | Pads |
|---|---|
| 050 | `U8.C`, `U8.D`, `U9.4A`, `U9.4Y`, `U9.5A`, `U9.5Y` |
| 060 | `U8.1Y` and `U17.1A` (`TICK_N`), `U10.1B`, `U3.QD` (new Net `V3`), `U10.3A`, `U10.3B`, `U10.3Y`, `U10.4A`, `U10.4B`, `U10.4Y`, `U14.4A`, `U14.4B`, `U14.4Y`, `U17.3A`, `U17.3B`, `U17.3C`, `U17.3Y` |
| 070 | `TICK_N` (`U8.1Y`, `U17.1A`, `J3.TRIG_N`, `J4.TRIG_N`), `U10.1B`, `U38.4A`, `U38.4B`, `U38.4Y`, `U51.4A`, `U51.4B`, `U51.4Y` |
| 080 | `U4.2A`, `U10.1B`, `U10.1D`, `U5.4A`, `U5.4Y` |
| 090 | `U4.2B`, `U4.2C`, `U5.6A`, `U5.6Y`, `U10.1B`, `U10.1D`, `U29.4A`, `U29.4B`, `U29.4Y` |

Steps (changed while implementing, 2026-11-19: the user routes whole Boards with Freerouting rather than editing stubs, and wants as few human steps as possible; [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §3, "Nets changed on a routed Board"):

1. 050 and 060, laid out by hand, get a `.place` from their Boards: `size` and one `at REF X Y [ANGLE]` a part, each footprint's courtyard centre and angle (050's `J1` at 90, `J2` at 180: `at` takes an angle, `src/board/place_rules.{hpp,cpp}`, `placer.cpp`, with parser and placer cases). `bw place` gives the Board back unrouted, every part under 0.64 mm from where it was (the 1.27 mm grid), angles kept.
2. 070, 080 and 090 keep their `.kicad_pcb` (tidied after `bw place`, which would anneal again): `tools/renet_board.py` deletes the tracks and vias and puts each pad on its Net from the netlist.
3. User: Freerouting on each whole Board. Agent: `bw check-board` ok, KiCad's DRC clean, leftovers by `tools/route_missing.py` (080's `CH0`).
4. Every crack Puzzle's crack resolved again on the new Traces and its symptom case run; `shorts power`'s typed `mend:` on 050's `VCC` crack moves to the crack's new point.

## Tests

### The window check (`tests/machines/tv_window`)

In its own file, not `tv_page`: `message_test.cpp` and `typewriter_test.cpp` have their own `FindPage` and `Matches`, which `tv_page.hpp`'s would make ambiguous. `InsideWindow(box, lines)` beside `LitBounds`.

```cpp
// The lit part of `frame` (cells >= 0.5): rows and columns, inclusive; nullopt if nothing is lit.
struct LitBox { int top, bottom, left, right; };
std::optional<LitBox> LitBounds(const TvFrame& frame, int fromRow = 0, int toRow = kTvRows);
```

Each Machine's fault-free case checks its box inside P6f's window (`kWindowLeft`, `kWindowRight`, `kWindowTop` from `src/view/tv_controls.hpp`; the window's bottom is `lines + 1`, `lines` from `tv().completedStats()`, 312 on every one of the five, so row 313): `left >= 80`, `right < 496`, `top >= 28`, `bottom < lines + 1`. The Test Pattern's case checks rows 28 onward that way, and that rows 24–27 hold its lines 0–3 (the top grid line on row 24) and nothing above row 24 is lit. The frame's first line (drawn 32 columns left, spec) is blank in every picture after the change; the check covers it.

### Per Machine

- `test_pattern_test.cpp`: `Decode`'s `vsync` = lines 288–291; the case that starts at `n0 = 308 * kPixelsPerLine + 1` starts at line 288; "decode sanity"'s `syncPixels == (v >= 308 ? 60 : 4)` becomes lines 288–291; "TV shows the grid" maps line v to row v + 24 for every line (line 0 included: it no longer follows an inverted sync pulse) and the border to rows < 24 and ≥ 312.
- `ball_test.cpp`: `leftWall == 96` unchanged; the picture's rows found from the top wall as now; the window check; the comments about line 0 drawn 32 columns early (lines 135–137, 284–285) dropped, and the row skipped for it no longer skipped if the search needs it.
- `tennis_test.cpp`: positions found from the picture as now (`lineZeroRow`); the window check; `tennis: short_frames` (Lost paddles): now the TV locked, no paddle drawn, the ball's column unchanged over a second; `tennis: rolling_picture`: the pad `U10` pin 2 now probes `VSYNC_N` from `U38.4Y`, no longer `PinNet("U8", "1Y")`, and its comment says why the paddles still work (`TICK_N`).
- `message_test.cpp`: `FindPage`'s top search (`top < 64`) still covers row 32; `left` bounds 41–47 become 89–95 (column 92, three columns either way, as now); `top <= 8` becomes the measured 32 ± 2; the Echo case: lines 288–311 now on rows 8–31 of the next frame (line l ≥ 280 on row l − 280); it checks row 3's glyph lines 2–3 on rows 28–31 and nothing lit under the page.
- `typewriter_test.cpp`: `left` 30–38 becomes 102–110; `top` the measured row ± 2.

Every picture case of the five loads the Board (`LoadNetlistMachine` → `LoadMachine` checks it against the netlist, `src/machine/machine.cpp`), so none runs until phase 2; only the "loads without warnings" cases (`DesignCircuit`) run on the netlist alone. The cases name the measured numbers in a comment with their derivation (the design's formulas), as the existing ones do.

## Puzzles

- `070/09-short-frames.bwp` (Lost paddles): comment "line 308 never comes, so no `TICK_N` (no paddle triggers) and no `MOVE`; the sync (lines 272–287) still comes: the picture holds"; brief "The paddles are gone and the ball never comes across the court." TENNIS.md's Puzzles row and TENNIS_DESIGN's explanation the same.
- `070/08-rolling-picture.bwp`: comment and TENNIS.md's row: the paddles prove `TICK_N` and the line counter good; `VSYNC_N` pulses at `U38` pin 11 and on `U10` pin 2's pad, not on the leg.
- `080/02-echo.bwp`: comment (rows 0–3 drawn again on lines 256–311, after the sync from 284: row 2 on rows 8–23 above the window, row 3's top on rows 24–31, its glyph lines 2–3 visible); brief along the lines of "A row of broken dots runs along the very top of the screen." (wording checked against the screenshot). MESSAGE.md's and MESSAGE_DESIGN's Echo rows the same.

## Manuals and Machine docs

Per Machine, written to STYLE's Manuals rules ([ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §2):

- User Manual: where the picture sits (from the screenshot at the marks); How it works: the sync's lines and why it comes before the picture (the beam's flyback), in a sentence.
- Service Manual: Components (`U8`'s `TICK_N`, the new gates' roles, the moved pins), Test points (every row naming a changed Net re-read with `bw run --probe` at the times it names; analyzer setups that trigger on `VSYNC_N` re-checked with `bw_ui --clip … --screenshot`), Troubleshooting unchanged unless a chart names a sync line.
- `docs/NAME.md` (circuit table: sync lines, timing paragraph; Puzzles rows) and `docs/NAME_DESIGN.md` (netlist excerpt, Board's crack notes, "Where the page lands on the TV" ("text starts … column 44", "34"), the Message Echo explanation).
- `machines/050-test-pattern/manuals/user.md`'s "18 horizontal" lines: 17 show on the TV.

## Recipes

- ADD_MACHINE §2, after the TV page bullet: a TV Machine's sync sits early in its blanking: sync plus back porch 10–14 µs before the first dot, the last dot before 62 µs after the sync edge (TV columns 80–496), and the vertical sync at least 28 lines before the picture's first; decode it from the counters there, not on their last values (P6n). The fault-free case calls the window check.
- ADD_MACHINE §3: Nets changed on a routed Board, no part added: the steps above (a `.place` for a hand-made Board, `renet_board.py` for a tidied one, Freerouting on the whole Board, cracks checked again); `at`'s angle in the real-board bullet. Done with phase 2.

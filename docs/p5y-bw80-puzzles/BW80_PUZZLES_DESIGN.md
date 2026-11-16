# P5y BW-80 Puzzles — Detailed Design

Implements [BW80_PUZZLES.md](BW80_PUZZLES.md): fifteen Puzzles on the BW-80, their tests, the Machine docs. No new code outside the tests.

Recipes: [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md) (design rules, the `.bwp` file, Fault syntax, tests, docs). Machine: [BW80.md](../machines/140-bw80/BW80.md), [BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md) (the memory map, the connectors, the tests); the display chain: [BW80_VIDEO_DESIGN.md](../p5v-bw80-video/BW80_VIDEO_DESIGN.md) §1. The user manual (`manuals/bw80/user.md`) has everything the briefs lean on (the moment of nothing at power-on, the `K` cursor, keyword entry, the screen blanking while busy), but names the keys wrong: PRINT is the O key and LET the K key (`src/parts/zx80_keys.cpp`, the ROM; P has no keyword), not P and O. The manual's sentence and BW80.md's are corrected in phase 3.

---

## 1. The test rig

`tests/machines/bw80_rig.{hpp,cpp}`, moved out of `bw80_test.cpp` unchanged but for the names (they leave the anonymous namespace, prefixed `Bw80`), shared by the Machine cases and the Puzzles:

- `Bw80NetlistText()`, `Bw80Glyphs()` (the ROM's character set, `$0E00`–`$0FFF`), `Bw80TestRomDirectory()`;
- `Bw80Build(text, testRom)` (a netlist text without a Board), `Bw80BoardCircuit(testRom)` (the Machine from its Board), and new `Bw80Built(const Machine&)` (a Puzzle's or repaired Machine's Circuit, `REQUIRE`d, as `Bw2001Built`);
- `Bw80Frames`, `Bw80LockedFrames(loaded, seed, count, cap)`, `Bw80CheckSynced`, `Bw80TestRomPage()`;
- `Bw80Run` (the simulator with the TV on `J2`, its Nets found by connector pin, `ConnectorNet(loaded, "J2", "VIDEO")` and `("J2", "SYNC_N")`, since a Puzzle's Nets are named from the Copper; `runTo`, `lockedFrames`) and `Bw80Type(run, loaded, text, at, settle)` (presses × 180 ms + settle). `Bw80LockedFrames` finds its Nets the same way.

`bw80_test.cpp` keeps its cases on the rig. `tests/CMakeLists.txt` gains `bw80_rig.cpp` and `bw80_puzzles_test.cpp`.

`tests/machines/bw80_puzzles_test.cpp` (phase 2, with the first Puzzles: a test file with helpers and no case does not build under `-Werror`):

- `LoadPuzzle(name)` (`LoadPuzzleFile` from `BW_PUZZLE_DIR`); `kPuzzles` (`std::array`, BW80.md's order) feeds the title and build cases.
- `KScreen(run)`: the `K` cursor screen locked by `cap` (the `K` case's check: a locked frame, the inverse `K` glyph found by `FindPage`, no other ink in the picture area), the fault-free check after a repair.
- `PageOf(loaded, glyphs, map)`: the TV page expected from the display file with a glyph mapping `map(code) -> 8 rows` (`Bw80ExpectedPage` takes the glyphs as they are; the Puzzles' pictures are the right codes through a wrong chain, so the page is built from the display file's codes with the fault's mapping: a row permutation for the line counter faults, a code change for the latch faults). If `Bw80ExpectedPage` cannot take a mapping, the rig gets `Bw80ExpectedPageMapped(loaded, glyphs, map)` beside it, with `Bw80ExpectedPage` calling it with the identity.
- `Cell(frame, left, top, row, column)`: a cell's 8 × 8 dots read off a frame at the page position `FindPage` gives, `kColumnsPerDot` as the `K` case computes it.
- `Toggles(sim, net, 10 µs)`: changes of a Net over 10 µs in 50 ns steps (BW-2001 `no_clock`'s loop), for the dead Puzzles' clues; Nets found by pin (`PinNet`): on a damaged Board Nets are named from the Copper.

Times (release, 0.25×): the `K` screen takes about 0.5 s simulated (2 s wall); the picture Puzzles' typed line 19 presses, about 5 s simulated with the ROM's work and the frames (about 20 s wall) plus the repair's `K` screen; a keyboard Puzzle's line 1.5–2 s simulated; a dead Puzzle on three seeds 3 × 0.3 s; about 4 min for the file.

## 2. The Puzzles, file by file

Each Puzzle: `puzzles/<file>.bwp` (comment line with the cause, `machine bw80`, `title`, `brief` the symptom only), one `TEST_CASE("bw80: <file>", "[machines][slow]")`, `SKIP` in debug.

Cracks: pads from the Board's routing (`boards/bw80/bw80.kicad_pcb`'s `segment`s, read with a script counting the segments at each pad: a pad touched by one segment is an end of its Trace). `LC2` and `ROM_CS1` are two-pad Nets. `KB2` runs `IC10.12` – `R15.1` – `J3.11` with `R15.1` the junction, so a crack between `IC10.A5` and `R15.1` cuts `IC10` alone (checked: `J3.KB2` keeps `R15`'s pull-up). `A13` runs `J3.6` – `IC1.3` – `J4.26` with `IC1.3` the junction: a crack between `J3.A13` and `IC1.A13` cuts `J3` alone (probed: `J3.A13` floats, `IC1.A13` and `J4.A13` pulse). `KBD_N` has `IC17.11`, `IC10.1` and `IC11.13` at ends and a junction via near `IC17` and `IC11` with a branch to the `IC10` pads: a crack at 90 % between `IC17.4Y` and `IC11.4B` lies past the via and cuts `IC11.13` alone (probed: `IC10.G1_N` and `G2_N` pulse, `IC11.4B` floats); at 10 % it lies before it and cuts `IC17` off from all. Cached cracks in `tests/machine/test_points.{hpp,cpp}`: `BottomHalvesCrack`, `EightKeysCrack`, `NoPrintCrack`, `RollingScreenCrack`, `VerticalStripesCrack`.

The ZX80's codes (LISTING, `tests/machines/bw80_screen.cpp`): space `$00`, `"` `$01`, digits `$1C`–`$25`, letters `$26`–`$3F` (`K` `$30`, `Q` `$36`), the inverse cursor `$B0`; `$76` ends a row. The matrix (`src/parts/zx80_keys.cpp`): column `KB2` holds X, D, E, 3, 8, I, K, M; row `A13` holds P, O, I, U, Y; PRINT is the O key in `K` mode.

| # | File | Fault line | Case: the symptom | Case: the repair |
|---|---|---|---|---|
| 1 | `q_everywhere` | `fault stuck IC16.3B 1` | `10 PRINT "HELLO 0123"` then `LIST` typed; the display file's row 0 right; on a locked frame row 0's cells match their glyphs and the cells of row 5 (an empty row) and past the text in row 0 match the `Q` glyph (`$36`) | `swap("IC16")`: `KScreen` |
| 2 | `doubled_lines` | `fault stuck IC21.QB 0` | the line typed; row 0's cells match their glyphs with rows `0 0 2 2 4 4 6 6` | `swap("IC21")`: `KScreen` |
| 3 | `bottom_halves` | `fault crack IC21.QD IC8.4A 50%` | the line typed; row 0's cells match rows `4 5 6 7 4 5 6 7`; `IC8.4A`'s Net is not `IC21.QD`'s | mend: `KScreen` |
| 4 | `wrong_letters` | `fault bridge IC5.15 IC5.16` | the line typed; the display file's row 0 as `Bw80DisplayRows` gives it (the listing, `10 PRINT "HELLO 0123"` after the line number's padding); row 0's cells match the glyphs of `code & ~6 | (bits 1 and 2 both set ? 6 : 0)` over those codes (the TV reads `;< LRILT "DCLLK <;23"`) | `removeBridge({"IC5","15"},{"IC5","16"})`: `KScreen` |
| 5 | `quotes_for_spaces` | `fault joint IC5.6` | the line typed; row 0's cells match the glyphs of `code | 1` over the display file's codes (the TV reads `""11"PRJNT""HFLLP"1133"`: the padding's spaces as `"`); the cursor's cell (row 23) matches `$B1`; `*PinNet(IC5, 3Q) != *PinNet(IC8, 1A)` | `resolder({"IC5","6"})`: `KScreen` |
| 6 | `plain_cursor` | `fault stuck IC12.2Y 0` | at 500 ms a locked frame: the plain `K` glyph (`$30`) found where the inverse one is not; no other ink in the picture area | `swap("IC12")`: `KScreen` |
| 7 | `blank_page` | `fault stuck IC9.CLK 1` | the line typed; the display file's row 0 right; on a locked frame the ink in the picture area is one cell's worth at most (the cursor's square: 64 dots, allow 100) and more than 0 | `swap("IC9")`: `KScreen` |
| 8 | `eight_keys` | `fault crack IC10.A5 R15.1 50%` | after the `K` screen `PRINT 12345` typed: row 0 `1245`, row 23 `0/-2`; `J3.KB2`'s Net is `R15.1`'s and not `IC10.A5`'s | mend: `PRINT 12345` gives `12345` |
| 9 | `same_eight_keys` | `fault stuck IC10.A5 1` | as 8; the Net at `IC10.A5`'s leg is `J3.KB2`'s (the stuck pin reads its own value, the Net is whole) | `swap("IC10")`: `12345` |
| 10 | `no_print` | `fault crack J3.A13 IC1.A13 50%` | `PRINT 2+2` typed: row 0 not `4` (the O key is dead: `2+2` sits in the edit line with the error marker); `J3.A13`'s Net is not `IC1.A13`'s | mend: `PRINT 2+2` gives `4` and `0/-2` |
| 11 | `rolling_screen` | `fault crack IC17.4Y IC11.4B 90%` | the display file's row 23 holds the cursor by 500 ms; `isLocked()` false at every 20 ms over the next second; `IC10.G1_N`'s Net is `IC17.4Y`'s and `IC11.4B`'s is not | mend: `KScreen` |
| 12 | `torn_picture` | `fault stuck IC18.1Q_N 1` | the display file's row 23 holds the cursor; `isLocked()` false over a second | `swap("IC18")`: `KScreen` |
| 13 | `still_starting` | `fault column IC3.IO4 0` | seeds 1–3: at 700 ms `isLocked()` false and the frame's `InkCount` over 2000 (the power-on pattern; the fault-free `K` screen locks by 450 ms); `IC3`'s `Ram2114::word(a) & 8` is 0 at every `a` (`IO4` is bit 3 of the 4-bit word, bit 2 of the byte through `Bw80Ram`) | `swap("IC3")`: `KScreen` |
| 14 | `vertical_stripes` | `fault crack IC6.2Y IC2.CS1 50%` | seeds 1–3: at 300 ms `isLocked()` false; `Toggles` of `IC6.2Y`'s Net over 10 µs above 0, of `IC2.CS1`'s 0 (it floats high) | mend: `KScreen` |
| 15 | `white_screen` | `fault stuck IC14.3Y 0` | seeds 1–3: at 300 ms `isLocked()` false and the frame's `InkCount` under 100; `Toggles` of `IC1.D3`'s Net 0 and of `IC2.O3`'s above 0 | `swap("IC14")`: `KScreen` |

The typed line of the picture Puzzles is `10 PRINT "HELLO 0123"` NEW LINE `LIST` NEW LINE (19 presses, about 3.4 s plus the ROM's work), so row 0 holds a line with a digit, a keyword, a string and spaces. A `PRINT` after a report loses its first key (the ROM takes it to leave the report), so a case types one command from a fresh `K` screen; the repair cases run from power-on again.

Briefs say what the owner sees with no cause and no reference (ADD_PUZZLE); the two keyboard ones name the eight keys as the keyboard shows them.

Seeds: the picture and keyboard Puzzles are deterministic by construction (the display chain, the matrix), and each is run on seeds 2 and 3 with `bw run --tv` before its brief is written (the plan's notes say so); the dead ones are checked on seeds 1–3 by their cases.

## 3. Machine docs

- [BW80.md](../machines/140-bw80/BW80.md) `## Puzzles`: the candidate list replaced by the fifteen in four groups, a line each, as BW2001.md. When it works: PRINT on O, LET on K (phase 3, with `manuals/bw80/user.md`).
- [BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md) `## Puzzles`: file, Fault, symptom, repair; the note on the crack positions (the `KBD_N` 90 %, the `KB2` and `A13` junctions).
- [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md): what this step found (a bridge needs two pins of one chip; reading a Trace's ends from the segments; a fault that is seed-dependent in detail is out even when every seed misbehaves; the ZX80's first key after a report).

## 4. Release check

Each phase: `ctest --preset debug -j 12 -R "bw80|manuals|puzzle"` (the Puzzle cases skip) and `ctest --preset release -j 12 -R "bw80"` (they run). No shared code changes, so the full debug suite only at the end. `bw_ui puzzles/<file>.bwp --screenshot … --after 4s` for the power-on ones (`--after` is wall time: about 0.9 s simulated at 0.23×); the typed ones `--type J3:500ms:"..."` with `--after 25s` (19 presses are 3.4 s simulated, about 15 s wall), or `bw run --tv` with `--type`, quicker; each viewed against its brief.

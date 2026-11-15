# P5r BW-2001 Puzzles — Detailed Design

Implements [BW2001_PUZZLES.md](BW2001_PUZZLES.md): fifteen Puzzles on the BW-2001, their tests, the Machine docs. No product code.

Recipes: [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md) (design rules, the `.bwp` file, Fault syntax, tests, docs). Machine: [BW2001.md](../machines/130-bw2001/BW2001.md), [BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md). Programs: `roms/bw2001_edit.asm` (`putch`, the retrace waits), `roms/bw2001_kernel.asm` (reset, the RAM sizing, the self-test and its rows). The user manual (`manuals/bw2001/user.md`) already has everything the briefs lean on (banner, bytes free, cursor, snow, the self-test by `DIAG`): no user manual change unless a phase finds a gap; one service manual note (§3).

---

## 1. The test rig

The Machine cases' helpers are in `tests/machines/bw2001_test.cpp`'s anonymous namespace. As P5m did for the BW-1 (`bw1_rig`):

- `tests/machines/bw2001_rig.{hpp,cpp}` (new): `NetlistPath`, `BoardCircuit`, `SnowWatch`, `Running`, `ScreenRows`, `Type`, `kBanner`, moved from `bw2001_test.cpp` unchanged but for two points:
  - `Running` gets a constructor from a `Machine` (`machine.build()`, `REQUIRE`d), beside the one from a circuit;
  - `SnowWatch`'s three Nets found by pin, not by name: `SEL8` `UE5.4Y`, `VIDEO_ON` `UC6.3Y`, `DIS_ON` `UC5.2Q`: on a damaged Board Nets are named from the Copper (ADD_PUZZLE Tests).
- `bw2001_test.cpp` includes it; `tests/CMakeLists.txt` adds `machines/bw2001_rig.cpp` and `machines/bw2001_puzzles_test.cpp`.
- In `bw2001_puzzles_test.cpp`: `LoadPuzzle(name)` as `bw1_puzzles_test.cpp`; `SelfTest(Machine&, seed)`: `machine.addWire({"J2", "DIAG"}, {"J2", "1"})` (the user manual's wire), build, `removeWire` again (a `Machine` cannot be copied), run to 1500 ms (the fault-free self-test reaches `KEY` by then, P5q), return `ScreenRows` (the video RAM, which the self-test writes; the TV is checked where the picture is the symptom). `Banner(Running&)`: `Bw2001Typist::waitForLine(1'500'000)` and `rows() == kBanner`, the fault-free check after a repair.
- The Puzzle list is a `std::array<const char*, N>` (`N` grows each phase; Phase 1 starts with the two debug cases and no Puzzle, so it starts in Phase 2). `bw2001: every Puzzle builds` checks `warnings` empty except, for `wrong_chip`, the one naming `UH8` `2A4` unconnected (the dry pin is on no Net: `electrical.cpp`, `builder.cpp` `warnUnconnectedInputs`). In debug each Board build takes several seconds: 15 builds about 1–2 min; measured in Phase 4, split into one case a group if over 2 min.
- Every case but the first two starts with `#ifndef NDEBUG SKIP(...)` (BW2001_DESIGN Tests).

Times (release, 0.24×): a case's power-on to the banner is 0.4 s simulated, about 2 s wall with the Board build; the self-test 1.5 s simulated (about 7 s). Most cases 10–20 s; `snow` (a 30-line loop faulted and repaired, a self-test) and `missing_keys`, `frozen_cursor` (typing) about a minute. About 6 min for the file.

## 2. The Puzzles, file by file

Each Puzzle: `puzzles/<file>.bwp` (ADD_PUZZLE File: comment line with the cause, `machine bw2001`, `title`, `brief` the symptom only), one `TEST_CASE("bw2001: <file>", "[machines][slow]")`, its name in the title-and-brief list. Each Fault was tried on the Board with `bw run --tv` (seed 1) while writing the spec; a phase measures what the case checks and writes it into the brief and the case.

Cracks: pads read from the Board's routing (`boards/bw2001/bw2001.kicad_pcb`'s `segment`s): `CD3` and `OSC8` are two-pad Nets; `VERT_DRIVE` runs `J7.3` – `UD8.11` – `J2.19`, so a crack on the first stretch cuts `J7` alone; `KIN0` runs `J5.11` – `UG8.10` – `R18.2`, so a crack on the first stretch cuts `J5` alone and leaves the PIA its pull-up (a crack on the second leaves `PB0` without one: a MOS input reading X, ghost keys, the first trial). A cached `const Crack& <Name>Crack()` in `tests/machine/test_points.{hpp,cpp}` for each (ADD_PUZZLE Tests).

| # | File | Fault line | Case: the symptom | Case: the repair |
|---|---|---|---|---|
| 1 | `odd_e` | `fault cell UA2.O<b> $2<x> <v>`: `E` is screen code 5, ROM words `$28`–`$2F` (line 0 first, `O7` the leftmost dot); the dot chosen in the phase so it shows on the `bw_ui` screenshot (a dot that breaks the stem or a bar, not one at an edge of a bar) | the video RAM holds the banner; on the TV every `E` cell (`Bw2001CellDots`) is the glyph of `E` with that one dot changed, every other cell matches | `swap("UA2")`: `Bw2001TvRows` is the banner |
| 2 | `striped_screen` | `fault crack UA2.O3 UB2.D 50%` | the video RAM holds the banner; every cell of the TV's text window but the cursor's (drawn reversed in one phase) has the same dot lit on all 8 lines (spaces included) | mend: `Bw2001TvRows` is the banner |
| 3 | `rolling_text` | `fault crack J7.VDRIVE UD8.4Y N%` (on the first stretch) | the video RAM holds the banner; `isLocked()` false over 1 s | mend: locked within 3 frames, the TV rows the banner |
| 4 | `stutter` | `fault stuck UD2.3Y 0` (`SA0`) | the TV's first row reads `**  WW2200  IIYYBBSSCC****` (measured), each pair of cells drawn alike | `swap("UD2")`: the banner |
| 5 | `negative_screen` | `fault stuck UC1.1Q 1` (`REV_D`: both `REV` and `REV_N` follow; `REV` alone stuck changes nothing, tried) | every blank cell of the window all dots lit (`0xFF` × 8), the text cells the inverse glyphs (`Bw2001TvRows` reads them as their characters: the banner) | `swap("UC1")`: blank cells dark |
| 6 | `short_memory` | `fault column UI5.IO1 0` (`$0C00`–`$0FFF`; the sizing writes `$55`, bit 0 set, and stops at `$0C00`) | the banner with `2046 BYTES FREE`; self-test row 4 `RAM BAD 0C00 01` (the RAM test's pattern `low ^ page ^ $A5` is `$A9` at `$0C00`: `IO1` and `IO4` fail at `$0C00`, `IO2`, `IO3` at `$0C02`, `$0C04`) | `swap("UI5")`: `7166 BYTES FREE` |
| 7 | `missing_keys` | `fault crack J5.COL0 UG8.PB0 N%` (on the first stretch) | after the banner, `Type(m, "QWERTYASDF")`: the cursor's row reads `ERTYDF` | mend: `QWERTYASDF` |
| 8 | `frozen_cursor` | `fault stuck UG8.CB1 0` | the banner; the cursor's cell the same in two frames 1/3 s apart (the fault-free case: inverse in one, plain in the other, P5q); `Type(m, "PRINT 2+2\r")` leaves the screen unchanged | `swap("UG8")`: the cursor blinks, `PRINT 2+2` shows ` 4` (P5q's typing case) |
| 9 | `random_screen` | `fault stuck UDE9.RST_N 0` | at 1 s the video RAM unchanged since 100 ms (nothing writes it) and not the banner; `RES_N` low; seeds 1–3; self-test rows too: no title | `swap("UDE9")`: the banner |
| 10 | `snow` | `fault cell UH3.O5 $067 0` (`$E067`, `and #$20` → `and #$00`: `putch` stops waiting; the clear, scroll and cursor waits are other code and still wait) | `SnowWatch` over `Type(m, "FOR I=1 TO 30:PRINT I:NEXT\r")` and its run: `accesses` well above 0 (the killer poke case: over 100), the printed rows right; self-test row `ROM E000 BAD E0` (`$FF`-`$DF`… measured) | `swap("UH3")`: `accesses == 0` over the same loop |
| 11 | `bad_zero_page` | `fault column UI8.IO1 1` | no banner by 1.5 s; the picture on seeds 1–3 checked as the brief says (with bit 0 stuck the sizing pointer walks odd addresses past `$2000`, through the open bus and the video RAM: the picture is measured, not assumed); self-test `ZERO PAGE BAD 0001 01` (the trial) | `swap("UI8")`: the banner |
| 12 | `wrong_chip` | `fault joint UH8.2A4` (`UH8` pin 17, the read half's input on `BBD0`; it floats high) | no banner, the picture on seeds 1–3 as 11's; self-test `ZERO PAGE BAD 0001 01` (the trial: the same as 11); after `swap("UI8")` the same code | `resolder({"UH8", "2A4"})`: the banner |
| 13 | `ghost_letter` | `fault cell UC3.IO<b> $<w> <v>`: a word in the banner's first row and a bit that changes its character (`*** BW-2001…`, screen codes `$2A`, `$02`, `$17`…), not in the self-test's rows 2–4 (it writes its report there) | the banner with that one character wrong on the TV and in the video RAM; after `CLR` (SHIFT HOME) and typing over the place, still wrong; self-test `VIDEO RAM BAD 80<w> <bits>` (the self-test's patterns; measured) | `swap("UC3")`: the banner |
| 14 | `no_clock` | `fault crack X1.OUT UE2.2B 50%` | `isLocked()` false; `CLK8` (by `UB2.CLK`) never changes over 1 ms; `X1.OUT` does; seeds 1–3 (`TV_VIDEO` freezes in a power-on state: the brief says no picture, not a colour) | mend: the banner |
| 15 | `joined_bits` | `fault bridge UF3.D0 UF3.D1` | no banner by 1.5 s, seeds 1–3; the self-test's title never appears | `removeBridge`: the banner |

Briefs say what the owner sees with no cause and no reference (ADD_PUZZLE): for example `snow`: "The BW-2001 works, but white specks flicker over the text whenever it prints, as if someone had typed the old POKE." A Puzzle whose symptom turns out to depend on the seed beyond what a brief can say is changed or dropped and the plan says why.

## 3. Machine docs

- `manuals/bw2001/service.md` `## Notes`: `missing_keys` depends on `J5`'s column order and the `R18`–`R25` order, assumed in the transcription (BW2001_DESIGN Netlist: listed only if a Puzzle depends on them): a note that they are assumed.
- [BW2001.md](../machines/130-bw2001/BW2001.md) `## Puzzles`: the list by group, a line each (title, what the owner sees), replacing the P5q placeholder.
- [BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md): a `## Puzzles` section: file, Fault, symptom, repair, as measured (ADD_PUZZLE Docs), and the screenshots taken.

## 4. Release check

The fence for each phase is the BW-2001 tests in both builds: `ctest --preset debug -j 12 -R "bw2001|manuals|puzzle"` (the Puzzle cases skip) and `ctest --preset release -j 12 -R "bw2001"` (they run). The full debug suite once at the end (a roadmap step's end). `bw_ui puzzles/<file>.bwp --screenshot /tmp/x.png --after 1s` for each (`--after 3s` and `--type` for the typing ones; `--repair wire:J2.DIAG:J2.1` for the self-test ones), viewed against its brief.

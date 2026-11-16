# P5r BW-2001 Puzzles — Detailed Design

Implements [BW2001_PUZZLES.md](BW2001_PUZZLES.md): sixteen Puzzles on the BW-2001, their tests, the Machine docs, and the read-only stuck pin Snow needs.

Recipes: [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md) (design rules, the `.bwp` file, Fault syntax, tests, docs). Machine: [BW2001.md](../machines/130-bw2001/BW2001.md), [BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md). Programs: `roms/bw2001_edit.asm` (the retrace waits: `bit VIA` / `lda VIA; and #$20` before each screen store), `roms/bw2001_kernel.asm` (reset, the RAM sizing, the self-test and its rows). The user manual (`manuals/bw2001/user.md`) already has everything the briefs lean on (banner, bytes free, cursor, snow, the self-test by `DIAG`): no manual change.

---

## 1. A stuck pin read only: `fault stuck REF.PIN 0|1 input`

A `stuck` Fault on an output ties its Net; on an input-only pin the chip reads the value through a Net of its own and the board's Net is unchanged. A port pin (6520, 6522, 6530: `Bidirectional` or `PullUp`) is a driver, so plain `stuck` ties its Net. `input` gives such a pin the input-only behaviour: a port whose input has failed.

- `src/machine/puzzle.{hpp,cpp}`: `StuckAtDecl::input`; the parser takes an optional trailing `input` (`expected: fault stuck REF.PIN 0|1 [input]`).
- `src/machine/machine.cpp`: `input` only on a Chip pin its chip reads (`IsReader`), else a load error.
- `src/netlist/builder.{hpp,cpp}`: `StuckPin::input`; with it the board's Net is not tied and the pin goes on its own Net (tied `Supply` for a bidirectional or pull-up pin, as before): the chip reads the value and drives nothing.
- Tests: `builder: a stuck input on a port pin leaves its net alone` (a 6530 `PA0` on a Net a 74LS04 drives low: the Net stays 0, the chip's own Net 1), `puzzle: a stuck port pin read only`, the parser's error text.

## 2. The test rig

- `tests/machines/bw2001_rig.{hpp,cpp}`: `Bw2001NetlistPath`, `Bw2001BoardCircuit`, `Bw2001Built(const Machine&)`, `SnowWatch`, `Running`, `Bw2001ScreenRows`, `Bw2001Type`, `kBw2001Banner`, moved out of `bw2001_test.cpp`. `SnowWatch` finds its Nets by pin (`SEL8` `UE5.4Y`, `VIDEO_ON` `UC6.3Y`, `DIS_ON` `UC5.2Q`): on a damaged Board Nets are named from the Copper.
- `tests/machines/bw2001_puzzles_test.cpp`: `LoadPuzzle`; `SelfTest(Machine&, seed)`: a wire from `J2` `DIAG` to `J2` pad 1 (the user manual's), build, the wire removed again, run to 1500 ms, the video RAM's rows; `Banner(Running&)`: the banner and `READY` by 1.5 s, the check after a repair; `Run`, `Row`, `Frame`, `Marks` (cells not spaces), `CheckBlankDead`.
- `kPuzzles` (`std::array`) feeds `bw2001: Puzzles have a title and a brief` and `bw2001: every Puzzle builds` (warnings empty, but for `wrong_chip`'s one naming `UH8` `2A4`: its dry pin is on no Net). Both run in debug (the builds about 25 s); every other case starts with `#ifndef NDEBUG SKIP(...)`.
- Snow counts from the banner: before reset ends (1 ms) the 6502's power-on address may select the screen for a moment.

Times (release): 2–37 s a case, about 3 min for the file.

## 3. The Puzzles, file by file

Each Puzzle: `puzzles/<file>.bwp` (comment line with the cause, `machine bw2001`, `title`, `brief` the symptom only), one `TEST_CASE("bw2001: <file>", "[machines][slow]")`.

Cracks: pads from the Board's routing (`boards/bw2001/bw2001.kicad_pcb`'s `segment`s): `CD3` and `OSC8` are two-pad Nets; `VERT_DRIVE` runs `J7.3` – `UD8.11` – `J2.19`, so a crack on the first stretch cuts `J7` alone; `KIN0` runs `J5.11` – `UG8.10` – `R18.2`, so a crack on the first stretch cuts `J5` alone and leaves the PIA its pull-up (on the second stretch `PB0` loses its pull-up: a MOS input reading X, ghost keys). Cached cracks in `tests/machine/test_points.{hpp,cpp}`: `StripedScreenCrack`, `RollingTextCrack`, `MissingKeysCrack`, `NoClockCrack`.

| # | File | Fault line | Case: the symptom | Case: the repair |
|---|---|---|---|---|
| 1 | `odd_e` | `fault cell UA2.O2 $2A 1` (`E` is screen code 5, ROM words `$28`–`$2F`, `O7` the leftmost dot; a lit dot in empty space shows on the TV, a gap in a stroke does not) | the video RAM holds the banner; every `E` cell on the TV is the glyph with bit 2 of line 2 set, every other cell matches | `swap("UA2")`: the TV rows the banner |
| 2 | `striped_screen` | `fault crack UA2.O3 UB2.D 50%` | the video RAM holds the banner; every cell but the cursor's has bit 3 (`0x08`) lit on all 8 lines | mend: the TV rows the banner |
| 3 | `rolling_text` | `fault crack J7.VDRIVE UD8.4Y 50%` | the video RAM holds the banner; `isLocked()` false over 1 s | mend: locked, the TV rows the banner |
| 4 | `stutter` | `fault stuck UD2.3Y 0` (`SA0`) | the TV's first row `**  WW2200  IIYYBBSSCC****`; each pair of cells drawn alike (the odd video RAM cells are never written) | `swap("UD2")`: the banner |
| 5 | `negative_screen` | `fault stuck UC1.1Q 1` (`REV_D`: `REV` and `REV_N` follow; `REV` alone stuck changes nothing) | a blank cell all dots lit, `B` drawn as the reversed glyph; the TV rows read as the banner | `swap("UC1")`: blank cells dark |
| 6 | `short_memory` | `fault column UI5.IO1 0` (the sizing writes `$55`, bit 0 set: it stops at `$0C00`) | the banner with `2046 BYTES FREE`; self-test row 4 `RAM BAD 0C00 01` | `swap("UI5")`: the banner |
| 7 | `missing_keys` | `fault crack J5.COL0 UG8.PB0 50%` | after the banner `QWERTYASDF` typed: row 5 `ERTYDF` | mend: `QWERTYASDF` |
| 8 | `frozen_cursor` | `fault stuck UG8.CB1 0` | the banner; 50 ms later (the editor puts the cursor out once itself) the cursor's cell unchanged over 1 s; `PRINT 2+2` leaves the screen the banner | `swap("UG8")`: the cursor blinks, `PRINT 2+2` shows `4` |
| 9 | `random_screen` | `fault stuck UDE9.RST_N 0` | seeds 1–3: the video RAM the same at 100 ms and 1 s, not the banner; `RES_N` low; the self-test's title never appears | `swap("UDE9")`: the banner |
| 10 | `snow` | `fault stuck UA5.PB5 0 input` (§1) | the snow watch over a 30-line loop from the banner: over 1000 screen accesses in the display; the rows right; `VIDEO_ON` defined at `UA5`'s pin | `swap("UA5")`: 0 accesses over the same loop |
| 11 | `bad_zero_page` | `fault column UI8.IO1 1` | seeds 1–3: by 1.5 s the screen cleared but for 1–99 cells, not the banner; self-test row 2 `ZERO PAGE BAD 0001 01` | `swap("UI8")`: the banner |
| 12 | `wrong_chip` | `fault joint UH8.2A4` (`UH8` pin 17, the read half's input on `BBD0`; it floats high) | as 11, seeds 1–3, the same self-test code, and the same again after `swap("UI8")` | `resolder({"UH8", "2A4"})`: the banner |
| 13 | `ghost_letter` | `fault cell UC3.IO2 $00C 1` (row 0, away from the self-test's report rows 2–4) | the banner's first row `*** BW-2001 VINY BASIC ***` in the video RAM and on the TV; self-test title `BW-2001 SELF/TEST`, row 3 `VIDEO RAM BAD 800C 02` | `swap("UC3")`: the banner |
| 14 | `garbled_text` | `fault column UC3.IO1 1` | every video RAM cell with bit 0 set; the TV's first row `+++!CW-3111!UIOY!CASIC!+++!!!!!!!!!!!!!!`, all 25 rows non-empty; the self-test's row 3 starts `WIEEO!SAM!CAE!` | `swap("UC3")`: the banner |
| 15 | `no_clock` | `fault crack X1.OUT UE2.2B 50%` | seeds 1–3: `isLocked()` false, the frame black; `CLK8` (at `UB2.CLK`) still over 10 µs while `X1.OUT` toggles | mend: the banner |
| 16 | `joined_bits` | `fault bridge UF3.D0 UF3.D1` | seeds 1–3: the video RAM the same at 100 ms and 1 s, over 500 cells not spaces; `RES_N` high; the self-test's title never appears | `removeBridge`: the banner |

Briefs say what the owner sees with no cause and no reference (ADD_PUZZLE).

## 4. Machine docs

- [BW2001.md](../machines/130-bw2001/BW2001.md) `## Puzzles`: the list by group, a line each.
- [BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md) `## Puzzles`: file, Fault, symptom, repair; the note that `missing_keys` depends on `J5`'s column order and the `R18`–`R25` order, which the transcription assumed (the service manual allows only its Components section).

## 5. Release check

Each phase: `ctest --preset debug -j 12 -R "bw2001|manuals|puzzle"` (the Puzzle cases skip) and `ctest --preset release -j 12 -R "bw2001"` (they run). The full debug suite at the end, and after §1 (shared code: the builder). `bw_ui puzzles/<file>.bwp --screenshot … --after 4s` for each (`--after` is wall time: about 0.5 s simulated in 3.5 s), `--type` for the typing ones, `--repair wire:J2.DIAG:J2.1` for the self-test ones, viewed against its brief.

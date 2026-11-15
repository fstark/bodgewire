# P5r — BW-2001 Puzzles

Roadmap step P5r ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): Machine 130 BW-2001 ([BW2001.md](../machines/130-bw2001/BW2001.md)) gets its Puzzles, on our firmware (P5p), each a different link. No new product code: Puzzle files, tests, Machine docs.

The BW-2001 is two machines on one board: the video (counters, video RAM, character ROM, shift register) draws the screen on its own, and the 6502 shares the video RAM with it. So a fault shows in the picture, in what the computer does, in both, or in neither but the self-test. The user manual already documents what the Puzzles lean on: the banner and its `7166 BYTES FREE`, the blinking cursor, the keyboard, snow (the killer poke), the self-test by `DIAG` on the user port and its rows.

## Decided (2026-11-15, with the user)

- **About 15 Puzzles.** Each test runs a second or more simulated at 0.24×, so fewer than the BW-1's nineteen.
- **Four kinds**: faults seen in the picture; computer faults seen in use (bytes free, keys, cursor, a screen that never clears); snow; the self-test naming the place, and dead machines only the instruments find.
- **Tests release-only.** Debug runs at about 0.004× (the banner alone takes about 100 s under ASan): every Puzzle case `SKIP`s in debug, as P5q's long cases; debug keeps `bw2001: Puzzles have a title and a brief` and a case that loads and builds each Puzzle.
- **Ghost letter by a cell, not a column** (found in review, 2026-11-15): a stuck video RAM column also garbles the self-test's own rows (spaces read `!`, its address and bits read back from the screen), so the self-test could not name it.
- **Snow by a ROM cell, not a stuck `PB5`** (chosen as "VIA `PB5` stuck"; found while asking, 2026-11-15): a `stuck` Fault on a 6522 port pin drives the Net (`VIDEO_ON` held low: a black picture, no interrupt), and a crack leaves `PB5` reading high through the port's pull-up (the editor waits forever: a screen that never clears, as Random screen). The link the player follows is the same, the editor's wait for the retrace: one bit of the wait's `and #$20` in the editor ROM `UH3` stuck 0 makes it `and #$00`, so the editor writes at once, as after the killer poke. The self-test's ROM row names `UH3` too.

## The Puzzles

As built (2026-11-15): all fifteen kept; the tables below are as planned. Changed when run: Odd E's dot is a lit one in the glyph's upper half (`$2A` `O2`), a gap in the stem cannot be seen; Snow's cell is in the scroll's first loop (`$E0D5`): `putch`'s wait gave only a few specks, and the brief says "whenever the screen scrolls"; Bad zero page and Wrong chip clear the screen and stop, a blank screen with a few stray characters on seeds 1–3, not the random one. Symptoms as measured: [BW2001.md](../machines/130-bw2001/BW2001.md#puzzles) and the plan's notes.

Faults tried on the Board's circuit before writing this (`bw run … --tv`, seed 1); the codes and counts marked "measured" are to be confirmed by the tests, the others are from the trials.

**A. The picture**

| # | File | Title | Fault | The owner sees | Repair |
|---|---|---|---|---|---|
| 1 | `odd_e` | Odd E | a cell of the character ROM `UA2` in the glyph of `E` | every `E` drawn with a dot wrong, every other letter right | swap `UA2` |
| 2 | `striped_screen` | Striped screen | crack on `CD3` (`UA2` → `UB2`, a two-pad Net): `UB2`'s `D` input floats high | a vertical line through every cell, spaces too: the text window striped | mend |
| 3 | `rolling_text` | Rolling text | crack on `VERT_DRIVE` between `UD8` and `J7` (`J7.3` is an end of the Trace) | the monitor never locks: the picture drifts up the screen | mend |
| 4 | `stutter` | Stutter | `UD2` output `SA0` stuck 0 | each character twice, every other one lost: `**  WW2200  IIYY…` | swap `UD2` |
| 5 | `negative_screen` | Negative | `UC1` output `REV_D` stuck 1 | the whole text window reversed: black letters on white | swap `UC1` |

**B. The computer, seen in use**

| # | File | Title | Fault | The owner sees | Repair |
|---|---|---|---|---|---|
| 6 | `short_memory` | Short memory | `UI5` (`$0C00`, the low nibble) column `IO1` (bit 0) stuck 0 | `2046 BYTES FREE` instead of 7166; self-test `RAM BAD 0C00 01` (the pattern there is `$A9`) | swap `UI5` |
| 7 | `missing_keys` | Missing keys | crack on `KIN0` between `J5` and `UG8` (`J5.11` is an end; `UG8` keeps `R18`'s pull-up) | Q, W, A, S, Z, X, `!`, `"`, the left SHIFT and RVS never type | mend |
| 8 | `frozen_cursor` | Frozen cursor | `UG8` input `CB1` stuck 0 (the retrace interrupt) | the banner, then a cursor that never blinks and keys that do nothing | swap `UG8` |
| 9 | `random_screen` | Random screen | `UDE9` output `RST_N` stuck 0 | the random characters of power-on stay forever; the self-test cannot start either | swap `UDE9` |
| 10 | `snow` | Snow | `UH3` cell `$067` bit 5 stuck 0 (`putch`'s `and #$20`) | white specks over the text while BASIC prints, as the killer poke; self-test `ROM E000 BAD E0` (measured) | swap `UH3` |

**C. The self-test names the place**

| # | File | Title | Fault | The owner sees | Repair |
|---|---|---|---|---|---|
| 11 | `bad_zero_page` | Bad zero page | `UI8` (`$0000`) column `IO1` (bit 0) stuck 1 | no banner; the picture as measured on seeds 1–3 (random characters in the trial); self-test `ZERO PAGE BAD 0001 01` | swap `UI8` (the standard RAM Puzzle) |
| 12 | `wrong_chip` | Wrong chip | dry Joint on `UH8.2A4`, the RAM buffer's read input of `BBD0` (floats high: bit 0 of every RAM read 1) | as Bad zero page, the same code; a `UI8` swap repairs nothing | resolder `UH8.2A4` |
| 13 | `ghost_letter` | Ghost letter | a cell of the video RAM `UC3` stuck, at a screen place the banner writes (the word and bit chosen so the banner's character there changes) | one place of the screen always shows the wrong character, whatever is written or scrolled there; self-test `VIDEO RAM BAD 80xx bb` (measured) | swap `UC3` |

**D. Dead, the self-test no help: the instruments**

| # | File | Title | Fault | The owner sees | Repair |
|---|---|---|---|---|---|
| 14 | `no_clock` | No clock | crack on `OSC8` (`X1` → `UE2`, a two-pad Net): `UE2`'s input floats high, `CLK8` still | no picture, the monitor unlocked (what the frozen video line shows measured on seeds 1–3); the probe finds `OSC8` at `X1` and nothing past the crack | mend |
| 15 | `joined_bits` | Joined bits | solder bridge `UF3.D0`–`UF3.D1` (pins 33, 32) | random characters forever, strapped or not | cut the bridge |

Links covered: a ROM cell (character and program), a RAM column (program RAM, zero page), a video RAM cell, stuck outputs (`UD2`, `UC1`, `UDE9`), a stuck input (`UG8`), four cracks (`CD3`, `VERT_DRIVE`, `KIN0`, `OSC8`), a dry Joint, a bridge. Random screen, Bad zero page, Wrong chip and Joined bits all look alike at power-on; the self-test tells Bad zero page and Wrong chip from the other two, the probe on `RES_N` tells Random screen from Joined bits.

## Tests

`tests/machines/bw2001_puzzles_test.cpp`, release-only but the first two:

- `bw2001: Puzzles have a title and a brief` over the fifteen; `bw2001: every Puzzle builds` (load, `build()`, no warnings), both in debug.
- One case a Puzzle (ADD_PUZZLE Tests): the symptom read as the player reads it, from the TV (the screen reader, `isLocked`), the video RAM, the self-test's rows by `DIAG` (a Bodge Wire from `J2` `DIAG` to `J2` pad 1), the snow watch; then the repair and the banner back (or the self-test past its failure). Wrong chip also checks that a `UI8` swap does not repair it.
- Seeds: 1 for every case; the dead Puzzles (9, 11, 12, 14, 15) also on seeds 2 and 3, their picture checked as the brief describes it (the brief says what every seed shows).
- `bw2001: every Puzzle builds` allows one warning: `wrong_chip`'s dry Joint leaves `UH8.2A4` on no Net, which the builder reports as an unconnected input (the Joint is the Fault).
- Release: `bw_ui` screenshots of each Puzzle's symptom as the brief describes it, seed 1.

Design: [BW2001_PUZZLES_DESIGN.md](BW2001_PUZZLES_DESIGN.md). Plan: [BW2001_PUZZLES_PLAN.md](BW2001_PUZZLES_PLAN.md). Machine: [BW2001.md](../machines/130-bw2001/BW2001.md), [BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md).

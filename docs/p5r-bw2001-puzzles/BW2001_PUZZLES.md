# P5r — BW-2001 Puzzles

Roadmap step P5r ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): Machine 130 BW-2001 ([BW2001.md](../machines/130-bw2001/BW2001.md)) gets its Puzzles, on our firmware (P5p), each a different link. Puzzle files, tests, Machine docs, and one product change: a stuck Fault that a chip reads while its Net is left alone (`fault stuck REF.PIN 0|1 input`), for Snow.

The BW-2001 is two machines on one board: the video (counters, video RAM, character ROM, shift register) draws the screen on its own, and the 6502 shares the video RAM with it. So a fault shows in the picture, in what the computer does, in both, or in neither but the self-test. The user manual already documents what the Puzzles lean on: the banner and its `7166 BYTES FREE`, the blinking cursor, the keyboard, snow (the killer poke), the self-test by `DIAG` on the user port and its rows.

## Decided (with the user)

- **Sixteen Puzzles** (about fifteen asked, 2026-11-15; Garbled text added 2026-11-16). Each test runs a second or more simulated at 0.24×, so fewer than the BW-1's nineteen.
- **Four kinds** (2026-11-15): faults seen in the picture; computer faults seen in use (bytes free, keys, cursor, a screen that never clears); snow; the self-test naming the place, and dead machines only the instruments find.
- **Tests release-only** (2026-11-15). Debug runs at about 0.004× (the banner alone takes about 100 s under ASan): every Puzzle case `SKIP`s in debug, as P5q's long cases; debug keeps `bw2001: Puzzles have a title and a brief` and a case that loads and builds each Puzzle.
- **Snow on the VIA's retrace input** (2026-11-16). The beam's position reaches the editor through VIA `PB5`, so Snow's fault is there, not in the program that reads it. Plain `stuck` on a port pin forces its Net (`VIDEO_ON` held: a black picture, no interrupt) and a crack leaves `PB5` reading high (the editor waits forever), so a new form: `fault stuck REF.PIN 0|1 input`, the chip reads the value through a Net of its own, the board's Net is left alone (a port pin whose input has failed).
- **Garbled text and Ghost letter both** (2026-11-16): a video RAM column (the whole screen one character along, the self-test's own report garbled too: the garbling is the clue) and a single video RAM cell the self-test names; they show different letters (`CASIC`, `VINY`).
- **Odd E's single dot is fine** (2026-11-16), though small on the TV.

## The Puzzles

As built and measured (seed 1; the dead ones on seeds 1–3). Fault lines: [BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md#puzzles).

**A. The picture**

| # | File | Title | Fault | The owner sees | Repair |
|---|---|---|---|---|---|
| 1 | `odd_e` | Odd E | a cell of the character ROM `UA2` in the glyph of `E` (word `$2A`, bit 2 stuck 1) | every `E` with a stray dot in its upper half, every other letter right | swap `UA2` |
| 2 | `striped_screen` | Striped screen | crack on `CD3` (`UA2` → `UB2`, a two-pad Net): `UB2`'s `D` input floats high | a vertical line through every cell, spaces too: the text window striped | mend |
| 3 | `rolling_text` | Rolling text | crack on `VERT_DRIVE` between `UD8` and `J7` (`J7.3` is an end of the Trace) | the monitor never locks: the picture drifts | mend |
| 4 | `stutter` | Stutter | `UD2` output `SA0` stuck 0 | each character twice, every other one lost: `**  WW2200  IIYYBBSSCC****` | swap `UD2` |
| 5 | `negative_screen` | Negative | `UC1` output `REV_D` stuck 1 | the whole text window reversed: black letters on white | swap `UC1` |

**B. The computer, seen in use**

| # | File | Title | Fault | The owner sees | Repair |
|---|---|---|---|---|---|
| 6 | `short_memory` | Short memory | `UI5` (`$0C00`, the low nibble) column `IO1` (bit 0) stuck 0 | `2046 BYTES FREE` instead of 7166; self-test `RAM BAD 0C00 01` | swap `UI5` |
| 7 | `missing_keys` | Missing keys | crack on `KIN0` between `J5` and `UG8` (`J5.11` is an end; `UG8` keeps `R18`'s pull-up) | Q, W, A, S, Z, X, `!`, `"`, the left SHIFT and RVS never type | mend |
| 8 | `frozen_cursor` | Frozen cursor | `UG8` input `CB1` stuck 0 (the retrace interrupt) | the banner, then a cursor that never blinks and keys that do nothing | swap `UG8` |
| 9 | `random_screen` | Random screen | `UDE9` output `RST_N` stuck 0 | the random characters of power-on stay forever; the self-test cannot start either | swap `UDE9` |
| 10 | `snow` | Snow | VIA `UA5` reads `PB5` as 0 (`stuck … input`): the beam always outside the text; `VIDEO_ON` itself fine | white specks all over the text whenever it prints or scrolls, as after the killer poke | swap `UA5` |

**C. The self-test names the place**

| # | File | Title | Fault | The owner sees | Repair |
|---|---|---|---|---|---|
| 11 | `bad_zero_page` | Bad zero page | `UI8` (`$0000`) column `IO1` (bit 0) stuck 1 | the screen cleared, a few stray characters, no banner; self-test `ZERO PAGE BAD 0001 01` | swap `UI8` (the standard RAM Puzzle) |
| 12 | `wrong_chip` | Wrong chip | dry Joint on `UH8.2A4`, the RAM buffer's read input of `BBD0` (floats high: bit 0 of every RAM read 1) | as Bad zero page, the same code; a `UI8` swap repairs nothing | resolder `UH8.2A4` |
| 13 | `ghost_letter` | Ghost letter | a cell of the video RAM `UC3` (word `$00C`, bit 1 stuck 1) | the banner reads `VINY BASIC`; that place often shows a wrong letter; self-test `VIDEO RAM BAD 800C 02` | swap `UC3` |
| 14 | `garbled_text` | Garbled text | video RAM `UC3` column `IO1` (bit 0) stuck 1 | every character one along (`+++!CW-3111!UIOY!CASIC!+++`), the empty screen full of `!`; the self-test's report garbled the same way (`WIEEO!SAM!CAE!…`) | swap `UC3` |

**D. Dead, the self-test no help: the instruments**

| # | File | Title | Fault | The owner sees | Repair |
|---|---|---|---|---|---|
| 15 | `no_clock` | No clock | crack on `OSC8` (`X1` → `UE2`, a two-pad Net): `UE2`'s input floats high, `CLK8` still | no picture, the monitor unlocked; the probe finds `OSC8` at `X1` and nothing past the crack | mend |
| 16 | `joined_bits` | Joined bits | solder bridge `UF3.D0`–`UF3.D1` (pins 33, 32) | the random characters of power-on forever, strapped or not; `RES_N` high | cut the bridge |

Links covered: a character ROM cell, RAM columns (program RAM, zero page, video RAM), a video RAM cell, stuck outputs (`UD2`, `UC1`, `UDE9`), stuck inputs (`UG8`, and `UA5`'s port pin read only), four cracks (`CD3`, `VERT_DRIVE`, `KIN0`, `OSC8`), a dry Joint, a bridge. Random screen and Joined bits look alike; the probe on `RES_N` tells them apart. Bad zero page and Wrong chip look alike and give the same self-test code; the leg and pad of `UH8` pin 17 tell them apart.

## Tests

`tests/machines/bw2001_puzzles_test.cpp`, release-only but the first two; helpers in `tests/machines/bw2001_rig.{hpp,cpp}`.

- `bw2001: Puzzles have a title and a brief` over the sixteen; `bw2001: every Puzzle builds` (load, `build()`, no warnings but `wrong_chip`'s: its dry pin is on no Net, which the builder reports as an unconnected input), both in debug.
- One case a Puzzle (ADD_PUZZLE Tests): the symptom read as the player reads it, from the TV (the screen reader, `isLocked`), the video RAM, the self-test's rows by `DIAG` (a Bodge Wire from `J2` `DIAG` to `J2` pad 1), the snow watch (screen accesses while the beam draws the text); then the repair and the banner back. Wrong chip also checks that a `UI8` swap does not repair it.
- Seeds: 1 for every case; the dead Puzzles (9, 11, 12, 15, 16) also on seeds 2 and 3.
- `fault stuck … input`: `builder: a stuck input on a port pin leaves its net alone`, `puzzle: a stuck port pin read only`.
- Release: `bw_ui` screenshots of each Puzzle's symptom as the brief describes it, seed 1.

Design: [BW2001_PUZZLES_DESIGN.md](BW2001_PUZZLES_DESIGN.md). Plan: [BW2001_PUZZLES_PLAN.md](BW2001_PUZZLES_PLAN.md). Machine: [BW2001.md](../machines/130-bw2001/BW2001.md), [BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md).

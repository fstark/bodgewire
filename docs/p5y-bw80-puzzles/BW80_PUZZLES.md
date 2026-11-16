# P5y — BW-80 Puzzles

Roadmap step P5y ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): Machine 140 BW-80 ([BW80.md](../machines/140-bw80/BW80.md)) gets its Puzzles, on the ZX80's own ROM, each a different link. Puzzle files, tests, Machine docs; no new code.

The BW-80 is one machine: the CPU draws the picture itself, so a fault in the display chain shows in the picture while the computer keeps working behind it, and a fault on the CPU's buses leaves the TV with nothing to lock on. The user manual already documents what the briefs lean on: the picture after a moment of nothing, the inverse `K` cursor, keyword entry (`PRINT 2+2` gives `4` and `0/-2`), the screen going blank while the BW-80 is busy, the keyboard window.

## Decided (with the user)

- **About fourteen Puzzles** (2026-11-16): fifteen as built, enough for every link once (stuck output, stuck input, crack, bridge, dry Joint, RAM column) across the four kinds; a release case costs 2–20 s at 0.25× (a typed line is about 5 s simulated).
- **Four kinds** (2026-11-16): the picture (the video chain misbehaving, not dying), the keyboard in use, the sync, and dead machines the instruments find. The user asked for faults that make the video misbehave rather than go black, for a fault that leaves the power-on pattern on the screen for good (Still starting), and for the margins' garbage to take part if a fault can bring it onto the screen (Q everywhere fills them too).
- **A typed command is fine** (2026-11-16): a brief may say what `PRINT 2+2` or a listing shows; no Puzzle needs the player's own program.
- **Tests release-only** (2026-11-16), as the BW-2001's: debug runs the BW-80 at about 0.004×; debug keeps `bw80: Puzzles have a title and a brief` and `bw80: every Puzzle builds`.

Found by the docs review (2026-11-16): the user manual and BW80.md put PRINT on the P key and LET on O; the ROM, the key table and the typing case put PRINT on O and LET on K (P has no keyword). Both are fixed in this step, since the keyboard briefs name keys as the manual does.

Found while trying candidates (not the user's): the NOP gates give nothing usable (a gate stuck low kills the CPU, a gate's input stuck leaves `$08`'s `EX AF,AF'` running harmlessly or a seed-dependent mess), `R28` and `R18` cut off show nothing (a TTL input floats high either way), a line counter bridge to the unused `QA` changes nothing, and the NOP condition ignoring `MD6` draws the cursor in the top right corner on seeds 2 and 3 but not on seed 1: out, as seed-dependent.

## The Puzzles

As built and measured (seed 1, seeds 2 and 3 as a robustness check; the dead ones checked on seeds 1–3 by their cases). Fault lines: [BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md#puzzles).

**A. The picture** (the TV locks, the display file in RAM is right, the picture is not)

| # | File | Title | Fault | The owner sees | Repair |
|---|---|---|---|---|---|
| 1 | `q_everywhere` | Q everywhere | `IC16` input `3B` (`CH6_N`) stuck 1: the load gate ignores bit 6, so each row's `HALT` (`$76`) loads a glyph too | every empty place on the screen filled with `Q`, the margins full of junk, the typed text readable over it | swap `IC16` |
| 2 | `doubled_lines` | Doubled lines | `IC21` output `QB` (`LC0`) stuck 0 | every letter made of its even rows, each drawn twice: squat, striped letters | swap `IC21` |
| 3 | `bottom_halves` | Bottom halves | crack on `LC2` (`IC21.QD` → `IC8.4A`, a two-pad Net): the multiplexer's input floats high | the top half of every letter replaced by its bottom half | mend |
| 4 | `wrong_letters` | Wrong letters | solder bridge `IC5` pins 15–16 (`CH1`–`CH2`): where the two bits differ both read 0 | the letters typed are not the letters shown (`PRINT` reads `LRILT`, `HELLO` `DCLLK`, `10` `;<`, spaces stay spaces); the display file holds the right codes | cut the bridge |
| 5 | `quotes_for_spaces` | Quotes for spaces | dry Joint on `IC5` pin 6 (`CH0`): `IC8` reads it high | every space shows as a `"`, every even code as the next one (`0` as `1`, `E` as `F`, `O` as `P`); the cursor's letter changes too (`$B0` to `$B1`, an inverse `L`) | resolder `IC5` pin 6 |
| 6 | `plain_cursor` | Plain cursor | `IC12` output `2Y` (`INV`) stuck 0 | the cursor is a plain `K`, not white on black; everything else right | swap `IC12` |
| 7 | `blank_page` | Blank page | `IC9` input `CLK` stuck 1: the shift register never shifts | the TV locks on a white page with one black square where the cursor is; what is typed never shows, though the BW-80 takes it (`PRINT 2+2` leaves the square where the report goes) | swap `IC9` |

**B. The keyboard, seen in use**

| # | File | Title | Fault | The owner sees | Repair |
|---|---|---|---|---|---|
| 8 | `eight_keys` | Eight keys | crack on `KB2` between `IC10` pin 12 and `R15` (`IC10.12` is an end of the Trace): `IC10`'s input floats high | X, D, E, 3, 8, I, K and M never type (`PRINT 12345` shows `1245`); the rest works | mend |
| 9 | `same_eight_keys` | The same eight keys | `IC10` input `A5` stuck 1 | the same eight keys dead; `KB2` reads the key press at the chip's leg, the chip ignores it | swap `IC10` |
| 10 | `no_print` | No PRINT | crack on `A13` between `J3` pin 6 (an end of the Trace) and `IC1`: the keyboard's row floats | P, O, I, U and Y do nothing: PRINT, INPUT, IF, REM and the symbols on those keys (`*`, `)`, `(`, `$`, `"`) cannot be typed; the other keys work | mend |

**C. The sync**

| # | File | Title | Fault | The owner sees | Repair |
|---|---|---|---|---|---|
| 11 | `rolling_screen` | Rolling screen | crack on `KBD_N` just before `IC11` pin 13 (an end of the Trace): the vertical sync latch is never reset | the picture rolls and never settles; the keyboard still works behind it | mend |
| 12 | `torn_picture` | Torn picture | `IC18` output `1Q_N` (`SYNC_ARM_N`) stuck 1: no horizontal sync pulse is ever armed | the picture torn into slanting streaks, never locked | swap `IC18` |

**D. Dead, found with the instruments**

| # | File | Title | Fault | The owner sees | Repair |
|---|---|---|---|---|---|
| 13 | `still_starting` | Still starting | `IC3` column `IO4` (bit 2) stuck 0 | the pattern the TV shows for a moment at power-on never goes away (the ROM clears its RAM for ever); the keyboard does nothing | swap `IC3` |
| 14 | `vertical_stripes` | Vertical stripes | crack on `ROM_CS1` (`IC6.2Y` → `IC2.CS1`, a two-pad Net): the ROM is never selected | fine vertical stripes over the whole screen, never locked; `ROM_CS1` toggles at `IC6` pin 7 and sits high at `IC2` pin 20 | mend |
| 15 | `white_screen` | White screen | `IC14` output `3Y` stuck 0: `D3` held low on the CPU's side of the bus | a white screen, never locked; `D3` still at `IC1` while `MD3` toggles across `R5` | swap `IC14` |

Links covered: stuck inputs (`IC16`, `IC9`, `IC10`), stuck outputs (`IC21`, `IC12`, `IC18`, `IC14`'s open-collector gate), five cracks (`LC2`, `KB2`, `A13`, `KBD_N`, `ROM_CS1`), a bridge, a dry Joint, a RAM column. Eight keys and The same eight keys look alike; a mend finds nothing on the second, and `IC10` pin 12's leg reads the pressed key. Blank page and White screen look alike at a glance; the TV locks on the first and not on the second, and the first still takes keys.

## Tests

`tests/machines/bw80_puzzles_test.cpp`, release-only but the first two; helpers in `tests/machines/bw80_rig.{hpp,cpp}` (moved out of `bw80_test.cpp`).

- `bw80: Puzzles have a title and a brief` over the fifteen; `bw80: every Puzzle builds` (load, `build()`, no warnings but `quotes_for_spaces`'s: its dry pin is on no Net), both in debug.
- One case a Puzzle (ADD_PUZZLE Tests): the symptom read as the player reads it, from the TV (a locked frame against the page the display file predicts, with the fault's own glyph mapping where the picture is wrong) and from the display file in the two 2114s (typing through `J3`); the dead ones on seeds 1–3 with the probe's clue (`ROM_CS1` and `D3` at both ends); then the repair and the `K` screen or the typed answer back. The same eight keys also checks the leg.
- Release: `bw_ui` screenshots of each Puzzle's symptom as the brief describes it, seed 1.

Design: [BW80_PUZZLES_DESIGN.md](BW80_PUZZLES_DESIGN.md). Plan: [BW80_PUZZLES_PLAN.md](BW80_PUZZLES_PLAN.md). Machine: [BW80.md](../machines/140-bw80/BW80.md), [BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md).

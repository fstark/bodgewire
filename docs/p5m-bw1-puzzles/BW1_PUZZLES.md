# P5m — BW-1 Puzzles

Roadmap step P5m ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), the last of four building the BW-1 on the KIM-1's own design (P5j chips, P5k monitor, P5l Machine, P5m Puzzles). This step gives Machine 120 BW-1 ([BW1.md](../machines/120-bw1/BW1.md)) its Puzzles, and its service manual the self-test the Puzzles lean on. No new product code: Puzzle files, manuals, tests.

The BW-1 is the most complex Machine so far, and its Puzzles show it: a fault can show in normal use, only in the self-test, or kill both the monitor and the self-test so that only the instruments find it.

## Decided (2026-11-15, with the user)

- **Many Puzzles.** The Machine is complex; a long list is fine, each Puzzle a different link.
- **Showing a fault may take an action the manual documents.** Running the self-test is no different from pressing the dice's button to roll: "everything works, but the self-test fails" is a fair Puzzle. So a RIOT RAM cell nothing else reads, or a ROM cell the monitor never runs, makes a Puzzle.
- **No Puzzle needs the player's own program.** A bad cell only a typed program hits is either caught by the self-test (a repeat) or not (unfair).
- **Keep a standard RAM Puzzle**: one 6102 bad, found by its bit, repaired by a swap. Beside it, Puzzles where the self-test points at RAM and a RAM swap does not repair it (the swap reflex, [GAME_DESIGN.md](../GAME_DESIGN.md)): the self-test says where to look, not what to replace.
- **Both matrix cracks**: a column (keys of three rows) and a row (seven keys); the only Puzzles where a group of keys dies.
- **Ghosting is not a Puzzle** (it needs two keys held and the idea to try it); the service manual may mention it.
- **Service manual: a Self-test section**: how to strap, what it shows while it runs, how to read a failure (`123408`: test 1, `$0234`, bit 3; the test numbers and which chip each tests). **User manual: one line** pointing to it when the BW-1 will not start or misbehaves.

## The Puzzles

Symptoms are predictions from the circuit until their phase runs them; only Dead machine is seen (seed 1, dark digits). A Puzzle whose fault shows nothing, or shows differently by seed where the brief cannot cover it, is changed or dropped and the plan says why (ADD_PUZZLE design rules).

**A. Seen in normal use**

| # | Name | Fault | The owner sees | Repair |
|---|---|---|---|---|
| 1 | Bad byte | cell in `U5` (bit 7) at a word the monitor does not use | a byte written with DA reads back with bit 7 changed; self-test `1xxx80` | swap `U5` |
| 2 | Odd seven | 002 ROM cell in `TABLE` (`$1FE7`) at the glyph of `7` (word `$3EE`), a segment bit `D0`–`D6` | every `7` drawn wrong, other digits right; self-test `4C00xx` | swap `U2` |
| 3 | Missing bar | `U17` output to `SEG_G` stuck 0 | the middle bar dark on every digit | swap `U17` |
| 4 | Half a segment | crack partway along a `SEG_x` Trace | one segment dark on the digits past the crack | mend |
| 5 | Bright mess | `U24` output to `DIG3` stuck low | `DS3` lit on every scan: every digit's segments at once | swap `U24` |
| 6 | DA does nothing | crack on the `PA3` column between `U17` and the keys | 3, A and DA dead | mend |
| 7 | AD and GO dead | crack on `ROW2` | E, F, AD, DA, `+`, GO, PC dead (or those past the crack, AD and GO among them) | mend |
| 8 | Deaf keypad | a matrix key stuck closed | `0200 xx`, no key does anything; self-test `PA55` and that key's code with no key held | swap the key |

**B. The self-test names the place**

| # | Name | Fault | The owner sees | Repair |
|---|---|---|---|---|
| 9 | Self-test fails | cell in `U3`'s RAM | works; the self-test stops on `37xxbb` | swap `U3` |
| 10 | No stop | cell in `U2`'s RAM at `NMIV` (`$17FA`) stuck 1 | ST and SST do not stop into the monitor, `BRK` does; self-test `27FAbb` | swap `U2` |
| 11 | Dead machine | `U9` `DOUT` column stuck 0 | dark digits; self-test `110008` (`$0100`: `$00` at `$0000` reads right) | swap `U9` (the standard RAM Puzzle) |
| 12 | Wrong suspect | crack on `DOUT3` between `U9` and `U13` | dead; self-test `100008`: bit 3 as in Dead machine, but a `U9` swap repairs nothing | mend |
| 13 | Half a byte | crack on `BUF_N` cutting `U13`'s four enables off (they float high) | dead; self-test bad bits within `0F` | mend |
| 14 | Two bits | a bridge joining two data bus pins (`D2`–`D3` on `U1`): the only neighbouring pins of two bits | the ROM fetches break too: dead, the self-test too; probably group C | cut the bridge |
| 15 | Aliased page | `U7` `A8` stuck inside the chip | pages alias in bit 5: self-test `100020` | swap `U7` |
| 16 | ROM test fails | 003 ROM cell in bytes the self-test never runs | works; the self-test stops on `5800xx`; the cell in the `$FF` fill after `.checksum` (words `$375`–`$3FF`), stuck 0 | swap `U3` |

**C. Dead, the self-test no help: the instruments**

| # | Name | Fault | The owner sees | Repair |
|---|---|---|---|---|
| 17 | Wrong block | `U4` input `A` (`A10`) stuck | dead, strapped or not: stuck 0, `K5` and `K7` never decode (no ports, no monitor ROM); stuck 1, `K6` never does (the strap's jump lands in `U2`) | swap `U4` |
| 18 | Held reset | RS (`SW23`) stuck closed | dark; `RES_N` low | swap `SW23` |
| 19 | SST kills it | `U15` input on `K7` stuck 1 | works until SST is at ON, then dies | swap `U15` |

A Puzzle may move between groups when run (a fault on the shared data bus also stops the self-test, which runs from ROM over that bus).

## Tests

- `bw1: Puzzles have a title and a brief` over every BW-1 Puzzle.
- One case a Puzzle (ADD_PUZZLE Tests): the symptom on the real Parts, read as the player reads it (the digits, the self-test's code through a strapped Board, a Net stuck or floating), on seeds 1–3 where the monitor's symptom depends on the RAMs' power-on (the self-test writes before it reads: its codes on seed 1); then the repair and the fault-free behaviour back: the monitor's `0200`, or for a self-test Puzzle the self-test past the failing test (`888888` at about 300 ms, after tests 1–5 pass). Only Deaf keypad runs to `PA55`.
- Wrong suspect also checks that the `U9` swap does not repair it.
- Release: `bw_ui` screenshots of each Puzzle's symptom as the brief describes it, seed 1.

Design: [BW1_PUZZLES_DESIGN.md](BW1_PUZZLES_DESIGN.md). Plan: [BW1_PUZZLES_PLAN.md](BW1_PUZZLES_PLAN.md). Machine: [BW1.md](../machines/120-bw1/BW1.md), [BW1_DESIGN.md](../machines/120-bw1/BW1_DESIGN.md).

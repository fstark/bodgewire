# 030 — Dice

Third rung of the tutorial ladder. Seven LEDs in the pattern of a die face; hold the button to roll, let go to stop on a face from 1 to 6. Its code comes with roadmap step P4d ([PUSH_BUTTON.md](../../p4d-push-button/PUSH_BUTTON.md)).

## Lesson

A counter's value is turned into something readable by decoding: each LED group lights for its own set of values, made from a few gates. A fault in one decoder gate shows on some faces only; a fault in the counter shows on all of them. The input is the player's own button, so it is part of the circuit to check too.

## Circuit

- `X1` 1 MHz `OSC` → net `CLK`.
- `SW1` push button between `BTN_N` and GND; `R9` pulls `BTN_N` up. Pressed, `BTN_N` is low.
- `U4` 74LS02 gate 1: `ROLL_CLK` = NOR(`CLK`, `BTN_N`). It follows the inverted clock while `SW1` is pressed and stays low otherwise.
- `U1` 74LS161 clocked by `ROLL_CLK`: counts 1 to 6, bits `N0`–`N2` (`QA`–`QC`); `QD` and `RCO` unused.
  - `ENP`, `ENT` high; `CLR_N` on `RESET_N` pulled up by `R1`.
  - Load value 1 (`A` high, `B`–`D` at 0 V); `LOAD_N` on `SIX_N`: at 6, the next clock loads 1.
- Decoder, each output active low (low = its LEDs lit):

  | Net | Gate | Low for | LEDs |
  |---|---|---|---|
  | `ODD_N` | `U2` 74LS04: NOT `N0` | 1, 3, 5 | `D4` (centre) |
  | `TWO_N` | `U4` 74LS02 gate 2: NOR(`N1`, `N2`) | 2–6 | `D1`, `D7` (one diagonal) |
  | `FOUR_N` | `U2` 74LS04: NOT `N2` | 4–6 | `D2`, `D6` (other diagonal) |
  | `SIX_N` | `U3` 74LS00: NAND(`N1`, `N2`) | 6 | `D3`, `D5` (middle row) |

- `D1`–`D7` red LEDs laid out as a die face (`D1` `D2` top, `D3` `D4` `D5` middle, `D6` `D7` bottom), wired as in the counter: +5 V → `R2`…`R8` → `Dn` anode (net `LED_An`) → cathode on its decoder Net.
- `J1` power connector.

## When it works

- While `SW1` is held, `ROLL_CLK` pulses at 1 MHz and `N0`–`N2` run 1, 2, … 6, 1 every 6 µs. All seven LEDs glow at once, at different brightness: the middle row (`SIX_N`, lit 1/6 of the time) dimmest, the first diagonal (`TWO_N`, 5/6) brightest.
- On release the count stops: the LEDs show one face, 1 to 6, until the next press. Each face is equally likely.
- Before the first roll, the counter holds its power-on value: a face, or all seven LEDs (value 7), or none (value 0). The first press clears this up (7 loads 1 on the first clock; 0 counts on to 1).

## Puzzles

| Puzzle | Fault | Symptom | Path to the fix |
|---|---|---|---|
| Dead button | Broken Trace on `BTN_N` between `SW1` and `R9` | rolling does nothing: the face never changes | `BTN_N` goes low at `SW1` when pressed, stays high at `U4` pin 3: mend the Trace |
| Loaded dice | Broken Trace on `N2` between `U2` pin 3 and `U4` pin 6 (`U2` and `U3` lose `N2`) | only 3 and 6: a 1 shows as the other diagonal's 3, and `U1` loads 1 again at 2, which lights six LEDs | `N2` is low at `U4` pin 6 but floats at `U2` pin 3 and `U3` pin 2: mend the Trace |
| Seven | `U3` output (`SIX_N`) stuck at 1 | never a 6; some rolls show no LED; 4 and 5 come up twice as often (the counter runs 0–15 without the load) | `N1` and `N2` are both high at times, but `SIX_N` never goes low: swap `U3` |
| Odd dice | `U1.QA` (`N0`) stuck at 1 | only 1, 3 and 5, or all seven LEDs | `N0` never goes low while `N1` and `N2` count: swap `U1` |

Netlist, Board, Puzzle files and manuals: [DICE_DESIGN.md](DICE_DESIGN.md).

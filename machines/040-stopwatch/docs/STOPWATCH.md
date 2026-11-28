# 040 — Stopwatch

Fourth rung of the tutorial ladder. Three 7-segment digits show seconds, tenths and hundredths, from 0.00 to 9.99; Start, Stop and Reset buttons. Its code comes with roadmap step P4f ([SEVEN_SEGMENT.md](../../../docs/p4f-seven-segment/SEVEN_SEGMENT.md)).

## Lesson

A number on a display goes through three stages: a counter holds it in binary, a decoder chip turns it into segments, the digit lights them. Each stage fails in its own way: a counter fault changes which numbers come up, a decoder fault draws wrong shapes, a digit or segment fault leaves bars dark. The chips are read from their datasheets, not from habit: the 74LS90 takes +5 V on pin 5, not pin 14, and counts to ten only through an outside link from `QA` to `CKB`.

## Circuit

- `X1` 1 MHz `OSC` → net `CLK`.
- Buttons, each between its signal and GND, pulled up by a 4.7 kΩ resistor; pressed, the signal is low:
  - `SW1` Start: `START_N` (`R1`); `SW2` Stop: `STOP_N` (`R2`); `SW3` Reset: `RST_N` (`R3`).
- Run flip-flop `U11` 74LS74, first half used only through its asynchronous inputs: `START_N` on `1PRE_N` sets `RUN` (`1Q`), `STOP_N` on `1CLR_N` clears it. `1D`, `1CLK` and the second half are tied off.
- `U12` 74LS00:
  - gate 1: `GCLK` = NAND(`CLK`, `RUN`): the inverted clock while running, high while stopped;
  - gate 2: `RESET` = NAND(`RST_N`, `RST_N`): high while `SW3` is pressed;
  - gates 3 and 4 unused.
- Seven 74LS90 decade counters, each wired as a ÷10 (`QA` → `CKB`), `R01`/`R02` on `RESET`, `R91`/`R92` at 0 V. Each counts on the falling edge at its `CKA`; its `QD` falls once every ten counts and clocks the next.
  - Prescaler `U1`–`U4`: `GCLK` → `U1` → … → `U4`: `F100K`, `F10K`, `F1K`, `F100` (their `QD`); `F500K`, `F50K`, `F5K`, `F500` are the `QA` → `CKB` links.
  - Display counters: `F100` → `U7` hundredths (`H0`–`H3`) → `U6` tenths (`T0`–`T3`) → `U5` seconds (`S0`–`S3`). `S3` goes nowhere: after 9.99 comes 0.00.
- Decoders, one 74LS47 per digit (BCD to 7-segment, open-collector outputs, low = segment lit): `U8` for `S0`–`S3`, `U9` for `T0`–`T3`, `U10` for `H0`–`H3`. `LT_N`, `RBI_N` and `BI_N` at +5 V.
- Digits `DS1` (seconds), `DS2` (tenths), `DS3` (hundredths), left to right: red 0.56-inch common-anode 7-segment digits. Each digit's two anode pins share one Net, `DSn_CA`, fed from +5 V through its own 330 Ω resistor (`R4`, `R5`, `R6`). Each segment pin sits straight on its decoder output (`DSn_A` … `DSn_G`). `DS1`'s decimal point is on 0 V, always lit, so the display reads `0.00`; the other two decimal points are unconnected.
- `J1` power connector.

## When it works

- Reset: while `SW3` is held, every counter is cleared and the display shows `0.00`.
- Start: the display counts up by 0.01 every 10 ms. The hundredths digit changes too fast to read and looks like a blur of segments; the tenths flicker; the seconds are steady. After 9.99 it goes on from 0.00.
- Stop: the count freezes until Start. Start continues from where it stopped. Pressing Start while running, or Stop while stopped, does nothing. Reset while running clears the count and it counts on from 0.00 when released.
- The 74LS47 draws 6 and 9 without their tails (no top bar on the 6, no bottom bar on the 9).
- At power-on the counters and the run flip-flop hold random values: the display may show any number, odd shapes (counts 10 to 15 the 74LS47 draws as fragments, or blank), and may already be counting. Reset and Stop bring it to 0.00.

## Puzzles

| Puzzle | Fault | Symptom | Path to the fix |
|---|---|---|---|
| Stuck at zero | `U12` gate 2 output (`RESET`) stuck at 1 | the display shows 0.00 whatever the buttons do | `RUN` follows Start and Stop, `GCLK` pulses, but `RESET` is high at every counter's `R01`/`R02` with `SW3` released: swap `U12` |
| Nineteen | Broken Trace on `T0` between `U6` pin 12 (`QA`) and `U6` pin 1 (`CKB`) | after Reset the watch reads 0.10; it counts 0.10 to 0.19, then starts again from 0.10; the seconds never move | `T0` toggles at `U6` pin 12, but floats (reads high) at `U6` pin 1 and `U9` pin 7: mend the Trace |
| Eights | `U9` output `SG_N` (`DS2_G`) stuck at 0 | after Reset the watch reads 0.80; the tenths show 8 for 0, and odd shapes for 1 and 7 | `DS2`'s middle bar is lit whatever `T0`–`T3` say; `DS2_G` is low for 0, which the datasheet draws without `g`: swap `U9` |
| Broken bar | Broken Trace on `DS1_E` between `U8` pin 9 and `DS1` pin 1 | the seconds draw wrong shapes: 0 with a gap at the bottom left, 6 like a 5, 8 like a 9 | `DS1_E` goes low at `U8` pin 9 for 0, but floats at `DS1` pin 1: mend the Trace |
| Non-stop | Broken Trace on `RUN` between `U11` pin 5 and `U12` pin 2 | Stop does nothing: the watch always counts; Reset brings it back to 0.00, and it counts on | `RUN` goes low at `U11` pin 5 after Stop, but floats (reads high) at `U12` pin 2: mend the Trace |
| Dark seconds | Broken Trace on `VCC` that cuts `U5` pin 5 off | the seconds digit stays dark but for its decimal point; tenths and hundredths count normally | `S0`–`S3` float at `U5` and `U8` reads 15 (blank); `U5` has no +5 V on pin 5, its VCC pin by the datasheet: mend the Trace |
| Missing eight | `U8` input `D` (`S3`) stuck at 0 inside the chip | the seconds never show 8 or 9: after 7 the digit shows 0 and 1, then 0 again | `S3` goes high on `U8` pin 6, leg and pad, at 8 s, but `DS1` shows 0: the decoder ignores a good input: swap `U8` |
| Second opinion | Two Faults: Dark seconds' broken `VCC` Trace at `U5` pin 5 and Broken bar's broken `DS1_E` Trace at `DS1` pin 1 | as Dark seconds; once `VCC` is mended, as Broken bar | `U5` has no +5 V: mend it; the seconds now come back with a gap in the 0, the masked Fault: `DS1_E` floats at `DS1` pin 1: mend it too. The first double Fault: the brief warns it may be trickier than it looks |

Netlist, Board, Puzzle files and manuals: [STOPWATCH_DESIGN.md](STOPWATCH_DESIGN.md).

# Knob Meter — Service Manual

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `CLK`
- `J1` power connector: +5 V and 0 V for the whole board
- `J2` paddle controller connector: `TRIG_N` to the controller, its pulse back on `PULSE`, 30 µs at the left stop to 9.93 ms at the right stop; the controller runs on the board's +5 V
- `R1` digit resistor: feeds the anodes of `DS1` (`DS1_CA`) and limits their current
- `R2` digit resistor: feeds the anodes of `DS2` (`DS2_CA`) and limits their current
- `U1` timebase: divides `CLK` (1 MHz) by ten into `F100K` (100 kHz)
- `U2` timebase: divides `F100K` by ten into `F10K` (10 kHz), the count clock
- `U3` timebase: divides `F10K` by ten into `F1K` (1 kHz)
- `U4` timebase: divides `F1K` by ten into `F100` (100 Hz)
- `U5` measuring cycle: steps 0 to 9 on `F100`, one step every 10 ms; `TRIG_N` falls at the step from 9 to 0; `TB1` and `TB2` are both high in steps 6 and 7 and clear the counters
- `U6` gates: `GCLK` is `F10K` let through while `PULSE` is high; `LATCH` rises when `PULSE` ends
- `U7` tens counter: counts 0 to 9 on `C3` into `C4`–`C7`; cleared by `TB1` and `TB2`
- `U8` units counter: counts 0 to 9 on `GCLK` into `C0`–`C3`, carries to `U7` on `C3`; cleared by `TB1` and `TB2`
- `U9` tens latch: takes `C4`–`C7` into `L4`–`L7` when `LATCH` rises
- `U10` units latch: takes `C0`–`C3` into `L0`–`L3` when `LATCH` rises
- `U11` tens decoder: `L4`–`L7` to the segments of `DS1`
- `U12` units decoder: `L0`–`L3` to the segments of `DS2`
- `DS1` tens digit: a segment lights while its `U11` output is low
- `DS2` units digit: a segment lights while its `U12` output is low

## Test points

What the logic probe shows on a working board. **P** is the pulse lamp: it stays lit while the signal keeps changing. The board measures once every tenth of a second, so the slow signals below change too fast to watch, but P stays lit on them. Readings that depend on the knob are given with the knob at 47 %. Click a pin below with the probe in hand to probe it.

| Probe | Net | A working board shows |
|---|---|---|
| `U1.CKA` | `CLK` | P lit |
| `U2.QD` | `F10K` | L, with P lit: the count clock, high one fifth of the time |
| `U4.QD` | `F100` | L, with P lit |
| `J2` pin 3 | `VCC` | H, steady. Without it the controller is dead |
| `J2` pin 4 | `GND` | L, steady |
| `U5.QD`, or `J2` pin 1 | `TRIG_N` | mostly L, with P lit. It is H for 20 ms of every 100 ms. When it falls, the controller starts a pulse |
| `J2` pin 2, or `U6.1B` | `PULSE` | mostly L, with P lit. It is H for the first few milliseconds of each round: longer the further the knob is turned. At the left stop the pulse is too short for the H lamp |
| `U6.1Y` | `GCLK` | H, with P lit. At the left stop: H, steady, P dark (nothing to count) |
| `U6.2Y` | `LATCH` | H, with P lit. It is L while `PULSE` is H |
| `U5.QB`, `U5.QC` | `TB1`, `TB2` | P lit on both. Both are H together for 20 ms of each round: that clears the counters |
| `U8.QA` | `C0` | P lit. At the left stop: L, steady |
| `U8.QD` | `C3` | P lit once the reading reaches 8: it is the carry to the tens. Below that: L, steady |
| `U10.1Q`, `U10.2Q`, `U10.3Q`, `U10.4Q` | `L0`–`L3` | steady while the knob is still: the units digit in binary, `U10.1Q` the lowest bit. At 47 %: H, H, H, L (7) |
| `U9.1Q`, `U9.2Q`, `U9.3Q`, `U9.4Q` | `L4`–`L7` | the same for the tens digit. At 47 %: L, L, H, L (4) |
| `U12.SA_N` to `U12.SG_N` | `DS2_A`–`DS2_G` | L where the segment is lit. Where it is dark, no lamp: the decoder lets go of the pin |
| `U9.CLR_N`, `U11.LT_N` | `VCC` | H, steady |

The logic analyzer shows one round in full. Clip `U5.QD` and trigger on its falling edge, then clip `U6.1B`, `U6.1Y`, `U6.2Y` and `U8.QD`. Sample every 10 µs and view 1 ms a division. At 47 %: `PULSE` is high for about 4.7 ms from the trigger. `GCLK` ticks all through it, 47 times. `LATCH` is low during the pulse and rises as it ends. `C3` goes high four times, once per ten counted.

## Troubleshooting

**The digits never change, whatever the knob does.**

1. Probe `J2` pin 3. It must read H: the controller needs the board's +5 V.
2. Probe `U5.QD`, on `TRIG_N`. It must show P. If not, follow the timebase back: `U4.QD`, `U3.QD`, `U2.QD`, `U1.CKA` must all show P.
3. Probe `J2` pin 2, on `PULSE`. With the knob turned up, it must show P. Then probe `U6.1B`: it is the same wire.
4. Probe `U6.2Y`, on `LATCH`. It must show P. Then probe `U9.CLK` and `U10.CLK`: they are the same wire.

**The number is wrong, or keeps climbing on its own.**

1. Probe `U5.QB` and `U5.QC`. Both must show P.
2. Probe `U8.R01`, `U8.R02`, `U7.R01` and `U7.R02`. Each must show P: they clear the counters every round.
3. Turn the knob to the left stop. `U8.QA` must read L, steady. A counter that still counts there is not being cleared.

**One digit is wrong.**

1. Read the digit's latch outputs: `U10.1Q`–`U10.4Q` for the units, `U9.1Q`–`U9.4Q` for the tens. Compare them with the number the digit should show, in binary.
2. If the latch is wrong, probe its inputs: `U8.QA`–`U8.QD` for the units, `U7.QA`–`U7.QD` for the tens. Each one used by the reading must show P.
3. The tens counter counts on `C3`. If the tens stay at 0, probe `U7.CKA`: with the reading past 10, it must show P, as `U8.QD` does.
4. If the latch is right, probe the decoder's inputs: `U12.A`–`U12.D` for the units, `U11.A`–`U11.D` for the tens. Each must read the same as the latch output it comes from.
5. Then probe the decoder's segment pins. Each lit segment must read L.

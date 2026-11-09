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
- `U6` gates: `GCLK` from `F10K` while `PULSE` is high; `LATCH` rises when `PULSE` ends
- `U7` tens counter: counts 0 to 9 on `C3` into `C4`–`C7`; cleared by `TB1` and `TB2`
- `U8` units counter: counts 0 to 9 on `GCLK` into `C0`–`C3`, carries to `U7` on `C3`; cleared by `TB1` and `TB2`
- `U9` tens latch: takes `C4`–`C7` into `L4`–`L7` when `LATCH` rises
- `U10` units latch: takes `C0`–`C3` into `L0`–`L3` when `LATCH` rises
- `U11` tens decoder: `L4`–`L7` to the segments of `DS1`
- `U12` units decoder: `L0`–`L3` to the segments of `DS2`
- `DS1` tens digit: a segment lights while its `U11` output is low
- `DS2` units digit: a segment lights while its `U12` output is low

# Stopwatch — Service Manual

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `CLK`
- `J1` power connector: +5 V and 0 V for the whole board
- `SW1` Start button: pulls `START_N` low while pressed
- `SW2` Stop button: pulls `STOP_N` low while pressed
- `SW3` Reset button: pulls `RST_N` low while pressed
- `R1` pull-up: holds `START_N` high while `SW1` is released
- `R2` pull-up: holds `STOP_N` high while `SW2` is released
- `R3` pull-up: holds `RST_N` high while `SW3` is released
- `R4` digit resistor: feeds the anodes of `DS1` (`DS1_CA`) and limits their current
- `R5` digit resistor: feeds the anodes of `DS2` (`DS2_CA`) and limits their current
- `R6` digit resistor: feeds the anodes of `DS3` (`DS3_CA`) and limits their current
- `U1` prescaler: divides `GCLK` (1 MHz) by ten into `F100K` (100 kHz)
- `U2` prescaler: divides `F100K` (100 kHz) by ten into `F10K` (10 kHz)
- `U3` prescaler: divides `F10K` (10 kHz) by ten into `F1K` (1 kHz)
- `U4` prescaler: divides `F1K` (1 kHz) by ten into `F100` (100 Hz)
- `U5` seconds counter: counts 0 to 9 on `T3` into `S0`–`S3`; cleared by `RESET`
- `U6` tenths counter: counts 0 to 9 on `H3` into `T0`–`T3`, carries to `U5` on `T3`; cleared by `RESET`
- `U7` hundredths counter: counts 0 to 9 on `F100` into `H0`–`H3`, carries to `U6` on `H3`; cleared by `RESET`
- `U8` seconds decoder: `S0`–`S3` to the segments of `DS1`
- `U9` tenths decoder: `T0`–`T3` to the segments of `DS2`
- `U10` hundredths decoder: `H0`–`H3` to the segments of `DS3`
- `U11` run flip-flop: `RUN` set by `START_N`, cleared by `STOP_N`
- `U12` clock gate and reset: `GCLK` from `CLK` while `RUN` is high; `RESET` high while `RST_N` is low
- `DS1` seconds digit: a segment lights while its `U8` output is low; its decimal point is always lit
- `DS2` tenths digit: a segment lights while its `U9` output is low
- `DS3` hundredths digit: a segment lights while its `U10` output is low

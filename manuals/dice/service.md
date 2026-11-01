# Dice — Service Manual

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `CLK`
- `J1` power connector: +5 V and 0 V for the whole board
- `SW1` roll button: pulls `BTN_N` low while pressed
- `R1` pull-up: holds `RESET_N` high so `U1` runs
- `R2` LED resistor: limits the current through `D1`
- `R3` LED resistor: limits the current through `D2`
- `R4` LED resistor: limits the current through `D3`
- `R5` LED resistor: limits the current through `D4`
- `R6` LED resistor: limits the current through `D5`
- `R7` LED resistor: limits the current through `D6`
- `R8` LED resistor: limits the current through `D7`
- `R9` pull-up: holds `BTN_N` high while `SW1` is released
- `U1` counter: counts 1 to 6 on `ROLL_CLK` (bits `N0`–`N2`), back to 1 after 6 when `SIX_N` loads it
- `U2` decoder: inverts `N0` into `ODD_N` and `N2` into `FOUR_N`
- `U3` decoder: NAND of `N1` and `N2` into `SIX_N`
- `U4` clock gate and decoder: `ROLL_CLK` from `CLK` while `BTN_N` is low; NOR of `N1` and `N2` into `TWO_N`
- `D1` LED, top left: lit while `TWO_N` is low
- `D2` LED, top right: lit while `FOUR_N` is low
- `D3` LED, middle left: lit while `SIX_N` is low
- `D4` LED, centre: lit while `ODD_N` is low
- `D5` LED, middle right: lit while `SIX_N` is low
- `D6` LED, bottom left: lit while `FOUR_N` is low
- `D7` LED, bottom right: lit while `TWO_N` is low

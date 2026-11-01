# LED Chaser — Service Manual

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `CLK`
- `J1` power connector: +5 V and 0 V for the whole board
- `R1` pull-up: holds `RESET_N` high so the counters and `U5` run
- `R2` LED resistor: limits the current through `D1`
- `R3` LED resistor: limits the current through `D2`
- `R4` LED resistor: limits the current through `D3`
- `R5` LED resistor: limits the current through `D4`
- `R6` LED resistor: limits the current through `D5`
- `R7` LED resistor: limits the current through `D6`
- `R8` LED resistor: limits the current through `D7`
- `R9` LED resistor: limits the current through `D8`
- `U1` divider bits 0–3: counts on every clock, carry `CARRY1` to `U2`
- `U2` divider bits 4–7: counts when `U1` carries, carry `CARRY2` to `U3`
- `U3` divider bits 8–11: counts when `U2` carries, carry `CARRY3` to `U4`
- `U4` divider bits 12–15: counts when `U3` carries; its top bit is `STEP`
- `U5` shift register: moves `S1`–`S8` one place on each rise of `STEP`
- `U6` feedback: 8-input NAND, feeds a new low bit into `SER` when `S1`–`S7` are all high
- `D1` LED: lit while `S1` is low
- `D2` LED: lit while `S2` is low
- `D3` LED: lit while `S3` is low
- `D4` LED: lit while `S4` is low
- `D5` LED: lit while `S5` is low
- `D6` LED: lit while `S6` is low
- `D7` LED: lit while `S7` is low
- `D8` LED: lit while `S8` is low

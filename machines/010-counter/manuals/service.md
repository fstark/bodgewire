# Counter — Service Manual

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `CLK`
- `J1` power connector: +5 V and 0 V for the whole board
- `R1` pull-up: holds `RESET_N` high so the counters run
- `R2` LED resistor: limits the current through `D1`
- `U1` counter bits 0–3: counts on every clock, carry `CARRY1` to `U2`
- `U2` counter bits 4–7: counts when `U1` carries, carry `CARRY2` to `U3`
- `U3` counter bits 8–11: counts when `U2` carries, carry `CARRY3` to `U4`
- `U4` counter bits 12–15: counts when `U3` carries, carry `CARRY4` to `U5`
- `U5` counter bits 16–19: counts when `U4` carries; `C19` drives `D1`
- `D1` LED: lit while `C19` is low

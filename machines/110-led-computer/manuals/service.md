# LED computer — Service Manual

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `PHI0` of `U1`
- `J1` power connector: +5 V and 0 V for the whole board
- `U1` processor: a 6502 running the program in `U2`; reads and writes on `A0`–`A15` and `DB0`–`DB7`, `RW` low while it writes
- `U2` program ROM: a 2716 at $F800–$FFFF (and again at each 2K from $E000), driving `DB0`–`DB7` while `ROM_N` is low
- `U3` RAM: the low half of each byte (`DB0`–`DB3`) at $0000–$03FF (and again up to $1FFF), selected by `RAM_N`, written while `WR_N` is low
- `U4` RAM: the high half of each byte (`DB4`–`DB7`), as `U3`
- `U5` address decoder: `A13`–`A15` select one 8K block: `RAM_N` low for $0000–$1FFF, `LATCH_N` for $8000–$9FFF, `ROM_N` for $E000–$FFFF
- `U6` write strobes: `WR` high in the second half of a write (`PHI1` and `RW` low), `WR_N` its opposite, `LATCH_WR` high while writing to the LEDs, `LATCH_CLK` its opposite, rising at the end of that write
- `U7` LED latch: keeps `DB0`–`DB7` on `L0`–`L7` at each rise of `LATCH_CLK`; `RES_N` low clears it
- `U8` power-on reset: holds `RES_N` low for 20 ms after power-on, then lets go
- `SW1` Reset button: pulls `RES_N` low while pressed
- `D1` LED: lit while `L0` is high
- `D2` LED: lit while `L1` is high
- `D3` LED: lit while `L2` is high
- `D4` LED: lit while `L3` is high
- `D5` LED: lit while `L4` is high
- `D6` LED: lit while `L5` is high
- `D7` LED: lit while `L6` is high
- `D8` LED: lit while `L7` is high
- `R1` resistor: limits the current of `D1`
- `R2` resistor: limits the current of `D2`
- `R3` resistor: limits the current of `D3`
- `R4` resistor: limits the current of `D4`
- `R5` resistor: limits the current of `D5`
- `R6` resistor: limits the current of `D6`
- `R7` resistor: limits the current of `D7`
- `R8` resistor: limits the current of `D8`
- `R9` resistor: pulls `RES_N` up once the power-on reset and `SW1` let go
- `R10` resistor: holds `IRQ_N` high
- `R11` resistor: holds `NMI_N` high

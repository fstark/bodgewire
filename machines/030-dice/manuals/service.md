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
- `U1` counter: counts 1 to 6 on `ROLL_CLK`, on `N0`–`N2`; after 6, `SIX_N` makes it load 1
- `U2` decoder: turns `N0` upside down into `ODD_N`, and `N2` into `FOUR_N`
- `U3` decoder: `SIX_N` low only while `N1` and `N2` are both high
- `U4` clock gate and decoder: passes `CLK` to `ROLL_CLK` while `BTN_N` is low; `TWO_N` low while `N1` or `N2` is high
- `D1` LED, top left: lit while `TWO_N` is low
- `D2` LED, top right: lit while `FOUR_N` is low
- `D3` LED, middle left: lit while `SIX_N` is low
- `D4` LED, centre: lit while `ODD_N` is low
- `D5` LED, middle right: lit while `SIX_N` is low
- `D6` LED, bottom left: lit while `FOUR_N` is low
- `D7` LED, bottom right: lit while `TWO_N` is low

## Test points

What the logic probe shows on a working board. **P** is the pulse lamp: it stays lit while the signal keeps changing. Click a pin below with the probe in hand to probe it. "Held" means `ROLL` held down; "idle" means not touched.

| Probe | Net | A working board shows |
|---|---|---|
| `X1.OUT`, `U4.1A` | `CLK` | P lit, held or idle |
| `SW1` pin 1, `U4.1B` | `BTN_N` | idle: H, steady. Held: L, steady |
| `U4.1Y`, `U1.CLK` | `ROLL_CLK` | idle: L, steady. Held: P lit |
| `U1.CLR_N` | `RESET_N` | H, steady |
| `U1.QA`, `U1.QB`, `U1.QC` | `N0`, `N1`, `N2` | held: P lit on all three. Idle: steady, one of them at least H once the dice has rolled |
| `U2.1Y`, `U4.2Y`, `U2.2Y`, `U3.1Y` | `ODD_N`, `TWO_N`, `FOUR_N`, `SIX_N` | held: P lit on all four. Idle: steady, L for the groups the face lights |
| `U1.LOAD_N` | `SIX_N` | the same as `U3.1Y`: it is the same wire |
| `D4` pin 1, `D1` pin 1, `D2` pin 1, `D3` pin 1 | `ODD_N`, `TWO_N`, `FOUR_N`, `SIX_N` | the same as the decoder output on the same wire |
| `D1` pin 2 to `D7` pin 2 | `LED_A1` to `LED_A7` | H, steady: +5 V through each LED's resistor |
| logic analyzer: clip `U4.1Y`, `U1.QA`, `U1.QB`, `U1.QC`, `U3.1Y`; trigger on `U3.1Y` falling; sample every 100 ns; 2 µs a division; hold `ROLL` | | `U1.QA` changes at every rise of `U4.1Y`. `U3.1Y` dips low for one tick, once every 6 µs. Read the three outputs as 1, 2, 4: right after the dip they count 1, 2, 3, 4, 5, 6 |

## Troubleshooting

**Rolling does nothing: the face never changes.**

1. Hold `ROLL` and probe `SW1` pin 1. It must read L while held, H when released.
2. Probe `U4.1B` the same way. It is the same wire, so it must read the same.
3. Probe `X1.OUT`. It must show P.
4. Hold `ROLL` and probe `U4.1Y`. It must show P. If it stays L, check `U4`.
5. Hold `ROLL` and probe `U1.CLK`. It must show P, like `U4.1Y`.
6. Probe `U1.CLR_N`. It must read H.

**Some faces never come up, or a face shows the wrong spots.**

1. Hold `ROLL` and probe `U1.QA`, `U1.QB` and `U1.QC`. Each must show P. One that stays steady stops part of the count.
2. Hold `ROLL` and probe `U1.LOAD_N`. It must show P. If it never goes low, the count runs past 6.
3. Roll a few times. After each roll, probe `U1.QA`, `U1.QB` and `U1.QC`. Add up 1, 2 and 4 for each one that reads H. That is the number the counter holds. It must be 1 to 6.
4. For that number, probe the four decoder outputs: `U2.1Y`, `U4.2Y`, `U2.2Y` and `U3.1Y`. Each must be L exactly when the User Manual's table says its group is lit.
5. Probe each input of a wrong decoder gate. A gate with good inputs and a wrong output points at its chip. A gate whose input differs from the counter output on the same wire points at the wire.

**One spot, or a pair, never lights.**

1. Find the LED's group in the Components list. Probe its decoder output while the face should light it. It must read L.
2. Probe the LED's pin 1. It must read the same as the decoder output.
3. Probe the LED's pin 2. It must read H.

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
- `U6` feedback: 8-input NAND, feeds a new low into `SER` when `S1`–`S7` are all high
- `D1` LED: lit while `S1` is low
- `D2` LED: lit while `S2` is low
- `D3` LED: lit while `S3` is low
- `D4` LED: lit while `S4` is low
- `D5` LED: lit while `S5` is low
- `D6` LED: lit while `S6` is low
- `D7` LED: lit while `S7` is low
- `D8` LED: lit while `S8` is low

## Test points

What the logic probe shows on a working board. **P** is the pulse lamp: it stays lit while the signal keeps changing. Click a pin below with the probe in hand to probe it.

| Probe | Net | A working board shows |
|---|---|---|
| `U1.CLK` (pin 2 of `U1`–`U4`) | `CLK` | P lit |
| `U1.CLR_N`, `U5.CLR_N` | `RESET_N` | H, steady |
| `U1.RCO`, `U2.RCO`, `U3.RCO` | `CARRY1`–`CARRY3` | L, with P lit. Each carry is high for one clock tick only, a millionth of a second |
| `U4.QD` | `STEP` | H and L flicker about 15 times a second; P lit |
| `U5.CLK` | `STEP` | the same as `U4.QD`: it is the same wire |
| `U5.QA`, `U5.QB`, … `U5.QH` | `S1`–`S8` | H most of the time. L for one step, about a fifteenth of a second, once every half second; P flashes at each change. Each output goes low one step after the one before it |
| `D1` pin 1, … `D8` pin 1 | `S1`–`S8` | the same as `U5.QA` … `U5.QH`: the LED's cathode is on the same wire |
| `D1` pin 2, … `D8` pin 2 | `LED_A1`–`LED_A8` | H, steady |
| `U6.Y` | `SER` | H most of the time. L for one step while `U5.QH` is low, once every half second |
| `U5.A` | `SER` | the same as `U6.Y`: it is the same wire |
| `U5.B`, `U6.H` | | H, steady |
| Logic analyzer: clip `U4.QD`, `U6.Y`, then `U5.QA` to `U5.QH` in order. Trigger on `U6.Y` falling. Sample every 20 µs, view at 100 ms a division | | `U4.QD` is a square wave, a rise about every 65 thousandths of a second. The low walks down the eight `U5` outputs like a staircase, one place at each rise of `U4.QD`. `U6.Y` is low while `U5.QH` is low; at the next rise, the low starts again on `U5.QA` |

## Troubleshooting

**No LED ever lights, or they all stay lit.**

1. Probe `U1.CLK`. It must show P. If it does not, the clock is not reaching the board.
2. Probe `U1.CLR_N` and `U5.CLR_N`, on `RESET_N`. Both must read H.
3. Follow the carries in order: `U1.RCO`, `U2.RCO`, `U3.RCO`. Each one must show P.
4. Probe `U4.QD`, then `U5.CLK`. Both must flicker with P lit. If not, the row never moves: see the first carry that stays dark.
5. Probe `U6.Y`. It must go low once every half second. If it stays high, no new light enters the row.

**The light runs along the row but never comes back.**

1. Probe `U5.QH`. It must go low once every half second.
2. Probe `U6.Y` at the same time. It must go low with it.
3. Probe `U5.A`. It is the same wire as `U6.Y`, so it must read the same.
4. Probe `U5.QA` to `U5.QG`. When the low is on `U5.QH`, they must all read H.

**One LED or more stay dark, or one stays lit, while the others run.**

1. Probe the matching `U5` output, `U5.QA` for `D1` up to `U5.QH` for `D8`. It must go low once every half second, for one step.
2. Probe the LED's pin 1. It is the same wire, so it must read the same as that output.
3. Probe the LED's pin 2. It must read H, steady.
4. Probe its resistor, `R2` for `D1` up to `R9` for `D8`, at both ends. Each end must read H, steady. Pin 1 is on +5 V.

**Two LEDs light together, or the light jumps a place.**

1. Hook the eight `U5` outputs on the logic analyzer, as in Test points.
2. Each output must go low on its own, one step after the one before it.
3. Two outputs that always match, or one that never goes low, show where the row goes wrong.

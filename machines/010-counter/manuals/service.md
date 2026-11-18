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

## Test points

What the logic probe shows on a working board. **P** is the pulse lamp: it stays lit while the signal keeps changing. Click a pin below with the probe in hand to probe it.

| Probe | Net | A working board shows |
|---|---|---|
| `U1.CLK` (pin 2 of every chip) | `CLK` | P lit |
| `U1.QA`, `U1.QB`, `U1.QC`, `U1.QD` | | P lit: far too fast for the eye. Hook all four on the oscilloscope, at 2 µs a division: each output changes half as often as the one before |
| `U1.RCO`, `U2.RCO`, `U3.RCO`, `U4.RCO` | `CARRY1`–`CARRY4` | L, with P lit. Each carry is high for one clock tick only, a millionth of a second. The H lamp is too slow to show it: it may flicker on `U1.RCO`, and stays dark on the others. The oscilloscope shows the pulse: hook the carry, trigger on its rising edge |
| `U5.QD` | `C19` | H for about half a second, then L for half a second; P flashes at each change |
| `D1` pin 1 | `C19` | the same as `U5.QD`: it is the same wire |
| `U1.CLR_N` | `RESET_N` | H, steady |

## Troubleshooting

**The LED never lights, or never goes out.**

1. Probe `U5.QD`, which drives `C19`. Does it change every half second?
2. If it does, probe `D1` pin 1. It is the same wire, so it must read the same.
3. If `U5.QD` never changes, the count stops before it: see below.

**The count stops somewhere.**

1. Probe the clock pin of each chip: `U1.CLK`, `U2.CLK`, `U3.CLK`, `U4.CLK`, `U5.CLK`. Each one must show P.
2. Probe `U1.CLR_N`, on `RESET_N`. It must read H.
3. Follow the carries in order: `U1.RCO`, `U2.RCO`, `U3.RCO`, `U4.RCO`. Each one must show P.
4. The first carry that stays dark is where the count stops. Check the chip that drives it, and the wire to the next chip's `ENT`.

**The LED blinks too fast.**

A chip counts more often than it should. A working carry is mostly L. Probe each carry, `U1.RCO` to `U4.RCO`: one that stays H makes the next chip count at every tick.

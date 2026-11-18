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

What the logic probe shows on a working board. **P** is the pulse lamp: it stays lit while the signal keeps changing.

| Probe | A working board shows |
|---|---|
| `CLK`, on pin 2 of any chip | P lit |
| `U1.QA`, `U1.QB`, `U1.QC`, `U1.QD` | P lit: far too fast for the eye. The logic analyzer shows each output changing half as often as the one before |
| `CARRY1`, `CARRY2`, `CARRY3`, `CARRY4` | L, with P lit: a short high blip now and then |
| `C19`, on `U5` pin 11 | H for about half a second, then L for half a second; P flashes at each change |
| `D1` pin 1 | the same as `C19`: it is the same wire |
| `RESET_N` | H, steady |

## Troubleshooting

**The LED never lights, or never goes out.**

1. Probe `C19` on `U5` pin 11. Does it change every half second?
2. If it does, probe `D1` pin 1. It is the same wire, so it must read the same.
3. If `C19` never changes, the count stops before it: see below.

**The count stops somewhere.**

1. Probe `CLK` on pin 2 of each chip, `U1` to `U5`. Each one must show P.
2. Probe `RESET_N`. It must read H.
3. Follow the carries in order: `CARRY1`, `CARRY2`, `CARRY3`, `CARRY4`. Each one must show P.
4. The first carry that stays dark is where the count stops. Check the chip that drives it, and the wire to the next chip's `ENT`.

**The LED blinks too fast.**

A chip counts more often than it should. A working carry is mostly L. Probe each carry: one that stays H makes the next chip count at every tick.

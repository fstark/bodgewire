# Counter — Service Manual

## How it works

**Clock.** The crystal oscillator `X1` makes a 1 MHz square wave on its pin 8 (`OUT`), net `CLK`. It goes to the clock pins of all five counters.

**Reset.** `R1` pulls `RESET_N` up to +5 V, so the counters are never cleared. Nothing pulls it low on this board.

**Counter.** `U1` to `U5` are five 4-bit counters chained into one 20-bit counter, bits `C0` (`U1` pin 14) to `C19` (`U5` pin 11). `U1` counts on every clock. When a counter reaches 15 while it is enabled, its ripple carry (pin 15) goes high for one clock and lets the next counter count one step: `CARRY1` from `U1` into `U2`, up to `CARRY4` from `U4` into `U5`. Each carry is 16 times rarer than the one before. The whole count wraps every 1.05 s.

**LED.** `R2` feeds +5 V into the anode of `D1` (net `LED_A`); the cathode sinks into `C19`. `D1` lights while `C19` is low and is dark while it is high: about 0.5 s each.

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

## Troubleshooting

Walk the chain from the clock up; the fault is where the signal stops.

- Check `X1` pin 8 (`OUT`): it must pulse. If not, nothing else can move: check power at `X1` pin 14.
- Check `U1` pin 1 (`CLR_N`): it must be high. If low, the counters are held at 0.
- Check `U1` pin 15 (`RCO`): a short pulse every 16 µs.
- Check `U2` pin 15 (`RCO`): a short pulse every 256 µs.
- Check `U3` pin 15 (`RCO`): a short pulse every 4.1 ms.
- Check `U4` pin 15 (`RCO`): a short pulse every 65.5 ms.
- A carry that never pulses: check the bits of that counter (pins 14, 13, 12, 11) and its enable `ENT` (pin 10). A carry that is high all the time makes every later counter run too fast.
- Check `U5` pin 11 (`QD`, `C19`): it changes about every 0.5 s. Then check `D1` pin 1 (`K`): it must follow `U5` pin 11. If it floats, the Trace between them is broken.

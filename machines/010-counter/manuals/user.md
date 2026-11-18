# Counter

## What it does

Welcome to the Counter, the simplest board on the bench. Its red LED `D1` blinks about once a second: half a second on, half a second off.

Behind that blink is a lot of counting. The oscillator `X1` ticks one million times a second. Five counter chips, `U1`–`U5`, divide those ticks by 1 048 576. What is left is one blink a second.

## How it works

**The goal.** The clock ticks a million times a second. The LED must change only once every half second. So the board slows the clock down about a million times. It does that by counting.

**The clock.** `X1` ticks one million times a second. All five chips get the same tick on their `CLK` pin.

**One chip counts from 0 to 15.** A 74LS161 holds a number. It shows it on four outputs, `QA` to `QD`. At each tick, it adds one. After 15, it goes back to 0.

**Five chips work like a car's odometer.** Each chip is one wheel. `U1` is the fastest wheel. A wheel turns only when the wheel before it rolls over.

- `U1` turns at every tick.
- When `U1` reaches 15, it warns `U2`: its `RCO` output goes high. That wire is `CARRY1`.
- `U2` turns only while `U2.ENT` is high. So `U2` turns once every 16 ticks.
- Just before `U2` rolls over, it warns `U3` the same way, on `CARRY2`. `U3` turns once every 256 ticks. And so on up to `U5`.

**The LED.** `U5`'s last output, `C19`, is the slowest signal on the board. It changes about every half second. The LED's other end gets +5 V through `R2`, so the LED is lit while `C19` is low.

**Pins with nothing to do.** The 74LS161 has features this board does not use. Their pins are tied so they stay out of the way:

| Pin | What it can do | Here |
|---|---|---|
| `ENP` | pause the count | tied high: never pauses |
| `LOAD_N`, `A`–`D` | load a starting number | `LOAD_N` tied high: never loads |
| `CLR_N` | reset the count to 0 | held high on `RESET_N` by `R1`: never resets |

The Service Manual lists what the probe shows on a working board.

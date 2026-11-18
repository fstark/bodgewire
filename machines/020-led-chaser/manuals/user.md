# LED Chaser

## What it does

Welcome to the LED Chaser. It has a row of eight red LEDs, `D1`–`D8`. One light runs along the row, from `D1` to `D8`, then starts again at `D1`. It takes about 15 steps a second, so a full run takes about half a second.

Just after power-on, two or three LEDs may light together. They run off the end within half a second. From then on, only one LED is lit at a time.

## How it works

**The goal.** The board must move a light one place along the row, about 15 times a second. It needs two things for that: a slow, steady beat, and something that moves the light at each beat.

**The clock.** `X1` ticks one million times a second. That is far too fast for the eye. Its tick goes to the clock pins of `U1`–`U4`, on `CLK`.

**Slowing the clock down.** `U1`–`U4` are four counters, chained like the wheels of a car's odometer. Each one counts from 0 to 15. When one rolls over, it moves the next one on by one, through `CARRY1`–`CARRY3`. The last output of `U4`, `U4.QD`, changes only about 30 times a second. That wire is `STEP`. It rises about 15 times a second: each rise is one step of the light.

**The row of eight.** `U5` is a shift register. Think of eight people in a line, passing buckets along. At each rise of `STEP`, each person passes their bucket to the next one. The last bucket falls off the end. The first person takes a new one.

- The eight places are `U5.QA` to `U5.QH`. Their wires are `S1`–`S8`.
- All the places hold a high, except one that holds a low.
- At each step, that low moves one place on: from `S1` to `S2`, and so on to `S8`.
- The new value comes in on `U5.A`, from the wire `SER`.

**Starting a new run.** `U6` watches the row. It is a gate with eight inputs. Seven of them are on `S1`–`S7`; the eighth is tied high. While the low is anywhere in `S1`–`S7`, `U6` keeps `SER` high, so only highs come in. When the low reaches `S8`, `S1`–`S7` are all high. Then `U6` pulls `SER` low. At the next step, that low enters at `S1`, and the low at `S8` falls off the end. The light is back at `D1`.

This also cleans up after power-on. The row may start with several lows in it. `U6` lets no new low in while any of `S1`–`S7` is low. So the extra lows run off the end, and a single one comes back.

**The LEDs.** Each LED gets +5 V through its resistor, `R2`–`R9`. Its other end sits on one of `S1`–`S8`. `D1` is on `S1`, `D2` on `S2`, and so on. An LED is lit while its wire is low. So the one low in the row is the one lit LED.

**Pins with nothing to do.** The chips have features this board does not use. Their pins are tied so they stay out of the way:

| Pin | What it can do | Here |
|---|---|---|
| `ENP` on `U1`–`U4` | pause the count | tied high: never pauses |
| `LOAD_N`, `A`–`D` on `U1`–`U4` | load a starting number | `LOAD_N` tied high: never loads |
| `CLR_N` on `U1`–`U5` | clear the counters and the row | held high on `RESET_N` by `R1`: never clears |
| `U4.RCO` | carry to a fifth counter | not connected |
| `U5.B` | a second data input | tied high, so `U5.A` alone decides |
| `U6.H` | an eighth input | tied high, so `S1`–`S7` alone decide |

The Service Manual lists what the probe shows on a working board.

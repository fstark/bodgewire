# LED Chaser

## What it does

A row of eight red LEDs, `D1` to `D8`, with one light running along it from `D1` to `D8` and back to `D1`, about 15 steps a second. A full run takes about half a second. Just after power-on a few LEDs may light together for up to half a second, until the light settles.

Inside, after power-on:

- the clock `CLK` pulses at 1 MHz;
- a 16-bit divider slows it down to `STEP`, which rises every 65.5 ms;
- on each rise of `STEP`, the shift register moves its eight stages `S1` to `S8` one place on;
- exactly one stage is low at a time, and the LED on that stage is lit.

Only the LEDs are slow enough to see; watch the rest with the logic probe, the analyzer or the scope.

## Connections

- `J1` power: +5 V on `VCC`, 0 V on `GND`.
- No signal connector: probe the chip pins or the Traces.

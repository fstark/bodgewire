# Counter

## What it does

A 20-bit binary counter driven by a 1 MHz crystal clock, with a red LED `D1` on its top bit. When it works, `D1` blinks about once a second: on for about 0.5 s, off for about 0.5 s.

Inside, after power-on:

- the clock `CLK` pulses at 1 MHz;
- the counter bits `C0` to `C19` all pulse, each at half the rate of the one before (`C0` 500 kHz, `C19` about 0.95 Hz);
- the carries `CARRY1` to `CARRY4` give a short pulse every 16 µs, 256 µs, 4.1 ms and 65.5 ms;
- `D1` is lit while `C19` is low.

Only `D1` is slow enough to see; watch the rest with the logic probe, the analyzer or the scope.

## Connections

- `J1` power: +5 V on `VCC`, 0 V on `GND`.
- No signal connector: probe the chip pins or the Traces.

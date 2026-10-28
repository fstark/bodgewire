# Counter

## What it does

A small test board: an 8-bit binary counter driven by a 1 MHz crystal clock, with a divide-by-two stage and a decoder on its top bits. It has no output connector; you watch it with the logic probe.

When it works, after power-on:

- the clock `CLK` pulses at 1 MHz;
- the counter bits `C0` to `C7` all pulse, each at half the rate of the one before (`C0` 500 kHz, `C7` about 3.9 kHz); the count wraps from 255 to 0 every 256 µs;
- `DIV` is a square wave at about 1.95 kHz (half the rate of `C7`);
- `TOP_N` goes low for 64 µs every 256 µs, while the count is 192 to 255.

## Connections

- `J1` power: +5 V on `VCC`, 0 V on `GND`.
- No signal connector: probe the chip pins or the Traces.

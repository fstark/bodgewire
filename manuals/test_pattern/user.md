# Test Pattern Generator

## What it does

Makes a test picture for a TTL monitor, so you can check the monitor and its cable. When it works, the monitor shows a steady white grid on black:

- 6 vertical lines, evenly spaced, 8 dots apart;
- 18 horizontal lines, 16 picture lines apart;
- the grid fills the top-left part of the screen; the right and bottom edges stay black.

The picture locks and holds still within half a second of power-on. It never rolls or tears. Frame rate about 50 Hz, line rate 15.625 kHz.

## Connections

- `J1` power: +5 V on `VCC`, 0 V on `GND`.
- `J2` monitor: `VIDEO` (high is white), `SYNC_N` (sync pulses, active low) and `GND`.

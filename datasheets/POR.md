# POR — power-on reset

## Function

Holds a processor in reset while the power comes up, then lets it start. A processor that starts before its supply and clock have settled can begin anywhere; a reset makes it start at its reset vector every time.

From power-on, `RST_N` pulls low for the time printed on the part, then lets go for good: a pull-up resistor brings the line high and the processor starts. On a real board this is a resistor and a capacitor charging, followed by a Schmitt trigger; here it is one package. `RST_N` is an open collector: it only ever pulls low, so a Reset button can pull the same line low at any time.

## Pins

- `RST_N`: reset output, open collector, low after power-on
- `NC2`, `NC3`, `NC5`–`NC7`: not connected
- `VCC`: +5 V
- `GND`: 0 V

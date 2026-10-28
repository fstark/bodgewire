# OSC — crystal oscillator

## Function

A crystal clock in a 14-pin metal can. As long as it has power, `OUT` is a steady square wave at the frequency printed on the can (half the time high, half low). Everything that counts or steps in the Machine takes its timing from here: if the clock stops, the whole Machine freezes.

## Pins

- `OUT`: square-wave clock output
- `NC1`–`NC6`, `NC9`, `NC10`–`NC13`: not connected
- `VCC`: +5 V
- `GND`: 0 V

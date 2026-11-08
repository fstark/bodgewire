# DIGIT — red 0.56-inch 7-segment digit, common anode

## Function

Eight LEDs in one package: seven bars `A`–`G` that draw a digit, and a decimal point `DP`. `A` is the top bar, `B` top right, `C` bottom right, `D` bottom, `E` bottom left, `F` top left, `G` the middle bar; the point sits at the bottom right.

All eight share their anodes: `CA1` and `CA2`, joined inside, go to +5 V (through a resistor). Each segment pin is its LED's cathode: the segment lights while that pin is pulled low, usually by a decoder output such as the 74LS47's. A segment pin left unconnected stays dark.

## Pins

- `A`–`G`: segment cathodes; `A` pad 7, `B` pad 6, `C` pad 4, `D` pad 2, `E` pad 1 (square pad), `F` pad 9, `G` pad 10
- `DP`: decimal point cathode, pad 5
- `CA1`, `CA2`: common anode, pads 3 and 8, joined inside

# DIGITCC — red 0.56-inch 7-segment digit, common cathode

## Function

Eight LEDs in one package: seven bars `A`–`G` that draw a digit, and a decimal point `DP`. `A` is the top bar, `B` top right, `C` bottom right, `D` bottom, `E` bottom left, `F` top left, `G` the middle bar; the point sits at the bottom right.

All eight share their cathodes: `CC1` and `CC2`, joined inside, go to 0 V, usually switched by a digit driver such as an output of a 74145, one digit at a time. Each segment pin is its LED's anode: the segment lights while that pin is high and the common pin low. With the common pin left floating (its driver off) the whole digit is dark; a segment pin left unconnected stays dark.

The same pads as `DIGIT`, the common-anode digit: only the polarity differs.

## Pins

- `A`–`G`: segment anodes; `A` pad 7, `B` pad 6, `C` pad 4, `D` pad 2, `E` pad 1 (square pad), `F` pad 9, `G` pad 10
- `DP`: decimal point anode, pad 5
- `CC1`, `CC2`: common cathode, pads 3 and 8, joined inside

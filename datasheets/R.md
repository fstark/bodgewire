# R — resistor

## Function

A resistor between its two pins. With one end on `VCC` or `GND` and the other on a signal, it is a pull-up or pull-down: the signal sits high (or low) whenever nothing else drives it. A chip output still wins against it.

Between two signal lines (the ZX80's data bus), it keeps them apart: a line that nothing else drives takes the other's value through it, weakly, while a chip driving a line still wins on its own side. Its value decides which of two weak pulls wins: the one through less resistance.

Every resistor needs its value in ohms: `value=470`, `value=1k`, `value=4k7`.

## Pins

- `1`, `2`: the two ends; either way round

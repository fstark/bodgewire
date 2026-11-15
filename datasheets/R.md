# R — resistor

## Function

A resistor between its two pins. With one end on `VCC` or `GND` and the other on a signal, it is a pull-up or pull-down: the signal sits high (or low) whenever nothing else drives it. A chip output still wins against it.

Between two signal lines (the ZX80's data bus), it keeps them apart: a line that nothing else drives takes the other's value through it, weakly, while a chip driving a line still wins on its own side. Its value (`value=`, in ohms: `470`, `1k`, `4k7`) is then required, as on any pull-up of those lines, and decides which of two weak pulls wins: the one through less resistance.

## Pins

- `1`, `2`: the two ends; either way round

# C — capacitor

## Function

A capacitor passes changes, not levels. With one end on a line that a chip drives and the other on a line held by a pull-up or pull-down resistor (the ZX80's C9 with R2, C11 with R25), each edge of the driven line appears on the pulled one as a short pulse, and the resistor then brings the line back. The pulse lasts about the resistor's ohms times the capacitor's farads: 470 Ω × 47 pF is about 22 ns.

Anywhere else (across the supply, between two driven lines, with no resistor on either side) it does nothing the game shows.

Every capacitor needs its value in farads: `value=47p`, `value=4n7`, `value=100n`, `value=1u`.

## Pins

- `1`, `2`: the two ends; either way round

# LED — red 5 mm LED

## Function

Lights while current flows from `A` to `K`: `A` high and `K` low. TTL usually sinks it: +5 V → resistor → `A`, and `K` on a chip output, so it lights while that output is low. It stays dark if either end is not connected.

## Pins

- `K`: cathode, pad 1 (square pad, flat side of the body)
- `A`: anode, pad 2

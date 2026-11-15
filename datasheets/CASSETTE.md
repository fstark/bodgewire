# CASSETTE — cassette port connector

## Function

One of the PET 2001's two cassette ports: a 6-way edge connector for a tape deck. The deck takes +5 V, the write line and a motor line from the board, and gives back its read line and a sense line that is grounded while a key on the deck is down.

On the PET the motor line is the output of a transistor stage that switches 9 V; here the motor pin carries the control line itself (PIA1 `CB2` for #1, VIA `PB4` for #2), the logic level only. Nothing is plugged in: the pins are places to probe or wire to.

## Pins

- `GND`: ground (pad 1, PET A)
- `VCC`: +5 V to the deck (pad 2, B)
- `MOTOR`: motor control (pad 3, C)
- `READ`: data from the deck (pad 4, D)
- `WRITE`: data to the deck, VIA `PB3`, shared by both ports (pad 5, E)
- `SENSE`: deck switch, grounded while a key is down: PIA1 `PA4` for #1, `PA5` for #2 (pad 6, F)

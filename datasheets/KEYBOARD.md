# KEYBOARD — ASCII keyboard connector

## Function

The 10-pin connector of an ASCII keyboard on a cable. Each key pressed puts its 7-bit ASCII code on `D0`–`D6`, then `STROBE` goes high for a short time, given in the Machine's Service Manual: the Board takes the code while `STROBE` is high, or on its fall. The code stays on `D0`–`D6` until the next key.

The keyboard sends upper case only: letters, digits, the space and the punctuation of ASCII codes 32 to 95. Return sends 13, Back space 8 and Esc 27. CTRL with a key of codes 64 to 95 (`@`, the letters, `[ \ ] ^ _`) sends that code minus 64 (0 to 31); CTRL with any other key sends the key's own code. Keys typed faster than the strobe wait their turn inside the keyboard.

The keyboard runs on the Board's +5 V through `VCC` and `GND`. Without it, its outputs drive nothing.

## Pins

- `D0`–`D6`: the key's code to the Board; `D0` is the lowest bit
- `STROBE`: high for a moment after each key, once the code is steady
- `VCC`: +5 V for the keyboard, from the Board
- `GND`: 0 V

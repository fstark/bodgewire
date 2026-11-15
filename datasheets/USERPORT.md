# USERPORT — user port edge connector

## Function

The PET 2001's user port: a 24-way edge connector at the back of the board. Its letter side is the VIA's port A (`PA0`–`PA7`) with `CA1` and `CB2`, eight lines a program can read and write, and `CB2` the PET's sound. Its number side has the diagnostic pin `DIAG` (PIA1's `PA7`: grounded at reset, it runs the self-test), the two cassette read lines and the write line, two IEEE-488 lines, and the video signals for an outside monitor.

Here it is a pin header with nothing plugged in. The PET's number row 1–12 is on the header's odd pads, its letter row A–N (no G or I) on the even pads, so each letter faces its number as on the edge.

## Pins

- `GND1`: ground (pad 1, PET 1)
- `VIDEO`: the picture, low for a lit dot (pad 3, 2)
- `SRQ_N`: IEEE-488 service request (pad 5, 3)
- `EOI_N`: IEEE-488 end or identify (pad 7, 4)
- `DIAG`: diagnostic sense, grounded at reset it runs the self-test (pad 9, 5)
- `READ1`: cassette #1 read (pad 11, 6)
- `WRITE`: cassette write (pad 13, 7)
- `READ2`: cassette #2 read (pad 15, 8)
- `HORZ`: horizontal drive (pad 17, 9)
- `VERT`: vertical drive (pad 19, 10)
- `GND2`, `GND3`: ground (pads 21, 23; PET 11, 12)
- `GND4`: ground (pad 2, A)
- `CA1`: VIA `CA1`, an input with its own edge flag (pad 4, B)
- `PA0`–`PA7`: VIA port A (pads 6–20 even, PET C–L)
- `CB2`: VIA `CB2`, the shift register's output: sound (pad 22, M)
- `GND5`: ground (pad 24, N)

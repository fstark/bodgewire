# IEEE488 — IEEE-488 edge connector

## Function

The PET 2001's IEEE-488 (GPIB) port, where its printers and disk drives plug in: a 24-way edge connector at the back of the board. Eight data lines and eight control lines, every one active low and open collector, so any device on the cable can pull a line low. On the board they come from the MC3446 bus transceivers (data and handshake), a 7417 (`EOI_N`, `IFC_N`) and PIA2 (`SRQ_N`).

Here it is a pin header with nothing plugged in: the pins are only places to probe or wire to. The PET's number row 1–12 is on the header's odd pads, its letter row A–N (no G or I) on the even pads, so each letter faces its number as on the edge.

## Pins

- `DIO1`–`DIO4`: data 1–4 (pads 1, 3, 5, 7; PET 1–4)
- `DIO5`–`DIO8`: data 5–8 (pads 2, 4, 6, 8; PET A–D)
- `EOI_N`: end or identify (pad 9, PET 5)
- `REN`: remote enable, on ground on the PET (pad 10, E)
- `DAV_N`: data valid (pad 11, 6)
- `NRFD_N`: not ready for data (pad 13, 7)
- `NDAC_N`: no data accepted (pad 15, 8)
- `IFC_N`: interface clear, the PET's reset (pad 17, 9)
- `SRQ_N`: service request (pad 19, 10)
- `ATN_N`: attention (pad 21, 11)
- `CHASSIS`: chassis ground (pad 23, 12)
- `GND1`–`GND7`: ground (pads 12, 14, 16, 18, 20, 22, 24; PET F, H, J, K, L, M, N)

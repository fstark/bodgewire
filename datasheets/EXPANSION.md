# EXPANSION — expansion bus edge connector

## Function

The ZX80's expansion bus: a 2 × 23 double-sided edge connector at the back of the board, numbered from the right looking into the back, with a key slot in place of contact 3. The upper row carries the memory-side data bus (the ZX80's D0'–D7', behind the 1K resistors that isolate the CPU's side), the RAM's chip select (`RAMCS_N`, behind its own resistor so an expansion can disable the inner RAM) and the Z80's control lines; the lower row the supplies, the inverted CPU clock and the address bus. Contact 23 of the lower row is the ZX81's ROMCS and is not connected on the ZX80, nor is the 9 V pad here (the board has no unregulated supply).

Here it is a pin header with nothing plugged in: upper contact n is pad 2n − 1, lower contact n is pad 2n, so each pair faces as on the edge.

## Pins

- `D7`: data bus bit 7, memory side (pad 1, upper 1)
- `RAMCS_N`: the RAM select, through its resistor (pad 3, upper 2)
- `SLOTA`, `SLOTB`: the key slot, no contact (pads 5, 6; 3)
- `D0`, `D1`, `D2`: data bus bits 0–2 (pads 7, 9, 11; upper 4–6)
- `D6`, `D5`, `D3`, `D4`: data bus bits 6, 5, 3, 4 (pads 13, 15, 17, 19; upper 7–10)
- `INT_N`, `NMI_N`, `HALT_N`, `MREQ_N`, `IORQ_N`, `RD_N`, `WR_N`, `BUSAK_N`, `WAIT_N`, `BUSRQ_N`, `RESET_N`, `M1_N`, `RFSH_N`: the Z80's lines (pads 21–45 odd; upper 11–23)
- `VCC5`: +5 V (pad 2, lower 1)
- `NC9V`: the 9 V supply, not connected here (pad 4, lower 2)
- `GND1`, `GND2`: 0 V (pads 8, 10; lower 4, 5)
- `PHI`: the CPU clock, inverted (pad 12, lower 6)
- `A0`–`A3`: address bus (pads 14, 16, 18, 20; lower 7–10)
- `A4`–`A9`, `A10`, `A11`, `A12`, `A13`, `A14`, `A15`: address bus, `A15` at pad 22 down to `A4` at pad 44 (even pads; lower 11–22)
- `NC23`: not connected, the ZX81's ROMCS (pad 46, lower 23)

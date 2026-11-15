# PET 2001 board 320137: the schematic, transcribed

The BW-2001's netlist is Commodore's, read pin by pin from schematic 320135 (sheets 1–3, "pertaining to board assembly #320137"), `320137.pdf` at zimmers.net (`pub/cbm/schematics/computers/pet/2001/`). Sheet 1 is drawing 320130, "circuitry common to all PET computers".

| File | Sheet |
|---|---|
| [SHEET1.md](SHEET1.md), [sheet1.bwn](sheet1.bwn) | 1: CPU, address and data buffers, decoding, PIA1, PIA2, VIA, IEEE-488, user port, cassette, keyboard connector, reset |
| [SHEET2.md](SHEET2.md), [sheet2.bwn](sheet2.bwn) | 2: RAM, RAM decoding and buffers, ROM sockets, power supply |
| [SHEET3.md](SHEET3.md), [sheet3.bwn](sheet3.bwn) | 3: clock, video timing, video RAM and its multiplexers, character ROM, dots, monitor connector |

Each `SHEETn.md` has every chip's used pins with their Nets, the connectors pin by pin, how the sheet works as read from its wiring, and the readings that were uncertain. Coordinates are pixels of the sheet rendered at 300 dpi (`pdftoppm -r 300 -png 320137.pdf`, 5100 × 3300); the crop files they name were working files and are not kept.

The `sheetN.bwn` files are the raw transcriptions, with placeholder parts (`?MC3446`, `?74LS139`, connectors by pin number) and resistor pull-ups as `PU_Rnn` Nets. `netlists/bw2001.bwn` is built from them (P5q, [BW2001_DESIGN.md](../BW2001_DESIGN.md)); where the two differ, the netlist is the truth and the Machine design says why.

Checked on 2026-11-15: the three sheets merged, with stand-ins for the parts the catalogue lacked, boot Commodore's BASIC 1 to its banner on the TV (`*** COMMODORE BASIC ***`, ` 7167 BYTES FREE`, `READY.`, the cursor), in the right place in the picture.

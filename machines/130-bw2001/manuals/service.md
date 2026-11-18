# BW-2001 — Service Manual

## Components

- `X1` clock: 8 MHz oscillator, through a gate of `UE2` to `CLK8`, the dot clock
- `J8` power connector: +5 V and 0 V for the whole board
- `UE2` gates: `CLK8` (the clock gate); the dot, flipped by the reverse-video bit (`PIXEL`); `BA0` from `A0`; the VIA's select from `BA6` and `BA11`
- `UC9` character divider: `CLK8` divided by 8 into 2 MHz (`CLK2M`) and 1 MHz (`CLK1M`, one character); its `QA` is `H0`, the lowest bit of the character count
- `UB5` character counter: `H1`–`H4`, counting `H0`; with `UC7` the 64 characters of a line
- `UA1` scan-line counter: `RA0`–`RA2`, the line inside a character row, stepped at the end of each line's text (`DIS_ON` falling), cleared at each vertical step
- `UC8` flip-flops: `PHI0` for the 6502 (`CLK1M` one dot later); the character load pulse `CHLOAD`, once a character
- `UC7` flip-flops: `H5`, the top bit of the character count; the vertical step pulse `VSTEP`
- `UC5` flip-flops: `DIS_ON`, high during the 40 characters of text of a line; `HORZ_DRIVE`, the monitor's horizontal drive (high 24 µs of every 64)
- `UB6` vertical state: `VS0`, `VS1` step through 20 lines of vertical drive, 20 blank, 200 of text and 20 blank: 260 lines a frame
- `UD5` video address bits `TA0`, `TA1`, counting `CHLOAD`, cleared outside the text
- `UD8` gates: the load pulse's clear; the end of 25 rows (`END25_N`); the end of a blank state (`VEND`); `VERT_DRIVE`, low while the state is vertical drive
- `UC6` gates: `VIDEO_ON` (the state video on, also the retrace signal to PIA1 and the VIA); the horizontal decodes that set and clear `DIS_ON`; the vertical step's enable
- `UC2` gates: the row-start latch's enable (`LATCH_G`); the dot and reverse-video halves of the XOR; `VREAD_N`, low while the 6502 reads the video RAM
- `UB1` gate: `ROW7_N`, low during the text of a row's last scan line, when `UE6` takes the next row's start
- `UE8` gates: `LAT_HI_N` (the latched row start's top bits); `BLKEND_N`, the end of a blank state
- `UE9` gates: `TV_VIDEO` (a dot, low when lit, while `VIDEO_ON`, `DIS_ON` and `BLANK_TV` are all high); the diagnostic LED's decode (`DIAG_LED_N`)
- `UD9` inverters: `CLK1M_N`; the counters' clear (`HCNT_CLR`, low unless `R12`'s line is grounded); a keyboard row bit for the LED; two gates of the PET's crystal oscillator, unused here
- `UE5` inverters: `BA11_N`, `BA11`; `SEL8` from `SEL8_N`; the data buffers' direction; `LAT_HI`; `SYNC_N`
- `UB2` dot shift register: loads the character ROM's 8 dots at each `CHLOAD_N`, shifts them out at 8 MHz on `DOT`, leftmost first
- `UA2` character ROM: 901447-08, the PET's 128 characters (and their second set on `GRAPHIC` high); addressed by the screen code on `SD0`–`SD6` and the scan line on `RA0`–`RA2`
- `UC1` flip-flops: carry the reverse-video bit `SD7` in step with its character's dots (`REV`)
- `UD6` video address counter: `TA2`–`TA5`; while `DIS_ON` is low it holds the row start from `UE6`
- `UD7` video address counter: `TA6`–`TA9`, as `UD6`
- `UE6` row-start latch: `LA2`–`LA9`, the address where the next character row starts, 40 further at each row
- `UD2` multiplexer: `SA0`, `SA1` from the video counters, or from `BA0`, `BA1` while `SEL8` is high (the 6502 touching the screen: a white speck if the beam is in the text); the video RAM's write enable (`VRW_N`) from `RW_RAM`
- `UD3` multiplexer: `SA2`–`SA5`, as `UD2`
- `UD4` multiplexer: `SA6`–`SA9`, as `UD2`
- `UC3` video RAM: the low 4 bits of each screen code, $8000–$83FF (and repeated to $8FFF), on `SD0`–`SD3`
- `UC4` video RAM: the high 4 bits, `SD4`–`SD7`
- `UB3` video RAM buffer: `SD0`–`SD3` to `BD0`–`BD3` while the 6502 reads the screen (`VREAD_N`), the other way while it writes (`VRW_N`)
- `UB4` video RAM buffer: `SD4`–`SD7`, as `UB3`
- `J7` monitor connector: `TV_VIDEO` (low for a lit dot), `VERT_DRIVE`, `HORZ_DRIVE`
- `UF3` processor: a 6502 at 1 MHz (`PHI0` from `UC8`); `RES_N` from `UDE9`, `IRQ_N` from the PIAs and the VIA
- `UE4` address buffer: `A6`–`A10` onto `BA6`–`BA10`, open collector, pulled up by `R34`–`R38`; also `IFC_N` from `RES_N`
- `UE3` address buffer: `A1`–`A5` onto `BA1`–`BA5`, open collector, pulled up by `R29`–`R33`; also `EOI_N` from `BLANK_TV`
- `UG2` address decoder: `A12`–`A15` select one 4 KB block, `SEL0_N` to `SELF_N`: 0 and 1 the RAM, 8 the screen, C to F the ROMs and the I/O
- `UG4` gates: `RW_E` (a read outside $E000–$EFFF) and the data buffers' direction
- `UG3` gates: `BPHI2` (the buffered φ2); the RAM select from `SEL0_N`, `SEL1_N`; the RAM buffers' read enable
- `UG5` data buffer: `D0`–`D3` and `BD0`–`BD3`, towards the 6502 for a read outside $C000–$FFFF, else away from it
- `UG6` data buffer: `D4`–`D7` and `BD4`–`BD7`, as `UG5`
- `UB8` PIA2 at $E820: the IEEE-488 data bytes in (port A) and out (port B) through `UA7`, `UA8`; `ATN_IN_N`, `NDAC_OUT_N`, `SRQ_N`, `DAV_OUT_N`
- `UA5` VIA at $E840: the user port (`PA0`–`PA7`, `USR_CA1`, `USR_CB2`); the IEEE handshake; cassette write and #2 motor; `VIDEO_ON` on `UA5.PB5` (the retrace the editor waits for); `GRAPHIC` on `UA5.CA2`
- `UG8` PIA1 at $E810: the keyboard row on `KEY_A` to `KEY_D` (to `UG9`), the columns `KIN0`–`KIN7` on port B; the cassette switches; `EOI_N` in; `DIAG` on `UG8.PA7`; `BLANK_TV` on `UG8.CA2`; `VIDEO_ON` on `UG8.CB1`, the retrace interrupt
- `UG9` keyboard row decoder: `KEY_A` to `KEY_D` pull one of `ROW0`–`ROW9` low
- `UDE9` power-on reset: holds `RES_N` low for 1 ms after power-on (the PET's 555 held it about a second)
- `UA7` IEEE-488 transceiver: data lines 1–4 between PIA2 and `J1`
- `UA8` IEEE-488 transceiver: data lines 5–8
- `UA9` IEEE-488 transceiver: `DAV_N`, `NRFD_N`, `NDAC_N`, `ATN_N` between PIA2, the VIA and `J1`
- `DS1` diagnostic LED: lit while software selects keyboard row 11 (`DIAG_LED_N` low)
- `J1` IEEE-488 connector: the bus lines, from `UA7`–`UA9`, `UE3`, `UE4` and PIA2
- `J2` user port: VIA PA0–PA7, CA1 and CB2 on its letter side; `DIAG` (`J2.DIAG`), the cassette read and write lines, `SRQ_N`, `EOI_N` and the video signals on its number side
- `J3` cassette #2: +5 V, `CASS2_MOTOR` (VIA PB4: the PET's motor transistors are not fitted), `CASS2_READ`, `CASS_WRITE`, `CASS2_SWITCH`
- `J5` keyboard connector: `ROW0`–`ROW9` out, `KIN0`–`KIN7` back
- `J6` cassette #1: as `J3`, with `CASS1_MOTOR` (PIA1 CB2), `CASS1_READ`, `CASS1_SWITCH`
- `R13` pull-up: `NMI_N`
- `R14` pull-up: `RDY`
- `R17` pull-up: `IRQ_N`
- `R26` pull-up: spare gate inputs of `UG3`, `UG4`
- `R28` pull-up: `BA0`
- `R29` pull-up: `BA1`
- `R30` pull-up: `BA2`
- `R31` pull-up: `BA3`
- `R32` pull-up: `BA4`
- `R33` pull-up: `BA5`
- `R34` pull-up: `BA6`
- `R35` pull-up: `BA7`
- `R36` pull-up: `BA8`
- `R37` pull-up: `BA9`
- `R38` pull-up: `BA10`
- `R10` pull-up: `IFC_N`
- `R48` pull-up: `EOI_N`
- `R18` pull-up: `KIN0`, a keyboard column
- `R19` pull-up: `KIN1`, a keyboard column
- `R20` pull-up: `KIN2`, a keyboard column
- `R21` pull-up: `KIN3`, a keyboard column
- `R22` pull-up: `KIN4`, a keyboard column
- `R23` pull-up: `KIN5`, a keyboard column
- `R24` pull-up: `KIN6`, a keyboard column
- `R25` pull-up: `KIN7`, a keyboard column
- `R99` pull-up: `RES_N` (not on the PET: its 555 drove the line)
- `UI8` RAM: the low 4 bits of each byte at $0000–$03FF, on `BBD0`–`BBD3`
- `UI7` RAM: the low 4 bits of each byte at $0400–$07FF, on `BBD0`–`BBD3`
- `UI6` RAM: the low 4 bits of each byte at $0800–$0BFF, on `BBD0`–`BBD3`
- `UI5` RAM: the low 4 bits of each byte at $0C00–$0FFF, on `BBD0`–`BBD3`
- `UI4` RAM: the low 4 bits of each byte at $1000–$13FF, on `BBD0`–`BBD3`
- `UI3` RAM: the low 4 bits of each byte at $1400–$17FF, on `BBD0`–`BBD3`
- `UI2` RAM: the low 4 bits of each byte at $1800–$1BFF, on `BBD0`–`BBD3`
- `UI1` RAM: the low 4 bits of each byte at $1C00–$1FFF, on `BBD0`–`BBD3`
- `UJ8` RAM: the high 4 bits of each byte at $0000, on `BBD4`–`BBD7`, as `UI8`
- `UJ7` RAM: the high 4 bits of each byte at $0400, on `BBD4`–`BBD7`, as `UI7`
- `UJ6` RAM: the high 4 bits of each byte at $0800, on `BBD4`–`BBD7`, as `UI6`
- `UJ5` RAM: the high 4 bits of each byte at $0C00, on `BBD4`–`BBD7`, as `UI5`
- `UJ4` RAM: the high 4 bits of each byte at $1000, on `BBD4`–`BBD7`, as `UI4`
- `UJ3` RAM: the high 4 bits of each byte at $1400, on `BBD4`–`BBD7`, as `UI3`
- `UJ2` RAM: the high 4 bits of each byte at $1800, on `BBD4`–`BBD7`, as `UI2`
- `UJ1` RAM: the high 4 bits of each byte at $1C00, on `BBD4`–`BBD7`, as `UI1`
- `UH1` ROM: BASIC, $C000–$C7FF
- `UH5` ROM: BASIC, $C800–$CFFF
- `UH2` ROM socket: $D000–$D7FF, an empty ROM with our firmware (Commodore's BASIC 1 has 901447-03 here)
- `UH6` ROM socket: $D800–$DFFF, an empty ROM with our firmware (901447-04 in BASIC 1)
- `UH3` ROM: the screen editor and keyboard, $E000–$E7FF
- `UH4` ROM: the machine-code monitor, $F000–$F7FF
- `UH7` ROM: reset, interrupts and the self-test, $F800–$FFFF (the 6502's vectors)
- `UI9` RAM decoder: `SEL0_N`, `SEL1_N`, `BA10`, `BA11` select one 1 KB pair of RAM chips
- `UH8` RAM buffer: `BD0`–`BD3` and the RAM's `BBD0`–`BBD3`, each way
- `UH9` RAM buffer: `BD4`–`BD7` and `BBD4`–`BBD7`, each way
- `UJ9` gates: `RW_RAM`, low while `BPHI2` is high in a write
- `R46` pull-up: the clock gate's enable (a test point there stops the clock)
- `R12` pull-up: the flip-flops' J, K and clear inputs; grounded, it stops the character counters
- `R39` pull-up: `UC5`'s clears, `UD5`'s J and K
- `R43` pull-up: `UA1`'s second clear input
- `R41` pull-up: `UB6`'s clears
- `R27` pull-up: a spare input of `UE8`
- `R40` pull-up: `UD2`'s unused half (the video RAM read when the 6502 is not writing)
- `R44` pull-up: `UC1`'s presets
- `R45` pull-up: `UC1`'s clears

## Test points

What the logic probe shows on a working board, idle at `READY`. **P** is the pulse lamp: it stays lit while the signal keeps changing, and flashes at a single change. Click a pin below with the probe in hand to probe it. Most signals on this board change millions of times a second: the probe shows only P there, and its H or L lamp means nothing.

| Probe | Net | A working board shows |
|---|---|---|
| `X1.OUT` | `OSC8` | P lit: the oscillator, eight million ticks a second |
| `UE2.2A` | `PU_R46` | H, steady: it lets the clock through |
| `UE2.2Y` | `CLK8` | P lit |
| `UD9.3Y` | `HCNT_CLR` | L, steady: the character counters run |
| `UC9.QD` | `CLK1M` | P lit: one tick per character |
| `UF3.PHI0` | `PHI0` | P lit: the 6502's clock |
| `UF3.RES_N` | `RES_N` | H, steady |
| `UF3.RDY`, `UF3.NMI_N` | `RDY`, `NMI_N` | H, steady |
| `UC5.1Q_N` | `HORZ_DRIVE` | P lit |
| `UC5.2Q` | `DIS_ON` | P lit |
| `UD8.4Y` | `VERT_DRIVE` | P lit |
| `UC6.3Y`, `UG8.CB1`, `UA5.PB5` | `VIDEO_ON` | P lit, the same at all three pins |
| `UA1.QD` | `RA2` | P lit |
| `UB2.QH` | `DOT` | P lit |
| `UE9.1Y` | `TV_VIDEO` | P lit |
| `UF3.SYNC`, `UF3.RW`, `UF3.A15`, `UF3.D0` | | P lit: the 6502 runs |
| `UF3.IRQ_N` | `IRQ_N` | H, with P lit: it drops for a moment once a frame, the retrace interrupt |
| `UE5.4Y` | `SEL8` | L. P flashes about three times a second, as the cursor blinks. While you type, P stays lit |
| `UG2.Y0_N`, `UG2.Y14_N` | `SEL0_N`, `SELE_N` | P lit: the 6502 reaches its memory and its editor |
| `UG8.PA0` | `KEY_A` | P lit: the keyboard is scanned |
| `UG8.PB0` | `KIN0` | H, steady. While A is typed again and again: P lit |
| `UG8.PA7` | `DIAG` | H, steady |
| `UG8.CA2` | `BLANK_TV` | H, steady |
| `UA5.CA2` | `GRAPHIC` | L, steady |
| `DS1.K` | `DIAG_LED_N` | H: the LED is dark. P lit: a glitch of a few billionths of a second once a frame, far too short to light it |
| Logic analyzer, a line: clip `UC5.1Q_N` triggered on its rising edge, `UC6.3Y` triggered while high, `UC5.2Q` and `UE9.1Y` | `HORZ_DRIVE`, `VIDEO_ON`, `DIS_ON`, `TV_VIDEO` | sample every 100 ns, 10 µs a division. `HORZ_DRIVE` is high 24 µs in every 64. `DIS_ON` is low while it is high, and high for the 40 µs between. `TV_VIDEO` drops in short pulses, the lit dots, only while `DIS_ON` is high |
| Logic analyzer, a frame: clip `UD8.4Y` triggered on its falling edge, `UC6.3Y` and `UC5.1Q_N` | `VERT_DRIVE`, `VIDEO_ON`, `HORZ_DRIVE` | sample every 10 µs, 2 ms a division. `VERT_DRIVE` is low for about 1.3 ms, then high until the next frame, 16.6 ms later. `VIDEO_ON` rises about 2.6 ms after `VERT_DRIVE` falls and stays high about 12.8 ms: the 200 lines of text. `HORZ_DRIVE` is a dense comb all the way |
| Logic analyzer, the video address: shift-click `UC3.A0` to clip the screen memory's whole address; clip `UC5.2Q` triggered on its rising edge, and `UC6.3Y` triggered while high | `SA0`–`SA9` | sample every 100 ns, 1 µs a division. The address lane counts up by one each microsecond: 000, 001, 002… on the top line of the text. A later row starts 40 further on each time: 028, 050… |
| Logic analyzer, the 6502 starting: shift-click `UF3.A0` and `UF3.D0` to clip both buses; clip `UF3.SYNC` triggered on its rising edge, and `UF3.RW`. Press Arm, then switch the power off and on | `A0`–`A15`, `D0`–`D7` | sample every 100 ns, 1 µs a division. The address reads FFFD, with F8 on the data: the top half of the start address. Then the first instruction: F806 with 78, then F807 with D8. `SYNC` goes high at each new instruction. `RW` stays high |
| Logic analyzer, the 6502 writing the screen: the same buses; clip `UE5.4Y` triggered on its rising edge, `UF3.RW` and `UC6.3Y`. Press Arm, then switch the power off and on | `SEL8`, `RW`, `VIDEO_ON` | sample every 100 ns, 1 µs a division. `SEL8` is high 2 µs, while the address reads 8000. `RW` drops low for the second microsecond: a write. `VIDEO_ON` stays low: the beam is outside the text |

## Troubleshooting

Start from the TV. A rolling or black screen points at the picture. A steady picture that shows the wrong thing points at the computer or the screen memory. The self-test, in the User Manual, names a bad memory chip or ROM directly.

**The TV shows nothing, and the picture does not lock.**

1. Probe `X1.OUT`. It must show P.
2. Probe `UE2.2A`. It must read H. Then `UE2.2Y`, `CLK8`, must show P.
3. Probe `UD9.3Y`. It must read L.
4. Probe `UC9.QD` and `UC5.2Q`. Each must show P.
5. Probe `UC5.1Q_N`, `UD8.4Y` and `UE9.1Y`, the three signals to the TV. Each must show P. Then probe the same signals at `J7`: `J7.HDRIVE`, `J7.VDRIVE` and `J7.VIDEO_N`.

**The picture rolls or tears, but text shows.**

1. Hook the logic analyzer on a frame, as in Test points. `VERT_DRIVE` must fall once every 16.6 ms.
2. Probe `UD8.4Y`, then `J7.VDRIVE`. Both must show P.
3. Do the same for the lines: `UC5.1Q_N`, then `J7.HDRIVE`.

**The picture is steady, but every character is wrong the same way.** A dot always lit or missing, a column doubled, every letter reversed.

1. Hook the logic analyzer on the video address, as in Test points. It must count up by one each microsecond, with no step skipped or repeated.
2. Probe the scan-line counter, `UA1.QB`, `UA1.QC` and `UA1.QD`. Each must show P.
3. Probe the character ROM's outputs, `UA2.O1` to `UA2.O6`. Each must show P, as the text changes along a line. `UA2.O0` and `UA2.O7` read L: letters leave their first and last dot dark. Then probe the same Nets at the shift register, `UB2.B` to `UB2.G`.
4. Probe `UC1.2Q`, the reverse-video bit. On a mostly plain screen it reads mostly L.
5. Run the self-test. A VIDEO RAM row that is not OK names the bad bit.

**The power-on characters stay on the screen, or the screen clears but no banner comes.**

1. Probe `UF3.PHI0`. It must show P.
2. Probe `UF3.RES_N`. It must read H.
3. Probe `UF3.RDY` and `UF3.NMI_N`. Each must read H.
4. Probe `UF3.SYNC`. It must show P. If it does not, the 6502 has stopped.
5. Hook the logic analyzer on the 6502 starting, as in Test points. Compare the addresses and bytes with a working board's. The first wrong one points at an address line, a data line or a ROM.
6. Run the self-test. If it runs, its rows name the bad memory or ROM.

**The banner shows a wrong number of bytes free.** The start-up code stops counting memory at the first bad kilobyte. Run the self-test: its RAM row names the address and the bad bits.

**The picture shows specks while the BW-2001 prints.**

1. Probe `UC6.3Y`, then `UA5.PB5`. Both must show P.
2. Hook the logic analyzer on the 6502 writing the screen, as in Test points. `VIDEO_ON` must be low at every screen write.

**The cursor does not blink, or keys do nothing.**

1. Probe `UF3.IRQ_N`. It must read H with P lit. If P stays dark, probe `UG8.CB1`: it must show P.
2. Probe `UG8.PA0`. It must show P: the program scans the keyboard.
3. Hold down a key that does nothing. Probe the eight columns at PIA1, `UG8.PB0` to `UG8.PB7`. One of them must show P.
4. If none does, probe the columns at the keyboard connector, `J5.COL0` to `J5.COL7`. A column that shows P there but not at `UG8` has lost its wire between them.
5. Run the self-test and hold the key: its KEY row must show the key's legend.

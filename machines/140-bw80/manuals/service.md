# BW-80 — Service Manual

## Components

- `J1` power connector: +5 V and 0 V for the whole board
- `J2` video connector: `VIDEO` (the dots, high for white) and `SYNC` (low during a sync pulse) to the TV, mixed by the ZX80's modulator, which is left out
- `J3` keyboard: the ZX80's membrane, in its own window; its rows are the address lines `A8`–`A15`, its columns `KB0`–`KB4`, a held key joining its column to its row's line through a diode inside the keyboard
- `J4` expansion bus: the ZX80's edge connector, nothing plugged in; the memory side of the data bus `MD0`–`MD7`, `RAM_CS_N`, the address bus, the Z80's control lines and `PHI_N`
- `X1` clock: 6.5 MHz, in place of the ceramic resonator and two gates of `IC20`; `DOT` to `IC20` and the shift clock, halved by `IC18`
- `RST1` power-on reset: holds `RESET_N` low for 1 ms (the ZX80's R21 and C10 hold it about 0.2 s)
- `IC1` Z80A processor: 3.25 MHz on `CLK`; its interrupt pin `IC1.INT_N` is on `A6`, so its refresh counter interrupts it at the end of each line of the picture; `NMI_N`, `BUSRQ_N`, `WAIT_N` pulled up
- `IC2` ROM: the ZX80's 4K program and character set (at $0E00), selected by `ROM_CS1` with `A12` low; its address `RA0`–`RA8` comes from `IC6`–`IC8`, `A9`–`A11` from the CPU
- `IC3` RAM: 1K, bits 7, 1, 0 and 2 of each byte at $4000–$43FF (`MD7`, `MD1`, `MD0`, `MD2`), selected by `RAM_CS_N`, written by `WR_N`
- `IC4` RAM: bits 4, 3, 5 and 6 (`MD4`, `MD3`, `MD5`, `MD6`), as `IC3`; a fast grade: its bit 6 must arrive in time to decide the NOP
- `IC5` data latch: `MD0`–`MD5` and `MD7` into `CH0`–`CH5` and `CH7`, and `NOP_COND_N` into `CHAR_FETCH_N`, while `LE` is high (a memory access outside the refresh, while `PHI_N` is high), frozen through the refresh: the character code for the ROM and the inverse logic, and whether it was a character at all
- `IC6` multiplexer: `RA8` from `A8` or `CH5`; `ROM_CS1` from `ROM_SEL_N` or low in the refresh; `A14B` and `A15B`, the CPU's `A14` and `A15` buffered
- `IC7` multiplexer: `RA4`–`RA7` from `A4`–`A7`, or `CH1`–`CH4` in the refresh (`RFSH_N` low)
- `IC8` multiplexer: `RA0`–`RA3` from `A0`–`A3`, or `LC0`–`LC2` and `CH0` in the refresh
- `IC9` dot shift register: loads a line of a character from `D0`–`D7` at `LOAD_N`, shifts it out on `PIX_N` at each rise of `SHIFT_CLK` (6.5 MHz)
- `IC10` keyboard buffer: `KB0`–`KB4` onto `D0`–`D4` and `EAR` onto `D7` while `KBD_N` is low
- `IC11` gates: `INV_STROBE_N` (the load pulse with `MREQ_N` high: resets the inverse latch); `INV_SET_N` (`LOAD_R` with `CH7`: sets it); the frame sync latch `VS_N` and `VS_RESET`, set by any OUT (`IOWR_N`), reset by a keyboard read
- `IC12` gates: the inverse latch `INV` and `INV_N`; `RAM_SEL_N` (`MREQ` with `A14B`); `ROM_SEL_N` (`MREQ` with `A14B_N`)
- `IC13` inverters: `MREQ`; `LE` from `LE_N`; `MD6_N`; `LOAD` from `LOAD_N`; `CHAR_FETCH` from `CHAR_FETCH_N`; `A14B_N`
- `IC14` NOP gates: pull `D0`, `D2`–`D5` and `D7` low while `NOP_EN` is high (open collector)
- `IC15` open-collector gates: `NOP_EN` from `NOP_N` (pulled up by `R3`); the NOP gates for `D1` and `D6`; `M1_CLK` from `M1_N` (pulled up by `R18`); two paralleled gates making `CLK` from `PHI_N` (pulled up by `R20`)
- `IC16` gates: `LE_N` (`RFSH_N`, `PHI_N`, `MREQ`); `NOP_COND_N` (`HALT_N`, `A15B`, `MD6_N`: a character read from the display file above $C000, the processor not halted); `LOAD_N` (`LOAD_PULSE`, `CHAR_FETCH`, `MREQ_N`)
- `IC17` gates: `IORD_N` (`RD_N` with `IORQ_N`); `IOWR_N` (`WR_N` with `IORQ_N`); `NOP_N` (`NOP_COND_N` with `M1_N`); `KBD_N` (`IORD_N` with `A0`: an even port)
- `IC18` flip-flops: the sync latch, set by `IORQ_N` (the processor answering the interrupt), cleared by `SYNC`, into `SYNC_ARM_N`; the 6.5 MHz divider giving `PHI` and `PHI_N`
- `IC19` flip-flops: `SYNC_NEXT` then `SYNC`, clocked by `M1_CLK`: two instruction reads after the interrupt's answer `SYNC` goes low, two more and it is high again; held low through `VS_N` for the frame sync
- `IC20` XOR gates: `DOT2` (the 6.5 MHz buffered); `VIDEO` (`PIX_N` XOR `INV`); two unused gates (the ZX80's oscillator, replaced by `X1`)
- `IC21` line counter: `LC0`–`LC2`, counting each fall of `SYNC`, cleared by `VS_RESET`: the line within a character
- `R1` pull-down: `EAR` (no cassette: reads low)
- `R2` pull-down: `SHIFT_CLK`, with `C9` the shift clock pulse
- `R3` pull-up: `NOP_EN`
- `R4` data bus resistor: `MD4`–`D4`
- `R5` data bus resistor: `MD3`–`D3`
- `R6` data bus resistor: `MD5`–`D5`
- `R7` data bus resistor: `MD6`–`D6`
- `R8` data bus resistor: `MD2`–`D2`
- `R9` data bus resistor: `MD7`–`D7`
- `R10` data bus resistor: `MD0`–`D0`
- `R11` data bus resistor: `MD1`–`D1`
- `R12` pull-up: `NMI_N`
- `R13` pull-up: `KB0`
- `R14` pull-up: `KB1`
- `R15` pull-up: `KB2`
- `R16` pull-up: `KB3`
- `R17` pull-up: `KB4`
- `R18` pull-up: `M1_CLK`
- `R19` series resistor: `RAM_SEL_N` to `RAM_CS_N`, so the expansion bus can override the RAM select
- `R20` pull-up: `CLK`, the CPU clock
- `R21` pull-up: `RESET_N`
- `R22` pull-up: `BUSRQ_N`
- `R23` pull-up: `WAIT_N`
- `R24` series resistor: `LOAD` to `LOAD_R`, the inverse set gate
- `R25` pull-down: `LOAD_PULSE`, with `C11` the load pulse
- `R28` pull-up: `MD6`, so a keyboard read gives bit 6 high (the UK margin of 56 lines; the US machine's diode D11 is not fitted)
- `C9` coupling capacitor: each edge of `DOT2` onto `SHIFT_CLK`, released by `R2`
- `C11` coupling capacitor: each rise of `PHI` onto `LOAD_PULSE`, released by `R25`

## Test points

What the logic probe shows on a working board, with the K cursor on the screen and no key held. **P** is the pulse lamp: it stays lit for a tenth of a second after each change. Most lines here change millions of times a second: P stays lit, and H or L shows only where the line spends most of its time. Click a pin below with the probe in hand to probe it.

"At start" means the first fifth of a second after power-on, while the BW-80 clears its memory.

| Probe | Net | A working board shows |
|---|---|---|
| `X1.OUT` | `DOT` | P lit |
| `IC18.2Q` | `PHI` | P lit |
| `IC1.CLK` | `CLK` | P lit |
| `IC1.RESET_N`, `RST1.RST_N` | `RESET_N` | L for the first thousandth of a second, then H, steady |
| `IC1.NMI_N`, `IC1.WAIT_N`, `IC1.BUSRQ_N` | `NMI_N`, `WAIT_N`, `BUSRQ_N` | H, steady |
| `IC1.M1_N`, `IC1.MREQ_N`, `IC1.RFSH_N`, `IC1.WR_N` | `M1_N`, `MREQ_N`, `RFSH_N`, `WR_N` | P lit |
| `IC1.A6`, `IC1.A15` | `A6`, `A15` | P lit |
| `IC1.HALT_N` | `HALT_N` | at start: H, steady. Then P lit. On a nearly empty screen the processor is halted through most of each line, so H and L both show |
| `IC1.IORQ_N` | `IORQ_N` | at start: H, steady. Then H, with P lit |
| `IC2.CS1`, `IC6.2Y` | `ROM_CS1` | P lit |
| `IC2.CS2` | `A12` | P lit |
| `IC12.3Y` | `RAM_SEL_N` | P lit |
| `IC3.CS_N`, `IC4.CS_N` | `RAM_CS_N` | P lit |
| `IC5.LE` | `LE` | P lit |
| `IC15.1Y` | `NOP_EN` | at start: L, steady. Then L, with P lit |
| `IC9.CLK` | `SHIFT_CLK` | P lit |
| `IC9.SH_LD_N` | `LOAD_N` | H, with P lit |
| `IC20.2Y` | `VIDEO` | H, with P lit: the page is mostly white |
| `IC12.2Y` | `INV` | L, with P lit |
| `IC18.1Q_N` | `SYNC_ARM_N` | H, with P lit |
| `IC19.1Q` | `SYNC` | H, with P lit |
| `IC21.QB`, `IC21.QC`, `IC21.QD` | `LC0`–`LC2` | P lit |
| `IC17.4Y` | `KBD_N` | at start: H, steady. Then H, with P lit |
| `IC17.2Y` | `IOWR_N` | H, with P lit |
| `IC11.3Y` | `VS_N` | H, with P lit |
| `IC10.A3`, `IC10.A4`, `IC10.A5`, `IC10.A2`, `IC10.A6` | `KB0`–`KB4` | no key held: H, steady. O held: `IC10.A4`, on `KB1`, reads L with P lit; the other four stay H, steady |
| `IC10.A1` | `EAR` | L, steady |

The logic analyzer sees what the probe cannot: the shape of each line, and each character being drawn. It starts capturing at power-on, so each setup below triggers on something that happens only once the picture runs. Clip a group by shift-clicking one of its pins: `IC2.A0` clips the ROM's address lines, `IC2.O0` its data lines.

| Logic analyzer | A working board shows |
|---|---|
| One line and its sync. Clip `IC1.HALT_N`, `IC1.IORQ_N`, `IC18.1Q_N`, `IC1.M1_N`, `IC19.1Q` and `IC21.QB`. Trigger on `IC1.HALT_N` falling. Sample every 100 ns; 20 µs a division | `IC1.HALT_N` low for about 40 µs: the processor halted at the end of an empty row. Then `IC1.IORQ_N` dips: the processor answers the interrupt, and `IC1.HALT_N` goes high. `IC18.1Q_N` dips at once. A moment later `IC19.1Q`, the sync, goes low for about 6 µs. `IC21.QB` changes as the sync falls: the next line of the characters. The pattern repeats every 64 µs |
| A character drawn. Clip `IC1.M1_N`, `IC15.1Y`, `IC1.MREQ_N`, `IC1.RFSH_N`, `IC5.LE`, `IC5.4Q`, `IC5.5Q`, `IC9.SH_LD_N`, `IC12.2Y`, `IC20.2Y` and `IC1.HALT_N`. Trigger on `IC1.M1_N` rising with `IC15.1Y` high. Sample every 10 ns; 500 ns a division | This catches the cursor. `IC15.1Y` is high through the whole instruction read: the NOP. `IC5.4Q` reads low and `IC5.5Q` high: the latch has kept a character fetch, and the cursor's bit 7, set. At the end of the refresh that follows, `IC9.SH_LD_N` dips: the shift register loads. `IC12.2Y` goes high, and `IC20.2Y` goes low for the width of one character: the black square. The next instruction is the HALT: `IC1.HALT_N` falls |
| The character shapes. Clip `IC1.M1_N`, `IC15.1Y`, `IC1.RFSH_N` and `IC9.SH_LD_N`, and the groups `IC2.A0` and `IC2.O0`. Trigger on `IC1.M1_N` rising with `IC15.1Y` high. Sample every 10 ns; 200 ns a division | During the instruction read, the ROM's data lines show B0: the cursor's code, coming from the memory. In the refresh, the ROM's address jumps to F80: the K's page in the character shapes, line 0. The ROM answers 00, the K's blank top line, as `IC9.SH_LD_N` dips. Then the address goes back to the memory's, and the data shows 76: the HALT |
| The frame sync. Clip `IC17.4Y`, `IC11.3Y` and `IC19.1Q`. Trigger on `IC17.4Y` falling. Sample every 100 ns; 100 µs a division | `IC17.4Y` dips nine times, about 25 µs apart: the keyboard read. `IC11.3Y` and `IC19.1Q` go low with the first dip, and stay low for about 390 µs. Then the line syncs start again on `IC19.1Q`, every 64 µs. This happens once a frame, 50 times a second |
| The keyboard rows. Hold O. Clip `IC17.4Y`, `IC1.A8` to `IC1.A15`, `IC10.A3`, `IC10.A4` and `IC10.A5`. Trigger on `IC17.4Y` falling. Sample every 100 ns; 20 µs a division | At each dip of `IC17.4Y`, one of `IC1.A8` to `IC1.A15` is low and the other seven are high: `A8` at the first read, `A15` at the eighth. `IC10.A4` is high at every read but the sixth, where `A13` is low: O joins `KB1` to row `A13`. `IC10.A3` and `IC10.A5` stay high. Between the reads, `IC10.A4` is often low too: the held key pulls it down whenever `A13` is low |

## Troubleshooting

**The TV never locks.** The processor makes the sync, so this means it is not running its program properly.

1. Probe `IC1.CLK`. It must show P. If not, probe `X1.OUT`, then `IC18.2Q`.
2. Probe `IC1.RESET_N`. It must read H.
3. Probe `IC1.M1_N` and `IC1.MREQ_N`. Each must show P. If they are still, the processor is stopped.
4. Probe `IC2.CS1`, then `IC6.2Y`. Both must show P: the ROM is picked. They are one wire.
5. Probe `IC1.HALT_N` and `IC17.4Y` about a second after power-on. Both must show P. If both stay H, steady, the program never gets past the memory check: suspect the memory, `IC3` and `IC4`, or the data lines.
6. Probe the data lines on both sides: `IC1.D0` to `IC1.D7`, and `IC5.3D`, `IC5.6D` and the other inputs of `IC5`. Each must show P. A line stuck on one side but moving on the other points at its resistor, or at a gate of `IC14` or `IC15` that holds it low.
7. Probe `IC15.1Y`. It must not be H, steady: that holds the processor's data lines low.

**The picture rolls, or tears, but the keys work.**

1. Set up the frame sync on the logic analyzer. `IC11.3Y` must go low with the keyboard read. If it never does, follow `KBD_N` from `IC17.4Y` to `IC11`.
2. Set up one line and its sync. Each dip of `IC1.IORQ_N` must be followed by `IC18.1Q_N` dipping, then `IC19.1Q` low for about 6 µs.
3. Probe `IC19.1Q`. It must show P. If it is still, check `IC15.4Y`, the clock of `IC19`: it must show P.

**The picture locks, but the characters are wrong.** The text is right in memory; the drawing goes wrong.

1. Every character the same, or blank: set up a character drawn. `IC15.1Y` must be high through the instruction read, and `IC9.SH_LD_N` must dip after the refresh.
2. Letters swapped for other letters: the code goes wrong on its way to the ROM. Probe `IC5.3Q` to `IC5.5Q` and the other outputs of `IC5`: each must show P. Then follow `CH0`–`CH5` to `IC6`–`IC8`.
3. Characters with lines doubled, missing or swapped: the line counter. Probe `IC21.QB`, `IC21.QC` and `IC21.QD`, then `IC8.3A`, `IC8.2A` and `IC8.4A`. Each must show P.
4. No dots at all, or a white page: probe `IC9.CLK`, then `IC9.QH_N`. Each must show P.
5. The cursor not white on black: probe `IC12.2Y`. It must show P.
6. Junk in the margins, outside the text: the shift register loads when it should not. Set up a character drawn, and check that `IC9.SH_LD_N` stays H after the HALT.

**A key does nothing.**

1. Set up the keyboard rows on the logic analyzer, and hold the key. Find its row and column in the User Manual. Its column must be low at the read where its row is low.
2. Five keys dead, one row: check the row's address line from `IC1` to `J3`.
3. Eight keys dead, one in each row: the column. Probe its pin on `IC10` with no key held: it must read H. Hold a key of that column: it must read L with P. If the key pulls the line low at `J3` but not at `IC10`, the wire between them is broken.
4. No key works at all: probe `IC17.4Y`. It must show P. Then probe `IC10.G1_N`: it must read the same.

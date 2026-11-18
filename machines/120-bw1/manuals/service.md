# BW-1 — Service Manual

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `PHI0` of `U1`
- `J1` power connector: +5 V and 0 V for the whole board
- `U1` processor: a 6502; reads and writes on `A0`–`A12` (A13 to A15 go nowhere) and `D0`–`D7`; `RW` low while it writes, `SYNC` high on each instruction read; stops into the monitor when `NMI_N` falls
- `U2` monitor: a 6530-002; its program at $1C00–$1FFF (selected by `K7`), its memory at $17C0–$17FF and its ports at $1740 (selected by `K5`); `PA0`–`PA6` light the segments and read the keypad's columns, `PB1`–`PB4` pick a digit or a row of keys through `U24`
- `U3` self-test: a 6530-003; its program at $1800–$1BFF (selected by `K6`), its memory at $1780–$17BF and its ports at $1700 (selected by `K5`; the ports are not wired)
- `U4` address decoder: `A10`–`A12` pick one 1K block: `K0` low for $0000–$03FF, `K5` for $1400–$17FF, `K6` for $1800–$1BFF, `K7` for $1C00–$1FFF; A13 to A15 are ignored, so everything repeats every 8K
- `U5` memory: bit 7 of each byte at $0000–$03FF, selected by `K0`, written while `RAM_RW` is low
- `U6` memory: bit 6, as `U5`
- `U7` memory: bit 5, as `U5`
- `U8` memory: bit 4, as `U5`
- `U9` memory: bit 3, as `U5`
- `U10` memory: bit 2, as `U5`
- `U11` memory: bit 1, as `U5`
- `U12` memory: bit 0, as `U5`
- `U13` memory gate: puts `DOUT0`–`DOUT3` on `D0`–`D3` while `BUF_N` is low
- `U14` memory gate: puts `DOUT4`–`DOUT7` on `D4`–`D7` while `BUF_N` is low
- `U15` gates: `RAM_RW` low during the second half of a write tick, `BUF_N` low while the processor reads the main memory, `SST_N` low on an instruction read outside $1C00–$1FFF
- `U16` inverters: `RW_N` and `K0_N`, for `U15`
- `U17` segment driver: `PA0`–`PA6` onto `SEG_A`–`SEG_G`, the segments of every digit
- `U24` display and keypad scanner: from `PB1`–`PB4`, outputs 0–2 pull `ROW0`–`ROW2` low, outputs 4–9 pull `DIG1`–`DIG6` low, one at a time; output 3 (pin 4, `STRAP`) is the self-test strap: joined to `PA0`, a reset runs the self-test in `U3` instead of the monitor
- `U25` power-on reset: holds `RES_N` low for 20 ms after power-on, then lets go
- `DS1` digit: the leftmost, lit while `DIG1` is low
- `DS2` digit: lit while `DIG2` is low
- `DS3` digit: lit while `DIG3` is low
- `DS4` digit: lit while `DIG4` is low
- `DS5` digit: lit while `DIG5` is low
- `DS6` digit: the rightmost, lit while `DIG6` is low
- `SW1` key 0: joins `ROW0` to `PA6`
- `SW2` key 1: `ROW0` to `PA5`
- `SW3` key 2: `ROW0` to `PA4`
- `SW4` key 3: `ROW0` to `PA3`
- `SW5` key 4: `ROW0` to `PA2`
- `SW6` key 5: `ROW0` to `PA1`
- `SW7` key 6: `ROW0` to `PA0`
- `SW8` key 7: `ROW1` to `PA6`
- `SW9` key 8: `ROW1` to `PA5`
- `SW10` key 9: `ROW1` to `PA4`
- `SW11` key A: `ROW1` to `PA3`
- `SW12` key B: `ROW1` to `PA2`
- `SW13` key C: `ROW1` to `PA1`
- `SW14` key D: `ROW1` to `PA0`
- `SW15` key E: `ROW2` to `PA6`
- `SW16` key F: `ROW2` to `PA5`
- `SW17` key AD: `ROW2` to `PA4`
- `SW18` key DA: `ROW2` to `PA3`
- `SW19` key +: `ROW2` to `PA2`
- `SW20` key GO: `ROW2` to `PA1`
- `SW21` key PC: `ROW2` to `PA0`
- `SW22` key ST: pulls `NMI_N` low
- `SW23` key RS: pulls `RES_N` low
- `SW24` SST slide switch: at ON, joins `NMI_N` to `SST_N`, so the processor stops after each instruction outside the monitor
- `R1` pull-up on `K0`
- `R2` pull-up on `K5`
- `R3` pull-up on `K6`
- `R4` pull-up on `K7`
- `R5` pull-up on `RES_N`
- `R6` pull-up on `NMI_N`
- `R7` pull-up on `IRQ_N`

## Test points

What the logic probe shows on a working board, with the monitor showing an address and no key held. **P** is the pulse lamp: it stays lit for a tenth of a second after each change. Most lines here change a million times a second: P stays lit, and H or L shows only where the line spends most of its time.

| Probe | Net | A working board shows |
|---|---|---|
| `X1.OUT`, `U1.PHI0` | `PHI0` | P lit |
| `U1.PHI2` | `PHI2` | P lit |
| `U25.RST_N`, `U1.RES_N`, `U2.RES_N`, `U3.RES_N` | `RES_N` | L for the first 20 thousandths of a second, then H, steady. RS held: L |
| `U1.NMI_N`, `SW22` pin 1 | `NMI_N` | H, steady. ST held: L |
| `U1.IRQ_N` | `IRQ_N` | H, steady |
| `U1.SYNC` | `SYNC` | P lit |
| `U1.A0`, `U1.D0`, `U1.D7` | `A0`, `D0`, `D7` | P lit |
| `U1.RW` | `RW` | H, and P lit: the monitor writes to its notes and its ports all the time |
| `U4.A`, `U4.B`, `U4.C` | `A10`, `A11`, `A12` | P lit |
| `U4.D` | `GND` | L, steady |
| `U4.Y0_N`, `U5.CE_N` | `K0` | P lit |
| `U4.Y5_N`, `U2.CS1` | `K5` | H, and P lit |
| `U4.Y6_N`, `U3.RS0` | `K6` | H, steady: only the self-test uses it |
| `U4.Y7_N`, `U2.RS0` | `K7` | L, and P lit: the monitor runs from `U2` |
| `U15.1Y` | `RAM_RW` | H, and P lit |
| `U15.2Y`, `U13.1OE_N` | `BUF_N` | H, and P lit |
| `U15.3Y`, `SW24` pin 1 | `SST_N` | H, steady while the monitor waits for a key. P lit while a program of yours runs |
| `U2.PB1`, `U2.PB2`, `U2.PB3`, `U2.PB4` | `PB1`–`PB4` | P lit |
| `U2.PA0` to `U2.PA6` | `PA0`–`PA6` | P lit |
| `U17.1Y1`, `DS1.A` | `SEG_A` | P lit. The other segments, `U17.1Y2` to `U17.2Y3`, the same while a digit shows them |
| `U24.Y4_N`, `DS1.CC1` | `DIG1` | L for a moment, then no lamp, over and over; P stays dark. `U24` only pulls the line low or lets it go. `DIG2` to `DIG6`, on `U24` pins 6, 7, 9, 10 and 11, the same |
| `U24` pin 4 | `STRAP` | no lamp: the monitor picks this line only once, just after a reset |

A program of yours running, for example a loop at 0200: the display goes dark. `U4.Y7_N` then reads H, steady, `U4.Y0_N` L, and `U15.3Y` shows P.

The logic analyzer sees what the probe cannot: each tick of the processor, and each step of the display. Clip a group by shift-clicking one of its pins: `U1.A0` clips the address lines `U1 A`, `U1.D0` the data lines `U1 D`. A group's trigger is an address in hex.

| Logic analyzer | A working board shows |
|---|---|
| The monitor reading the display's byte. Clip the groups `U1 A` and `U1 D`, and `U1.RW` and `U1.SYNC`. Trigger on `U1 A` = 1F19. Sample every 100 ns; 1 µs a division | Just before the trigger, a write to 01FE: `RW` low, the monitor keeping its way back. Then `SYNC` high at 1F19 and 1F1B, two instructions read from the monitor. Then reads at 00FA and 00FB give 00 and 02: the address shown. Last, a read at 0200 gives the byte shown |
| The digit scan. Clip `U2.PB1`, `U2.PB2`, `U2.PB3`, `U2.PB4`, and the seven segment outputs `U17.1Y1`, `U17.1Y2`, `U17.1Y3`, `U17.1Y4`, `U17.2Y1`, `U17.2Y2`, `U17.2Y3`. Trigger on `U2.PB3` rising. Sample every 1 µs; 500 µs a division | `PB1`–`PB4` count 4, 5, 6, 7, 8, 9, about half a thousandth of a second each: one digit each, left to right. While each count lasts, the segments hold that digit's pattern: a, b, c, d, e and f high for the 0 in `DS1`, then a, b, d, e and g for the 2 in `DS2`. After the sixth digit, a short gap for the keypad, and the round starts again about every 3.5 thousandths of a second |
| A key read. Hold GO. Clip `U2.PB1`, `U2.PB2`, `U2.PB3`, `U2.PB4`, `U2.PA0`, `U2.PA1` and `U2.PA2`. Trigger on `U2.PB3` rising. Sample every 1 µs; 100 µs a division, centred about 3.3 thousandths of a second after the trigger | After the sixth digit, `PB1`–`PB4` read 0, 1, then 2: rows `ROW0`, `ROW1`, `ROW2` in turn. While row 2 is picked, `PA1` is low: GO joins `ROW2` to `PA1`. `PA0` and `PA2` stay high there. Any other key shows the same way, on its own row and column |

## Troubleshooting

**The digits stay dark.**

1. Probe `U1.PHI0`. It must show P.
2. Probe `U1.RES_N`. It must read H. If it stays L, probe `U25.RST_N` and `SW23` pin 1: it is the same wire.
3. Probe `U1.SYNC`. It must show P. If it does not, the processor is stopped.
4. Probe `U4.A`, `U4.B` and `U4.C`. Each must show P. Probe `U4.D`: it must read L.
5. Probe `U4.Y7_N`. It must read L with P: the processor reads the monitor. If it stays H, probe `U4.Y0_N`, `U4.Y5_N` and `U4.Y6_N`. A select that never moves, or one low all the time, points at the decoder or its address lines.
6. Probe `U2.PB1` to `U2.PB4`. Each must show P: the monitor is scanning.
7. Run the self-test (User Manual). It needs no memory. If it shows a test number and stops, read the code: it names the address and the bits. If the self-test stays dark too, the fault is in steps 1 to 5.

**The digits show something, but it freezes, or makes no sense.**

1. The monitor keeps its notes and its way back in the main memory, at $0000–$01FF. Bad memory stops it. Run the self-test.
2. Set up the monitor reading the display's byte on the logic analyzer. The reads at 00FA and 00FB must give 00 and 02 after a reset. The data lines must change freely: two lines that always match are joined.
3. Probe `U15.2Y` and `U13.1OE_N`. They must show P together. Probe `U15.1Y`: it must show P, and `U5.CE_N` likewise.

**A segment or a digit is wrong.**

1. Set up the digit scan on the logic analyzer.
2. One segment wrong on every digit: follow it from `U2`'s `PA0`–`PA6` to `U17`, then to the digits. The segment lines go to all six digits, one after the other: a segment missing only from the right-hand digits is lost between two of them.
3. One digit dark, or lit with every other: probe its line on `U24`, pins 5, 6, 7, 9, 10 and 11 for `DS1` to `DS6`. It must read L only for a moment, then let go.
4. One character always wrong, wherever it shows: the monitor's table of digit shapes is in `U2`. The self-test checks it as test 4.

**A key does nothing.**

1. Set up the key read on the logic analyzer, and hold the key. Its column must go low while its row is picked.
2. Three keys dead in one column (for example DA, A and 3, all on `PA3`): the column wire is broken between `U2` and the keys.
3. Seven keys dead in one row: probe the row on `U24`, pins 1, 2 and 3 for `ROW0` to `ROW2`.
4. No key works at all: the monitor waits for every key to be up before it reads the next. A key that never lets go blocks them all. Run the self-test to its keypad test: with no hand on the keypad, it must show PA55 and nothing after it.

**ST, RS or SST does nothing.**

1. ST: probe `U1.NMI_N` while you hold ST. It must read L.
2. RS: probe `U1.RES_N` while you hold RS. It must read L.
3. SST: run a program of yours with SST at OFF. Probe `U15.3Y`: it must show P. Then probe `U4.Y7_N`: it must stay H while your program runs. At ON, `U1.NMI_N` must show P too. In the monitor, `U15.3Y` must stay H, or SST stops the monitor itself.

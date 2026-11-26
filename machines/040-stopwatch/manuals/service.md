# Stopwatch — Service Manual

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `CLK`
- `J1` power connector: +5 V and 0 V for the whole board
- `SW1` Start button: pulls `START_N` low while pressed
- `SW2` Stop button: pulls `STOP_N` low while pressed
- `SW3` Reset button: pulls `RST_N` low while pressed
- `R1` pull-up: holds `START_N` high while `SW1` is released
- `R2` pull-up: holds `STOP_N` high while `SW2` is released
- `R3` pull-up: holds `RST_N` high while `SW3` is released
- `R4` digit resistor: feeds the anodes of `DS1` (`DS1_CA`) and limits their current
- `R5` digit resistor: feeds the anodes of `DS2` (`DS2_CA`) and limits their current
- `R6` digit resistor: feeds the anodes of `DS3` (`DS3_CA`) and limits their current
- `U1` prescaler: divides `GCLK` (1 MHz) by ten into `F100K` (100 kHz)
- `U2` prescaler: divides `F100K` (100 kHz) by ten into `F10K` (10 kHz)
- `U3` prescaler: divides `F10K` (10 kHz) by ten into `F1K` (1 kHz)
- `U4` prescaler: divides `F1K` (1 kHz) by ten into `F100` (100 Hz)
- `U5` seconds counter: counts 0 to 9 on `S0`–`S3`, moved on by `T3`; cleared by `RESET`
- `U6` tenths counter: counts 0 to 9 on `T0`–`T3`, moved on by `H3`, carries to `U5` on `T3`; cleared by `RESET`
- `U7` hundredths counter: counts 0 to 9 on `H0`–`H3`, moved on by `F100`, carries to `U6` on `H3`; cleared by `RESET`
- `U8` seconds decoder: `S0`–`S3` to the segments of `DS1`
- `U9` tenths decoder: `T0`–`T3` to the segments of `DS2`
- `U10` hundredths decoder: `H0`–`H3` to the segments of `DS3`
- `U11` run flip-flop: `RUN` set by `START_N`, cleared by `STOP_N`
- `U12` clock gate and reset: `GCLK` from `CLK` while `RUN` is high; `RESET` high while `RST_N` is low
- `DS1` seconds digit: a segment lights while its `U8` output is low; its decimal point is always lit
- `DS2` tenths digit: a segment lights while its `U9` output is low
- `DS3` hundredths digit: a segment lights while its `U10` output is low

## Test points

What the logic probe shows on a working board. **P** is the pulse lamp: it stays lit while the signal keeps changing. Click a pin below with the probe in hand to probe it.

"Running" means after START, "stopped" after STOP. The times are counted from 0.00 on the display.

| Probe | Net | A working board shows |
|---|---|---|
| `U12.1A` | `CLK` | P lit, always |
| `U11.1PRE_N` | `START_N` | H; L while START is held |
| `U11.1CLR_N` | `STOP_N` | H; L while STOP is held |
| `U12.2A` | `RST_N` | H; L while RESET is held |
| `U11.1Q` | `RUN` | H while running, L while stopped |
| `U12.1Y` | `GCLK` | P lit while running. H, steady, while stopped |
| `U12.2Y`, `U1.R01` (the same on every counter's R01 and R02 pins) | `RESET` | L; H while RESET is held |
| `U1.QD`, `U2.QD`, `U3.QD`, `U4.QD` | `F100K`, `F10K`, `F1K`, `F100` | P lit while running; no P while stopped |
| `U4.QD`, `U7.QA`, `U7.QD`, `U6.QA` | `F100`, `H0`, `H3`, `T0` | Hook all four on the oscilloscope, at 10 ms a division, trigger on `U4.QD` rising, while running. `F100` is a short high pulse every 10 ms. `H0` changes at each of its falls. `H3` is high for 20 ms in every 100 ms. When `H3` falls, `T0` changes |
| `U6.QD` | `T3` | while running: H for two tenths of every second (at .8 and .9), L the rest; P flashes at each change |
| `U5.QA` | `S0` | while running: H for one second, L for the next; P flashes at each change |
| `U5.QD` | `S3` | while running: H only at 8 and 9 seconds, L the rest |
| `U8.SB_N` | `DS1_B` | L while the seconds read 1: bar b lit |
| `U8.SA_N` | `DS1_A` | while the seconds read 1: neither H nor L, the decoder lets go of a dark bar. L at 2: bar a lit |
| `DS1.CA1` | `DS1_CA` | H, steady (the same on `DS2.CA1` and `DS3.CA1`) |

The decoders' outputs work the same way on every digit. A lit bar reads L. A dark bar lights neither H nor L.

## Troubleshooting

**Nothing counts after START.**

1. Probe `U12.1A`, on `CLK`. It must show P.
2. Probe `U11.1PRE_N` while you press START. It must go L.
3. Probe `U11.1Q`, on `RUN`. After START, it must read H.
4. Probe `U12.1Y`, on `GCLK`. While running, it must show P.
5. Probe `U1.R01`, on `RESET`. With RESET released, it must read L.
6. Follow the clock down the chain: `U1.QD`, `U2.QD`, `U3.QD`, `U4.QD`. Each must show P. The first that stays dark is where the count stops. Check that chip and the wire from its `QD` to the next chip's `CKA`.

**STOP does nothing, or RESET does nothing.**

1. STOP: probe `U11.1CLR_N` while you press STOP. It must go L. Then `U11.1Q` must read L, and `U12.1Y` must stop showing P.
2. RESET: probe `U12.2A` while you press RESET. It must go L. Then `U12.2Y` must read H.
3. While RESET is held, probe `U1.R01` and `U1.R02`, then the same pins on `U2`–`U7`. Each must read H.

**One digit never moves, or skips numbers.**

1. Probe the counter's `QA` and its `CKB`: for the tenths, `U6.QA` and `U6.CKB`. It is one wire, so they must read the same.
2. Probe the counter's four outputs, `QA` to `QD`. Each must change: `QA` the most often, `QD` the least.
3. If the digit never moves at all, probe its input: `U7.CKA` for the hundredths, `U6.CKA` for the tenths, `U5.CKA` for the seconds. It must change as the digit to its right rolls over from 9 to 0.
4. Check that the counter has +5 V on pin 5. This chip has it there, not on pin 14.

**A digit draws wrong shapes, or a bar never lights.**

Remember: this decoder draws 6 without its top bar and 9 without its bottom bar.

1. Probe the decoder's inputs `A`–`D`, for example `U8.A`, `U8.B`, `U8.C`, `U8.D`. Each must read the same as the counter output it comes from.
2. Probe the decoder output for the bad bar, for example `U8.SE_N` for bar e of the seconds. It must read L whenever the bar should be lit.
3. Probe the same wire at the digit's pin. It must read the same as the decoder's pin.
4. If a whole digit is dark, probe its anode: `DS1.CA1` must read H.

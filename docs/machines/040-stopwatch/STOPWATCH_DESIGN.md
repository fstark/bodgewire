# 040 — Stopwatch — Machine Design

How the Machine in [STOPWATCH.md](STOPWATCH.md) is built: netlist, Board, Puzzles, manuals. The code it needs comes with P4f ([SEVEN_SEGMENT.md](../../p4f-seven-segment/SEVEN_SEGMENT.md)).

## Files

Machine name `stopwatch`:

```
netlists/stopwatch.bwn               design netlist
boards/stopwatch/stopwatch.kicad_pcb Board (KiCad 10, laid out by hand)
manuals/stopwatch/user.md, service.md
puzzles/stuck_at_zero.bwp, nineteen.bwp, eights.bwp, broken_bar.bwp, non_stop.bwp, dark_seconds.bwp
```

## Netlist

`netlists/stopwatch.bwn` (STOPWATCH.md, Circuit), with the header comment of [ADD_MACHINE.md](../../recipes/ADD_MACHINE.md) step 2 (Machine 040, this spec's path, one-paragraph summary). References by column: seconds `U5` → `U8` → `DS1` (anode resistor `R4`), tenths `U6` → `U9` → `DS2` (`R5`), hundredths `U7` → `U10` → `DS3` (`R6`).

Resistors only go between a rail and a signal (the builder refuses series resistors), so each digit has one resistor on its common anode instead of one per segment: logic-only, the two are the same (a segment lights while its pin is low and the anode high). Every unused TTL input is tied, so the netlist loads without warnings; unused outputs (`U1`–`U4` `QB`/`QC`, `U11` `1Q_N`, `2Q`, `2Q_N`) and the `DP` of `DS2`, `DS3` stay unconnected.

```
part X1 OSC freq=1MHz
part J1 POWER
part SW1 BUTTON
part SW2 BUTTON
part SW3 BUTTON
part R1 R value=4k7
part R2 R value=4k7
part R3 R value=4k7
part R4 R value=330
part R5 R value=330
part R6 R value=330
part U1 74LS90
...
part U7 74LS90
part U8 74LS47
part U9 74LS47
part U10 74LS47
part U11 74LS74
part U12 74LS00
part DS1 DIGIT
part DS2 DIGIT
part DS3 DIGIT

# Buttons, run flip-flop, clock gate, reset
net CLK      X1.OUT U12.1A
net START_N  R1.2 SW1.1 U11.1PRE_N
net STOP_N   R2.2 SW2.1 U11.1CLR_N
net RST_N    R3.2 SW3.1 U12.2A U12.2B
net RUN      U11.1Q U12.1B
net GCLK     U12.1Y U1.CKA
net RESET    U12.2Y U1.R01 U1.R02 U2.R01 U2.R02 U3.R01 U3.R02 U4.R01 U4.R02
net RESET    U5.R01 U5.R02 U6.R01 U6.R02 U7.R01 U7.R02

net VCC      R1.1 R2.1 R3.1 R4.1 R5.1 R6.1 U11.2PRE_N U11.2CLR_N
net VCC      U8.LT_N U8.RBI_N U8.BI_N U9.LT_N U9.RBI_N U9.BI_N U10.LT_N U10.RBI_N U10.BI_N
net GND      SW1.2 SW2.2 SW3.2 DS1.DP U11.1D U11.1CLK U11.2D U11.2CLK
net GND      U12.3A U12.3B U12.4A U12.4B
net GND      U1.R91 U1.R92 U2.R91 U2.R92 U3.R91 U3.R92 U4.R91 U4.R92
net GND      U5.R91 U5.R92 U6.R91 U6.R92 U7.R91 U7.R92

# Prescaler: 1 MHz to 100 Hz
net F500K    U1.QA U1.CKB
net F100K    U1.QD U2.CKA
net F50K     U2.QA U2.CKB
net F10K     U2.QD U3.CKA
net F5K      U3.QA U3.CKB
net F1K      U3.QD U4.CKA
net F500     U4.QA U4.CKB
net F100     U4.QD U7.CKA

# Display counters
net H0       U7.QA U7.CKB U10.A
net H1       U7.QB U10.B
net H2       U7.QC U10.C
net H3       U7.QD U10.D U6.CKA
net T0       U6.QA U6.CKB U9.A
net T1       U6.QB U9.B
net T2       U6.QC U9.C
net T3       U6.QD U9.D U5.CKA
net S0       U5.QA U5.CKB U8.A
net S1       U5.QB U8.B
net S2       U5.QC U8.C
net S3       U5.QD U8.D

# Digits: anode through one resistor, segments straight on the decoder (DS1 shown; DS2 with U9, R5; DS3 with U10, R6)
net DS1_CA   R4.2 DS1.CA1 DS1.CA2
net DS1_A    U8.SA_N DS1.A
net DS1_B    U8.SB_N DS1.B
...
net DS1_G    U8.SG_N DS1.G
```

`SW1`–`SW3` pad 1 is the signal end, pad 2 the `GND` end. `DS1.DP` on `GND` is lit for good (the cathode at a Supply 0, the anode high through `R4`).

## Board

`bw export-kicad netlists/stopwatch.bwn`, then placed and routed by hand in KiCad ([KICAD_GUIDE.md](../../p2a-board-layout/KICAD_GUIDE.md)):

- `DS1` `DS2` `DS3` in a row near the front edge, upright: the pad 1 row (pins 1–5) at the bottom. The Board view draws each digit standing on its pad 1 side.
- `SW1` Start, `SW2` Stop, `SW3` Reset along the front edge below the digits, with `R1`–`R3`.
- Behind each digit its decoder (`U8`, `U9`, `U10`) and anode resistor, then its counter (`U5`, `U6`, `U7`); prescaler `U1`–`U4`, `X1`, `U11`, `U12` and `J1` at the back.
- Wanted for the Puzzles (each needs a straight segment away from pads and corners):
  - `T0`: `U6` pin 1 (`CKB`) at an end of the Trace, so one crack cuts it off alone (e.g. route `U9` pin 7 → `U6` pin 12 → `U6` pin 1).
  - `RUN` and `DS1_E`: any route (two pads each).
  - `VCC` at `U5` pin 5: a branch that feeds only that pad, so a crack there unpowers `U5` alone.
- `bw check-board` clean; `drc_stopwatch` passes (picked up by the `boards/*/*.kicad_pcb` glob).
- Cracks (pads and positions filled in after the layout, P4e syntax):
  - `T0`: `U6.1 <neighbour pad> N%`.
  - `DS1_E`: `U8.9 DS1.1 N%`.
  - `RUN`: `U11.5 U12.2 N%`.
  - `VCC`: `U5.5 <neighbour pad> N%`.

## Puzzles

`machine stopwatch` in each file (STOPWATCH.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `stuck_at_zero.bwp` | `fault stuck U12.2Y 1` | `RESET` |
| `nineteen.bwp` | `fault crack U6.1 … N%` | `T0` at `U6` `CKB` |
| `eights.bwp` | `fault stuck U9.SG_N 0` | `DS2_G` |
| `broken_bar.bwp` | `fault crack U8.9 DS1.1 N%` | `DS1_E` |
| `non_stop.bwp` | `fault crack U11.5 U12.2 N%` | `RUN` |
| `dark_seconds.bwp` | `fault crack U5.5 … N%` | `VCC` at `U5` |

Briefs:

- Stuck at zero: "The stopwatch shows 0.00 and none of the buttons makes it count."
- Nineteen: "The stopwatch never gets past 0.19: it starts again from 0.00."
- Eights: "After Reset the stopwatch reads 0.80, and some numbers look wrong."
- Broken bar: "The seconds digit draws some numbers wrongly: its 0 has a gap."
- Non-stop: "Stop does nothing: the stopwatch keeps counting."
- Dark seconds: "The left digit shows only its decimal point; the other two count normally."

## Electrical Nets on a damaged Board

- Nineteen: `U6` pin 1 alone on its island, named `T0` from `U6`'s own Connection or `T0~2`, depending on Connection order; it has no Driver: floating, reads high, so `U6`'s ÷5 section never clocks. After Reset it holds 0: tenths show only `T0`.
- Broken bar: `DS1` pin 1 alone on its island, no Driver: floating, so segment `E` stays dark (a floating cathode never lights).
- Non-stop: `U12` pin 2 alone, floating: gate 1 is an inverter of `CLK`, whatever `RUN` says.
- Dark seconds: `U5` pin 5 on a `VCC~2` island with no supply pin: `U5` is not Powered, its Pins get private Nets. `S0`–`S3` hold only TTL inputs of `U8`: floating, read high, `U8` decodes 15 (blank). `DS1`'s decimal point does not involve `U5` and stays lit.

## Manuals

- User: What it does (one paragraph: power `J1`; `SW1` starts, `SW2` stops, `SW3` resets to 0.00; the display `DS1` `DS2` `DS3` reads seconds, tenths and hundredths up to 9.99, then starts again; at power-on it may show anything, press Stop and Reset).
- Service: Components, one bullet per reference starting with it, netlist order (`CheckServiceManual`):
  - `X1` clock: 1 MHz crystal oscillator, drives `CLK`
  - `J1` power connector: +5 V and 0 V for the whole board
  - `SW1` Start button: pulls `START_N` low while pressed; `SW2` Stop button: `STOP_N`; `SW3` Reset button: `RST_N`
  - `R1`, `R2`, `R3` pull-ups: hold `START_N`, `STOP_N`, `RST_N` high while released
  - `R4`, `R5`, `R6` digit resistors: feed the anodes of `DS1`, `DS2`, `DS3` (`DS1_CA` …) and limit their current
  - `U1`–`U4` prescaler: each divides by ten, naming its input and output (`U1`: `GCLK` 1 MHz → `F100K`, …, `U4`: `F1K` → `F100` 100 Hz)
  - `U5` seconds counter, `U6` tenths counter, `U7` hundredths counter: count 0 to 9 on their `CKA`, carry on `QD`; cleared by `RESET`
  - `U8`, `U9`, `U10` decoders: `S0`–`S3` (`T0`–`T3`, `H0`–`H3`) to the segments of `DS1` (`DS2`, `DS3`)
  - `U11` run flip-flop: `RUN` set by `START_N`, cleared by `STOP_N`
  - `U12` clock gate and reset: `GCLK` from `CLK` while `RUN` is high; `RESET` high while `RST_N` is low
  - `DS1` seconds digit, `DS2` tenths digit, `DS3` hundredths digit: a segment lights while its decoder output is low; `DS1`'s decimal point always lit

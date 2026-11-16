# 045 — Knob Meter — Machine Design

How the Machine in [KNOB_METER.md](KNOB_METER.md) is built: netlist, Board, Puzzles, manuals. The code it needs comes with P4i ([PADDLE.md](../../../docs/p4i-paddle/PADDLE.md)).

## Files

Machine name `knob`:

```
knob.bwn              design netlist
board/knob.kicad_pcb  Board (KiCad 10, laid out by hand)
manuals/user.md, service.md
puzzles/NN-*.bwp      5 Puzzles, easiest first
```

## Netlist

`machines/045-knob-meter/knob.bwn` (KNOB_METER.md, Circuit), with the header comment of [ADD_MACHINE.md](../../../docs/recipes/ADD_MACHINE.md) step 2 (Machine 045, this spec's path, one-paragraph summary). References by column: tens `U7` → `U9` → `U11` → `DS1` (`R1`), units `U8` → `U10` → `U12` → `DS2` (`R2`).

Every unused TTL input is tied, so the netlist loads without warnings; unused outputs (`U1`–`U4` `QB`/`QC`, `U5` nothing, `U6` `3Y`/`4Y`, `U9`/`U10` `Q_N`) and both digits' `DP` stay unconnected. `J2`'s `VCC` and `GND` join the rails implicitly, as a chip's do.

```
part X1 OSC freq=1MHz
part J1 POWER
part J2 PADDLE min=30us max=9.93ms
part R1 R value=330
part R2 R value=330
part U1 74LS90
...
part U5 74LS90
part U6 74LS00
part U7 74LS90
part U8 74LS90
part U9 74LS175
part U10 74LS175
part U11 74LS47
part U12 74LS47
part DS1 DIGIT
part DS2 DIGIT

# Timebase: 1 MHz to the 10 kHz count clock and 100 Hz; U5 steps every 10 ms
net CLK      X1.OUT U1.CKA
net F500K    U1.QA U1.CKB
net F100K    U1.QD U2.CKA
net F50K     U2.QA U2.CKB
net F10K     U2.QD U3.CKA U6.1A
net F5K      U3.QA U3.CKB
net F1K      U3.QD U4.CKA
net F500     U4.QA U4.CKB
net F100     U4.QD U5.CKA
net F50      U5.QA U5.CKB
net TB1      U5.QB U7.R01 U8.R01
net TB2      U5.QC U7.R02 U8.R02
net TRIG_N   U5.QD J2.TRIG_N

# Controller pulse: gates the count clock, latches the count when it ends
net PULSE    J2.OUT U6.1B U6.2A U6.2B
net GCLK     U6.1Y U8.CKA
net LATCH    U6.2Y U9.CLK U10.CLK

# Counters: units U8, tens U7; QD carries to the next
net C0       U8.QA U8.CKB U10.1D
net C1       U8.QB U10.2D
net C2       U8.QC U10.3D
net C3       U8.QD U10.4D U7.CKA
net C4       U7.QA U7.CKB U9.1D
net C5       U7.QB U9.2D
net C6       U7.QC U9.3D
net C7       U7.QD U9.4D

# Latches to decoders
net L0       U10.1Q U12.A
net L1       U10.2Q U12.B
net L2       U10.3Q U12.C
net L3       U10.4Q U12.D
net L4       U9.1Q U11.A
net L5       U9.2Q U11.B
net L6       U9.3Q U11.C
net L7       U9.4Q U11.D

net VCC      R1.1 R2.1 U9.CLR_N U10.CLR_N
net VCC      U11.LT_N U11.RBI_N U11.BI_N U12.LT_N U12.RBI_N U12.BI_N
net GND      U1.R01 U1.R02 U1.R91 U1.R92 U2.R01 U2.R02 U2.R91 U2.R92
net GND      U3.R01 U3.R02 U3.R91 U3.R92 U4.R01 U4.R02 U4.R91 U4.R92
net GND      U5.R01 U5.R02 U5.R91 U5.R92 U7.R91 U7.R92 U8.R91 U8.R92
net GND      U6.3A U6.3B U6.4A U6.4B

# Digits: common anode through one resistor, segments straight on the decoder outputs
net DS1_CA   R1.2 DS1.CA1 DS1.CA2
net DS1_A    U11.SA_N DS1.A
...
net DS1_G    U11.SG_N DS1.G
net DS2_CA   R2.2 DS2.CA1 DS2.CA2
net DS2_A    U12.SA_N DS2.A
...
net DS2_G    U12.SG_N DS2.G
```

`J2` pins: 1 `TRIG_N`, 2 `OUT`, 3 `VCC`, 4 `GND` (the `PADDLE` Part, P4i design §3).

## Board

`bw export-kicad machines/045-knob-meter/knob.bwn`, then placed and routed by hand in KiCad ([KICAD_GUIDE.md](../../../docs/p2a-board-layout/KICAD_GUIDE.md)):

- `DS1` `DS2` in a row near the front edge, upright: the pad 1 row (pins 1–5) at the bottom.
- `J2` on the left edge (the controller's cable), `J1` at the back.
- Behind each digit its decoder (`U11`, `U12`) and anode resistor (`R1`, `R2`), then its latch (`U9`, `U10`), then its counter (`U7`, `U8`); `U6` next to `J2`; timebase `U1`–`U5` and `X1` at the back.
- Wanted for the Puzzles:
  - `VCC` at `J2` pin 3: a branch that feeds only that pad (a leaf of `VCC`), with a straight stretch away from pads and corners, so a crack there unpowers `J2` alone.
  - `C3`: `U7` pin 14 (`CKA`) at an end of the routed Trace, e.g. `U8` pin 11 → `U10` pin 13 → `U7` pin 14, with a straight stretch on the last leg, so a crack there leaves `U7` `CKA` alone.
  - `U11` pin 7 and `U12` pins 2–3: no constraint (a dry Joint and a Solder Bridge need no Copper).
- `bw check-board` clean; `drc_knob` passes (picked up by the `machines/*/board/*.kicad_pcb` glob).
- Cracks (P4e syntax), on the routed Board: `VCC` `J2.3 U6.14 50%` (`J2` pin 3 is a leaf: a straight B.Cu run to `U6` pin 14, then on to the rest of `VCC`); `C3` `U8.11 U7.14 50%` (routed `U10` pin 13 → `U8` pin 11 → `U7` pin 14 on F.Cu).

## Puzzles

`machine knob` in each file (KNOB_METER.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `dead_knob.bwp` | `fault crack J2.3 U6.14 50%` | `VCC` to `J2` |
| `runaway.bwp` | `fault stuck U5.QC 0` | `TB2` |
| `single_figures.bwp` | `fault crack U8.11 U7.14 50%` | `C3` at `U7` `CKA` |
| `odd_tens.bwp` | `fault joint U11.7` | `L4` at `U11` `A` |
| `four_to_seven.bwp` | `fault bridge U12.2 U12.3` | `L2` to `LT_N` (+5 V) |

Briefs:

- Dead knob: "The meter shows the same thing from power-on, whatever the knob does."
- Runaway: "The number keeps going up by itself, faster the further the knob is turned."
- Single figures: "The left digit always shows 0, and the right digit goes round from 0 to 9 as the knob turns up."
- Odd tens: "The left digit never shows an even number: at the left stop the meter reads 10."
- Four to seven: "The right digit only ever shows 4, 5, 6 or 7, or an odd shape."

Each test turns the knob before power-on (`Paddle::setKnob`), reads the digits after 250 ms (two full cycles, past any power-on state) and again after the next cycle.

| Puzzle | Knob → reading with the Fault | After the Repair |
|---|---|---|
| Dead knob | 20 % then 80 %: the segment masks of both digits at 250 ms and at 450 ms are the same (any shape), `J2` in `power().unpowered` | 20 % → 20, 80 % → 79 |
| Runaway | 30 %: two readings 100 ms apart differ by 30 (mod 100) | 30 % → 30 twice |
| Single figures | 47 % → 07 | 47 |
| Odd tens | 0 % → 10, 25 % → 35 | 00, 25 |
| Four to seven | 0 % → 04, 9 % → `0` and the shape of 13 (`a` `d` `f` `g`) | 00, 09 |

(Reading ⌊99 `k` + 0.5⌋: 20 % → 20, 25 % → 25, 30 % → 30, 47 % → 47, 80 % → 79, 9 % → 9.)

## Electrical Nets on a damaged Board

- Dead knob: `J2` pin 3 on an island with no supply pin (unnamed: power Pins name no island, so the meter reads nothing there, as on `dark_seconds`): `J2` is not Powered (P4i design §4), its Pins get private Nets. `PULSE` holds only TTL inputs of `U6`: floating, read high. `GCLK` is `F10K` inverted, so the counters count freely between clears; `LATCH` is a steady 0, so the latches never take a count and hold their power-on values. `TRIG_N` still pulses at `U5` and at `J2` pin 1's pad.
- Runaway: `TB2` tied at 0 (Strong): `R0` of `U7`, `U8` never high. The latched value grows by the knob's count every cycle, mod 100.
- Single figures: `U7` pin 14 alone on its island: floating, read high, `U7` never counts and is cleared to 0 every cycle. `U10` pin 13 stays with `U8` pin 11 and the units are right.
- Odd tens: `U11` pin 7 is off `L4` (dry Joint): the leg floats, `U11` reads `A` high. `L4` keeps `U9` `1Q` as Driver and the pad.
- Four to seven: the bridge joins `L2` to the `VCC` island: `L2` is +5 V (Supply beats `U10` `3Q`). No Dead Rail (one rail on the island).

## Manuals

- User: What it does (one paragraph: power `J1`, plug the paddle controller into `J2`; the two digits `DS1` `DS2` show how far the knob is turned, 00 at the left stop to 99 at the right, ten times a second; at power-on they may show anything for a moment).
- Service: Components, one bullet per reference starting with it, netlist order (`CheckServiceManual`):
  - `X1` clock: 1 MHz crystal oscillator, drives `CLK`
  - `J1` power connector: +5 V and 0 V for the whole board
  - `J2` paddle controller connector: `TRIG_N` to the controller, its pulse back on `PULSE`, 30 µs at the left stop to 9.93 ms at the right; the controller runs on the board's +5 V
  - `R1`, `R2` digit resistors: feed the anodes of `DS1` (`DS1_CA`), `DS2` (`DS2_CA`) and limit their current
  - `U1`–`U4` timebase: each divides by ten (`U1`: `CLK` 1 MHz → `F100K`, …, `U2` gives the 10 kHz count clock `F10K`, `U4`: `F1K` → `F100` 100 Hz)
  - `U5` measuring cycle: steps 0–9 every 10 ms; `TRIG_N` falls at 9 → 0; `TB1` and `TB2` both high in steps 6 and 7 clear the counters
  - `U6` gates: `GCLK` from `F10K` while `PULSE` is high; `LATCH` rises when `PULSE` ends
  - `U7` tens counter: counts on `C3` into `C4`–`C7`; `U8` units counter: counts on `GCLK` into `C0`–`C3`, carries on `C3`; both cleared by `TB1` and `TB2`
  - `U9` tens latch: `C4`–`C7` → `L4`–`L7` on `LATCH`; `U10` units latch: `C0`–`C3` → `L0`–`L3`
  - `U11` tens decoder: `L4`–`L7` to the segments of `DS1`; `U12` units decoder: `L0`–`L3` to `DS2`
  - `DS1` tens digit, `DS2` units digit: a segment lights while its decoder output is low

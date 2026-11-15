# PET 2001 sheet 1 (drawing 320130): transcription notes

Source: `hp-1.png` (5100 x 3300, 300 dpi). All coordinates below are full-image pixels of
`hp-1.png`; crops are in `sheet1/` (made with `sheet1/c.sh` or `magick ... -level`). Netlist:
`sheet1.bwn`. Pin check: `python3 -I sheet1/check.py sheet1.bwn` (pin names against the
project's pinouts, no pin in two Nets, unused pins listed).

## How the sheet works (read from the wiring)

- **Address buffers.** A1–A10 go through two 7417s (E3, E4: open collector, 1 kΩ pull-ups
  R29–R38) to BA1–BA10. A0 goes through an AND gate (E2 12/13→11, the other input on the
  NMI_N pull-up, so it is a plain buffer) to BA0 (pull-up R28). A11 goes through two inverters
  of E5: E5.2 = **BA11_N** (off-sheet), E5.4 = **BA11** (totem pole, no pull-up). BA0–BA11 go
  to J4 1–12 and off-sheet as a bus.
- **Decoding.** G2 (74LS154) takes A12–A15 (A=A12 … D=A15, both enables on GND): Y0–Y15 =
  SEL0_N–SELF_N, one per 4 KB block. SEL1–SEL7, SEL9–SEL11 go to J4; SEL0, SEL1, SELC–SELF go
  off-sheet; SEL8_N is inverted by E5 9→8 to **SEL8** (off-sheet, the video RAM).
- **I/O selects.** All three I/O chips have CS2_N = SELE_N. PIAs: CS0 = BA11, PIA1 CS1 = BA4
  ($E810), PIA2 CS1 = BA5 ($E820). VIA: CS1 = BA6 AND BA11 (E2 9/10→8) ($E840). RS0/RS1 =
  BA0/BA1 on all three, VIA RS2/RS3 = BA2/BA3.
- **Data buffers.** G5/G6 (74LS244) are wired as bidirectional pairs between D0–D7 (CPU side:
  6502, PIAs, VIA, off-sheet D bus) and BD0–BD7 (J4, off-sheet BD bus). They are never both off:
  - G4 gate 1: RW_E_N = SELE_N · R/W · 1 · 1 ("R/W • Ē" on J4.30 and an off-sheet arrow);
  - G4 gate 2: **BD_TO_D** = RW_E_N · SELC_N · SELD_N · SELF_N = a read outside $C000–$FFFF;
  - BD_TO_D high → E5 5→6 low → pin 19 (2OE_N) of G5/G6: BD → D;
  - BD_TO_D low → pin 1 (1OE_N): D → BD (all writes, and reads of the ROMs/I/O, where the D
    side drives BD).
- **Clock.** Ø0 comes in from sheet 3 to F3.37. Ø2 (F3.39) clocks the PIAs and VIA and is
  buffered by two NANDs of G3 (13→11→10→8, the spare inputs 9 and 12 on R26) to **BØ2**
  (off-sheet arrow, J4.29). Ø1 (F3.3) is drawn with a stub only: unconnected.
- **Reset.** LM555 UDE9 as a one-shot (R15 1M / C5 1µF on 6-7, R16 1M / C6 0.1µF on 2, C14
  0.01µF on 5, 4 and 8 on +5V) → pin 3 → D9 1→2 = RES_N (≈1.1 s low after power-on). RES_N goes
  to the 6502, both PIAs, the VIA, J4.27 and E4 13→12 = IFC_N. **We replace the 555 by `POR`**:
  UDE9 POR drives RES_N directly with pull-up RPOR (not on the drawing); D9 gate 1 is left with
  its input on GND and its output unconnected. `time=1ms` as on the bench; the real hold is
  ≈1.1 s.
- **Interrupts.** IRQ_N: 6502.4, PIA1 IRQA/IRQB, PIA2 IRQA/IRQB, VIA IRQ, J4.28, R17 3.3k to
  +5V. NMI_N (6502.6): R13 1k to +5V, and E2.13. RDY (6502.2): R14 1k to +5V.
- **Diagnostic LED.** E9 gate 2 (74LS20) = NAND(KEY A, KEY B, KEY D, NOT KEY C via D9 13→12):
  low when PIA1 PA3–PA0 = 1011 (row 11, a row the 74LS145 does not decode). DS1 LED from +5V
  through R9 470 Ω to E9.8 (test point TP2-2): lit while the software outputs row 11.
- **DIAG jumper.** PIA1 PA7 ("DIG JUMPER") → J2.5 ("DIAG"); the 6520's own PA pull-up, no
  resistor on the sheet. Grounding J2.5 (to J2.A/N/1/11/12) selects the diagnostic.

## Chips: every used pin

Pin numbers are the drawing's; names are the project's.

### UF3 6502

| Pin | Name | Net |
|---|---|---|
| 1, 21 | GND, GND2 | GND (boxed together, ground symbol) |
| 2 | RDY | RDY (R14 1k to +5V) |
| 3 | PHI1 | — (stub, unconnected) |
| 4 | IRQ_N | IRQ_N |
| 5, 35, 36 | NC1–NC3 | "N.C." |
| 6 | NMI_N | NMI_N (R13 1k to +5V; also E2.13) |
| 7 | SYNC | not drawn: unconnected |
| 8 | VCC | +5V (the row drawn "VSS" joined to the R13/R14 +5V node, see uncertain #2) |
| 9–20, 22–25 | A0–A11, A12–A15 | A0–A15 |
| 26–33 | D7–D0 | D7–D0 |
| 34 | RW | RW |
| 37 | PHI0 | PHI0 (from sheet 3, arrow "Ø0") |
| 38 | SO | not drawn: tied to VCC in the netlist |
| 39 | PHI2 | PHI2 |
| 40 | RES_N | RES_N |

### UE3 7417 (open collector)

| Gate | In (pin, Net) | Out (pin, Net) |
|---|---|---|
| 1 | 1A (1) BLANK_TV | 1Y (2) EOI_N (R48 2.4k up, R47 5.1k down) |
| 2 | 2A (3) A1 | 2Y (4) BA1 (R29) |
| 3 | 3A (5) A2 | 3Y (6) BA2 (R30) |
| 4 | 4A (9) A3 | 4Y (8) BA3 (R31) |
| 5 | 5A (11) A4 | 5Y (10) BA4 (R32) |
| 6 | 6A (13) A5 | 6Y (12) BA5 (R33) |

### UE4 7417 (open collector)

| Gate | In | Out |
|---|---|---|
| 1 | 1A (1) A6 | 1Y (2) BA6 (R34) |
| 2 | 2A (3) A7 | 2Y (4) BA7 (R35) |
| 3 | 3A (5) A8 | 3Y (6) BA8 (R36) |
| 4 | 4A (9) A9 | 4Y (8) BA9 (R37) |
| 5 | 5A (11) A10 | 5Y (10) BA10 (R38) |
| 6 | 6A (13) RES_N | 6Y (12) IFC_N (R10 2.4k up, R11 5.1k down) |

### UE5 74LS04

| Gate | In | Out |
|---|---|---|
| 1 | 1A (1) A11 | 1Y (2) BA11_N (off-sheet) |
| 2 | 2A (3) BA11_N | 2Y (4) BA11 |
| 3 | 3A (5) BD_TO_D | 3Y (6) BD_TO_D_N |
| 4 | 4A (9) SEL8_N | 4Y (8) SEL8 (off-sheet) |
| 5, 6 | 11, 13 | not on this sheet (check sheets 2/3; tie if unused) |

### UE2 74LS08 (gates 3 and 4 here; 1 and 2 on sheet 3)

| Gate | Inputs | Out |
|---|---|---|
| 3 | 3A (9) BA11, 3B (10) BA6 | 3Y (8) VIA_CS1 |
| 4 | 4A (12) A0, 4B (13) NMI_N | 4Y (11) BA0 (R28) |

### UG3 74LS00 (gates 3 and 4 here)

| Gate | Inputs | Out |
|---|---|---|
| 4 | 4A (12) PU_R26, 4B (13) PHI2 | 4Y (11) G3_11 |
| 3 | 3A (9) PU_R26, 3B (10) G3_11 | 3Y (8) BPHI2 |

Both drawn facing left (output bubble on the left). Gates 1, 2 not on this sheet.

### UG4 74LS21

| Gate | Inputs | Out |
|---|---|---|
| 1 | 1A (1) SELE_N, 1B (2) RW, 1C (4) PU_R26, 1D (5) PU_R26 | 1Y (6) RW_E_N |
| 2 | 2A (9) RW_E_N, 2B (10) SELC_N, 2C (12) SELD_N, 2D (13) SELF_N | 2Y (8) BD_TO_D |

### UG2 74LS154

| Pin | Name | Net |
|---|---|---|
| 23, 22, 21, 20 | A, B, C, D | A12, A13, A14, A15 |
| 18, 19 | G1_N, G2_N | GND |
| 1 | Y0_N | SEL0_N (off-sheet) |
| 2 | Y1_N | SEL1_N (J4.16 and off-sheet) |
| 3–8 | Y2_N–Y7_N | SEL2_N–SEL7_N (J4.17–22) |
| 9 | Y8_N | SEL8_N → E5.9 |
| 10, 11, 13 | Y9_N, Y10_N, Y11_N | SEL9_N, SEL10_N, SEL11_N (J4.23–25) |
| 14–17 | Y12_N–Y15_N | SELC_N, SELD_N, SELE_N, SELF_N (off-sheet) |

### UG5, UG6 74LS244

| Pin | Name | UG5 Net | UG6 Net |
|---|---|---|---|
| 1 | 1OE_N | BD_TO_D | BD_TO_D |
| 19 | 2OE_N | BD_TO_D_N | BD_TO_D_N |
| 2 / 18 | 1A1 → 1Y1 | D0 → BD0 | D4 → BD4 |
| 4 / 16 | 1A2 → 1Y2 | D1 → BD1 | D5 → BD5 |
| 6 / 14 | 1A3 → 1Y3 | D2 → BD2 | D6 → BD6 |
| 8 / 12 | 1A4 → 1Y4 | D3 → BD3 | D7 → BD7 |
| 17 / 3 | 2A4 → 2Y4 | BD0 → D0 | BD4 → D4 |
| 15 / 5 | 2A3 → 2Y3 | BD1 → D1 | BD5 → D5 |
| 13 / 7 | 2A2 → 2Y2 | BD2 → D2 | BD6 → D6 |
| 11 / 9 | 2A1 → 2Y1 | BD3 → D3 | BD7 → D7 |

### UB8 6520 PIA2 ("PIA#2")

| Pin | Name | Net |
|---|---|---|
| 21 | RW | RW |
| 22 | CS0 | BA11 |
| 24 | CS1 | BA5 |
| 23 | CS2_N | SELE_N |
| 25 | PHI2 | PHI2 |
| 34 | RES_N | RES_N |
| 36, 35 | RS0, RS1 | BA0, BA1 |
| 37, 38 | IRQB_N, IRQA_N | IRQ_N (joined) |
| 33–26 | D0–D7 | D0–D7 |
| 2–9 | PA0–PA7 | DI1–DI8 ("DI-1" … "DI-8") |
| 10–17 | PB0–PB7 | DO1–DO8 ("DO-1" … "DO-8") |
| 40 | CA1 | ATN_IN_N ("ATN IN") |
| 39 | CA2 | NDAC_OUT_N ("NDAC OUT") |
| 18 | CB1 | SRQ_N ("SRQ IN") |
| 19 | CB2 | DAV_OUT_N ("DAV OUT") |
| 1, 20 | GND, VCC | rails |

### UA5 6522 VIA ("VIA#1")

| Pin | Name | Net |
|---|---|---|
| 22 | RW | RW |
| 38, 37, 36, 35 | RS0–RS3 | BA0–BA3 |
| 24 | CS1 | VIA_CS1 (E2.8) |
| 23 | CS2_N | SELE_N |
| 25 | PHI2 | PHI2 |
| 34 | RES_N | RES_N |
| 21 | IRQ_N | IRQ_N |
| 33–26 | D0–D7 | D0–D7 |
| 40 | CA1 | USR_CA1 → J2.B ("CA1") |
| 39 | CA2 | GRAPHIC (off-sheet arrow) |
| 2–9 | PA0–PA7 | PA0–PA7 → J2.C–L |
| 10 | PB0 | NDAC_IN_N ("NDAC IN") |
| 11 | PB1 | NRFD_OUT_N ("NRFD OUT") |
| 12 | PB2 | ATN_OUT_N ("ATN OUT") |
| 13 | PB3 | CASS_WRITE ("CASS WRITE") |
| 14 | PB4 | CASS2_MOTOR ("#2 CASS MOTOR", to R3) |
| 15 | PB5 | SYNC (from sheet 3) |
| 16 | PB6 | NRFD_IN_N ("NRFD IN") |
| 17 | PB7 | DAV_IN_N ("DAV IN") |
| 18 | CB1 | CASS2_READ ("#2 CASS READ") |
| 19 | CB2 | USR_CB2 → J2.M ("CB2") |
| 1, 20 | GND, VCC | rails |

### UG8 6520 PIA1 (drawn "G8 PIA#1 6520": the location is G8)

| Pin | Name | Net |
|---|---|---|
| 21 | RW | RW |
| 22 | CS0 | BA11 |
| 24 | CS1 | BA4 (labelled "BA4" at the pin) |
| 23 | CS2_N | SELE_N |
| 25 | PHI2 | PHI2 |
| 34 | RES_N | RES_N |
| 36, 35 | RS0, RS1 | BA0, BA1 |
| 38, 37 | IRQA_N, IRQB_N | IRQ_N |
| 33–26 | D0–D7 | D0–D7 |
| 2–5 | PA0–PA3 | KEY_A–KEY_D ("KEY A" …) |
| 6 | PA4 | CASS1_SWITCH ("#1 CASS SWITCH") |
| 7 | PA5 | CASS2_SWITCH ("#2 CASS SWITCH") |
| 8 | PA6 | EOI_N ("EOI IN") |
| 9 | PA7 | DIAG ("DIG JUMPER") |
| 10–17 | PB0–PB7 | KIN0–KIN7 (R18–R25 10k pull-ups) |
| 40 | CA1 | CASS1_READ ("#1 CASS READ") |
| 39 | CA2 | BLANK_TV (off-sheet "BLANK TV"; also EOI out through E3 1→2) |
| 18 | CB1 | SYNC (from sheet 3) |
| 19 | CB2 | CASS1_MOTOR ("#1 CASS MOTOR", to R5) |
| 1, 20 | GND, VCC | rails |

### UG9 74LS145

| Pin | Name | Net |
|---|---|---|
| 15, 14, 13, 12 | A, B, C, D | KEY_A, KEY_B, KEY_C, KEY_D |
| 1–7 | Y0_N–Y6_N | ROW0–ROW6 → J5.1–7 |
| 9, 10, 11 | Y7_N, Y8_N, Y9_N | ROW7–ROW9 → J5.8–10 |

### UE9 74LS20 (gate 2 here)

| Pin | Name | Net |
|---|---|---|
| 13 | 2D | KEY_A |
| 12 | 2C | KEY_B |
| 10 | 2B | KEY_D |
| 9 | 2A | KEY_C_N (D9.12) |
| 8 | 2Y | DIAG_LED_N (TP2-2, R9 470 → DS1 cathode) |

Gate 1 (1–6) not on this sheet.

### UD9 74LS04 (gates 1 and 6 here)

| Gate | In | Out |
|---|---|---|
| 1 | 1A (1): 555 pin 3 on the drawing; **GND in the netlist** | 1Y (2): RES_N on the drawing; **unconnected** in the netlist |
| 6 | 6A (13) KEY_C | 6Y (12) KEY_C_N |

Gates 2–5 not on this sheet.

### UDE9 LM555 (replaced by POR)

| 555 pin | Connection |
|---|---|
| 1 | GND (not drawn) |
| 2 TRIG | R16 1M to +5V, C6 0.1µF to GND |
| 3 OUT | D9.1 |
| 4 RESET, 8 VCC | +5V |
| 5 CTRL | C14 0.01µF to GND |
| 6 THR, 7 DIS | joined; R15 1M to +5V, C5 1.0µF tantalum to GND |

### UA7, UA8, UA9 MC3446 (not simulated)

A channel is: driver input (from the 6520/6522) → bus pin (J1), bus pin → receiver output
(to the 6520/6522). Pins 4 and 12 (the enables) are on GND on all three; 8 GND and 16 VCC are not
drawn.

| Chip | Ch | Driver in ← from | Bus pin → J1 | Receiver out → to |
|---|---|---|---|---|
| UA7 | A | 3 ← PIA2 PB0 (DO1) | 2 DIO1 → J1.1 | 1 → PIA2 PA0 (DI1) |
| UA7 | B | 5 ← PIA2 PB1 (DO2) | 6 DIO2 → J1.2 | 7 → PIA2 PA1 (DI2) |
| UA7 | C | 11 ← PIA2 PB2 (DO3) | 10 DIO3 → J1.3 | 9 → PIA2 PA2 (DI3) |
| UA7 | D | 13 ← PIA2 PB3 (DO4) | 14 DIO4 → J1.4 | 15 → PIA2 PA3 (DI4) |
| UA8 | A | 3 ← PIA2 PB4 (DO5) | 2 DIO5 → J1.A | 1 → PIA2 PA4 (DI5) |
| UA8 | B | 5 ← PIA2 PB5 (DO6) | 6 DIO6 → J1.B | 7 → PIA2 PA5 (DI6) |
| UA8 | C | 11 ← PIA2 PB6 (DO7) | 10 DIO7 → J1.C | 9 → PIA2 PA6 (DI7) |
| UA8 | D | 13 ← PIA2 PB7 (DO8) | 14 DIO8 → J1.D | 15 → PIA2 PA7 (DI8) |
| UA9 | A | 3 ← PIA2 CB2 (DAV OUT) | 2 DAV → J1.6 | 1 → VIA PB7 (DAV IN) |
| UA9 | B | 5 ← VIA PB1 (NRFD OUT) | 6 NRFD → J1.7 | 7 → VIA PB6 (NRFD IN) |
| UA9 | C | 11 ← PIA2 CA2 (NDAC OUT) | 10 NDAC → J1.8 | 9 → VIA PB0 (NDAC IN) |
| UA9 | D | 13 ← VIA PB2 (ATN OUT) | 14 ATN → J1.11 | 15 → PIA2 CA1 (ATN IN) |

Not through an MC3446: SRQ (J1.10 = J2.3 = PIA2 CB1 directly), EOI (out: PIA1 CA2 → E3 7417
1→2 → J1.5; in: J1.5 → PIA1 PA6; R48 2.4k up / R47 5.1k down), IFC (RES_N → E4 7417 13→12 →
J1.9; R10 2.4k up / R11 5.1k down; output only). REN (J1.E) is on GND.

For a header-connector model without the 3446s: a channel is an open-collector driver (bus low
while the driver input is at the asserting level) plus a receiver copying the bus. The MC3446's
polarity (inverting or not) cannot be read from this sheet; check its data sheet before
modelling (the PET's IEEE code complements the data bytes in software, which suggests the
3446 passes levels through, but this is not verified here).

## Connectors

### J1 IEEE-488 (edge, 1–12 and A–N)

| Pin | Label | Net |
|---|---|---|
| 1, 2, 3, 4 | DIO-1 … DIO-4 | DIO1–DIO4 (UA7 2, 6, 10, 14) |
| 5 | EOI (overline) | EOI_N |
| 6 | DAV (overline) | DAV_N (UA9.2) |
| 7 | NRFD (overline) | NRFD_N (UA9.6) |
| 8 | NDAC (overline) | NDAC_N (UA9.10) |
| 9 | IFC (overline) | IFC_N |
| 10 | SRQ (overline) | SRQ_N |
| 11 | ATN (overline) | ATN_N (UA9.14) |
| 12 | CHASSIS | chassis ground symbol (Net CHASSIS) |
| A, B, C, D | DIO-5 … DIO-8 | DIO5–DIO8 (UA8 2, 6, 10, 14) |
| E–N (E F H J K L M N) | "E to N" bracket | GND |

### J2 user port (edge, 1–12 and A–N)

| Pin | Label | Net |
|---|---|---|
| 1, 11, 12 | — | GND |
| 2 | VIDEO | VIDEO (label only: from sheet 3) |
| 3 | SRQ (overline) | SRQ_N |
| 4 | EOI (overline) | EOI_N |
| 5 | DIAG ("JUMP" beside it) | DIAG (PIA1 PA7) |
| 6 | #1 READ | CASS1_READ |
| 7 | WRITE | CASS_WRITE |
| 8 | #2 READ | CASS2_READ |
| 9 | HORZ | HORZ (label only: from sheet 3) |
| 10 | VERT | VERT (label only: from sheet 3) |
| A, N | — | GND |
| B | CA1 | USR_CA1 (VIA CA1) |
| C, D, E, F, H, J, K, L | PA0 … PA7 | PA0–PA7 (VIA) |
| M | CB2 | USR_CB2 (VIA CB2) |

### J3 cassette #2 (pins "A-1" … "F-6"; numbered 1–6 in the netlist)

| Pin | Label | Net |
|---|---|---|
| A-1 | — | GND |
| B-2 | +5V | VCC |
| C-3 | #2 CASS MOTOR | CASS2_MOTOR_OUT (Q1 TIP29 emitter, ≈+9V switched) |
| D-4 | #2 READ | CASS2_READ (VIA CB1) |
| E-5 | WRITE | CASS_WRITE (VIA PB3) |
| F-6 | — | CASS2_SWITCH (PIA1 PA5) |

### J6 cassette #1 (pins "A1" … "F6"; numbered 1–6 in the netlist)

| Pin | Label | Net |
|---|---|---|
| A1 | — | GND |
| B2 | +5V | VCC |
| C3 | #1 CASS MOTOR | CASS1_MOTOR_OUT (Q4 TIP29 emitter) |
| D4 | — | CASS1_READ (PIA1 CA1) |
| E5 | WRITE | CASS_WRITE |
| F6 | — | CASS1_SWITCH (PIA1 PA4) |

### J4 memory expansion

| Pin | Label | Net |
|---|---|---|
| 1–12 | BA0–BA11 | BA0–BA11 |
| 13, 14, 15 | — | not drawn on this sheet |
| 16–22 | SEL1–SEL7 (overline) | SEL1_N–SEL7_N |
| 23, 24, 25 | SEL9, SEL10, SEL11 (overline) | SEL9_N, SEL10_N, SEL11_N |
| 26 | — | not drawn |
| 27 | RES (overline) | RES_N |
| 28 | IRQ (overline) | IRQ_N (R17 3.3k to +5V) |
| 29 | BØ2 | BPHI2 |
| 30 | R/W • Ē | RW_E_N |
| 31, 32 | — | not drawn |
| 33–40 | BD0–BD7 | BD0–BD7 |

### J5 keyboard

| Pin | Net |
|---|---|
| 1–10 | ROW0–ROW9 (G9 Y0_N–Y9_N, pins 1–7, 9–11) |
| A, B, C, D, E, F, H, J | KIN0–KIN7 (PIA1 PB0–PB7; "KIN 0 ↓ KIN 7"; letters illegible, see uncertain #9) |

## Inputs the PIAs/VIA see from the analog circuits

| Input | Source | Pull-up |
|---|---|---|
| PIA1 CA1 (CASS1_READ) | J6.4 / J2.6 from the cassette deck's own read amplifier | none on the sheet; the 6520's CA1 has none either |
| VIA CB1 (CASS2_READ) | J3.4 / J2.8 | none on the sheet (the 6522's CB1 has its own) |
| PIA1 PA4 (CASS1_SWITCH) | J6.6, the deck's switch to ground | none on the sheet; 6520 PA pull-up |
| PIA1 PA5 (CASS2_SWITCH) | J3.6 | none on the sheet; 6520 PA pull-up |
| PIA1 PA6 (EOI_N) | IEEE bus EOI | R48 2.4k to +5V, R47 5.1k to GND |
| PIA1 PA7 (DIAG) | J2.5 jumper to ground | none on the sheet; 6520 PA pull-up |
| PIA2 CB1 (SRQ_N) | IEEE bus SRQ, direct | **none** (6520 CB1 has none: floats unless a device drives it) |
| PIA1 PB0–7 (KIN0–7) | keyboard columns | R18–R25 10k |
| VIA PB5, PIA1 CB1 (SYNC) | sheet 3 | none |

Cassette outputs (notes only, not simulated):

- **#2 motor** (VIA PB4): R3 1k → Q3 2N3904 base (R2 10k base to GND, emitter GND); Q3
  collector pulls the base of Q2 2N4401 (with VR1 7.2V zener, R1 1.5k to +9V UNREG, C1 0.01µF);
  Q2/Q1 TIP29 emitter follower → J3.C-3. PB4 high → Q3 on → motor off? (Q3 on pulls the
  regulator base low): motor runs while PB4 is low; reading only, not verified.
- **#1 motor** (PIA1 CB2): R5 1k → Q6 2N3904 (R6 10k), VR2 7.2V, R4 1.5k, C2 0.01µF, Q5 2N4401,
  Q4 TIP29 → J6.C3. Same circuit.
- **+9V UNREG** comes in by label (regulators on sheet 2).

## Off-sheet signals

| Net | Direction | Where on sheet 1 |
|---|---|---|
| PHI0 ("Ø0") | in (sheet 3) | arrow at top, x 2435 |
| BPHI2 ("BØ2") | out | arrow at top, x 2312; J4.29 |
| D0–D7 ("D0-D7 (8)") | both | bus arrow at top, x 2487 |
| BD0–BD7 ("BD0-BD7 (8)") | both | bus arrow at top, x 957; J4.33–40 |
| BA0–BA11 ("BA0-BA11 (12)") | out | bus arrow at top, x 1047; J4.1–12 |
| BA11_N ("BA11" overlined) | out | arrow at top, x 1307 |
| SELF_N, SELE_N, SELD_N, SELC_N | out | arrows at top, x 1333–1422 |
| SEL1_N, SEL0_N | out | arrows at top, x 1496, 1516 |
| RW_E_N ("R/W•Ē") | out | arrow at top, x 1543; J4.30 |
| SEL8 | out | arrow at top, x 1680 (E5.8) |
| GRAPHIC | out | arrow at bottom, x 3316 (VIA CA2); "BUSINESS" written beside it |
| SYNC | in (sheet 3) | arrow at bottom, x 3465 |
| BLANK_TV ("BLANK TV", "EOI" beside it) | out | arrow at bottom, x 3535 (PIA1 CA2) |
| VIDEO, HORZ, VERT | in (sheet 3) | labels on J2.2, J2.9, J2.10 only |
| +9V UNREG | in (sheet 2) | cassette motor drivers |
| VCC, GND | in | power not drawn on this sheet |

## Uncertain readings

1. **E2.13 = NMI_N.** E2.13 runs right to x 1984, up to y 720, right to x 2452, down to y 883,
   left and down onto the 6502 NMI row, where R13 also lands (`f3pins.png`, `f3nmi2.png`,
   `e2b.png`, `trace1.png`). The 6502's pin numbers on that row are hidden under the dots. Read:
   the NMI pull-up doubles as a constant high for the spare AND input, so BA0 = A0. If it is
   another 6502 pin (SYNC 7 would be the only output), BA0 would be wrong.
2. **6502 VCC/SO/SYNC.** Only R/W, Ø0, Ø1, Ø2, RES, IRQ, NMI, RDY, "VSS", "VSS 1 / 21" are drawn.
   The row drawn "VSS" (y ≈ 955) joins the +5V node of R13/R14: read as VCC (pin 8). SO (38) and
   SYNC (7) are not drawn; SO tied to VCC in the netlist.
3. **R13 vs R14.** R13 read as NMI, R14 as RDY (labels stacked "R13 R14 1K"); both 1k to +5V, so it
   does not change the Nets.
4. **Stray "D0-D7" label** at (2300, 1830) above the line at y 1850. The line comes from the BA11
   vertical (x 1200) and goes to E2.9, PIA2 CS0 and PIA1 CS0 (`d07.png`, `babus.png`): read as
   BA11, the label as belonging to the nearby D bus.
5. **SELE_N crossing the D bus** at (2487, 394): a blob where a thin line crosses the thick bus
   (`b8cs.png`). Read as a crossing.
6. **BA4/BA5 to the PIA CS1s** turn at x 2697 (up to PIA2) and x 2700 (down to PIA1)
   (`busC.png`). The "BA4" label at PIA1.24 confirms the order.
7. **BA pull-up numbering** R28 (BA0) … R38 (BA10): labels sit between resistors; only R28 is
   marked "1K", the others assumed 1k (`abuf1.png`, `abuf2.png`).
8. **R18–R25 order**: drawn as one resistor "R25 – R18, 10K (8 resistors)"; R18 = KIN0 assumed.
9. **J5 column letters**: eight pins after 10, letters illegible (`j5b.png`); read A, B, C, D,
   E, F, H, J (no G, I), KIN0 on A.
10. **J1.9 label** small, read "IFC" (overline); it is the 7417 output with R10/R11, labelled
    "IFC" at the driver (`ifc.png`).
11. **IEEE handshake and DI/DO wires** are drawn as bundles: the connections in the MC3446 table
    are by matching labels at both ends ("DAV IN", "NRFD OUT", …, "DI-1", "DO-1"), not traced
    wire by wire. They match the PET's documented IEEE port.
12. **VIA CA2 label** at the pin reads like "C/B"; the wire goes down x 3316 to the "GRAPHIC" arrow
    ("BUSINESS" written beside it): named GRAPHIC.
13. **HORZ/VERT** on J2: overlines unclear; sheet 3 may call them HORZ DRIVE / VERT DRIVE.
14. **J3/J6 pin B = +5V**: the "+5V" label sits next to a short stub on pin B (`j36.png`).
15. **Cassette read/switch routing** to J6.D4/F6 and J3.F6 by the order of the three parallel
    lines (`j6.png`, `cass.png`, `vcol.png`): #1 READ top → J6.D4, #1 SWITCH → J6.F6,
    #2 SWITCH → J3.F6.
16. **RW_E_N name**: the label is "R/W • Ē" (overline on E only, `rwe.png`). The logic is
    RW AND SELE_N. Other sheets may name it differently.
17. **Cassette motor polarity** (notes only): from the transistor topology; not verified.

## Choices made for the simulation

- 555 + D9 gate 1 → `POR` on RES_N with pull-up RPOR 3.3k (user's decision); D9.1 on GND.
- IEEE pull-downs R11, R47 left out (an `R` is a pull-up/down only; with both, the line would be a
  resistive divider). The 2.4k pull-ups R10, R48 are kept.
- DS1: anode on VCC, cathode straight on E9.8 (R9 470 Ω in series not modelled).
- MC3446s, connectors J1–J6 and J4 are `?TYPE` parts with numbered/lettered pins, per the brief.
- Unused gates whose inputs are not on this sheet (E5 5/6, E2 1/2, G3 1/2, E9 1, D9 2–5) are left
  out of the netlist: check sheets 2 and 3 before tying them.

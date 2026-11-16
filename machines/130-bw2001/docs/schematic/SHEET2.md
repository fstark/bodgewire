# PET 2001 board 320137, sheet 2 (memory section): transcription notes

Source: `hp-2.png` (5100 x 3300), checked against `320137-2.gif` where faint. The drawing's title
block says "#320135 Sheet 2 of 3, PET Computer Main Logic, Memory Section, pertaining to board
assembly #320137". Crops in `sheet2/` (coordinates below are full-resolution hp-2.png pixels).
Netlist: `sheet2.bwn`. Built as the 8K machine (all sixteen 2114s, the 8K jumper closed).

## Chips (28)

| ref | type | job |
|---|---|---|
| UI8 UI7 UI6 UI5 / UI4* UI3* UI2* UI1* | 2114 | RAM low nibble, $0000-$03FF ... $1C00-$1FFF |
| UJ8 UJ7 UJ6 UJ5 / UJ4* UJ3* UJ2* UJ1* | 2114 | RAM high nibble, same blocks |
| UH1 UH5 UH2 UH6 UH3 UH4 UH7 | 2316B | ROM sockets $C000 $C800 $D000 $D800 $E000 $F000 $F800 |
| UI9 | 74LS139 (no datasheet: `?74LS139`, pins by number) | RAM 1 KB block decode |
| UH8 UH9 | 74LS244 | RAM data buffers, both directions |
| UJ9 | 74LS00 (gates 1, 2) | RW_RAM |
| UG3 | 74LS00 (gates 1, 2) | RAM select, read-buffer enable |

\* = "FOR 8K MACHINE ONLY" (drawing note 1). Included.

Not in the netlist: R12 10K, jumper "FOR 8K ONLY" (treated as a wire), TP5-2, and the whole
power section (below).

## How the memory section works (derived from the wiring)

- **ROM**: every 2316B socket is wired identically: pin 20 (CS1) to its block select, pin 18 (CS2)
  to BA11, pin 21 (CS3) to +5V, A0-A10 = BA0-BA10, O0-O7 (pins 9 10 11 13 14 15 16 17) straight
  onto the CPU-side **D0-D7** bus (not the BD bus). The 2 KB half is chosen by the mask, so our
  `select=` (CS1 CS2 CS3) is:

  | socket | addresses (drawn) | CS1 pin 20 | CS2 pin 18 | CS3 pin 21 | `select=` |
  |---|---|---|---|---|---|
  | H1 | C000-C7FF | SELC_N | BA11 | +5V | LLH |
  | H5 | C800-CFFF | SELC_N | BA11 | +5V | LHH |
  | H2 | D000-D7FF | SELD_N | BA11 | +5V | LLH |
  | H6 | D800-DFFF | SELD_N | BA11 | +5V | LHH |
  | H3 | E000-E7FF | SELE_N | BA11 | +5V | LLH |
  | H4 | F000-F7FF | SELF_N | BA11 | +5V | LLH |
  | H7 | F800-FFFF | SELF_N | BA11 | +5V | LHH |

  There is no E800 socket (I/O lives there, sheet 1). Same as `tests/data/bw2001_firmware.bwn`.
- **RAM decode**: I9 74LS139 (pinout: 1 1G, 2 1A, 3 1B, 4-7 1Y0-1Y3, 8 GND, 9-12 2Y3-2Y0,
  13 2B, 14 2A, 15 2G, 16 VCC; the drawing calls half 1 "Y" and half 2 "Z"). A inputs (pins 2 and
  14, tied) = BA10, B inputs (pins 3 and 13, tied) = BA11. 1G (pin 1) = SEL0_N, 2G (pin 15) =
  SEL1_N. Y0 (pin 4) selects I8/J8 $0000, Y1 (5) I7/J7 $0400, Y2 (6) I6/J6 $0800, Y3 (7) I5/J5
  $0C00, Z0 (12) I4/J4 $1000, Z1 (11) I3/J3 $1400, Z2 (10) I2/J2 $1800, Z3 (9) I1/J1 $1C00. This
  agrees with the address ranges written in each 2114 box.
- **R/W**: J9 gate 1 (pins 1, 2 tied = R/W·E; out pin 3) inverts R/W·E; J9 gate 2 (pin 4 = that,
  pin 5 = BØ2; out pin 6) gives **R/W RAM = NAND(BØ2, NOT R/W·E)**: low only during phase 2 of a
  write. It goes to every 2114's pin 10 (WE_N), to H8/H9 pin 1, and off the sheet (arrow to the
  left at the top, for the video RAM on sheet 3).
- **RAM select**: G3 gate 1 (drawn as an OR with inverted inputs = NAND): pin 1 = SEL0_N, pin 2 =
  SEL1_N through the "JUMPER FOR 8K ONLY", pulled up by R12 10K to +5V, TP5-2 on that node. Out
  pin 3 = RAMSEL, high for $0000-$0FFF (and $1000-$1FFF with the jumper in). G3 gate 2: pin 4 =
  RAMSEL, pin 5 = R/W·E, out pin 6 = RAM_READ_N, low for a RAM read.
- **Data buffers (the question in the brief)**: the 2114s sit on a private bus BBD0-BBD7. Each 244
  is used as a bidirectional transceiver by wiring its two groups in opposite directions, so there
  is no second path:
  - group 1 (enable pin 1, `1OE_N`) = **BD -> BBD** (write). Inputs 1A1-1A4 (pins 2 4 6 8) on
    BD0/4, BD1/5, BD2/6, BD3/7; outputs 1Y1-1Y4 (pins 18 16 14 12) on the matching BBD.
    `1OE_N` = R/W RAM: drives the RAM bus only during phase 2 of a write (any write, not only to
    RAM: harmless, BBD has nothing else on it, and the 2114s are deselected).
  - group 2 (enable pin 19, `2OE_N`) = **BBD -> BD** (read). Inputs 2A1-2A4 (pins 11 13 15 17) on
    BBD3/7, BBD2/6, BBD1/5, BBD0/4; outputs 2Y1-2Y4 (pins 9 7 5 3) on BD3/7, BD2/6, BD1/5, BD0/4.
    `2OE_N` = G3 pin 6 = NAND(RAMSEL, R/W·E): on for the whole cycle of a read in $0000-$1FFF (not
    gated by Ø2).
  - Per bit the schematic joins BDn to pins (8, 9), (6, 7), (4, 5), (2, 3) and BBDn to (12, 11),
    (14, 13), (16, 15), (18, 17). H8: BD3..BD0 / BBD3..BBD0 top to bottom; H9: BD7..BD4 /
    BBD7..BBD4. The 2114 D0-D3 pins (14 13 12 11, our IO1-IO4) take BBD0-3 (I) or BBD4-7 (J).
  - The two directions can never both be on: write needs R/W·E low, read needs it high.

## Off-sheet signals

| net (ours) | drawing label | direction | to/from |
|---|---|---|---|
| D0-D7 | D0-D7 | out (ROM outputs) | sheet 1, CPU data bus |
| BD0-BD7 | BD0-BD7 (diamond terminals at H8/H9) | both | sheet 1, buffered data bus |
| BA0-BA9 | BA0-BA9 | in | sheet 1 |
| BA10 | BA10 (twice: top left, bottom left) | in | sheet 1 |
| BA11 | BA11 (twice: top left, bottom left) | in | sheet 1 |
| SELC_N SELD_N SELE_N SELF_N | SELC SELD SELE SELF, overlined | in | sheet 1 74LS154 |
| SEL0_N SEL1_N | SEL0 SEL1, overlined (bottom left) | in | sheet 1 74LS154 |
| BPHI2 | BØ2 | in | sheet 1 (buffered Ø2) |
| RW_E | R/W·E (printed "R/W - E") | in | sheet 1 |
| RW_RAM | R/W RAM | out | sheets 1/3 (video RAM write) |

The on-sheet-only nets: BBD0-BBD7, RAM0_N-RAM7_N, RAMSEL, RAM_READ_N, RW_E_N.

## Uncertain readings (10)

1. **H8/H9 pin 1 (1OE_N) = R/W RAM** (`sheet2/strip_a.png`, `sheet2/mid.png`). Pin 1 of both
   244s joins a line that runs right at y~1805 to x~1260, up to y~1669, then right to a dot on the
   R/W RAM vertical at x~1606. Clear in the crop; flagged because it is the one non-obvious
   enable. Best reading as written. Logic agrees (write direction only during a write).
2. **I9 1G = SEL0_N, 2G = SEL1_N** (`sheet2/g3.png`, `sheet2/strip_a.png`). SEL1 rises at
   x~335 to the line at y~1825 (pin 15, GZ); SEL0 rises at x~360 to y~1852 (pin 1, GY). No dots
   where these cross the 244 enable verticals. Confirmed by the drawn ranges (the 8K-only chips are
   on Z).
3. **H8 pins 9, 5, 3 to BD3, BD1, BD0** (`sheet2/h8.png`): drawn dotted/faint in hp-2.png;
   solid in the GIF (`sheet2/h8_gif.png`). Joined.
4. **Two R/W RAM horizontals** (y~1852 for the J row, y~2053 for the I row, `sheet2/band0.png`):
   the lower one's junction with the R/W RAM vertical at x~1605 has no clear dot; both are
   labelled R/W RAM, so one net.
5. **RAM CS verticals** (`sheet2/band1-4.png`): each pair's pin 8 shares one vertical crossing
   all eight decoder lines, with one dot. Read: x~2210 dot on y~1876 (Y0, I8/J8), x~2636 on 1893
   (Y1), 2911 on 1917 (Y2), 3190 on 1942 (Y3), 3468 on 1971 (Z0), 3748 on 1996 (Z1), 4025 on 2015
   (Z2), 4313 on 2039 (Z3). Lines 1876-2039 are 20-25 px apart, so a one-line slip is possible,
   but the result matches the addresses printed in the boxes.
6. **Bus label "BA0-BA9, BØ2 (11)"** (`sheet2/left.png`, y~1076): BØ2 goes nowhere on this
   sheet except J9 pin 5 (its own line); the bus that leaves this label becomes "BA0-BA9 (10)".
   Read as a drafting slip; BØ2 not added to anything else.
7. **Net names for sheet joins**: `BØ2` -> `BPHI2`, `R/W·E` -> `RW_E`. Sheet 1 must use the same
   spelling (not `B02`, `RWE`).
8. **74LS00/74LS244 power pins** are not drawn; standard 14/7 and 20/10 assumed. 2114 18/9,
   2316B 24/12, I9 16/8 are drawn.
9. **G3 and J9 gates 3, 4** (pins 8-13) are not on this sheet: left out, check sheets 1 and 3.
10. **Power section** (notes only): regulator pinout and which rail each regulator feeds read
    from small, crowded drawing (`sheet2/pwr1.png`, `sheet2/pwr2.png`), see below.

## Unused inputs

None on this sheet: every used gate has both inputs connected (J9 gate 1 has both inputs on
R/W·E). G3 pin 2 floats high through R12 in the 4K build; in our 8K build it is SEL1_N.

## Power section (notes only, not in the netlist)

Note: this "J8" is the power connector, not the 2114 at location J8.

- **J8** connector, pins 1 2 3 4 5. Pins 1 and 5: AC from the transformer; CR1 and CR2 (1N5402)
  anodes on pins 1 and 5, cathodes joined: full-wave rectifier, its output goes back out on pin
  4 (to the off-board filter capacitor, presumably). C51 .02 µF 500V from pin 1 to pin 4 and C50
  .02 µF 500V from pin 5 to pin 4 (my reading of the dots; `sheet2/pwr1.png`).
- J8 pin 2: unregulated DC back in; pin 3: ground. C11, C12, C13 1.0 µF 25V and CR3 (1N5402,
  reverse protection, cathode to pin 2) between pins 2 and 3.
- Pin 2's rail feeds pin 1 (top) of four regulators **VR3 VR4 VR5 VR6** (type not printed).
  Each has pin 3 on the left and pin 2 at the bottom; between a vertical on the pin-3 side and a
  vertical on the pin-2 side sit a 47 µF 16V electrolytic (C7 VR3, C8 VR4, C9 VR5, C10 VR6, its
  "+" drawn on the pin-2 side) and 0.1 µF 10V decoupling groups (VR3: C15, C33; VR4: C34, C36-C41;
  VR5: C45-C49, C52; VR6: C35, C42-C44, C55); the pin-2 node carries a test point (TP1-1 VR3,
  TP1-2 VR4, TP1-4 VR6, VR5's presumably TP1-3, cut off). Best reading (`sheet2/vr3.png`): TO-3
  5 V regulators (LM309K / 7805K style): **pin 1 in, pin 2 = +5 V out, pin 3 = case = ground**,
  which fits the "+" mark and the test point on pin 2. Each regulator feeds a separate +5 V section
  of the board; the drawing does not say which chips each powers. Our netlist uses one VCC.

## Pin check-list
### 2114 RAM (16)

| pin | name | UI8 | UI7 | UI6 | UI5 | UI4 | UI3 | UI2 | UI1 |
|---|---|---|---|---|---|---|---|---|---|
| 1 | A6 | BA6 | BA6 | BA6 | BA6 | BA6 | BA6 | BA6 | BA6 |
| 2 | A5 | BA5 | BA5 | BA5 | BA5 | BA5 | BA5 | BA5 | BA5 |
| 3 | A4 | BA4 | BA4 | BA4 | BA4 | BA4 | BA4 | BA4 | BA4 |
| 4 | A3 | BA3 | BA3 | BA3 | BA3 | BA3 | BA3 | BA3 | BA3 |
| 5 | A0 | BA0 | BA0 | BA0 | BA0 | BA0 | BA0 | BA0 | BA0 |
| 6 | A1 | BA1 | BA1 | BA1 | BA1 | BA1 | BA1 | BA1 | BA1 |
| 7 | A2 | BA2 | BA2 | BA2 | BA2 | BA2 | BA2 | BA2 | BA2 |
| 8 | CS_N | RAM0_N | RAM1_N | RAM2_N | RAM3_N | RAM4_N | RAM5_N | RAM6_N | RAM7_N |
| 9 | GND | GND | GND | GND | GND | GND | GND | GND | GND |
| 10 | WE_N | RW_RAM | RW_RAM | RW_RAM | RW_RAM | RW_RAM | RW_RAM | RW_RAM | RW_RAM |
| 11 | IO4 | BBD3 | BBD3 | BBD3 | BBD3 | BBD3 | BBD3 | BBD3 | BBD3 |
| 12 | IO3 | BBD2 | BBD2 | BBD2 | BBD2 | BBD2 | BBD2 | BBD2 | BBD2 |
| 13 | IO2 | BBD1 | BBD1 | BBD1 | BBD1 | BBD1 | BBD1 | BBD1 | BBD1 |
| 14 | IO1 | BBD0 | BBD0 | BBD0 | BBD0 | BBD0 | BBD0 | BBD0 | BBD0 |
| 15 | A9 | BA9 | BA9 | BA9 | BA9 | BA9 | BA9 | BA9 | BA9 |
| 16 | A8 | BA8 | BA8 | BA8 | BA8 | BA8 | BA8 | BA8 | BA8 |
| 17 | A7 | BA7 | BA7 | BA7 | BA7 | BA7 | BA7 | BA7 | BA7 |
| 18 | VCC | VCC | VCC | VCC | VCC | VCC | VCC | VCC | VCC |

| pin | name | UJ8 | UJ7 | UJ6 | UJ5 | UJ4 | UJ3 | UJ2 | UJ1 |
|---|---|---|---|---|---|---|---|---|---|
| 1 | A6 | BA6 | BA6 | BA6 | BA6 | BA6 | BA6 | BA6 | BA6 |
| 2 | A5 | BA5 | BA5 | BA5 | BA5 | BA5 | BA5 | BA5 | BA5 |
| 3 | A4 | BA4 | BA4 | BA4 | BA4 | BA4 | BA4 | BA4 | BA4 |
| 4 | A3 | BA3 | BA3 | BA3 | BA3 | BA3 | BA3 | BA3 | BA3 |
| 5 | A0 | BA0 | BA0 | BA0 | BA0 | BA0 | BA0 | BA0 | BA0 |
| 6 | A1 | BA1 | BA1 | BA1 | BA1 | BA1 | BA1 | BA1 | BA1 |
| 7 | A2 | BA2 | BA2 | BA2 | BA2 | BA2 | BA2 | BA2 | BA2 |
| 8 | CS_N | RAM0_N | RAM1_N | RAM2_N | RAM3_N | RAM4_N | RAM5_N | RAM6_N | RAM7_N |
| 9 | GND | GND | GND | GND | GND | GND | GND | GND | GND |
| 10 | WE_N | RW_RAM | RW_RAM | RW_RAM | RW_RAM | RW_RAM | RW_RAM | RW_RAM | RW_RAM |
| 11 | IO4 | BBD7 | BBD7 | BBD7 | BBD7 | BBD7 | BBD7 | BBD7 | BBD7 |
| 12 | IO3 | BBD6 | BBD6 | BBD6 | BBD6 | BBD6 | BBD6 | BBD6 | BBD6 |
| 13 | IO2 | BBD5 | BBD5 | BBD5 | BBD5 | BBD5 | BBD5 | BBD5 | BBD5 |
| 14 | IO1 | BBD4 | BBD4 | BBD4 | BBD4 | BBD4 | BBD4 | BBD4 | BBD4 |
| 15 | A9 | BA9 | BA9 | BA9 | BA9 | BA9 | BA9 | BA9 | BA9 |
| 16 | A8 | BA8 | BA8 | BA8 | BA8 | BA8 | BA8 | BA8 | BA8 |
| 17 | A7 | BA7 | BA7 | BA7 | BA7 | BA7 | BA7 | BA7 | BA7 |
| 18 | VCC | VCC | VCC | VCC | VCC | VCC | VCC | VCC | VCC |

### 2316B ROM sockets (7)

| pin | name | UH1 | UH5 | UH2 | UH6 | UH3 | UH4 | UH7 |
|---|---|---|---|---|---|---|---|---|
| 1 | A7 | BA7 | BA7 | BA7 | BA7 | BA7 | BA7 | BA7 |
| 2 | A6 | BA6 | BA6 | BA6 | BA6 | BA6 | BA6 | BA6 |
| 3 | A5 | BA5 | BA5 | BA5 | BA5 | BA5 | BA5 | BA5 |
| 4 | A4 | BA4 | BA4 | BA4 | BA4 | BA4 | BA4 | BA4 |
| 5 | A3 | BA3 | BA3 | BA3 | BA3 | BA3 | BA3 | BA3 |
| 6 | A2 | BA2 | BA2 | BA2 | BA2 | BA2 | BA2 | BA2 |
| 7 | A1 | BA1 | BA1 | BA1 | BA1 | BA1 | BA1 | BA1 |
| 8 | A0 | BA0 | BA0 | BA0 | BA0 | BA0 | BA0 | BA0 |
| 9 | O0 | D0 | D0 | D0 | D0 | D0 | D0 | D0 |
| 10 | O1 | D1 | D1 | D1 | D1 | D1 | D1 | D1 |
| 11 | O2 | D2 | D2 | D2 | D2 | D2 | D2 | D2 |
| 12 | GND | GND | GND | GND | GND | GND | GND | GND |
| 13 | O3 | D3 | D3 | D3 | D3 | D3 | D3 | D3 |
| 14 | O4 | D4 | D4 | D4 | D4 | D4 | D4 | D4 |
| 15 | O5 | D5 | D5 | D5 | D5 | D5 | D5 | D5 |
| 16 | O6 | D6 | D6 | D6 | D6 | D6 | D6 | D6 |
| 17 | O7 | D7 | D7 | D7 | D7 | D7 | D7 | D7 |
| 18 | CS2 | BA11 | BA11 | BA11 | BA11 | BA11 | BA11 | BA11 |
| 19 | A10 | BA10 | BA10 | BA10 | BA10 | BA10 | BA10 | BA10 |
| 20 | CS1 | SELC_N | SELC_N | SELD_N | SELD_N | SELE_N | SELF_N | SELF_N |
| 21 | CS3 | VCC | VCC | VCC | VCC | VCC | VCC | VCC |
| 22 | A9 | BA9 | BA9 | BA9 | BA9 | BA9 | BA9 | BA9 |
| 23 | A8 | BA8 | BA8 | BA8 | BA8 | BA8 | BA8 | BA8 |
| 24 | VCC | VCC | VCC | VCC | VCC | VCC | VCC | VCC |

### UI9 (?74LS139)

| pin | name | net |
|---|---|---|
| 1 | 1G (GY) | SEL0_N |
| 2 | 1A (AY) | BA10 |
| 3 | 1B (BY) | BA11 |
| 4 | 1Y0 (Y0) | RAM0_N |
| 5 | 1Y1 (Y1) | RAM1_N |
| 6 | 1Y2 (Y2) | RAM2_N |
| 7 | 1Y3 (Y3) | RAM3_N |
| 8 | GND | GND |
| 9 | 2Y3 (Z3) | RAM7_N |
| 10 | 2Y2 (Z2) | RAM6_N |
| 11 | 2Y1 (Z1) | RAM5_N |
| 12 | 2Y0 (Z0) | RAM4_N |
| 13 | 2B (BZ) | BA11 |
| 14 | 2A (AZ) | BA10 |
| 15 | 2G (GZ) | SEL1_N |
| 16 | VCC | VCC |

### UH8 (74LS244)

| pin | name | net |
|---|---|---|
| 1 | 1OE_N | RW_RAM |
| 2 | 1A1 | BD0 |
| 3 | 2Y4 | BD0 |
| 4 | 1A2 | BD1 |
| 5 | 2Y3 | BD1 |
| 6 | 1A3 | BD2 |
| 7 | 2Y2 | BD2 |
| 8 | 1A4 | BD3 |
| 9 | 2Y1 | BD3 |
| 10 | GND | GND |
| 11 | 2A1 | BBD3 |
| 12 | 1Y4 | BBD3 |
| 13 | 2A2 | BBD2 |
| 14 | 1Y3 | BBD2 |
| 15 | 2A3 | BBD1 |
| 16 | 1Y2 | BBD1 |
| 17 | 2A4 | BBD0 |
| 18 | 1Y1 | BBD0 |
| 19 | 2OE_N | RAM_READ_N |
| 20 | VCC | VCC |

### UH9 (74LS244)

| pin | name | net |
|---|---|---|
| 1 | 1OE_N | RW_RAM |
| 2 | 1A1 | BD4 |
| 3 | 2Y4 | BD4 |
| 4 | 1A2 | BD5 |
| 5 | 2Y3 | BD5 |
| 6 | 1A3 | BD6 |
| 7 | 2Y2 | BD6 |
| 8 | 1A4 | BD7 |
| 9 | 2Y1 | BD7 |
| 10 | GND | GND |
| 11 | 2A1 | BBD7 |
| 12 | 1Y4 | BBD7 |
| 13 | 2A2 | BBD6 |
| 14 | 1Y3 | BBD6 |
| 15 | 2A3 | BBD5 |
| 16 | 1Y2 | BBD5 |
| 17 | 2A4 | BBD4 |
| 18 | 1Y1 | BBD4 |
| 19 | 2OE_N | RAM_READ_N |
| 20 | VCC | VCC |

### UJ9 (74LS00)

| pin | name | net |
|---|---|---|
| 1 | 1A | RW_E |
| 2 | 1B | RW_E |
| 3 | 1Y | RW_E_N |
| 4 | 2A | RW_E_N |
| 5 | 2B | BPHI2 |
| 6 | 2Y | RW_RAM |
| 7 | GND | GND |
| 8 | 3Y | — (not on this sheet) |
| 9 | 3A | — (not on this sheet) |
| 10 | 3B | — (not on this sheet) |
| 11 | 4Y | — (not on this sheet) |
| 12 | 4A | — (not on this sheet) |
| 13 | 4B | — (not on this sheet) |
| 14 | VCC | VCC |

### UG3 (74LS00)

| pin | name | net |
|---|---|---|
| 1 | 1A | SEL0_N |
| 2 | 1B | SEL1_N |
| 3 | 1Y | RAMSEL |
| 4 | 2A | RAMSEL |
| 5 | 2B | RW_E |
| 6 | 2Y | RAM_READ_N |
| 7 | GND | GND |
| 8 | 3Y | — (not on this sheet) |
| 9 | 3A | — (not on this sheet) |
| 10 | 3B | — (not on this sheet) |
| 11 | 4Y | — (not on this sheet) |
| 12 | 4A | — (not on this sheet) |
| 13 | 4B | — (not on this sheet) |
| 14 | VCC | VCC |

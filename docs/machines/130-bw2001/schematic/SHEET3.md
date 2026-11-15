# PET 2001 board 320137, sheet 3 (video logic): transcription notes

Source: `hp-3.png` (5100 x 3300), checked against `320137-3.gif` (offset about -38, -127 px from
the PNG) and the older board's sheet `320081-3.gif`. Title block: "#320135 Sheet 3 of 3, PET
Computer Main Logic, Video Logic, pertaining to board assembly #320137". Netlist: `sheet3.bwn`.
Crops in `sheet3/`; coordinates below are full-resolution hp-3.png pixels (x, y).

Method: besides hand crops, every wire was traced with a small line extractor (horizontal and
vertical strokes, junction dots, chip bodies masked; scripts in `../s3tools/`), which drew each
Net in colour over the sheet (`sheet3/r_*.png`). Every Net was then checked by eye on the
colour overlays and on zoomed crops; the extractor's few false merges (lines passing within a few
pixels of a corner) and missed joins (pen gaps, a junction dot clipped by a body mask) were
corrected by hand and are listed below.

## Chips (30) and other parts

| ref | type | job on this sheet |
|---|---|---|
| UE2 | 74LS08 | gate 2: clock gate (8 MHz); gate 1: pixel = DOT xor REV. Gates 3, 4 not on this sheet |
| UC9 | 74LS93 | /8 of CLK8 (QB 4 MHz unused, QC 2 MHz, QD 1 MHz), QA = H0 |
| UB5 | 74LS93 | H1-H4 |
| UA1 | 74LS93 | scan line in the row, RA0-RA2 (QB QC QD), clocked by DIS.ON falling |
| UC8 | 74LS107 | FF1 character load pulse; FF2 phi0 |
| UC7 | 74LS107 | FF1 vertical step pulse; FF2 H5 |
| UC5 | 74LS107 | FF1 horizontal drive; FF2 DIS.ON |
| UB6 | 74LS107 | vertical state, 2-bit Johnson counter |
| UD5 | 74LS107 | TA0, TA1 |
| UD8 | 74LS00 | all four gates |
| UC6 | 74LS08 | all four gates |
| UC2 | 74LS00 | all four gates |
| UB1 | 74LS20 | gate 2 only (gate 1 not on this sheet) |
| UE8 | 74LS20 | both gates |
| UE9 | 74LS20 | gate 1 only (gate 2 not on this sheet) |
| UD9 | 74LS04 | 3->4, 5->6 (+ probably the oscillator's 11->10, 9->8) |
| UE5 | 74LS04 | 13->12, 11->10 (other sections not on this sheet; see uncertain 2) |
| UB2 | 74LS165 | dot shift register |
| UA2 | 2316B | character ROM, `select=LLH` (CS1 20 and CS2 18 to GND, CS3 21 to +5V) |
| UC1 | 74LS74 | reverse-video bit pipeline |
| UD6, UD7 | 74177 | video address counter TA2-TA5, TA6-TA9 |
| UE6 | 74100 | row start latch LA2-LA9 |
| UD2, UD3, UD4 | 74LS157 | CPU / video address multiplexer to SA0-SA9; D2 also the RAM R/W |
| UC3, UC4 | 2114 | video RAM, 1 KB (SD0-SD3, SD4-SD7) |
| UB3, UB4 | 74LS244 | SD <-> BD buffers, both directions |

Other parts:

- **Y1** (sim): `OSC freq=8MHz`, OUT on Net OSC8 = E2 pin 5. On the board: crystal Y1 8 MHz,
  C3 22 pF, C4 10 pF (TP2-1), R7 and R8 470 ohm, two 74LS04 sections (11->10, then 9->8; the
  location letters are blacked out, most likely **D9**, see uncertain 1). Pin 8 output was the
  node going to E2 pin 5; the inverters' pins 8-11 are left unconnected in the netlist.
- **J7** (`?VIDEO_CONNECTOR`, pins by number): 1 TV VIDEO DRIVE (E9 pin 6), 3 VERT DRIVE,
  5 HORZ. DRIVE, 2 4 7 to GND. Pin 6 not drawn.
- Resistors, all 1K to +5V unless said (not in the netlist; the Net they pull up has no driver,
  named `PU_Rnn`): R46 (E2 pin 4, TP5-1), R12 (TP3-2), R41 (B6 clears, TP3-4), R39 (TP3-1),
  R43 (A1 pin 3), R40 (D2 pins 2 3 5, TP4-1), R44 (C1 presets, TP4-2), R45 (C1 clears, TP4-3),
  R27 (E8 pin 9). 2200 ohm from VERT DRIVE to GND. **The simulation must hold every PU_ Net high**
  (an R part, or tie to VCC): several of them are J/K/clear inputs that must be 1.
- Test points: TP2-1 (oscillator mid node), TP5-1 (PU_R46), TP3-2 (PU_R12), TP3-3 (VSTEP),
  TP3-4 (PU_R41), TP3-5 (CHLOAD_N), TP3-1 (PU_R39), TP4-1 (PU_R40), TP4-2 (PU_R44), TP4-3 (PU_R45).

## Pin check-list

Every pin of every chip with its Net (VCC/GND pins left out: all on the rails). "unused" = not
connected on this sheet (an output, or a spare gate that may be used on sheet 1).


**Y1** OSC: 8 OUT: OSC8

**UE2** 74LS08: 1 1A: PIX_B_N; 2 1B: PIX_A_N; 3 1Y: PIXEL; 4 2A: PU_R46; 5 2B: OSC8; 6 2Y: CLK8; 8 3Y: unused; 9 3A: unused; 10 3B: unused; 11 4Y: unused; 12 4A: unused; 13 4B: unused

**UC9** 74LS93: 1 CKB: CLK8; 2 R01: HCNT_CLR; 3 R02: HCNT_CLR; 8 QC: CLK2M; 9 QB: unused; 11 QD: CLK1M; 12 QA: H0; 14 CKA: CLK1M

**UB5** 74LS93: 1 CKB: H1; 2 R01: HCNT_CLR; 3 R02: HCNT_CLR; 8 QC: H3; 9 QB: H2; 11 QD: H4; 12 QA: H1; 14 CKA: H0

**UA1** 74LS93: 1 CKB: DIS_ON; 2 R01: VSTEP; 3 R02: PU_R43; 8 QC: RA1; 9 QB: RA0; 11 QD: RA2; 12 QA: unused; 14 CKA: unused

**UC8** 74LS107: 1 1J: PU_R12; 2 1Q_N: CHLOAD_N; 3 1Q: CHLOAD; 4 1K: GND; 5 2Q: PHI0_N; 6 2Q_N: PHI0; 8 2J: CLK1M; 9 2CLK: CLK8; 10 2CLR_N: PU_R12; 11 2K: CLK1M_N; 12 1CLK: CLK1M; 13 1CLR_N: CHLOAD_CLR_N

**UC7** 74LS107: 1 1J: VSTEP_J; 2 1Q_N: VSTEP_N; 3 1Q: VSTEP; 4 1K: PU_R12; 5 2Q: H5; 6 2Q_N: H5_N; 8 2J: PU_R12; 9 2CLK: H4; 10 2CLR_N: PU_R12; 11 2K: PU_R12; 12 1CLK: CLK2M; 13 1CLR_N: PU_R12

**UC5** 74LS107: 1 1J: DIS_ON; 2 1Q_N: HORZ_DRIVE; 3 1Q: unused; 4 1K: DIS_OFF; 5 2Q: DIS_ON; 6 2Q_N: DIS_OFF; 8 2J: DISON_SET; 9 2CLK: H0; 10 2CLR_N: PU_R39; 11 2K: DISON_CLR; 12 1CLK: H2; 13 1CLR_N: PU_R39

**UB6** 74LS107: 1 1J: VS1_N; 2 1Q_N: VS0_N; 3 1Q: VS0; 4 1K: VS1; 5 2Q: VS1; 6 2Q_N: VS1_N; 8 2J: VS0; 9 2CLK: VSTEP; 10 2CLR_N: PU_R41; 11 2K: VS0_N; 12 1CLK: VSTEP; 13 1CLR_N: PU_R41

**UD5** 74LS107: 1 1J: PU_R39; 2 1Q_N: unused; 3 1Q: TA1; 4 1K: PU_R39; 5 2Q: TA0; 6 2Q_N: unused; 8 2J: PU_R39; 9 2CLK: CHLOAD; 10 2CLR_N: DIS_ON; 11 2K: PU_R39; 12 1CLK: TA0; 13 1CLR_N: DIS_ON

**UD8** 74LS00: 1 1A: LA3; 2 1B: LAT_HI; 3 1Y: END25_N; 4 2A: END25_N; 5 2B: BLKEND_N; 6 2Y: VEND; 8 3Y: CHLOAD_CLR_N; 9 3A: CLK8; 10 3B: CHLOAD; 11 4Y: VERT_DRIVE; 12 4A: VS1_N; 13 4B: VS0_N

**UC6** 74LS08: 1 1A: H5_N; 2 1B: H4; 3 1Y: DISON_CLR; 4 2A: H5; 5 2B: H3; 6 2Y: DISON_SET; 8 3Y: SYNC; 9 3A: VS0; 10 3B: VS1; 11 4Y: VSTEP_J; 12 4A: DIS_OFF; 13 4B: VEND

**UC2** 74LS00: 1 1A: REV; 2 1B: DOT; 3 1Y: PIX_A_N; 4 2A: REV_N; 5 2B: DOT_N; 6 2Y: PIX_B_N; 8 3Y: LATCH_G; 9 3A: VSTEP_N; 10 3B: ROW7_N; 11 4Y: VREAD_N; 12 4A: SEL8; 13 4B: RW_E

**UB1** 74LS20: 1 1A: unused; 2 1B: unused; 4 1C: unused; 5 1D: unused; 6 1Y: unused; 8 2Y: ROW7_N; 9 2A: RA2; 10 2B: RA0; 12 2C: RA1; 13 2D: DIS_ON

**UE8** 74LS20: 1 1A: LA9; 2 1B: LA8; 4 1C: LA6; 5 1D: LA7; 6 1Y: LAT_HI_N; 8 2Y: BLKEND_N; 9 2A: PU_R27; 10 2B: TA6; 12 2C: RA2; 13 2D: SYNC_N

**UE9** 74LS20: 1 1A: PIXEL; 2 1B: SYNC; 4 1C: BLANKTV; 5 1D: DIS_ON; 6 1Y: TV_VIDEO; 8 2Y: unused; 9 2A: unused; 10 2B: unused; 12 2C: unused; 13 2D: unused

**UD9** 74LS04: 1 1A: unused; 2 1Y: unused; 3 2A: CLK1M; 4 2Y: CLK1M_N; 5 3A: PU_R12; 6 3Y: HCNT_CLR; 8 4Y: unused; 9 4A: unused; 10 5Y: unused; 11 5A: unused; 12 6Y: unused; 13 6A: unused

**UE5** 74LS04: 1 1A: unused; 2 1Y: unused; 3 2A: unused; 4 2Y: unused; 5 3A: unused; 6 3Y: unused; 8 4Y: unused; 9 4A: unused; 10 5Y: LAT_HI; 11 5A: LAT_HI_N; 12 6Y: SYNC_N; 13 6A: SYNC

**UB2** 74LS165: 1 SH_LD_N: CHLOAD_N; 2 CLK: CLK8; 3 E: CD4; 4 F: CD5; 5 G: CD6; 6 H: CD7; 7 QH_N: DOT_N; 9 QH: DOT; 10 SER: GND; 11 A: CD0; 12 B: CD1; 13 C: CD2; 14 D: CD3; 15 CLK_INH: GND

**UA2** 2316B: 1 A7: SD4; 2 A6: SD3; 3 A5: SD2; 4 A4: SD1; 5 A3: SD0; 6 A2: RA2; 7 A1: RA1; 8 A0: RA0; 9 O0: CD0; 10 O1: CD1; 11 O2: CD2; 13 O3: CD3; 14 O4: CD4; 15 O5: CD5; 16 O6: CD6; 17 O7: CD7; 18 CS2: GND; 19 A10: GRAPHIC; 20 CS1: GND; 21 CS3: VCC; 22 A9: SD6; 23 A8: SD5

**UC1** 74LS74: 1 1CLR_N: PU_R45; 2 1D: SD7; 3 1CLK: PHI0_N; 4 1PRE_N: PU_R44; 5 1Q: REV_D; 6 1Q_N: unused; 8 2Q_N: REV_N; 9 2Q: REV; 10 2PRE_N: PU_R44; 11 2CLK: CHLOAD; 12 2D: REV_D; 13 2CLR_N: PU_R45

**UD6** 74177: 1 CT_LOAD_N: DIS_ON; 2 QC: TA4; 3 C: LA4; 4 A: LA2; 5 QA: TA2; 6 CLK2: TA2; 8 CLK1: TA1; 9 QB: TA3; 10 B: LA3; 11 D: LA5; 12 QD: TA5; 13 CLR_N: VSTEP_N

**UD7** 74177: 1 CT_LOAD_N: DIS_ON; 2 QC: TA8; 3 C: LA8; 4 A: LA6; 5 QA: TA6; 6 CLK2: TA6; 8 CLK1: TA5; 9 QB: TA7; 10 B: LA7; 11 D: LA9; 12 QD: TA9; 13 CLR_N: VSTEP_N

**UE6** 74100: 2 1D1: TA2; 3 1D2: TA3; 4 1Q2: LA3; 5 1Q1: LA2; 8 2Q1: LA6; 9 2Q2: LA7; 10 2D2: TA7; 11 2D1: TA6; 12 2G: LATCH_G; 15 2D3: TA8; 16 2D4: TA9; 17 2Q4: LA9; 18 2Q3: LA8; 19 1Q3: LA4; 20 1Q4: LA5; 21 1D4: TA5; 22 1D3: TA4; 23 1G: LATCH_G

**UD2** 74LS157: 1 S: SEL8; 2 1A: PU_R40; 3 1B: PU_R40; 4 1Y: unused; 5 2A: PU_R40; 6 2B: RW_RAM; 7 2Y: VRW_N; 9 3Y: SA0; 10 3B: BA0; 11 3A: TA0; 12 4Y: SA1; 13 4B: BA1; 14 4A: TA1; 15 G_N: GND

**UD3** 74LS157: 1 S: SEL8; 2 1A: TA2; 3 1B: BA2; 4 1Y: SA2; 5 2A: TA3; 6 2B: BA3; 7 2Y: SA3; 9 3Y: SA4; 10 3B: BA4; 11 3A: TA4; 12 4Y: SA5; 13 4B: BA5; 14 4A: TA5; 15 G_N: GND

**UD4** 74LS157: 1 S: SEL8; 2 1A: TA6; 3 1B: BA6; 4 1Y: SA6; 5 2A: TA7; 6 2B: BA7; 7 2Y: SA7; 9 3Y: SA8; 10 3B: BA8; 11 3A: TA8; 12 4Y: SA9; 13 4B: BA9; 14 4A: TA9; 15 G_N: GND

**UC3** 2114: 1 A6: SA6; 2 A5: SA5; 3 A4: SA4; 4 A3: SA3; 5 A0: SA0; 6 A1: SA1; 7 A2: SA2; 8 CS_N: GND; 10 WE_N: VRW_N; 11 IO4: SD3; 12 IO3: SD2; 13 IO2: SD1; 14 IO1: SD0; 15 A9: SA9; 16 A8: SA8; 17 A7: SA7

**UC4** 2114: 1 A6: SA6; 2 A5: SA5; 3 A4: SA4; 4 A3: SA3; 5 A0: SA0; 6 A1: SA1; 7 A2: SA2; 8 CS_N: GND; 10 WE_N: VRW_N; 11 IO4: SD7; 12 IO3: SD6; 13 IO2: SD5; 14 IO1: SD4; 15 A9: SA9; 16 A8: SA8; 17 A7: SA7

**UB3** 74LS244: 1 1OE_N: VREAD_N; 2 1A1: SD0; 3 2Y4: SD0; 4 1A2: SD1; 5 2Y3: SD1; 6 1A3: SD2; 7 2Y2: SD2; 8 1A4: SD3; 9 2Y1: SD3; 11 2A1: BD3; 12 1Y4: BD3; 13 2A2: BD2; 14 1Y3: BD2; 15 2A3: BD1; 16 1Y2: BD1; 17 2A4: BD0; 18 1Y1: BD0; 19 2OE_N: VRW_N

**UB4** 74LS244: 1 1OE_N: VREAD_N; 2 1A1: SD4; 3 2Y4: SD4; 4 1A2: SD5; 5 2Y3: SD5; 6 1A3: SD6; 7 2Y2: SD6; 8 1A4: SD7; 9 2Y1: SD7; 11 2A1: BD7; 12 1Y4: BD7; 13 2A2: BD6; 14 1Y3: BD6; 15 2A3: BD5; 16 1Y2: BD5; 17 2A4: BD4; 18 1Y1: BD4; 19 2OE_N: VRW_N

**J7** ?VIDEO_CONNECTOR: 1=TV_VIDEO, 2=GND, 3=VERT_DRIVE, 4=GND, 5=HORZ_DRIVE, 7=GND

Unused inputs: A1 pin 14 (CKA) is drawn as a stub, open (A1's QA unused); spare gates B1 gate 1,
E9 gate 2, E2 gates 3-4, D9 sections 1 and 6 (and 4-5 once the oscillator is replaced), E5
sections 1-4 are not drawn here (presumably used or tied on sheet 1). Tied inputs drawn: C8 pin 4
(FF1 K) to GND; B2 pins 10 (SER) and 15 (CLK INH) to GND; D2/D3/D4 pin 15 (G) to GND; 2114 pin 8
(CS) to GND; A2 pins 18, 20 to GND and 21 to +5V; D2 pins 2, 3, 5 to PU_R40; E8 pin 9 to PU_R27.
Unused outputs (stubs): C9 9 (QB), C5 3, D5 2 and 6, C1 6, D2 4, A1 12.

## Off-sheet signals

| Net | label on the sheet | direction | where |
|---|---|---|---|
| PHI0 | "phi0" (arrow out, left edge; overlined "VERT DRIVE" text beside it, see uncertain 5) | out | C8 pin 6, to the CPU |
| SYNC | "SYNC" (arrow out, right) | out | C6 pin 8 = VIDEO ON (high during the 200 text lines) |
| BLANKTV | "BLANK TV" (arrow in) | in | E9 pin 4: low blanks the picture |
| GRAPHIC | "GRAPHIC" (arrow in) | in | A2 pin 19 (A10): character set select |
| SEL8 | "SEL8" (arrow in) | in | C2 pin 12, D2/D3/D4 pin 1 (S). Must be **high** for a CPU access to $8000-$8FFF (see uncertain 6) |
| RW_E | "R/W.E" with a bar over E (arrow in) | in | C2 pin 13 (sheet 2 calls it RW_E; high = read) |
| RW_RAM | "R/W RAM" (arrow in) | in | D2 pin 6; from sheet 2 (J9 pin 6), low = write in phase 2 |
| BA0-BA9 | "BA0"-"BA9" | in | D2 10 13, D3 3 6 10 13, D4 3 6 10 13 |
| BD0-BD7 | "BD0"-"BD7" | both | B3/B4 pins 18,17 / 16,15 / 14,13 / 12,11 |
| TV_VIDEO, VERT_DRIVE, HORZ_DRIVE | J7 1, 3, 5 | out | to the monitor |

Not off-sheet but labelled: DIS_ON ("DIS. ON"), DIS_OFF ("DIS. OFF"), SD0-SD7 (bus "SD7-0 (8)"),
SA0-SA9 (bus "SA2-SA9 (8)"), VERT_DRIVE ("VERT DRIVE"), HORZ_DRIVE ("HORZ. DRIVE").
Names I made up: CLK8 CLK2M CLK1M(_N) PHI0_N CHLOAD(_N) CHLOAD_CLR_N HCNT_CLR H0-H5 H5_N
DISON_SET DISON_CLR RA0-RA2 ROW7_N VSTEP(_N) VSTEP_J VEND VS0 VS1 (_N) SYNC_N BLKEND_N LAT_HI(_N)
END25_N LATCH_G TA0-TA9 LA2-LA9 VRW_N VREAD_N CD0-CD7 DOT(_N) REV REV_N REV_D PIX_A_N PIX_B_N
PIXEL OSC8 PU_Rnn.

## Timing, derived from the wiring

All 74LS93/107/177 count on falling edges, the 74LS74 and 74LS165 on rising edges.

**Dot and character clock.** CLK8 = 8 MHz (125 ns dot). C9 is used as /8 on its CKB input:
QB 4 MHz (unused), QC = CLK2M (2 MHz), QD = CLK1M (1 MHz, one character = 8 dots = 1 us).
C8 FF2 (J = CLK1M, K = not CLK1M, clock CLK8 falling) copies CLK1M one dot later: Q = PHI0_N,
Q_N = **PHI0** to the CPU (1 MHz, high while CLK1M was low). C8 FF1 (J = 1, K = 0) is set by
CLK1M falling and cleared through D8 pin 8 = NAND(CLK8, its own Q) as soon as CLK8 is high:
CHLOAD is a pulse a fraction of a dot wide (roughly 20-60 ns with LS delays; timing-sensitive in
a simulation) once per character. CHLOAD_N (TP3-5) parallel-loads B2; CHLOAD's falling edge
advances the address counter (D5) and its rising edge moves the reverse bit into C1 FF2.

**Horizontal counter.** H0 = C9 QA (C9's CKA is on CLK1M, so H0 changes once a character),
H1-H4 = B5 QA-QD (B5 is a /16 on H0), H5 = C7 FF2 (J = K = 1, toggled by H4 falling). Count
n = H0 + 2 H1 + 4 H2 + 8 H3 + 16 H4 + 32 H5, 0..63: **64 characters = 64 us a line, 15.625 kHz**.
The counters' reset inputs (HCNT_CLR = D9 pin 6 = not PU_R12) are low unless TP3-2 is grounded.

**Display enable (DIS.ON, C5 FF2).** Clocked by H0 falling (count odd -> even), J = H3 and H5
(counts 40-47, 56-63), K = H4 and not H5 (counts 16-31), sampled from the count before the edge.
Set at the 41->42 edge, cleared at the 17->18 edge: **DIS.ON high for counts 42..63, 0..17 = 40
characters (40 us), low 24 us.** DIS.OFF = its Q_N.

**Horizontal drive (C5 FF1).** Clocked by H2 falling (every 8 counts), J = DIS.ON, K = DIS.OFF:
Q follows DIS.ON at the next multiple of 8. HORZ DRIVE = Q_N: **high counts 24..47 (24 us), low
counts 48..23 (40 us).**

**Video address while displaying.** D5 FF2 (TA0) and FF1 (TA1) are a /4 on CHLOAD, cleared while
DIS.ON is low; D6 (74177, CLK1 = TA1, CLK2 = its QA) and D7 (CLK1 = D6 QD, CLK2 = its QA) carry
TA2-TA9. Both 74177s have CT_LOAD_N = DIS.ON: during DIS.OFF their outputs follow the latch E6
(row start, bits 2-9; row starts are multiples of 40, so bits 0-1 are 0 and D5 can just be
cleared). During DIS.ON they count: 40 characters, address + 40. E6 (74100, both G = LATCH_G =
NAND(ROW7_N, VSTEP_N)) is transparent while ROW7_N is low, i.e. during DIS.ON of scan line 7
(B1: NAND(DIS.ON, RA0, RA1, RA2)); it closes when DIS.ON falls, holding start + 40 = the next
row's start. So each row's 8 scan lines re-read the same 40 bytes and the 9th line moves on.

**Scan line counter.** A1 (74LS93, /8 part only: CKB = DIS.ON, QB QC QD = RA0-RA2) steps at the
end of each line's display; R01 = VSTEP (R02 pulled up), so it restarts at each vertical step.
RA0-RA2 = character ROM A0-A2. 8 scan lines per row.

**Vertical state (B6).** FF1 J = VS1_N, K = VS1; FF2 J = VS0, K = VS0_N; both clocked by VSTEP
falling: (VS1 VS0) = 00 -> 01 -> 11 -> 10 -> 00, the drawing's legend:
00 VERT DRIVE (overlined), 01 TOP BLANK, 11 VIDEO ON, 10 BOTTOM BLANK. VERT DRIVE = D8 pin 11 =
NAND(VS0_N, VS1_N): **low in state 00**. VIDEO ON = C6 pin 8 = VS0 and VS1 (Net SYNC, also to
sheet 1).

**Vertical step (C7 FF1).** J = VSTEP_J = DIS.OFF and VEND, K = 1, clock CLK2M falling. VEND =
D8 pin 6 = NAND(END25_N, BLKEND_N):
- END25_N (D8 pin 3) = NAND(LA3, LA6, LA7, LA8, LA9 through E8 gate 1 and E5): the latched row
  start is 1000 = %1111101000 (bits 3, 6-9 set), the end of the 25th row;
- BLKEND_N (E8 pin 8) = NAND(TA6, RA2, not VIDEO ON, 1): in a blank state, address bit 6 (during
  DIS.OFF the 74177s show the latch, so LA6) with scan line >= 4.

When VEND and DIS.OFF are high, the next CLK2M fall sets VSTEP: VSTEP_N clears D6/D7 and opens
the latch (latch -> 0), VSTEP resets A1; that removes VEND, so the following CLK2M fall (K = 1)
clears VSTEP again: **a single 500 ns pulse**, whose falling edge steps B6.

Line counts between steps (a state starts with row start 0, scan line 0):
- VIDEO ON: ends after row 24's scan line 7 (latch = 1000): **25 rows x 8 = 200 lines**;
- each blank state: the latch reaches 80 (bit 6 set) after 2 rows; BLKEND fires in the DIS.OFF
  after scan line 3 of the third row: **2 x 8 + 4 = 20 lines**. TOP BLANK, VERT DRIVE, BOTTOM
  BLANK are 20 lines each (VERT DRIVE low for 20 x 64 us = 1.28 ms).
- Frame: 200 + 3 x 20 = **260 lines x 64 us = 16.64 ms, 60.1 Hz**. Text window **40 x 25**
  characters, 320 x 200 dots, display 40 us of each 64 us line.

**Dots.** Character ROM A2: A0-A2 = RA0-RA2, A3-A9 = SD0-SD6, A10 = GRAPHIC; outputs CD0-CD7 to
B2 A-H, so CD7 (H) is shifted out first (leftmost dot). B2 loads on CHLOAD_N, shifts on CLK8.
SD7 is the reverse bit: C1 FF1 latches it on PHI0_N rising, FF2 passes it on CHLOAD rising, in
step with the load. Pixel = E2 pin 3 = AND(NAND(DOT, REV), NAND(DOT_N, REV_N)) = DOT xor REV.
TV VIDEO DRIVE = E9 pin 6 = NAND(PIXEL, VIDEO ON, BLANKTV, DIS.ON) (active low dot).
Pipeline: the address advances at CHLOAD's fall and the byte is loaded at the next CHLOAD, so the
dots run one character behind DIS.ON (that is how the hardware is wired).

**CPU access to the screen ($8000).** With SEL8 high the three 74LS157s select B: SA0-SA9 =
BA0-BA9 (the 1 KB repeats through the 4 KB block), and D2's 2Y = RW_RAM drives the 2114 WE_N
(VRW_N) and the 244s' 2OE_N (BD -> SD, write). VREAD_N = NAND(SEL8, RW_E) enables the 244s' 1OE_N
(SD -> BD, read). Meanwhile the video side sees whatever the CPU addressed: SD feeds the
character ROM, so during the display the CPU's byte is drawn in place of the screen byte (the
PET 2001 "snow"). With SEL8 low the 157s select A (TA0-TA9), 2Y = PU_R40 (high: RAM read), both
buffer halves are off.

## Uncertain readings (15)

Coordinates in hp-3.png pixels; crops in `sheet3/`.

1. **Oscillator 74LS04 location** (~560-770, 510-570; `osc_png.png`, `osc_gif.png`, `osc081.png`):
   the location letters inside both gates are blacked out on all three scans. Best reading: D9
   (D9 3->4 and 5->6 are on this sheet, 9->8 and 11->10 free). E5 is excluded only if the 11->10
   inverter near D8 is E5 (item 2). Pins 11->10 and 9->8 read clearly.
2. **Inverter 11->10 near D8** (2045-2155, 2755-2835; `inv11p.png`, `inv11g.png`): location
   blacked out. Read as E5 like the 13->12 one beside it, whose label reads "E5" on the GIF
   (`e8b_g.png`). Could be another 74LS04.
3. **E8 gate 2 input pins** (2380-2500, 2700-2790; `e8b.png`, `e8bz.png`, `e8bz081.png`): the four
   labels are cramped ("10", a faint "9", "13", "12", a boxed "9"). Read: 10 = TA6 (D7 QA),
   13 = SYNC_N (E5 pin 12), 12 = RA2 (A1 QD), 9 = PU_R27. All four inputs of one NAND, so a
   swap does not change the logic.
4. **E6 latch input nest** (1560-2560, 2200-2600; `d1.png`-`d4.png`, `r_dense1.png`): eight
   parallel U-shaped lines. TA2 TA3 TA4 TA6 TA7 traced end to end; TA5 -> pin 21, TA8 -> 15,
   TA9 -> 16 by the nesting order (the extractor merged those three where they cross), which is
   also the only order consistent with the others.
5. **"VERT DRIVE phi0" at the left edge** (60-380, 1230-1260; `phi0.png`, `phi0g.png`): the arrow
   from C8 pin 6 carries the text "phi0" with an overlined "VERT DRIVE" in front of it. Read as
   PHI0 only (the older board's sheet 320081-3 has just "phi0" there; VERT DRIVE has its own
   wire to J7 pin 3).
6. **SEL8 polarity** (4750-5080, 1180-1290; `sel8.png`, `sel8g.png`): the stroke is under the "8",
   not an overline. The wiring needs SEL8 high for a CPU access (157 S high = B = BA inputs,
   NAND with RW_E for the read buffer), so it cannot be the 74LS154's active-low output itself;
   sheet 1 must provide it inverted. Check there.
7. **"R/W.E" with a bar over E** (C2 pin 13, `sel8.png`): named RW_E to join sheet 2 (whose label
   is "R/W.E", RW_E there). Logic needs it high for a read. Check the bar against sheet 1.
8. **B1 pin 10** (2985-3015, 925-945; `b1z.png`, `b1zg.png`): label overwritten ("1φ" blob between
   13 and 12). Read 10 (A1 QB); the only free input of gate 2.
9. **C8 pin 5 to C1 pin 3** (1636, 1176; `jog.png`): the line has a pen gap at a jog. Read as one
   wire (no other end nearby).
10. **B2 pin 9 to C2 pin 2** (3678, 600-640; `gap2.png`): faint dropout on the vertical. Read as
    one wire.
11. **C2 pin 11 to B3 pin 1** (4740-4815, 1405-1440; `c2d.png`): joins the B3 pin 1 / B4 pin 1
    line at a dot beside the pin. Read as one Net (VREAD_N).
12. **DIS.ON to A1 pin 1, B1 pin 13 and E9 pin 5** (2942, 900; `r_a1top.png`, `r_a1b1.png`): dot
    at the corner; clear on the scan (the extractor first missed it).
13. **C9 pin 9 (QB) and C5 pin 3** (`a_cnt.png`, `a_c5.png`): drawn with a short stub and no
    destination. Read unconnected.
14. **A1 pin 14 (CKA) / pin 12 (QA)** (`b1a1.png`): stubs only. CKA left open (floats high in TTL).
15. **E9 input order** (`e9.png`): lines at y 675 / 705 / 732 under labels 1 / 2 / 4, pin 5 from
    the top. Read 1 = PIXEL, 2 = SYNC (VIDEO ON), 4 = BLANKTV, 5 = DIS.ON; all one NAND anyway.

Confident (checked on zoomed crops): every 74LS107/93/74/157/165/177/100/2114/2316B/244 pin
number, the D6/D7/D3/D4 junctions (`d1.png`-`d4.png`), E6 pin 4 -> D8 pin 1 dot (`e6q.png`),
the bus crossings at the SD bus (`bus1.png`, `bus2.png`: no dots), J7 (`j7.png`), D2-D4 pin 15
ground symbol (`g15.png`), C6 pin 12 = DIS.OFF and pin 13 = D8 pin 6 (`r_c6d.png`).

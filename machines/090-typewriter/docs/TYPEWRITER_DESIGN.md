# 090 — TV Typewriter — Machine Design

How the Machine in [TYPEWRITER.md](TYPEWRITER.md) is built: netlist, Board, Puzzle files, manuals. P4q brings the 2524 and the 2518 (and the DIP-8 package); every other Part is in the catalogue (P4n the 74LS244 and the `KEYBOARD`, P4l the 2513 and the 74LS165). The step is [SHIFT_TYPEWRITER.md](../../../docs/p4q-shift-typewriter/SHIFT_TYPEWRITER.md).

## Files

Machine name `typewriter`:

```
typewriter.bwn                          design netlist
board/typewriter.place                  panel parts and the 2513 (bw place rules)
board/typewriter.kicad_pcb, .kicad_pro  Board (placed by bw place, routed by the user)
manuals/user.md, service.md
puzzles/NN-*.bwp                        6 Puzzles, easiest first
```

The font is 080's `machines/080-message/roms/2513.rom`, unchanged.

## Netlist

`machines/090-typewriter/typewriter.bwn` (TYPEWRITER.md, Circuit), with the header comment of [ADD_MACHINE.md](../../../docs/recipes/ADD_MACHINE.md) step 2. 40 chips, `X1`, `SW1`, `R1`, `J1`–`J3`. Every unused TTL input is tied (spare gates and unused data inputs to `GND`, unused NAND inputs to `VCC`), so the netlist loads without warnings. Unused outputs stay unconnected: `U1` `QA` `QD`, `U5` gate 6, `U3` `QC` `QD` `RCO`, `U8` `QB`–`QD` `RCO`, `U24` `QH_N`, `U25` `1Q_N`–`3Q_N` and `3Q`, `4Q`, `U26` and `U27` `2Y3` `2Y4`, `U28` `1Q_N`, `U29` gate 4, `U33` `QB`–`QD` `RCO`, `U34` `RCO`, `U39` `1Q_N` and half 2, `U40` `QB` `QC`. The negative supply pins (2513 `VGG` `VDD`, 2524 `VDD`, 2518 `VGG`) and the 2513's `NC` pins are no-connect.

```
part X1  OSC freq=4MHz
part U1  74LS161    # dot counter 10-15
part U2  74LS161    # C0-C3
part U3  74LS161    # C4, C5
part U4  74LS20     # LINE_END_N, HSYNC_N
part U5  74LS04     # DOT_LOAD_N, QB_N, CLK_N, D1_N, D4_N
part U6  74LS161    # V0-V3
part U7  74LS161    # V4-V7
part U8  74LS161    # V8
part U9  74LS30     # FRAME_END_N
part U10 74LS20     # VSYNC_N, ROW_CLK
part U11 74LS86     # composite sync; cursor compare V6, V7
part U12 74LS02     # ACTIVE, CTL, CUR_SHOW, MATCH
part U13 74LS08     # GL1-GL3, ROW_TOP
part U14 74LS02     # V01_ZERO, V23_ZERO, CLEAR, GO
part U15 74LS20     # PHI1_N, PHI2_N
part U16 2524       # page, bit 0
part U17 2524       # page, bit 1
part U18 2524       # page, bit 2
part U19 2524       # page, bit 3
part U20 2524       # page, bit 4
part U21 2524       # page, bit 5
part U22 2518       # row buffer
part U23 2513 contents=roms/2513.rom
part U24 74LS165    # dots out
part U25 74LS175    # output registers, LINE_END
part U26 74LS244    # keyboard onto the write bus
part U27 74LS244    # spaces onto the write bus (Clear)
part U28 74LS74     # KEY, WR
part U29 74LS00     # WRK_N, WRITE, ROW_LOAD_N
part U30 74LS30     # Return decoder
part U31 74LS00     # PRINT, ADV, XC_N, YEN
part U32 74LS161    # cursor X0-X3
part U33 74LS161    # cursor X4
part U34 74LS161    # cursor Y0-Y3
part U35 74LS86     # cursor compare C0-C3
part U36 74LS86     # cursor compare C4, V4, V5; cursor into the video
part U37 74LS27     # cursor compare
part U38 74LS20     # MATCH_N, GO_N
part U39 74LS74     # cursor, one character late
part U40 74LS93     # blink
part SW1 BUTTON     # Clear
part R1  R value=4k7
part J1  POWER
part J2  VIDEO
part J3  KEYBOARD strobe=25ms

net CLK         X1.OUT U1.CLK U2.CLK U3.CLK U25.CLK U5.3A

# Dot counter 10..15: 6 dots a character; DOT_END, QB and QC time the page and row buffer clocks.
net VCC         U1.CLR_N U1.ENP U1.ENT U1.B U1.D
net GND         U1.A U1.C
net DOT_END     U1.RCO U5.1A U2.ENP U3.ENP U4.1A U39.1CLK U15.1C
net DOT_LOAD_N  U5.1Y U1.LOAD_N U24.SH_LD_N
net QB          U1.QB U5.2A
net QC          U1.QC U10.2A U15.2C
net QB_N        U5.2Y U15.2D
net VCC         U15.1D

# Column counter 0..41, reloaded with 0 after character 41.
net VCC         U2.CLR_N U2.ENT U3.CLR_N
net GND         U2.A U2.B U2.C U2.D U3.A U3.B U3.C U3.D
net C0          U2.QA U4.1D U35.1A
net C1          U2.QB U35.2A
net C2          U2.QC U35.3A
net C3          U2.QD U4.1C U4.2B U35.4A
net CC          U2.RCO U3.ENT
net C4          U3.QA U36.1A
net C5          U3.QB U4.1B U4.2A U12.1A
net VCC         U4.2C U4.2D
net LINE_END_N  U4.1Y U2.LOAD_N U3.LOAD_N U25.4D
net LINE_END    U25.4Q_N U6.CLK U7.CLK U8.CLK
net HSYNC_N     U4.2Y U11.1A

# Line counter 0..311, clocked at each line end, loaded with 0 at the end of line 311.
net VCC         U6.CLR_N U6.ENP U6.ENT U7.CLR_N U7.ENP U8.CLR_N U8.ENP U9.G U9.H
net GND         U6.A U6.B U6.C U6.D U7.A U7.B U7.C U7.D U8.A U8.B U8.C U8.D
net V0          U6.QA U9.F U14.1A
net V1          U6.QB U9.E U13.1A U14.1B
net V2          U6.QC U9.D U10.1D U13.2A U14.2A
net V3          U6.QD U13.3A U14.2B
net VC1         U6.RCO U7.ENT
net V4          U7.QA U9.C U10.1C U36.2A
net V5          U7.QB U9.B U10.1B U36.3A
net V6          U7.QC U11.3A
net V7          U7.QD U11.4A
net VC2         U7.RCO U8.ENT
net V8          U8.QA U9.A U10.1A U12.1B U40.CKA
net FRAME_END_N U9.Y U6.LOAD_N U7.LOAD_N U8.LOAD_N
net VSYNC_N     U10.1Y U11.1B

# Composite sync: HSYNC XOR VSYNC, then inverted (as 060).
net SYNC        U11.1Y U11.2A
net VCC         U11.2B
net SYNC_N_D    U11.2Y U25.1D

# Text area: characters 0-31 of lines 0-255. Outside it the character ROM reads its blank line 0.
net ACTIVE      U12.1Y U13.1B U13.2B U13.3B U15.1B U15.2B U10.2B U38.1D
net GL1         U13.1Y U23.A1
net GL2         U13.2Y U23.A2
net GL3         U13.3Y U23.A3

# Page clocks: on the top line of each text row, PHI2 on dots 12-13 and PHI1 on dot 15 of characters 0-31.
net V01_ZERO    U14.1Y U13.4A U29.3A
net V23_ZERO    U14.2Y U13.4B U29.3B
net ROW_TOP     U13.4Y U15.1A U15.2A U38.2B
net PHI1_N      U15.1Y U16.PHI1 U17.PHI1 U18.PHI1 U19.PHI1 U20.PHI1 U21.PHI1
net PHI2_N      U15.2Y U16.PHI2 U17.PHI2 U18.PHI2 U19.PHI2 U20.PHI2 U21.PHI2 U28.2CLK

# Page: six 512-bit recirculating shift registers, one per bit; a cell written from W0-W5 while WRITE is high.
net VCC         U16.READ U17.READ U18.READ U19.READ U20.READ U21.READ
net WRITE       U29.2Y U16.WRITE U17.WRITE U18.WRITE U19.WRITE U20.WRITE U21.WRITE
net W0          U16.IN U26.1Y1 U27.1Y1
net W1          U17.IN U26.1Y2 U27.1Y2
net W2          U18.IN U26.1Y3 U27.1Y3
net W3          U19.IN U26.1Y4 U27.1Y4
net W4          U20.IN U26.2Y1 U27.2Y1
net W5          U21.IN U26.2Y2 U27.2Y2
net P0          U16.OUT U22.IN1
net P1          U17.OUT U22.IN2
net P2          U18.OUT U22.IN3
net P3          U19.OUT U22.IN4
net P4          U20.OUT U22.IN5
net P5          U21.OUT U22.IN6

# Row buffer: one text row, round once a line; it takes the row from the page on the row's top line.
net VCC         U10.2C U10.2D
net ROW_CLK     U10.2Y U22.CLK
net ROW_LOAD_N  U29.3Y U22.REC
net B0          U22.OUT1 U23.A4
net B1          U22.OUT2 U23.A5
net B2          U22.OUT3 U23.A6
net B3          U22.OUT4 U23.A7
net B4          U22.OUT5 U23.A8
net B5          U22.OUT6 U23.A9

# Character ROM: the dots of one glyph line, F5 leftmost.
net GND         U23.CE_N
net F1          U23.O1 U24.D
net F2          U23.O2 U24.E
net F3          U23.O3 U24.F
net F4          U23.O4 U24.G
net F5          U23.O5 U24.H

# Shift register: loads on a character's last dot, shifts in the middle of each dot.
net CLK_N       U5.3Y U24.CLK
net GND         U24.CLK_INH U24.SER U24.A U24.B U24.C
net VIDEO_D     U24.QH U36.4A

# Output registers: sync, video with the cursor and the line end, re-timed on the clock.
net VCC         U25.CLR_N
net GND         U25.3D
net SYNC_N      U25.1Q J2.SYNC_N
net VIDEO_M     U36.4Y U25.2D
net VIDEO       U25.2Q J2.VIDEO

# Keyboard: bits 0-5 onto the write bus; Return and printable codes decoded.
net D0          J3.D0 U26.1A1 U30.F
net D1          J3.D1 U26.1A2 U5.4A
net D2          J3.D2 U26.1A3 U30.D
net D3          J3.D3 U26.1A4 U30.C
net D4          J3.D4 U26.2A1 U5.5A
net D5          J3.D5 U26.2A2 U12.2B
net D6          J3.D6 U12.2A
net GND         U26.2A3 U26.2A4
net D1_N        U5.4Y U30.E
net D4_N        U5.5Y U30.B
net CTL         U12.2Y U30.A U31.1A U31.1B U31.2A
net VCC         U30.G U30.H
net CR_N        U30.Y U31.2B U31.4B U32.LOAD_N U33.LOAD_N
net PRINT       U31.1Y U29.1B
net ADV         U31.2Y U32.ENT

# Write control: KEY set by the strobe; GO while the cursor's cell is at the page's output; WR after its PHI2.
net STROBE      J3.STROBE U28.1CLK
net VCC         U28.1D U28.1PRE_N U28.2PRE_N U28.2CLR_N U38.2D
net KEY         U28.1Q U38.2A
net MATCH       U12.4Y U38.2C
net GO_N        U38.2Y U14.4A U14.4B
net GO          U14.4Y U28.2D U29.1A
net WRK_N       U29.1Y U29.2A
net WR          U28.2Q U32.CLK U33.CLK U34.CLK
net WR_N        U28.2Q_N U28.1CLR_N

# Cursor counter: X0-X4 the column, Y0-Y3 the row; steps when WR rises after a printable key or Return.
net GND         U32.A U32.B U32.C U32.D U33.A U33.B U33.C U33.D U34.A U34.B U34.C U34.D
net VCC         U32.ENP U33.ENP U34.ENP U34.LOAD_N
net X0          U32.QA U35.1B
net X1          U32.QB U35.2B
net X2          U32.QC U35.3B
net X3          U32.QD U35.4B
net XRCO        U32.RCO U33.ENT U31.3A
net X4          U33.QA U36.1B U31.3B
net XC_N        U31.3Y U31.4A
net YEN         U31.4Y U34.ENT
net Y0          U34.QA U36.2B
net Y1          U34.QB U36.3B
net Y2          U34.QC U11.3B
net Y3          U34.QD U11.4B

# Clear: spaces onto the write bus, every cell written, the cursor home.
net VCC         R1.1
net CLEAR_N     R1.2 SW1.1 U27.1OE_N U27.2OE_N U29.2B U14.3A U14.3B U32.CLR_N U33.CLR_N U34.CLR_N
net GND         SW1.2
net CLEAR       U14.3Y U26.1OE_N U26.2OE_N
net GND         U27.1A1 U27.1A2 U27.1A3 U27.1A4 U27.2A1 U27.2A3 U27.2A4
net VCC         U27.2A2

# Cursor: beam address = cursor address; shown one character late, blinking.
net NE0         U35.1Y U37.1A
net NE1         U35.2Y U37.1B
net NE2         U35.3Y U37.1C
net NE3         U35.4Y U37.2A
net NE4         U36.1Y U37.2B
net NE5         U36.2Y U37.2C
net NE6         U36.3Y U37.3A
net NE7         U11.3Y U37.3B
net NE8         U11.4Y U37.3C
net EQ0         U37.1Y U38.1A
net EQ1         U37.2Y U38.1B
net EQ2         U37.3Y U38.1C
net MATCH_N     U38.1Y U39.1D U12.4A U12.4B
net VCC         U39.1PRE_N U39.1CLR_N
net CURSOR_N    U39.1Q U12.3A
net FRAME2      U40.QA U40.CKB
net GND         U40.R01 U40.R02
net BLINK       U40.QD U12.3B
net CUR_SHOW    U12.3Y U36.4B

# Spare gates.
net GND         U5.6A U29.4A U29.4B U39.2D U39.2CLK
net VCC         U39.2PRE_N U39.2CLR_N
```

### Timing notes

- `DOT_END`, `CC`, `VC1`: carries with `ENT` tied high or chained from one, no glitch (as 080). `V8` alone clocks `U40`. `LINE_END` is re-timed in `U25` (P4p).
- Page clocks: `PHI2_N` = NAND(`ROW_TOP`, `ACTIVE`, `QC`, `QB_N`) is low while `U1` is at 12 or 13 (`QD`–`QA` = `110x`): `QC` rises into it at 12 while `QB_N` is still low, `QB_N` rises 10 ns later and holds it until `QB` rises at 14; at 10 and 11 `QC` is low. `PHI1_N` = NAND(`ROW_TOP`, `ACTIVE`, `DOT_END`) is low during dot 15 and rises at the character boundary as `DOT_END` falls (18 ns); `ACTIVE` changes at characters 0 and 32 only after that (25 ns), and `ROW_TOP` in line 0's character 0 later still. Each edge comes from one signal. Pulses 500 ns and 250 ns low (2524: at least 135 ns); `PHI1_N` falls about 250 ns after `PHI2_N` rises, and `PHI2_N` falls about 1 µs after `PHI1_N` rises (at least 10 ns). The dot counter's `QA`, which changes every dot, reaches no gate: fewer evaluations at 4 MHz.
- `ROW_CLK` = NAND(`QC`, `ACTIVE`) is low during dots 12–15 (1 µs; 2518: at least 300 ns low, 200 ns high: dots 10–11 give 500 ns) and rises when `QC` falls at a character's start (15 ns after the clock, `U1`). Within a character only `QC` moves it, once each way. At the start of character 0, `ACTIVE` rises after `QC` has fallen (`C5` falls 15 ns after the clock, then 10 ns through `U12`; `QC` 15 ns): no glitch; at character 32 `ACTIVE` falls while `QC` is low already. At line 0, `V8` changes about 35 ns into the line, later still.
- In character `k` (0–31) of a top line: cell (`r`, `k`) has been on `P0`–`P5` since 100 ns into the character (the rise of `PHI1_N` at the end of character `k` − 1, or of row `r` − 1's top line, or of line 240 for row 0); `PHI2_N` rises at the end of dot 13, writing that cell if `WRITE` is high; at the end of dot 15 `ROW_CLK` and `PHI1_N` rise together: `U22` takes `P0`–`P5` (`ROW_LOAD_N` low on `REC`: 2518 data setup 100 ns, recirculate setup 150 ns, hold 50 ns) and the page shows the next cell 100 ns later. After the last top line of a frame, cell 0 is at the page's output. That replaces the word shown in column `k`, which on a top line is glyph line 0, blank.
- `U22`'s word `k` is on `B0`–`B5` 300 ns into character `k` (its clock rose as `k` started; word 0 since the end of character 31 of the line before). The 2513 answers 500 ns later, during dot 13; `DOT_LOAD_N` falls at the start of dot 15: about 450 ns to spare. Character 0 also takes the new line's `V1`–`V3` about 35 ns in, 450 ns before the bus settles.
- `MATCH_N` and so `GO_N`, `GO`, `WRK_N` and `WRITE` decode column bits and may glitch at a character's start: they are read only at the rise of `PHI2_N` (the 2524's `WRITE` and `U28`'s `D`), at the end of dot 13, and `U39` samples `MATCH_N` on `DOT_END` (P4p). `WRITE` changes again no sooner than about 60 ns after that rise (`WR`, then the cursor counters, the comparator and three gates): 2524 hold 20 ns. One race is left, as on any asynchronous key input: a `STROBE` rising within about 20 ns before the rise of `PHI2_N` on the cursor's cell reaches `U28`'s `D` (`GO`, two gates after `KEY`) but not `WRITE` (two more): the cursor steps and the key is not written. About one key in a million; not modelled away.
- `WR` rises 13 ns after the rise of `PHI2_N` that wrote the cell, steps the cursor counters and clears `KEY` through `WR_N` (25 ns): the next rise of `PHI2_N` (the next character, the next top line after column 31, or line 0 of the next frame after row 15's: 4.54 ms) takes `GO` low and `WR` falls. While `WR` is high `KEY` is held clear. A key is taken at most 19.66 ms after its `STROBE` rose and `WR` is low at most 4.54 ms after that: 24.2 ms, under the 25.002 ms to the next `STROBE`. A strobe shorter than about 24.2 ms would lose keys.
- The longest pause in the page clocks is from line 240's character 31 to line 0's character 0: 72 lines, 4.5 ms, under the 10 ms the model keeps the bits (P4q spec).
- Clear held: `WRITE` high throughout, `U27` on the write bus, every cell written with 0x20 as it comes round; `WR` may still rise for a waiting key (its cell is written with a space), and the cursor counters stay cleared.

### Where the page lands on the TV

As P4p: the text starts about TV column 34 (17 dots after the line start, one dot 2 columns) and is 384 columns wide; line 0 is about 4 rows below the frame start. The tests read both offsets from the picture.

## Board

Placed by `bw place machines/090-typewriter/typewriter.bwn machines/090-typewriter/board/typewriter.place -o machines/090-typewriter/board/typewriter.kicad_pcb` (P4k), then routed by the user in KiCad ([KICAD_GUIDE.md](../../../docs/p2a-board-layout/KICAD_GUIDE.md) steps 7–8), 2 layers, no zones.

- `typewriter.place`:

  ```
  # 090 TV typewriter: panel parts and the character ROM (machines/090-typewriter/docs/TYPEWRITER_DESIGN.md, Board)
  edge J1 left middle    # power
  edge J2 back right     # TV
  edge J3 front left     # keyboard
  edge SW1 front right   # Clear
  next R1 SW1 left       # CLEAR_N pull-up
  row back left U23      # character ROM: DIP-24, no slot in the chip grid
  ```

- The 2513 is a 600-mil DIP-24, too big for the chip grid (as on 080). The 2524s (DIP-8, P4q), the 2518 (DIP-16) and the 74LS244s (DIP-20) fit the grid. The 39 other chips and `X1` go in the annealed grid.
- Puzzle constraints (P4r): `U18` pin 1 at the end of `PHI2_N`'s Trace (from `U17` pin 1, through a via) and `U30` pin 1 on its own branch of `CTL` (straight from `U12` pin 4); Zeros needs only `U20` pins 2 and 3 as wired (`OUT`, `READ` tied to VCC). A re-route keeps those or moves the cracks.
- `bw check-board` prints `ok`; `drc_typewriter` and `place_drc_typewriter` pass (picked up by the globs).

## Puzzles

`machine typewriter` in each file (TYPEWRITER.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `lookalikes.bwp` | `fault crack U17.1 U18.1 50%` | `PHI2_N` |
| `wallpaper.bwp` | `fault stuck U29.2Y 1` | `WRITE` |
| `copies.bwp` | `fault stuck U22.REC 1` | `ROW_LOAD_N` into the row buffer |
| `every_other_line.bwp` | `fault joint U36.5` | `Y0` into the cursor comparator |
| `zeros.bwp` | `fault bridge U20.2 U20.3` | `P4` to the VCC tie on `READ` |
| `m_is_return.bwp` | `fault crack U12.4 U30.1 50%` | `CTL` at the Return decoder |

Titles and briefs:

- Lookalikes: "Some letters come out as others that look a little like them: HELLO shows as HAHHK."
- Wallpaper: "The whole screen is one character over and over, and each key typed fills the screen with that key. Clear blanks it only while it is held."
- Copies: "Every line of the screen shows the same row of characters. Typing and Clear change nothing on the screen, though the block still moves."
- Every other line: "After Clear the block is on the second line, not the first. Return goes down a line only every second time: the next line types over the one before."
- Zeros: "Clear fills the screen with zeros instead of blanking it, and the letters A to O come out as other letters and signs."
- M is Return: "Every M ends its line: the M shows, and what is typed next starts on the line below. A hyphen does the same."

### Electrical Nets on a damaged Board

- Lookalikes: `U18` `PHI2` alone on its Net, which has only a MOS input: it reads X but never rises. 10 ms after power-on (`PHI1` rose last) every bit of `U18` fades to 0, and no key writes it. A new `U18` fades the same way.
- Wallpaper: `WRITE` stuck at 1 (Strong): every cell is written at each rise of `PHI2_N`, from `W0`–`W5`: `U26`'s copy of `D0`–`D5`, or spaces from `U27` while Clear is held. Before the first key the keyboard leaves `D0`–`D6` undriven, so what fills the page depends on the power-on seed. `KEY`, `WR` and the cursor work as usual.
- Copies: `U22` reads `REC` as 1: it recirculates its power-on words forever. The page is written as usual (the test reads it from the 2524s); the TV shows the buffer's 32 words on every row.
- Every other line: `U36` pin 5 floats (reads high): `NE5` = NOT `V4`, so the cursor's row `r` matches text row `r` OR 1. The cursor counter is right (the test reads it).
- Zeros: `P4` and the `READ` tie are one Net with VCC (Supply) winning against `U20`'s output: `U22` takes bit 4 as 1 in every word. The page is right. A new `U20` drives the same Contention.
- M is Return: `U30` pin 1 floats (reads high): `CR_N` is low for any code ending in `01101` (13, `-`, `M`). `PRINT` is unchanged, so `M` and `-` are written, then `CR_N` loads 0 into `U32`, `U33` and `YEN` steps the row at `WR`.

The test (`tests/machines/typewriter_test.cpp`) presses Clear, types, and checks the page from the 2524s (or the TV, for Copies and Zeros, where the page is right and the picture is not); then the repair, and the fault-free page. The TV checks keep the cursor off text row 0: the TV draws the frame's first line about four characters to the left, which only a cursor on row 0 shows (a small bar in the left border; the Machine's timing is right, a VCD shows `CUR_SHOW` on line 0 at the cursor's column).

## Manuals

Written to STYLE's Manuals rules (P6k, the Counter's as the model):

- User: What it does (a welcome: a keyboard and a TV made a typewriter, 16 lines of 32 characters, the blinking cursor, the page in shift registers as in 1973); Using it (random page at power-on, CLEAR (`SW1`) blanks it and homes the cursor, typing, wrap at 32 characters, Return, other control keys ignored, no scrolling, keys faster than about 40 a second); How it works (the goal, then four parts: the beam counters and the sync; the page going round, a merry-go-round of 512 seats moved by `PHI1_N` and `PHI2_N` on each row's top line; the row buffer and the character ROM, one row brought back on each of its 16 lines, `GL1`–`GL3`; the keyboard and the cursor: `KEY`, the cell coming round, `WRITE`, `WR`, Return, CLEAR; no reset; the unused pins in a table).
- Service: Components (every reference, netlist order, formulas reworded); Test points (probe readings idle, after CLEAR, while CLEAR is held and after a key or Return, checked with `bw run --probe` at 1 s, 1.08 s, 1.3 s and 1.5 s idle, and with `--press SW1:500ms:700ms --type 'J3:1s:HI\n'` at 0.6 s, 0.9 s, 1.002 s, 1.05 s, 1.15 s and 1.25 s; four logic analyzer setups: a text line, a row's top line with the page clocks, a picture, a key written, each checked with `bw_ui --clip … --screenshot` after 1 s, the key's after `--press SW1:500ms:700ms --type J3:1s:HI` at 1.1 s); Troubleshooting (no picture or no lock, wrong characters, CLEAR does not blank, typing goes wrong: ordered checks, never naming a fault).

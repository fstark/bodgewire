# 090 — TV Typewriter — Machine Design

How the Machine in [TYPEWRITER.md](TYPEWRITER.md) is built: netlist, Board, manuals. Every Part it uses is in the catalogue (P4n brought the 2114, the 74LS244 and the `KEYBOARD`; P4l the 2513 and the 74LS165); the step is [TV_TYPEWRITER.md](../../p4p-tv-typewriter/TV_TYPEWRITER.md).

## Files

Machine name `typewriter`:

```
netlists/typewriter.bwn                        design netlist
boards/typewriter/typewriter.place             panel parts and the 2513 (bw place rules)
boards/typewriter/typewriter.kicad_pcb, .kicad_pro   Board (placed by bw place, routed by the user)
manuals/typewriter/user.md, service.md
```

The font is 080's `roms/2513.rom`, unchanged.

## Netlist

`netlists/typewriter.bwn` (TYPEWRITER.md, Circuit), with the header comment of [ADD_MACHINE.md](../../recipes/ADD_MACHINE.md) step 2. 36 chips, `X1`, `SW1`, `R1`, `J1`–`J3`. Every unused TTL input is tied (spare gates and unused data inputs to `GND`, unused NAND inputs to `VCC`), so the netlist loads without warnings. Unused outputs stay unconnected: `U1` `QA`–`QD`, `U3` `QC` `QD` `RCO`, `U8` `QB`–`QD` `RCO`, `U5` gates 2 and 6, `U12` gate 4, `U16` `2Y`–`4Y`, `U18` `IO3` `IO4` (MOS, not warned), `U20` `QH_N`, `U21` `1Q_N`–`3Q_N` and `4Q`, `U22` and `U23` `2Y3` `2Y4`, `U24` `1Q_N` `2Q_N`, `U28` nothing, `U29` `QB`–`QD` `RCO`, `U30` `RCO`, `U34` gate 2, `U35` `1Q_N` and half 2, `U36` `QB` `QC`. The 2513's `VGG`, `VDD` and `NC` pins are no-connect.

```
part X1  OSC freq=4MHz
part U1  74LS161    # dot counter 10-15
part U2  74LS161    # C0-C3
part U3  74LS161    # C4, C5
part U4  74LS20     # LINE_END_N, HSYNC_N
part U5  74LS04     # DOT_LOAD_N, CLK_N, D1_N, D4_N
part U6  74LS161    # V0-V3
part U7  74LS161    # V4-V7
part U8  74LS161    # V8
part U9  74LS30     # FRAME_END_N
part U10 74LS20     # VSYNC_N, KEY_CLR_N
part U11 74LS86     # composite sync; cursor compare V6, V7
part U12 74LS02     # ACTIVE, CTL, CUR_SHOW
part U13 74LS08     # R1-R3, WE_N
part U14 74LS157    # RAM address 0-3
part U15 74LS157    # RAM address 4-7
part U16 74LS157    # RAM address 8
part U17 2114       # page RAM, bits 0-3
part U18 2114       # page RAM, bits 4-5
part U19 2513 contents=../roms/2513.rom
part U20 74LS165    # dots out
part U21 74LS175    # output registers, write strobe, LINE_END
part U22 74LS244    # keyboard onto the bus
part U23 74LS244    # spaces onto the bus (Clear)
part U24 74LS74     # KEY, WR
part U25 74LS30     # write decoder
part U26 74LS30     # Return decoder
part U27 74LS00     # PRINT, ADV, XC_N, YEN
part U28 74LS161    # cursor X0-X3
part U29 74LS161    # cursor X4
part U30 74LS161    # cursor Y0-Y3
part U31 74LS86     # cursor compare C0-C3
part U32 74LS86     # cursor compare C4, V4, V5; cursor into the video
part U33 74LS27     # cursor compare
part U34 74LS20     # MATCH_N
part U35 74LS74     # cursor, one character late
part U36 74LS93     # blink
part SW1 BUTTON     # Clear
part R1  R value=4k7
part J1  POWER
part J2  VIDEO
part J3  KEYBOARD strobe=100us

net CLK         X1.OUT U1.CLK U2.CLK U3.CLK U21.CLK U5.3A

# Dot counter 10..15: 6 dots a character.
net VCC         U1.CLR_N U1.ENP U1.ENT U1.B U1.D
net GND         U1.A U1.C
net DOT_END     U1.RCO U5.1A U2.ENP U3.ENP U4.1A U35.1CLK
net DOT_LOAD_N  U5.1Y U1.LOAD_N U20.SH_LD_N

# Column counter 0..41, reloaded with 0 after character 41.
net VCC         U2.CLR_N U2.ENT U3.CLR_N
net GND         U2.A U2.B U2.C U2.D U3.A U3.B U3.C U3.D
net C0          U2.QA U4.1D U14.1A U25.C U31.1A
net C1          U2.QB U14.2A U25.D U31.2A
net C2          U2.QC U14.3A U31.3A
net C3          U2.QD U4.1C U4.2B U14.4A U31.4A
net CC          U2.RCO U3.ENT
net C4          U3.QA U15.1A U32.1A
net C5          U3.QB U4.1B U4.2A U12.1A U14.S U15.S U16.S U25.B U24.2CLK
net VCC         U4.2C U4.2D
net LINE_END_N  U4.1Y U2.LOAD_N U3.LOAD_N U21.4D
net LINE_END    U21.4Q_N U6.CLK U7.CLK U8.CLK U28.CLK U29.CLK U30.CLK U10.2B
net HSYNC_N     U4.2Y U11.1A

# Line counter 0..311, clocked at each line end (LINE_END from U21 below), loaded with 0 at the end of line 311.
net VCC         U6.CLR_N U6.ENP U6.ENT U7.CLR_N U7.ENP U8.CLR_N U8.ENP U9.G U9.H
net GND         U6.A U6.B U6.C U6.D U7.A U7.B U7.C U7.D U8.A U8.B U8.C U8.D
net V0          U6.QA U9.F
net V1          U6.QB U9.E U13.1A
net V2          U6.QC U9.D U10.1D U13.2A
net V3          U6.QD U13.3A
net VC1         U6.RCO U7.ENT
net V4          U7.QA U9.C U10.1C U15.2A U32.2A
net V5          U7.QB U9.B U10.1B U15.3A U32.3A
net V6          U7.QC U15.4A U11.3A
net V7          U7.QD U16.1A U11.4A
net VC2         U7.RCO U8.ENT
net V8          U8.QA U9.A U10.1A U12.1B U36.CKA
net FRAME_END_N U9.Y U6.LOAD_N U7.LOAD_N U8.LOAD_N
net VSYNC_N     U10.1Y U11.1B

# Composite sync: HSYNC XOR VSYNC, then inverted (as 060).
net SYNC        U11.1Y U11.2A
net VCC         U11.2B
net SYNC_N_D    U11.2Y U21.1D

# Blanking: outside columns 0-31 and lines 0-255 the character ROM is asked for its blank line 0.
net ACTIVE      U12.1Y U13.1B U13.2B U13.3B U34.1D
net R1          U13.1Y U19.A1
net R2          U13.2Y U19.A2
net R3          U13.3Y U19.A3

# RAM address: the beam during characters 0-31, the cursor during 32-41.
net GND         U14.G_N U15.G_N U16.G_N U16.2A U16.2B U16.3A U16.3B U16.4A U16.4B
net RA0         U14.1Y U17.A0 U18.A0
net RA1         U14.2Y U17.A1 U18.A1
net RA2         U14.3Y U17.A2 U18.A2
net RA3         U14.4Y U17.A3 U18.A3
net RA4         U15.1Y U17.A4 U18.A4
net RA5         U15.2Y U17.A5 U18.A5
net RA6         U15.3Y U17.A6 U18.A6
net RA7         U15.4Y U17.A7 U18.A7
net RA8         U16.1Y U17.A8 U18.A8
net GND         U17.A9 U18.A9 U17.CS_N U18.CS_N
net WE_N        U13.4Y U17.WE_N U18.WE_N

# Data bus: the RAMs, the two buffers and the character ROM's character address.
net B0          U17.IO1 U22.1Y1 U23.1Y1 U19.A4
net B1          U17.IO2 U22.1Y2 U23.1Y2 U19.A5
net B2          U17.IO3 U22.1Y3 U23.1Y3 U19.A6
net B3          U17.IO4 U22.1Y4 U23.1Y4 U19.A7
net B4          U18.IO1 U22.2Y1 U23.2Y1 U19.A8
net B5          U18.IO2 U22.2Y2 U23.2Y2 U19.A9

# Character ROM: the dots of one glyph line, F5 leftmost.
net GND         U19.CE_N
net F1          U19.O1 U20.D
net F2          U19.O2 U20.E
net F3          U19.O3 U20.F
net F4          U19.O4 U20.G
net F5          U19.O5 U20.H

# Shift register: loads on a character's last dot, shifts in the middle of each dot.
net CLK_N       U5.3Y U20.CLK
net GND         U20.CLK_INH U20.SER U20.A U20.B U20.C
net VIDEO_D     U20.QH U32.4A

# Output registers: sync, video with the cursor, the write strobe and the line end, re-timed on the clock.
net VCC         U21.CLR_N
net SYNC_N      U21.1Q J2.SYNC_N
net VIDEO_M     U32.4Y U21.2D
net VIDEO       U21.2Q J2.VIDEO
net WE_D        U25.Y U21.3D
net WEK_N       U21.3Q U22.1OE_N U22.2OE_N U13.4A

# Keyboard: bits 0-5 onto the bus during a write; Return and printable codes decoded.
net D0          J3.D0 U22.1A1 U26.F
net D1          J3.D1 U22.1A2 U5.4A
net D2          J3.D2 U22.1A3 U26.D
net D3          J3.D3 U22.1A4 U26.C
net D4          J3.D4 U22.2A1 U5.5A
net D5          J3.D5 U22.2A2 U12.2B
net D6          J3.D6 U12.2A
net GND         U22.2A3 U22.2A4
net D1_N        U5.4Y U26.E
net D4_N        U5.5Y U26.B
net CTL         U12.2Y U26.A U27.1A U27.1B U27.2A
net VCC         U26.G U26.H
net CR_N        U26.Y U27.2B U27.4B U28.LOAD_N U29.LOAD_N
net PRINT       U27.1Y U25.E
net ADV         U27.2Y U28.ENT

# Write control: KEY set by the strobe, WR for one line from character 32, KEY cleared at the line end.
net STROBE      J3.STROBE U24.1CLK
net VCC         U24.1D U24.1PRE_N U24.2PRE_N U24.2CLR_N
net KEY         U24.1Q U24.2D
net WR          U24.2Q U10.2A U25.A U28.ENP U29.ENP U30.ENP
net VCC         U10.2C U10.2D U25.F U25.G U25.H
net KEY_CLR_N   U10.2Y U24.1CLR_N

# Cursor counter: X0-X4 the column, Y0-Y3 the row; steps at a line end after a write or Return.
net GND         U28.A U28.B U28.C U28.D U29.A U29.B U29.C U29.D U30.A U30.B U30.C U30.D
net VCC         U30.LOAD_N
net X0          U28.QA U14.1B U31.1B
net X1          U28.QB U14.2B U31.2B
net X2          U28.QC U14.3B U31.3B
net X3          U28.QD U14.4B U31.4B
net XRCO        U28.RCO U29.ENT U27.3A
net X4          U29.QA U15.1B U32.1B U27.3B
net XC_N        U27.3Y U27.4A
net YEN         U27.4Y U30.ENT
net Y0          U30.QA U15.2B U32.2B
net Y1          U30.QB U15.3B U32.3B
net Y2          U30.QC U15.4B U11.3B
net Y3          U30.QD U16.1B U11.4B

# Clear: spaces onto the bus, every cell written, the cursor home.
net VCC         R1.1
net CLEAR_N     R1.2 SW1.1 U23.1OE_N U23.2OE_N U13.4B U28.CLR_N U29.CLR_N U30.CLR_N
net GND         SW1.2
net GND         U23.1A1 U23.1A2 U23.1A3 U23.1A4 U23.2A1 U23.2A3 U23.2A4
net VCC         U23.2A2

# Cursor on the screen: beam address = cursor address, one character late, blinking.
net NE0         U31.1Y U33.1A
net NE1         U31.2Y U33.1B
net NE2         U31.3Y U33.1C
net NE3         U31.4Y U33.2A
net NE4         U32.1Y U33.2B
net NE5         U32.2Y U33.2C
net NE6         U32.3Y U33.3A
net NE7         U11.3Y U33.3B
net NE8         U11.4Y U33.3C
net EQ0         U33.1Y U34.1A
net EQ1         U33.2Y U34.1B
net EQ2         U33.3Y U34.1C
net MATCH_N     U34.1Y U35.1D
net VCC         U35.1PRE_N U35.1CLR_N
net CURSOR_N    U35.1Q U12.3A
net FRAME2      U36.QA U36.CKB
net GND         U36.R01 U36.R02
net BLINK       U36.QD U12.3B
net CUR_SHOW    U12.3Y U32.4B

# Spare gates.
net GND         U5.2A U5.6A U12.4A U12.4B U34.2A U34.2B U34.2C U34.2D U35.2D U35.2CLK
net VCC         U35.2PRE_N U35.2CLR_N
```

### Timing notes

- `DOT_END`, `CC`: carries with `ENT` tied high, no glitch (as 080). `C5` alone clocks `WR` and `V8` alone clocks `U36`: single counter outputs, clean.
- `LINE_END_N` decodes `DOT_END` with column bits, and `DOT_END` falls about 5 ns after the column changes at a character's start: a real chip could give a runt pulse there (the simulator's inertial delay swallows it). It only goes to `LOAD_N` inputs, sampled at the next edge, and to `U21`, which samples it the same way: `LINE_END` (`U21` `4Q_N`) is high for exactly one dot, the first dot of character 0, and clocks the line counter and the cursor counter. `FRAME_END_N` likewise only goes to `LOAD_N` inputs.
- `HSYNC_N`, `WE_D` and `MATCH_N` decode column bits that change on the same edge and may glitch just after it: `SYNC_N` and `WEK_N` are re-timed by `U21`, `MATCH_N` is sampled by `U35` on `DOT_END`, five dots after the column changes.
- Within a character (dots 10–15 of `U1`, 0.25 µs each): the column changes at the start of dot 10; `RA0`–`RA8` settle 9–18 ns later; the bus 450 ns after that (into dot 12); `F1`–`F5` 500 ns later (into dot 14) or 450 ns after a row change; `DOT_LOAD_N` falls at the start of dot 15. About 230 ns to spare.
- The write window: `C5` switches the multiplexer to the cursor at the start of character 32. `WEK_N` is low from one dot into character 35 to one dot into 36, and again at 39; the cursor's address has been steady since character 32 and stays so until `LINE_END` rises 20 ns into character 0, when the multiplexer is back on the beam. The RAMs write the bus at the cursor's cell twice, the same code each time. At the end of each write `WE_N` rises 8 ns after `WEK_N` (`U13`) and `U22` lets go of the bus 10 ns after it: the 2114 keeps the data held just before `WE_N` rises, 2 ns to spare in the model (P4n, no hold time).
- The line counter steps about 35 ns into character 0, so `V1`–`V7` (and `ACTIVE` at lines 0 and 256) change as character 0 starts its fetch: the RAM answers about 500 ns into it and the 2513 about 1 µs, 250 ns before the load. Character 0 of every line is fetched with the new line, as on 080.
- `U35` takes `MATCH_N` on the rise of `DOT_END` (start of the last dot of character k) and holds it until the next: `CURSOR_N` covers the dots `U21` samples for the cell addressed in k (the end of dot 15 of k to the end of dot 14 of k + 1).
- `KEY_CLR_N` falls only with `WR` high, at a line end at most 79 µs after the `STROBE` that set `KEY`; the next `STROBE` rises at least 101 µs after it, so no key is lost to the clear.
- Clear held: `WE_N` low throughout, the multiplexer's address sweeps every cell each frame (each cell on 16 lines, rows 0–3 again during lines 256–311), and the bus carries 0x20 from `U23`. A key written while Clear is held fights `U23` on the bus.

### Where the page lands on the TV

As on 080, the TV starts a line on the fall of `SYNC_N`, re-timed one dot after the start of character 40; the first text dot is shown from the start of character 1. The text therefore starts 3 characters less 1 dot after the line start: 17 dots, about TV column 34 (one dot is 2 columns), and is 384 columns wide. Line 0 is about 4 rows below the frame start. The tests read both offsets from the picture.

## Board

Placed by `bw place netlists/typewriter.bwn boards/typewriter/typewriter.place -o boards/typewriter/typewriter.kicad_pcb` (P4k), then routed by the user in KiCad ([KICAD_GUIDE.md](../../p2a-board-layout/KICAD_GUIDE.md) steps 7–8), 2 layers, no zones.

- `typewriter.place`:

  ```
  # 090 TV typewriter: panel parts and the character ROM (docs/machines/090-typewriter/TYPEWRITER_DESIGN.md, Board)
  edge J1 left middle    # power
  edge J2 back right     # TV
  edge J3 front left     # keyboard
  edge SW1 front right   # Clear
  next R1 SW1 left       # CLEAR_N pull-up
  row back left U19      # character ROM: DIP-24, no slot in the chip grid
  ```

- The 2513 is a 600-mil DIP-24, too big for the chip grid (as on 080). The 2114s (DIP-18) and 74LS244s (DIP-20) fit the grid (P4n). The 35 other chips and `X1` go in the annealed grid.
- No routing constraints: there are no Puzzles yet. P4r reads the routing it needs from the finished Board.
- `bw check-board` prints `ok`; `drc_typewriter` and `place_drc_typewriter` pass (picked up by the globs).

## Manuals

- User: What it does (power on `J1`, TV on `J2`, ASCII keyboard on `J3`; random characters at power-on: press Clear, the button at the front right, to blank the page; what is typed appears at the blinking block, 16 lines of 32 characters, upper case; Return starts a new line; after the last line typing goes on at the top, over what is there; other control keys do nothing).
- Service: Components, one bullet per reference with its Nets, netlist order (`X1`, `U1`–`U36`, `SW1`, `R1`, `J1`–`J3`):
  - `X1` dot clock: 4 MHz crystal oscillator, net `CLK`
  - `U1` dot counter: counts 10–15, `DOT_END` on a character's last dot; 6 dots a character
  - `U2` column counter, low bits: `C0`–`C3`, carry `CC`
  - `U3` column counter, high bits: `C4`, `C5`; with `U2`, 42 characters a line, reloaded with 0 by `LINE_END_N`
  - `U4` line decoders: `LINE_END_N` on the last dot of character 41, `HSYNC_N` for characters 40–41
  - `U5` inverters: `DOT_LOAD_N`, `CLK_N`; `D1_N`, `D4_N` for the Return decoder
  - `U6` line counter, bits 0–3: `V0` (each glyph line drawn twice), `V1`–`V3` (glyph line); clocked by `LINE_END`
  - `U7` line counter, bits 4–7: `V4`–`V7`, the text row
  - `U8` line counter, bit 8: `V8`; with `U6` and `U7` reloaded with 0 after line 311
  - `U9` frame-end decoder: `FRAME_END_N` low during line 311
  - `U10` vertical sync decoder `VSYNC_N` for lines 308–311; `KEY_CLR_N`, clearing `KEY` at the end of a writing line
  - `U11` sync mixer: composite sync from `HSYNC_N` and `VSYNC_N` into `SYNC_N_D`; two gates compare `V6`, `V7` with `Y2`, `Y3` (`NE7`, `NE8`)
  - `U12` text area `ACTIVE` (columns 0–31 of lines 0–255); `CTL`, high for control codes; `CUR_SHOW`, the blinking cursor
  - `U13` row gates: `R1`–`R3` = `V1`–`V3` while `ACTIVE`, else 0; `WE_N` = `WEK_N` AND `CLEAR_N`
  - `U14` address multiplexer, bits 0–3: `RA0`–`RA3` from the beam's `C0`–`C3` during the text, from the cursor's `X0`–`X3` while `C5` is high
  - `U15` address multiplexer, bits 4–7: `RA4`–`RA7` from `C4`, `V4`–`V6` or from `X4`, `Y0`–`Y2`
  - `U16` address multiplexer, bit 8: `RA8` from `V7` or from `Y3`
  - `U17` page RAM (2114), bits 0–3 of each character on `B0`–`B3`; 512 cells, row × 32 + column, at `RA0`–`RA8`
  - `U18` page RAM (2114), bits 4–5 of each character on `B4`–`B5`
  - `U19` character ROM (2513): address `R1`–`R3`, `B0`–`B5`; out the line's dots `F5` (left) to `F1`
  - `U20` dot shift register: loads `F5`–`F1` on `DOT_LOAD_N`, shifts them out on `CLK_N` as `VIDEO_D`
  - `U21` output register: re-times `SYNC_N`, `VIDEO`, the write strobe `WEK_N` and `LINE_END` (the first dot of each line) on the clock
  - `U22` keyboard buffer: drives `D0`–`D5` onto the bus while `WEK_N` is low
  - `U23` space buffer: drives 0x20 onto the bus while `CLEAR_N` is low
  - `U24` key flip-flops: `KEY` set by `STROBE`; `WR` takes it at character 32, a line that writes
  - `U25` write decoder: `WE_D` low during characters 35 and 39 of a writing line, for printable codes
  - `U26` Return decoder: `CR_N` low while the last code is 13
  - `U27` key gates: `PRINT`, `ADV` (printable or Return), `XC_N` (column 31), `YEN` (step the row)
  - `U28` cursor column, bits 0–3: `X0`–`X3`, carry `XRCO`; loaded with 0 by `CR_N`
  - `U29` cursor column, bit 4: `X4`
  - `U30` cursor row: `Y0`–`Y3`; all three cursor counters step at a line end while `WR` is high, cleared by `CLEAR_N`
  - `U31` cursor comparator: `NE0`–`NE3` high where `C0`–`C3` and `X0`–`X3` differ
  - `U32` cursor comparator: `NE4`–`NE6` for `C4`, `V4`, `V5` against `X4`, `Y0`, `Y1`; gate 4 XORs `CUR_SHOW` into the video (`VIDEO_M`)
  - `U33` comparator: `EQ0`–`EQ2`, high where three bits agree
  - `U34` cursor match: `MATCH_N` low while the beam addresses the cursor's cell
  - `U35` cursor delay: `CURSOR_N`, `MATCH_N` one character late, clocked by `DOT_END`
  - `U36` blink counter: counts frames on `V8`; `BLINK` changes every 8 frames
  - `SW1` Clear button: pulls `CLEAR_N` low while pressed
  - `R1` 4.7 kΩ pull-up on `CLEAR_N`
  - `J1` power connector: +5 V and 0 V for the whole board
  - `J2` monitor connector: `VIDEO` and `SYNC_N` out
  - `J3` keyboard connector: the key's code on `D0`–`D6`, `STROBE` high for 100 µs after each key; the keyboard is powered from the board

# 080 — Message Display — Machine Design

How the Machine in [MESSAGE.md](MESSAGE.md) is built: netlist, ROM contents, Board, manuals. The code it needs (the 2716 and 2513 ROM Parts with contents files, the 74LS165, the DIP-24 package) comes with P4l ([ROM_PARTS.md](../../p4l-rom-parts/ROM_PARTS.md)).

## Files

Machine name `message`:

```
netlists/message.bwn                     design netlist
roms/message.rom                         the page (2716 contents)
roms/2513.rom                            the font (2513 contents)
boards/message/message.place             panel parts and ROMs (bw place rules)
boards/message/message.kicad_pcb, .kicad_pro   Board (placed by bw place, routed by the user)
manuals/message/user.md, service.md
```

## Netlist

`netlists/message.bwn` (MESSAGE.md, Circuit), with the header comment of [ADD_MACHINE.md](../../recipes/ADD_MACHINE.md) step 2. 17 chips, `X1`, `J1`, `J2`; no resistors, no buttons. Every unused TTL input is tied (spare gates to `GND`, unused load inputs to `GND`, unused NAND inputs to `VCC`), so the netlist loads without warnings. Unused outputs stay unconnected: `U1` `QA`–`QD`, `U3` `QB`–`QD` and `RCO`, `U8` `QB`–`QD` and `RCO`, `U5` gates 4–6, `U10` gate 2, `U11` gates 3–4, `U12` gates 2–4, `U13` gate 4, `U14` `O6` `O7`, `U16` `QH_N`, `U17` `1Q_N` `2Q_N`. The 2513's `VGG`, `VDD` and `NC` pins are no-connect.

```
part X1  OSC freq=2MHz
part U1  74LS161    # dot counter 10-15
part U2  74LS161    # C0-C3
part U3  74LS161    # C4
part U4  74LS20     # LINE_END_N, HSYNC_N
part U5  74LS04     # DOT_LOAD_N, LINE_END, CLK_N
part U6  74LS161    # V0-V3
part U7  74LS161    # V4-V7
part U8  74LS161    # V8
part U9  74LS30     # FRAME_END_N
part U10 74LS20     # VSYNC_N
part U11 74LS86     # composite sync
part U12 74LS02     # ACTIVE
part U13 74LS08     # R1-R3
part U14 2716 contents=../roms/message.rom
part U15 2513 contents=../roms/2513.rom
part U16 74LS165    # dots out
part U17 74LS74     # output registers
part J1  POWER
part J2  VIDEO

net CLK         X1.OUT U1.CLK U2.CLK U3.CLK U6.CLK U7.CLK U8.CLK U17.1CLK U17.2CLK U5.3A

# Dot counter 10..15: 6 dots a character.
net VCC         U1.CLR_N U1.ENP U1.ENT U1.B U1.D
net GND         U1.A U1.C
net DOT_END     U1.RCO U5.1A U2.ENP U3.ENP U4.1A
net DOT_LOAD_N  U5.1Y U1.LOAD_N U16.SH_LD_N

# Column counter 0..20, reloaded with 0 after character 20.
net VCC         U2.CLR_N U2.ENT U3.CLR_N
net GND         U2.A U2.B U2.C U2.D U3.A U3.B U3.C U3.D
net C0          U2.QA U14.A0
net C1          U2.QB U14.A1
net C2          U2.QC U14.A2 U4.1B U4.2A
net C3          U2.QD U14.A3
net CC          U2.RCO U3.ENT
net C4          U3.QA U4.1C U4.2B U12.1A
net VCC         U4.1D U4.2C U4.2D
net LINE_END_N  U4.1Y U2.LOAD_N U3.LOAD_N U5.2A
net LINE_END    U5.2Y U6.ENP U6.ENT U7.ENP U8.ENP U9.G
net HSYNC_N     U4.2Y U11.1A

# Line counter 0..311 (as 060): counts at each line end, loaded with 0 at the frame end.
net VCC         U6.CLR_N U7.CLR_N U8.CLR_N U9.H
net GND         U6.A U6.B U6.C U6.D U7.A U7.B U7.C U7.D U8.A U8.B U8.C U8.D
net V0          U6.QA U9.F
net V1          U6.QB U9.E U13.1A
net V2          U6.QC U9.D U10.1D U13.2A
net V3          U6.QD U13.3A
net VC1         U6.RCO U7.ENT
net V4          U7.QA U9.C U10.1C U14.A4
net V5          U7.QB U9.B U10.1B U14.A5
net V6          U7.QC U14.A6
net V7          U7.QD U14.A7
net VC2         U7.RCO U8.ENT
net V8          U8.QA U9.A U10.1A U12.1B
net FRAME_END_N U9.Y U6.LOAD_N U7.LOAD_N U8.LOAD_N
net VSYNC_N     U10.1Y U11.1B
net GND         U10.2A U10.2B U10.2C U10.2D

# Composite sync: HSYNC XOR VSYNC, then inverted (as 060).
net SYNC        U11.1Y U11.2A
net VCC         U11.2B
net SYNC_N_D    U11.2Y U17.1D
net GND         U11.3A U11.3B U11.4A U11.4B

# Blanking: outside columns 0-15 and lines 0-255 the character ROM is asked for its blank line 0.
net ACTIVE      U12.1Y U13.1B U13.2B U13.3B
net GND         U12.2A U12.2B U12.3A U12.3B U12.4A U12.4B U13.4A U13.4B
net R1          U13.1Y U15.A1
net R2          U13.2Y U15.A2
net R3          U13.3Y U15.A3

# Message ROM: one character code a cell, row x 16 + column.
net GND         U14.A8 U14.A9 U14.A10 U14.CE_N U14.OE_N
net VCC         U14.VPP
net CH0         U14.O0 U15.A4
net CH1         U14.O1 U15.A5
net CH2         U14.O2 U15.A6
net CH3         U14.O3 U15.A7
net CH4         U14.O4 U15.A8
net CH5         U14.O5 U15.A9

# Character ROM: the dots of one glyph line, F5 leftmost.
net GND         U15.CE_N
net F1          U15.O1 U16.D
net F2          U15.O2 U16.E
net F3          U15.O3 U16.F
net F4          U15.O4 U16.G
net F5          U15.O5 U16.H

# Shift register: loads on a character's last dot, shifts in the middle of each dot.
net CLK_N       U5.3Y U16.CLK
net GND         U16.CLK_INH U16.SER U16.A U16.B U16.C
net VIDEO_D     U16.QH U17.2D

# Output registers.
net VCC         U17.1PRE_N U17.1CLR_N U17.2PRE_N U17.2CLR_N
net SYNC_N      U17.1Q J2.SYNC_N
net VIDEO       U17.2Q J2.VIDEO
net GND         U5.4A U5.5A U5.6A
```

### Timing notes

- `DOT_END` is `U1`'s carry with `ENT` tied high: high for the whole of count 15, no glitch (the ADD_MACHINE `RCO` gotcha does not apply). `CC` is `U2`'s carry with `ENT` tied high too.
- `LINE_END_N` and `HSYNC_N` decode counter bits that change on the same clock edge, so they may glitch for a few ns just after it; nothing samples them then (counter `LOAD_N` and `ENP` are read at the next edge, and `SYNC_N` is re-timed by `U17`).
- Within a character (dots 10–15 of `U1`): the column counter changes at the start of dot 10; `CH0`–`CH5` settle 450 ns later (dot 10 end), `F1`–`F5` 500 ns after that (dot 11 end, row address unchanged) or 450 ns after a row change; `DOT_LOAD_N` falls about 30 ns into dot 15, 1.5 µs later.
- `U16`: `CLK_N` rises in the middle of each dot. During dot 15 `SH_LD_N` is low across that rise, so no shift; `QH` follows `H` = `F5` from about 60 ns into dot 15. `U17` samples `QH` at the end of each dot: `F5` (end of dot 15), then after the mid-dot shifts `F4`, `F3`, `F2`, `F1`, and at the end of dot 14 the `C` stage, 0.
- Glyph line change: `V1`–`V3` change at the end of character 20, a blanked character, so the first character of every line is fetched with the new line.

### Where the page lands on the TV

The TV starts a line on the fall of `SYNC_N`. `SYNC_N_D` falls just after the edge that starts character 20, and `U17` takes it at the next edge, one dot later; `VIDEO` is re-timed the same way, but the first text dot is the one sampled at the end of character 0, so it shows from the start of character 1. The text therefore starts 2 characters less 1 dot after the line start: 11 dots, about TV column 44 (one dot is 4 columns), and is 384 columns wide (as on 060, where the left wall shows at column 96 for the same reason). The TV starts a frame during the vertical sync (lines 308–311); line 0 is about 4 rows later. The test reads both offsets from the picture rather than assuming them.

## ROM contents

Both files use the contents format of [ROM_PARTS.md](../../p4l-rom-parts/ROM_PARTS.md) (`;` comments, `@` addresses, hex words, `"strings"`, `#`/`.` dot rows).

- `roms/message.rom`: 256 bytes at 0x00–0xFF, one string of 16 characters a row (rows padded with spaces; rows 0, 1, 14 and 15 all spaces, written `20*16`), the page of MESSAGE.md. Only upper case, digits, space and the 2513's punctuation.
- `roms/2513.rom` (already drafted): 512 words of 5 bits, in character order 0x00–0x3F (`@ A … Z [ \ ] ^ _`, then space `! " # $ % & ' ( ) * + , - . /`, `0`–`9`, `: ; < = > ?`), 8 dot rows each: line 0 `.....`, lines 1–7 the glyph, `#` lit, leftmost dot first (bit 4 = `O5`). Our own 5 × 7 drawings in the classic style of the 2513's ASCII font (CM2140); a comment names each character.

## Board

Placed by `bw place netlists/message.bwn boards/message/message.place -o boards/message/message.kicad_pcb` (P4k), then routed by the user in KiCad ([KICAD_GUIDE.md](../../p2a-board-layout/KICAD_GUIDE.md) steps 7–8), 2 layers, no zones.

- `message.place`:

  ```
  # 080 Message display: panel parts and ROMs (docs/machines/080-message/MESSAGE_DESIGN.md, Board)
  edge J1 left middle      # power
  edge J2 back right       # TV
  row front middle U14 U15 # the ROMs: DIP-24, no slot in the chip grid
  ```

- The ROMs are 600-mil DIP-24s, too big for the chip grid's 16 × 27 mm slots, so a rule places them side by side on the front edge (ROM_PARTS.md, decided while designing). The 15 other chips and `X1` go in the annealed grid.
- No Puzzle constraints yet: P4m reads the routing of whatever Nets its cracks need.
- `bw check-board` prints `ok`; `drc_message` and `place_drc_message` pass (picked up by the globs).

## Manuals

- User: What it does (power on `J1`, TV on `J2`; the poem, 16 lines of 16 characters, white on black, still; locks within half a second, the same page every time).
- Service: Components, one bullet per reference with its Nets, netlist order (`X1`, `U1`–`U17`, `J1`, `J2`):
  - `X1` dot clock: 2 MHz crystal oscillator, net `CLK`
  - `U1` dot counter: counts 10–15, `DOT_END` on a character's last dot; 6 dots a character
  - `U2` column counter, low bits: `C0`–`C3`, carry `CC`
  - `U3` column counter, high bit: `C4`; with `U2`, 21 characters a line, reloaded with 0 by `LINE_END_N`
  - `U4` line decoders: `LINE_END_N` on the last dot of character 20, `HSYNC_N` for character 20
  - `U5` inverters: `DOT_LOAD_N`, `LINE_END`, `CLK_N`
  - `U6` line counter, bits 0–3: `V0` (each glyph line drawn twice), `V1`–`V3` (glyph line); counts at each line end
  - `U7` line counter, bits 4–7: `V4`–`V7`, the text row
  - `U8` line counter, bit 8: `V8`; with `U6` and `U7` reloaded with 0 after line 311
  - `U9` frame-end decoder: `FRAME_END_N` low at the end of line 311
  - `U10` vertical sync decoder: `VSYNC_N` for lines 308–311
  - `U11` sync mixer: composite sync from `HSYNC_N` and `VSYNC_N`, inverted into `SYNC_N_D`
  - `U12` text area: `ACTIVE` high on columns 0–15 of lines 0–255
  - `U13` row gates: `R1`–`R3` = `V1`–`V3` while `ACTIVE`, else 0 (the blank line of every character)
  - `U14` message ROM (2716): the page, one character code a cell; address `C0`–`C3`, `V4`–`V7`; out `CH0`–`CH5`
  - `U15` character ROM (2513): address `R1`–`R3`, `CH0`–`CH5`; out the line's dots `F5` (left) to `F1`
  - `U16` dot shift register: loads `F5`–`F1` on `DOT_LOAD_N`, shifts them out on `CLK_N` as `VIDEO_D`
  - `U17` output register: re-times `SYNC_N` and `VIDEO` on the clock
  - `J1` power connector: +5 V and 0 V for the whole board
  - `J2` monitor connector: `VIDEO` and `SYNC_N` out

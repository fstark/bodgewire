# 080 — Message Display — Machine Design

How the Machine in [MESSAGE.md](MESSAGE.md) is built: netlist, ROM contents, Board, Puzzle files, manuals. The code it needs (the 2716 and 2513 ROM Parts with contents files, the 74LS165, the DIP-24 package) comes with P4l ([ROM_PARTS.md](../../../docs/p4l-rom-parts/ROM_PARTS.md)).

## Files

Machine name `message`:

```
message.bwn                          design netlist
roms/message.rom                     the page (2716 contents)
roms/2513.rom                        the font (2513 contents)
board/message.place                  panel parts and ROMs (bw place rules)
board/message.kicad_pcb, .kicad_pro  Board (placed by bw place, routed by the user)
manuals/user.md, service.md
puzzles/NN-*.bwp                     7 Puzzles, easiest first
```

## Netlist

`machines/080-message/message.bwn` (MESSAGE.md, Circuit), with the header comment of [ADD_MACHINE.md](../../../docs/recipes/ADD_MACHINE.md) step 2. 17 chips, `X1`, `J1`, `J2`; no resistors, no buttons. Every unused TTL input is tied (spare gates to `GND`, unused load inputs to `GND`, unused NAND inputs to `VCC`), so the netlist loads without warnings. Unused outputs stay unconnected: `U1` `QA`–`QD`, `U3` `QB`–`QD` and `RCO`, `U8` `QB`–`QD` and `RCO`, `U5` gates 5–6, `U10` gate 2, `U11` gates 3–4, `U12` gates 2–4, `U13` gate 4, `U14` `O6` `O7`, `U16` `QH_N`, `U17` `1Q_N` `2Q_N`. The 2513's `VGG`, `VDD` and `NC` pins are no-connect.

```
part X1  OSC freq=2MHz
part U1  74LS161    # dot counter 10-15
part U2  74LS161    # C0-C3
part U3  74LS161    # C4
part U4  74LS20     # LINE_END_N, HSYNC_N
part U5  74LS04     # DOT_LOAD_N, LINE_END, CLK_N, V2_N
part U6  74LS161    # V0-V3
part U7  74LS161    # V4-V7
part U8  74LS161    # V8
part U9  74LS30     # FRAME_END_N
part U10 74LS20     # VSYNC_N
part U11 74LS86     # composite sync
part U12 74LS02     # ACTIVE
part U13 74LS08     # R1-R3
part U14 2716 contents=roms/message.rom
part U15 2513 contents=roms/2513.rom
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
net C1          U2.QB U14.A1 U4.2A
net C2          U2.QC U14.A2 U4.1B
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
net V2          U6.QC U9.D U13.2A U5.4A
net V3          U6.QD U13.3A U10.1B
net VC1         U6.RCO U7.ENT
net V4          U7.QA U9.C U10.1C U14.A4
net V5          U7.QB U9.B U14.A5
net V6          U7.QC U14.A6
net V7          U7.QD U14.A7
net VC2         U7.RCO U8.ENT
net V8          U8.QA U9.A U10.1A U12.1B
net FRAME_END_N U9.Y U6.LOAD_N U7.LOAD_N U8.LOAD_N
net V2_N        U5.4Y U10.1D
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
net GND         U5.5A U5.6A
```

### Timing notes

- `DOT_END` is `U1`'s carry with `ENT` tied high: high for the whole of count 15, no glitch (the ADD_MACHINE `RCO` gotcha does not apply). `CC` is `U2`'s carry with `ENT` tied high too.
- `LINE_END_N` and `HSYNC_N` decode counter bits that change on the same clock edge, so they may glitch for a few ns just after it; nothing samples them then (counter `LOAD_N` and `ENP` are read at the next edge, and `SYNC_N` is re-timed by `U17`).
- Within a character (dots 10–15 of `U1`): the column counter changes at the start of dot 10; `CH0`–`CH5` settle 450 ns later (dot 10 end), `F1`–`F5` 500 ns after that (dot 11 end, row address unchanged) or 450 ns after a row change; `DOT_LOAD_N` falls about 30 ns into dot 15, 1.5 µs later.
- `U16`: `CLK_N` rises in the middle of each dot. During dot 15 `SH_LD_N` is low across that rise, so no shift; `QH` follows `H` = `F5` from about 60 ns into dot 15. `U17` samples `QH` at the end of each dot: `F5` (end of dot 15), then after the mid-dot shifts `F4`, `F3`, `F2`, `F1`, and at the end of dot 14 the `C` stage, 0.
- Glyph line change: `V1`–`V3` change at the end of character 20, a blanked character, so the first character of every line is fetched with the new line.

### Where the page lands on the TV

The TV starts a line on the fall of `SYNC_N`. `SYNC_N_D` falls just after the edge that starts character 18, and `U17` takes it at the next edge, one dot later; `VIDEO` is re-timed the same way, but the first text dot is the one sampled at the end of character 0, so it shows from the start of character 1. The text therefore starts 4 characters less 1 dot after the line start: 23 dots, 11.5 µs, TV column 92 (one dot is 4 columns), and is 384 columns wide, to column 476 (P6n; measured with `bw run --tv`: lit columns 92–447, the widest line of the poem). The TV starts a frame 16 µs into the vertical sync (lines 280–283), so line `l` is on row `l` − 280 and line 0 on row 32 (lit rows 64–255: text rows 2–13). A monitor's window (columns 80–495, rows from 28) cuts nothing. The test reads both offsets from the picture rather than assuming them.

## ROM contents

Both files use the contents format of [ROM_PARTS.md](../../../docs/p4l-rom-parts/ROM_PARTS.md) (`;` comments, `@` addresses, hex words, `"strings"`, `#`/`.` dot rows).

- `machines/080-message/roms/message.rom`: 256 bytes at 0x00–0xFF, one string of 16 characters a row (rows padded with spaces; rows 0, 1, 14 and 15 all spaces, written `20*16`), the page of MESSAGE.md. Only upper case, digits, space and the 2513's punctuation.
- `machines/080-message/roms/2513.rom` (already drafted): 512 words of 5 bits, in character order 0x00–0x3F (`@ A … Z [ \ ] ^ _`, then space `! " # $ % & ' ( ) * + , - . /`, `0`–`9`, `: ; < = > ?`), 8 dot rows each: line 0 `.....`, lines 1–7 the glyph, `#` lit, leftmost dot first (bit 4 = `O5`). Our own 5 × 7 drawings in the classic style of the 2513's ASCII font (CM2140); a comment names each character.

## Board

Placed by `bw place machines/080-message/message.bwn machines/080-message/board/message.place -o machines/080-message/board/message.kicad_pcb` (P4k), then routed by the user in KiCad ([KICAD_GUIDE.md](../../../docs/KICAD_GUIDE.md) steps 7–8), 2 layers, no zones.

- `message.place`:

  ```
  # 080 Message display: panel parts and ROMs (machines/080-message/docs/MESSAGE_DESIGN.md, Board)
  edge J1 left middle      # power
  edge J2 back right       # TV
  row front middle U14 U15 # the ROMs: DIP-24, no slot in the chip grid
  ```

- The ROMs are 600-mil DIP-24s, too big for the chip grid's 16 × 27 mm slots, so a rule places them side by side on the front edge (ROM_PARTS.md, decided while designing). The 15 other chips and `X1` go in the annealed grid.
- Puzzle constraints: none on the routing. Both cracks are on two-pin Nets (`CH5`, `CLK_N`), so any layout keeps their pads at the Trace's ends; Thin letters needs only `U16` pins 13 and 14 as wired (`C` tied to GND, `D` on `F1`).
- `bw check-board` prints `ok`; `drc_message` and `place_drc_message` pass (picked up by the globs).

## Puzzles

`machine message` in each file (MESSAGE.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `half_letters.bwp` | `fault stuck U13.3Y 0` | `R3` into the character ROM |
| `echo.bwp` | `fault stuck U12.1B 0` | `V8` into the text-area gate |
| `gibberish.bwp` | `fault crack U14.O5 U15.A9 50%` | `CH5` |
| `stammer.bwp` | `fault joint U14.8` | `C0` at the message ROM |
| `thin_letters.bwp` | `fault bridge U16.13 U16.14` | `F1` to the GND tie on `C` |
| `block_letters.bwp` | `fault crack U5.3Y U16.CLK 50%` | `CLK_N` |
| `slanted_page.bwp` | `fault clock X1 1.9MHz` | `X1`, a Wrong Clock (P8k): the can still marked `2.000MHz` |

Titles and briefs:

- Half letters: "Only the top of each letter shows, and it shows twice, one copy above the other."
- Echo: "A row of broken dots runs along the very top of the screen, above the poem."
- Gibberish: "The poem has turned into digits and punctuation marks, though the words keep their lengths and places."
- Stammer: "Each letter is printed twice over and half of them are missing: the poem reads like a stammer."
- Thin letters: "Every letter has lost its right-hand edge: an O looks like a C, and an H like a squashed F."
- Block letters: "The letters have melted into solid bars and blocks; the poem can no longer be read."
- Slanted page: "The TV cannot hold the page: the words slide across the screen in slanting streaks."

### Electrical Nets on a damaged Board

- Half letters: `R3` stuck at 0 (Strong): the 2513 gets glyph lines 0–3 only, so a text row's 16 lines draw lines 0–3 twice (each line still twice); line 0 is blank, so each row shows the glyph's lines 1–3, a gap, and lines 1–3 again. The borders stay dark (line 0 there as before).
- Echo: `U12` reads `1B` as 0, so `ACTIVE` = NOT `C4` on every line. Lines 256–311 draw text rows 0–3 again (`V4`–`V7` wrap with `V8` high): rows 0 and 1 blank (lines 256–287, before and through the vertical sync), row 2 (the first line of the poem) on lines 288–303, the top half of row 3 (the second line) on lines 304–311. The frame starts at line 280, so these land above the page on rows 8–31: the first line on rows 8–23, above a monitor's window, and row 3's glyph lines 2–3 (lines 308–311) on rows 28–31, a strip of broken dots along the screen's top edge. The `V8` Net is unchanged (the leg reads it).
- Gibberish: `U15` pin 22 floats (reads high): every code gets 0x20, so letters 0x01–0x1A read as `!` to `:`; space and the punctuation and digits (0x20–0x3F) are unchanged.
- Stammer: `U14` pin 8 floats (reads high): the message ROM reads column 2k + 1 for columns 2k and 2k + 1. The blanking is unchanged (it does not go through `C0`).
- Thin letters: `F1` and the `C` tie are one Net with GND (Strong) winning against `U15`'s output: `F1` is always 0, `U16` loads 0 into stage `D`. A new `U15` drives the same Contention.
- Block letters: `U16` `CLK` floats (reads high, never rises): no shift ever. The asynchronous load on each character's last dot puts `F5` on `QH`, where it stays: `U17` samples `F5` on all six dots of the next character.

The test (`tests/machines/message_test.cpp`) builds each faulty page from the two ROM files with the same change and finds it on the TV where the real page is, with nothing outside it (Echo: the real page, and lines 288–311 of the faulty frame on rows 8–31 above it, nothing under it); after the repair the real page is back.

## Manuals

Written to STYLE's Manuals rules (P6k):

- User: What it does (a welcome: the poem, 16 lines of 16 characters, white on black, still; no computer, two ROMs and counters); no Using it (nothing to operate); How it works (the goal, reading a page with a finger and a font book, the clock, the dot, column and line counters with `V0` drawing each line twice, the sync, the message ROM, the character ROM, the shift register on `CLK_N`, the output register, working one character ahead, the border through `R1`–`R3` and line 0, no reset, the unused pins in a table).
- Service: Components (every reference, netlist order, `U13`'s formula put in words); Test points (probe readings on a working board, checked with `bw run --probe` at 0.5 s, 1 s, 1.5 s and 1.517 s; four logic analyzer setups, the dots, the line, the lookups and the picture, each checked with `bw_ui --clip … --group … --sample … --view … --screenshot` after 1 s, 1.5 s for the picture, the PNG viewed); Troubleshooting (no picture or no lock, a locked but dark page, wrong text: ordered checks, never naming a fault).

# 140 — BW-80 — Machine Design

How the Machine in [BW80.md](BW80.md) is built: netlist, Board, manuals, tests. The new code it needs (`MEMBRANEKB`, `EXPANSION`, the window's ZX80 look, `bw place`'s `at`) is P5x's: [BW80_MACHINE_DESIGN.md](../../../docs/p5x-bw80/BW80_MACHINE_DESIGN.md). The display chain and the netlist it grows from are P5v's: [BW80_VIDEO_DESIGN.md](../../../docs/p5v-bw80-video/BW80_VIDEO_DESIGN.md) §5. Recipe: [ADD_MACHINE.md](../../../docs/recipes/ADD_MACHINE.md).

## Files

Machine name `bw80`:

```
bw80.bwn                              the ZX80 (P5v), with J3 and J4
roms/zx80_ic2.bin, zx80_ic2.md        the ZX80's ROM (P5v)
roms/bw80_test.asm, .rom              P5v's test ROM (every character), run through --roms
board/bw80.place                      every chip at its ZX80 position, the connectors, the passives beside their chips
board/bw80.kicad_pcb, .kicad_pro      Board (placed by bw place, routed by the user)
manuals/user.md, service.md
tests/machines/bw80_test.cpp          P5v's picture cases, then typing; built from the Board once there is one
tests/machines/bw80_screen.{hpp,cpp}  reads the display file out of the two 2114s as text
puzzles/NN-*.bwp                      15 Puzzles, easiest first
```

## Memory map

| Select | Addresses | Chip |
|---|---|---|
| `ROM_SEL_N` (`A14` low, `MREQ`), `CS2` = `A12` low | $0000–$0FFF, mirrored at $2000, $8000, $A000 | IC2 |
| `RAM_SEL_N` (`A14` high, `MREQ`) through `R19` | $4000–$43FF, mirrored every 1K to $7FFF and from $C000 to $FFFF | IC3 (bits 7, 1, 0, 2), IC4 (bits 4, 3, 5, 6) |
| `KBD_N` (`IORD_N`, `A0` low) | any even `IN` port | IC10: `KB0`–`KB4` on `D0`–`D4`, `EAR` on `D7`; `D5`, `D6` float (`D6` high through `R7`/`R28`) |
| `IOWR_N` | any `OUT` | sets the vertical sync latch |

The display file's echo: `A15` high over the RAM (`$C000` +) is where the ROM runs it; the same bytes. System variables at $4000 (LISTING): `D_FILE` at $400C points at the display file (a `$76`, then 24 rows of up to 32 codes, each ended by `$76`).

## Netlist

`machines/140-bw80/bw80.bwn` as P5v left it (every chip of Searle's issue 2 schematic at its designator, [BW80_VIDEO_DESIGN.md](../../../docs/p5v-bw80-video/BW80_VIDEO_DESIGN.md) §5), with:

| Change | Why |
|---|---|
| header: P5x, [BW80.md](BW80.md); "keys and edge connector" in; the cassette and modulator left out | the Machine |
| `part J3 MEMBRANEKB` after `J2`; `A8`–`A15` lines gain `J3.A8` … `J3.A15`; `KB0`–`KB4` gain `J3.KB0` … `J3.KB4` | the keyboard (P5x §2) |
| `part J4 EXPANSION`; `MD0`–`MD7` gain `J4.D0` … `J4.D7`; `RAM_CS_N` gains `J4.RAMCS_N`; `A0`–`A15` gain `J4.A0` … `J4.A15`; `INT_N`, `NMI_N`, `HALT_N`, `MREQ_N`, `IORQ_N`, `RD_N`, `WR_N`, `WAIT_N`, `BUSRQ_N`, `RESET_N`, `M1_N`, `RFSH_N` gain their `J4` pin; `PHI_N` gains `J4.PHI`; `net BUSAK_N IC1.BUSAK_N J4.BUSAK_N` (new: the Z80's pin was unconnected); `VCC` gains `J4.VCC5`, `GND` gains `J4.GND1 J4.GND2` | the edge connector (P5x §3); `A13` was a one-pin Net until now |
| `INT_N` on `A6` | as P5v: `A6`'s line gains `J4.INT_N` beside `IC1.INT_N` (the ZX80's INT is the `A6` line) |

Nothing else moves: the keyboard's pull-ups, IC10 and `KBD_N` are P5v's. The netlist stays loadable without warnings (a case checks it).

### Connectors

| Ref | Part | Pins (pad: name, Net) |
|---|---|---|
| `J1` | `POWER` | |
| `J2` | `VIDEO` | `VIDEO`, `SYNC_N` (the Net `SYNC`), `GND` |
| `J3` | `MEMBRANEKB` | 1–8 `A8`–`A15`; 9–13 `KB0`–`KB4` |
| `J4` | `EXPANSION` | upper n at pad 2n − 1, lower n at pad 2n ([P5x §3](../../../docs/p5x-bw80/BW80_MACHINE_DESIGN.md#3-expansion--the-zx80s-edge-connector)): `D0`–`D7` = `MD0`–`MD7`, `RAMCS_N` = `RAM_CS_N`, `PHI` = `PHI_N`, the address and control lines by name; `SLOTA`, `SLOTB`, `NC9V`, `NC23` unconnected |

## Board

`machines/140-bw80/board/bw80.place`: the ZX80 issue 2 board's component area, from Searle's foil layout (ZX80: the silk drawing and the hole table, mm from the board's bottom left, the keyboard at the bottom). The ZX80 board is 155.5 mm wide and 204 mm deep, the keyboard its front 62 mm; the Board keeps the rest: **155.5 × 142 mm** (`size 155.5 142`), the back edge (the edge connector's) at the top of the view. Every chip stands upright with its notch at the back, as on the ZX80, which is `bw place`'s 0°. Positions are the DIP centres computed from the hole table (the two rows of holes 2.54 mm apart, 7.62 or 15.24 mm between rows), which `at` takes as the courtyard's centre; y measured from the back: y = 204 − Searle's y.

```
# BW-80 (Machine 140): the ZX80 issue 2 board's component area, after Grant Searle's foil
# layout (machines/140-bw80/docs/BW80_DESIGN.md). Every chip stands with its notch at the back.
size 155.5 142
at IC1  64.0  66.2         # Z80, the middle
at IC2  134.9 57.2         # ROM, right
at IC3  14.4  48.3         # RAM, top left
at IC4  28.5  48.3
at IC5  90.6  80.1         # data latch
at IC6  109.6 52.1         # ROM address multiplexers
at IC7  109.7 80.1
at IC8  90.5  52.1
at IC9  28.5  76.3         # dots
at IC10 14.4  76.3         # keyboard buffer
at IC11 89.3  111.4        # the gates, the front row left to right: IC14 IC15 IC16 IC13 IC17 IC11 IC12 IC18 IC20
at IC12 105.8 111.4
at IC13 58.9  111.4
at IC14 17.0  111.4
at IC15 29.6  111.4
at IC16 46.2  111.4
at IC17 72.9  111.4
at IC18 118.5 111.4
at IC19 142.5 84.0         # HSYNC
at IC20 131.3 111.4
at IC21 128.5 84.0         # line counter
```

Then the panel parts: `edge J4 back left` (the expansion bus, lying along the back edge at the left, as the ZX80's slot), `edge J2 back 62%` (video, where the modulator sits), `edge J1 back 88%` (power, the 9 V socket's corner), `edge J3 front 30%` (the keyboard's tracks left the component area under IC14–IC16). The passives beside the chips they serve, as the drawing, each `next` chain short enough to stay clear of the next chip (an axial courtyard is about 10 × 3 mm, `next` never rotates): `R4`–`R12` in two columns in the gap between IC9 and IC1 (`next R4 IC9 right`, `next R5 R4 front` … `R8`; `next R9 R4 right`, then `R10`–`R12` chained front); `R1`–`R3` in front of IC10 (`next R1 IC10 front`, chained front); `R13`–`R17` in front of IC14 and IC15 (`next R13 IC14 front`, chained right); `R18`, `R19` in front of IC16; `R20` in front of IC15 beside `R15`; `R21`–`R23` in front of IC1; `R24`, `R25` and `C11` in front of IC17 and IC11; `X1` right of IC20; `R28` right of IC2; `C9` behind IC15 (`next C9 IC15 back`, the drawing's place); `RST1` in front of IC1 beside `R21`. A part the rules miss goes free, nearest its Nets (`placeFree`); a part the rules put off the Board or over another is refused, and the rule moves.

The user routes it in KiCad (ADD_MACHINE §3), two layers, DRC 0; `bw check-board` ok. Then the per-Machine loops get `"bw80"` and the Machine cases build from the Board.

## Manuals

Written to STYLE's Manuals rules (P6k):

- User: What it does (a welcome: a ZX80 chip for chip; the processor, ROM, memory, keyboard and TV in a line each; no video chip; the power-on picture, blank for about 0.2 s then the K cursor); Using it (the keyboard window and the PC keys, one key one keyword with the K and L cursors, `PRINT 2+2` and its report 0/-2, a three-line program and its report 0/20, LIST and NEW, the screen blanking while busy, no cassette, `J4`); How it works (the goal, the clock, the reset, the processor and its refresh, the memory map in a table, the two data buses, the display file, the processor drawing the picture as a finger under the words: the NOP forced on `D` while `IC5` keeps the code, the refresh addressing the character shapes with the code and `IC21`, `IC9` shifting the dots; the shapes as a book of stencils; inverse; the line sync from HALT, `A6` as an egg timer and `IC18`/`IC19`; the frame sync from the keyboard read; why the screen blanks; the keyboard as a roll call with its rows in a table; the unused features in a table).
- Service: Components (every reference, netlist order; `J2`'s `VIDEO` corrected to high for white); Test points (probe readings checked with `bw run --probe` at 0.5 ms, 100 ms, 250 ms, 400 ms, 600 ms and 1 s, the columns with `--type J3:300ms:O` read at 330 and 345 ms; five logic analyzer setups, each checked with `bw_ui --clip … --screenshot --after 3s` (about 0.75 s simulated): one line and its sync (`IC1.HALT_N` falling, 100 ns, 20 µs a division), a character drawn and the character shapes (`IC1.M1_N` rising with `IC15.1Y` high, 10 ns, 500 and 200 ns a division: the cursor's $B0, the ROM at F80), the frame sync (`IC17.4Y` falling, 100 ns, 100 µs a division) and the keyboard rows (O held from 150 ms, 20 µs a division); the timings read from `bw run --vcd` (sync 6.14 µs every 63.7 µs, the frame sync 386 µs every 19.7 ms, nine keyboard reads 25 µs apart, the picture starting at 172 ms)); Troubleshooting (the TV never locks, the picture rolls or tears, wrong characters, a key does nothing: ordered checks, never naming a fault). The analyzer arms at power-on, so each setup triggers on something that only happens once the picture runs.

## Tests

`tests/machines/bw80_test.cpp`, tag `[machines][slow]`; P5v's helpers kept (the netlist text, `Build`, `LockedFrames`, the page helpers). Until the Board exists the cases build the netlist as today; after it, `BoardCircuit` as `ball_test.cpp`, the "loads without warnings" case on the netlist.

The screen reader `Bw80Screen` (`tests/machines/bw80_screen.{hpp,cpp}`): reads `D_FILE` from the two 2114s (`Ram2114::word`: IC3 holds bits 7, 1, 0, 2 as IO1–IO4, IC4 bits 4, 3, 5, 6) and returns the 24 rows as text, the ZX80's codes mapped to ASCII (LISTING's character set: $00 space, $01 `"`, $0C £, $0D `$`, $0E `:`, $0F `?`, $10–$1B `( ) - + * / = > < ; , .`, $1C–$25 digits, $26–$3F letters; bit 7 inverse, shown lower case; tokens $D5 (THEN) and up as their keyword in angle brackets, from `TKN-TABLE`), and the TV page expected from it (`ExpectedPage` over the ROM's glyphs) to compare with a locked frame's picture area, as P5v's `K` case does.

Typing goes through `J3` (`KeyMatrixOf(loaded, "J3")->type(text)`, then a wake) and runs `presses × 180 ms + 300 ms` (a press is held 60 ms and released 120 ms; the ROM's processing and a frame).

- **"bw80: netlist loads without warnings"** (P5v).
- **"bw80: the test ROM shows every character"**, **"a 450 ns RAM misses the NOP"**, **"the ZX80 ROM shows its K cursor"** (P5v, release-only).
- **"bw80: PRINT 2+2 typed through J3"** (release-only), seed 1: after the `K` cursor, `PRINT 2+2\n` typed; the display file's row 0 reads `4` and row 23 `0/-2` (the ROM's report: code 0, the edit line's number -2); a locked frame's picture area matches the page expected from the display file.
- **"bw80: a program is entered and run"** (release-only), seed 1: `10 LET A=6\n20 PRINT A*7\nRUN\n` (the `*` through SHIFT+P, the keywords through the tokenizer); row 0 `42`, row 23 `0/20`.
- **Board** (after the routing): `check-board` ok; the cases above built from the Board; `session: bw80 with every instrument` (`tests/session/session_test.cpp`: the TV locked on the `K` screen by 400 ms, `matrixKeyboards() == {"J3"}`, `matrixKind("J3") == Bw80`, 32 Clips on IC1's buses, 4 Hooks, the probe on `NOP_EN`, the meter).

Debug: P5v's picture cases run about 75–135 s each in debug and are release-only; the typing cases simulate about 1.5 s (`PRINT 2+2`: 5 presses) to 3 s (the program: 14 presses) and are release-only too (about 6–12 s each in release at 0.25×). Each case's simulated time and debug or release wall time go in the plan's notes.

## Puzzles

`machine bw80` in each file (BW80.md, Puzzles); tests in `tests/machines/bw80_puzzles_test.cpp` (release-only but the title and build cases), helpers in `tests/machines/bw80_rig.{hpp,cpp}`. Each Fault was tried with `bw run --tv` on seeds 1–3 before its brief was written (P5y plan notes).

| File | Fault line | Symptom (seeds 1–3 the same) | Repair |
|---|---|---|---|
| `q_everywhere.bwp` | `fault stuck IC16.3B 1` | after a row's text its `HALT`'s glyph (`Q`, `$36`), then the glyph of the byte after the `HALT` to the row's end (the next row's first code, or what follows the display file), the margins full | swap `IC16` |
| `doubled_lines.bwp` | `fault stuck IC21.QB 0` | glyph rows `0 0 2 2 4 4 6 6` | swap `IC21` |
| `bottom_halves.bwp` | `fault crack IC21.QD IC8.4A 50%` | glyph rows `4 5 6 7 4 5 6 7` | mend |
| `wrong_letters.bwp` | `fault bridge IC5.15 IC5.16` | the glyph of the code with bits 1 and 2 cleared unless both set: `;< LRILT "DCLLK <;23"` | cut the bridge |
| `quotes_for_spaces.bwp` | `fault joint IC5.6` | the glyph of the code with bit 0 set: `""11"PRJNT""HFLLP"1133"`, the cursor an inverse `L` | resolder `IC5` pin 6 |
| `plain_cursor.bwp` | `fault stuck IC12.2Y 0` | the cursor's `$B0` drawn as a plain `K` | swap `IC12` |
| `blank_page.bwp` | `fault stuck IC9.CLK 1` | no dots; the cursor's cell a black square | swap `IC9` |
| `eight_keys.bwp` | `fault crack IC10.A5 R15.1 50%` | `PRINT 12345` gives `1245`; `KB2` high at `J3` | mend |
| `same_eight_keys.bwp` | `fault stuck IC10.A5 1` | as `eight_keys`; no crack to find | swap `IC10` |
| `no_print.bwp` | `fault crack J3.A13 IC1.A13 50%` | `PRINT 2+2` leaves `2+s2l` in the edit line (the O key dead) | mend |
| `rolling_screen.bwp` | `fault crack IC17.4Y IC11.4B 90%` | the display file right, the TV never locked; `IC10` keeps `KBD_N` | mend |
| `torn_picture.bwp` | `fault stuck IC18.1Q_N 1` | the display file right, the TV never locked | swap `IC18` |
| `still_starting.bwp` | `fault column IC3.IO4 0` | unlocked and blank at 700 ms (under 2000 ink points: the unblanked sync only); `IC3`'s bit 3 clear in every word | swap `IC3` |
| `vertical_stripes.bwp` | `fault crack IC6.2Y IC2.CS1 50%` | unlocked; `ROM_CS1` moves at `IC6.2Y`, high and still at `IC2.CS1` | mend |
| `white_screen.bwp` | `fault stuck IC14.3Y 0` | unlocked, under 1500 ink points (the frame's edges); `D3` low and still at `IC1` while `CLK` runs | swap `IC14` |

Crack positions, from the Board's segments (a pad touched by one segment is an end of its Trace): `LC2` and `ROM_CS1` are two-pad Nets; `KB2` runs `IC10.12` – `R15.1` – `J3.11` with `R15.1` the junction, so a crack between `IC10.A5` and `R15.1` cuts `IC10` alone; `A13` runs `J3.6` – `IC1.3` – `J4.26` with `IC1.3` the junction; `KBD_N` has `IC17.11`, `IC10.1` and `IC11.13` at ends and a junction via near `IC17` and `IC11` with a branch to `IC10`, so 90 % between `IC17.4Y` and `IC11.4B` lies past the via and cuts `IC11.13` alone (10 % would cut `IC17` off from all).

A line typed after a report loses its first key (the ROM takes it to leave the report): the cases type one command from a fresh `K` screen.

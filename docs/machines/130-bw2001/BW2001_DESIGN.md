# 130 — BW-2001 — Machine Design

How the Machine in [BW2001.md](BW2001.md) is built: netlist, Board, manuals, tests. The new code it needs (the `MONITOR`, `MATRIXKB`, `IEEE488`, `USERPORT`, `CASSETTE` connectors, the `74LS139` and `MC3446`, the port drive strength, the TV's separate drives, the keyboard window, the PET grid in `bw place`, the editor's retrace wait) is P5q's: [BW2001_MACHINE_DESIGN.md](../../p5q-bw2001/BW2001_MACHINE_DESIGN.md). Recipe: [ADD_MACHINE.md](../../recipes/ADD_MACHINE.md).

## Files

Machine name `bw2001`:

```
netlists/bw2001.bwn                       design netlist (grown in three stages)
roms/bw2001_*.asm, .rom                   P5p's firmware (the editor's retrace wait, P5q)
roms/901447-08.bin                        the character ROM (P5n)
boards/bw2001/bw2001.place                connectors, the PET grid, resistors beside their chips
boards/bw2001/bw2001.kicad_pcb, .kicad_pro  Board (placed by bw place, routed by the user)
manuals/bw2001/user.md, service.md
tests/machines/bw2001_test.cpp            fault-free cases on the netlist, then the Board
tests/machines/bw2001_screen.{hpp,cpp}    reads the 40 x 25 text off a TV frame
```

## Memory map

As P5p's ([BW2001_FIRMWARE.md](../../p5p-bw2001-firmware/BW2001_FIRMWARE.md#the-pet-it-runs-on)), now with the PET's own decoding: `$0000`–`$1FFF` RAM (`UI9` 74LS139 from `SEL0_N`, `SEL1_N`); `$8000`–`$8FFF` the 1 KB video RAM four times (`SEL8`); `$C000`–`$DFFF`, `$E000`–`$E7FF`, `$F000`–`$FFFF` ROM sockets (`UG2` `SELC_N`–`SELF_N`, `BA11` on each 2316B's `CS2`); `$E810` PIA1, `$E820` PIA2, `$E840` VIA, each mirrored in `$E800`–`$EFFF` by the bits it ignores. `SEL2_N`–`SEL7_N`, `SEL9_N`–`SELB_N` go to the expansion connector, which is left out: nothing answers there (the data bus floats; with `UG5`/`UG6` turned towards the CPU, a read gives the `BD` bus's last value).

## Netlist

`netlists/bw2001.bwn` is the three transcribed sheets ([schematic/](schematic/)) joined by Net name, with these changes, each said in a comment where it applies:

| Transcription | Netlist | Why |
|---|---|---|
| `Y1` crystal, `C3`, `C4`, `R7`, `R8`, two `UD9` gates (9→8, 11→10) | `X1` `OSC freq=8MHz` on `UE2` pin 5; `UD9` 9 and 11 tied to `GND` | crystal oscillators are not simulated (080, 090) |
| LM555 `UDE9` and `UD9` gate 1 | `UDE9` `POR time=1ms` on `RES_N`, `R99` 3K3 pull-up (not on the PET); `UD9` pin 1 on `GND` | the `POR` stands for the 555 (P5n); 1 ms, not the PET's 1.1 s |
| `PU_Rnn` Nets (sheet 3: `R12`, `R27`, `R39`–`R41`, `R43`–`R46`) | `R` parts, 1K to `VCC` | pull-ups are Parts |
| `RW_E_N` (sheet 1), `RW_E` (sheets 2, 3); sheet 2's own `RW_E_N` | `RW_E` (the PET's "R/W·Ē", high for a read outside `$E000`); sheet 2's inverter output `RW_E_INV` | one name for the signal the three sheets share |
| `BLANK_TV` (sheet 1), `BLANKTV` (sheet 3) | `BLANK_TV` | same |
| `?74LS139` `UI9` (pins by number) | `UI9 74LS139` | new Part (P5q) |
| `?MC3446` `UA7`–`UA9` | `UA7`–`UA9 MC3446` | new Part (P5q) |
| `J1`, `J2`, `J3`, `J6` by pin number | `J1 IEEE488`, `J2 USERPORT`, `J3 CASSETTE`, `J6 CASSETTE` | passive connectors, the PET's pin labels as pin names (P5q) |
| `J5` | `J5 MATRIXKB` | the keyboard device (P5q) |
| `J7` | `J7 MONITOR` | separate drives (P5q) |
| — | `J8 POWER` | the PET's power connector |
| `J4` memory expansion | left out | decided 2026-11-15: about 50 more Nets to route for nothing to plug in |
| `R11`, `R47` (IEEE pull-downs), `R9` (LED series), the 2200 Ω pull-down on `VERT_DRIVE` (sheet 3) | left out | the builder takes a resistor only from a rail to a signal, and a pull-down beside a totem-pole output changes nothing in logic |
| sheet 2's `R12` 10K on `SEL1_N` past the 8K jumper (the same reference as sheet 3's 1K `R12`) | left out, the jumper closed | the 8K machine; `R12` stays sheet 3's |
| cassette motor transistors (`Q1`–`Q6`, `VR1`, `VR2`, …) | `CASS1_MOTOR` = PIA1 `CB2` straight to `J6` pin 3, `CASS2_MOTOR` = VIA `PB4` to `J3` pin 3 | logic-only equivalents (GAME_DESIGN): the control level, not the 9 V |
| `UF3.SO` not drawn | on `VCC` | as on P5n's bench |
| spare gate inputs | tied to `VCC` or `GND` | loads without warnings |

The transcription's uncertain readings ([SHEET1.md](schematic/SHEET1.md#uncertain-readings), [SHEET2.md](schematic/SHEET2.md), [SHEET3.md](schematic/SHEET3.md#uncertain-readings-15)) that BASIC 1's boot settles: `UE2` pin 13 on `NMI_N` (else `BA0` is wrong and nothing boots), `SEL8` active high from `UE5`, the 74LS139's enables and taps, the 74LS244 directions on both sheets, the 74177/74100 nest (the text is in the right place, row after row), `UA2`'s address lines (the glyphs are right). Those it does not settle stay as read and are listed in the service manual's notes only if a Puzzle (P5r) depends on them: `J5`'s column letters, `R18`–`R25` order, the IEEE handshake bundle (checked against the MC3446 pinout and the PET's documented IEEE port), the cassette read and switch lines.

**Three stages** (ADD_MACHINE: a big Machine grows in tested stages, each a commit with its ratio):

1. **Video** (sheet 3, `X1`, `J7`, `J8`, the video RAM): the CPU side held still: `UE5` pin 9 (`SEL8_N`) on `VCC`, so `UE5` itself drives `SEL8` low (the multiplexers on the video counters; the `SEL8` Net is never grounded under a driving output), `RW_RAM` and `RW_E` on `VCC`, `BA0`–`BA9` and `BD0`–`BD7` on `GND`, `GRAPHIC` on `GND` (the first character set, as the firmware sets it from stage 2) and `BLANK_TV` on `VCC`; the inputs of the sheet-1 gates in chips shared with sheet 3 tied (`UE2` 9, 10, 12, 13; `UE5` 1, 3, 5; `UE9` gate 2; `UD9` 1, 13); `PHI0` and `VIDEO_ON` (the Net `SYNC` in the transcription, renamed: it is the PET's "video on" state, and `SYNC` is the 6502's pin) to nothing. The TV shows the 2114s' random power-on bytes as 25 rows of 40 characters, locked.
2. **Computer** (sheets 1 and 2 without their connectors): the 6502, buffers, decoding, RAM, ROMs with our firmware, PIA1, PIA2, the VIA, `UG9`, the keyboard pull-ups, `DS1`, `UDE9`. The ties of stage 1 removed. The banner on the TV.
3. **Edges**: `J1`, `J2`, `J3`, `J5`, `J6`, the MC3446s. Keys typed through `J5`, the self-test by `J2` pin 5.

Every 74LS/7417/74177/74100 Part's unused gates tied. References are the PET's: `U` and the location (`UA1` … `UJ9`, `UDE9`), `R` numbers as Commodore's (`R10`–`R48`), `R99` ours, `X1` the oscillator, `DS1`.

### Connectors

| Ref | Part | Pins (pad: name, Net) |
|---|---|---|
| `J1` | `IEEE488` | 1–4 `DIO1`–`DIO4`, 5 `EOI_N`, 6 `DAV_N`, 7 `NRFD_N`, 8 `NDAC_N`, 9 `IFC_N`, 10 `SRQ_N`, 11 `ATN_N`, 12 `CHASSIS` (`GND`), 13–16 `DIO5`–`DIO8` (the PET's A–D), 17 `REN` (`GND`), 18–24 `GND` (F–N) |
| `J2` | `USERPORT` | 1 `GND`, 2 `VIDEO` (`TV_VIDEO`), 3 `SRQ_N`, 4 `EOI_N`, 5 `DIAG` (PIA1 `PA7`), 6 `READ1` (`CASS1_READ`), 7 `WRITE` (`CASS_WRITE`), 8 `READ2` (`CASS2_READ`), 9 `HORZ` (`HORZ_DRIVE`), 10 `VERT` (`VERT_DRIVE`), 11, 12 `GND`; 13 `GND` (A), 14 `CA1` (VIA), 15–22 `PA0`–`PA7` (VIA, the PET's C–L), 23 `CB2` (VIA, M), 24 `GND` (N) |
| `J3`, `J6` | `CASSETTE` | 1 `GND`, 2 `VCC`, 3 `MOTOR`, 4 `READ`, 5 `WRITE`, 6 `SENSE` (the PET's A–F) |
| `J5` | `MATRIXKB` | 1–10 `ROW0`–`ROW9` (`UG9` `Y0_N`–`Y9_N`), 11–18 `COL0`–`COL7` (PIA1 `PB0`–`PB7`, `KIN0`–`KIN7`), 19, 20 unused |
| `J7` | `MONITOR` | 1 `VIDEO_N` (`TV_VIDEO`), 2 `GND`, 3 `VDRIVE` (`VERT_DRIVE`), 4 `GND`, 5 `HDRIVE` (`HORZ_DRIVE`), 6 unused, 7 `GND` |
| `J8` | `POWER` | |

Pads: the table's numbers are the PET's (1–12, then its letters A B C D E F H J K L M N as 13–24, no G or I); on the 2 × 12 header PET pin n is pad 2n − 1 and the k-th letter pad 2k, so a letter faces its number as on the edge connector (P5q design §4). Pin names are the PET's labels, so the netlist and manuals never depend on pads.

## Board

`boards/bw2001/bw2001.place`:

```
grid pet                       # every U<row><column> chip at its PET location (P5q)
edge J3 back 10%
edge J2 back 40%
edge J1 back 75%
edge J6 left back
edge J7 left 60%
edge J5 front middle
edge J8 right front
edge DS1 right 30%              # the diagnostic LED
next R28 UE3 front             # then a chain: next R29 R28 right, …
…                              # each resistor beside the chip it serves
```

`grid pet` (P5q): rows A (back) to J (front), columns 1 (left) to 9 (right), every chip lying horizontally with pin 1 at the left, as on the PET's board; a column is as wide as its longest chip (the DIP-40s in columns 3, 5 and 8), a row as tall as a DIP-24 is wide plus a resistor's gap. `UDE9` between rows D and E in column 9, as its name says. `X1` (a DIP-14 oscillator) at `UD9`'s free side: placed by annealing into a free cell, as the resistors that have no `next`. About 370 × 230 mm. The user routes it (two layers; it is the biggest Board so far: about 90 chips and 400 Nets).

Each resistor's `next` from the transcription, chained along a row where several serve one chip (`next` centres a part on its other): `R28`–`R33` in front of `UE3` and `R34`–`R38` in front of `UE4` (address pull-ups; 11 resistors of 10.2 mm courtyard in two chip widths: checked when placing), `R18`–`R25` beside `UG8` (keyboard columns), `R10`, `R48` beside `UA9`, `R12`, `R27`, `R39`, `R40`, `R41`, `R43`, `R44`, `R45`, `R46` beside the chip whose input they hold (SHEET3.md), `R13`, `R14`, `R17`, `R26`, `R99` beside `UF3`/`UG3`. Any that does not fit is left to annealing into a free cell.

## Manuals

- `manuals/bw2001/user.md`: `# BW-2001`; what it does (the banner, BASIC, the keyboard window, the TV); the self-test: `.T` from the monitor, or a wire from user port pin 5 (`J2` pad 5) to a ground pin of the same connector, then reset (power off and on); the killer poke as a curiosity of the original ("the screen fills with snow while it prints"). Plain addresses.
- `manuals/bw2001/service.md`: `## Components`, one bullet per reference in netlist order (about 130: every chip, resistor, connector, `X1`, `DS1`, `UDE9`), each with its role and Nets as the BW-1's; a `## Notes` section with the timing (dots, characters, lines, the four vertical states), the snow, and the parts left out. Addresses and codes plain, not backticked.

## Tests

`tests/machines/bw2001_test.cpp`, `[machines][slow]`, release figures in brackets. The screen reader `Bw2001Screen` (`tests/machines/bw2001_screen.{hpp,cpp}`) reads the completed TV frame: it finds the text window from the first lit dot rows of a frame known to hold text, then for each of the 25 × 40 cells takes the 8 × 8 dots (one TV column a dot: the TV samples at 8 MHz, `kColumnTime` 125 ns) and matches them against the 901447-08 glyphs (screen codes `$00`–`$7F`, and `$80`–`$FF` as their inverse), giving a screen code or `?`.

- **Stage 1**: loads without warnings; the TV locks within 3 frames (`isLocked`), 260 lines a frame, `HORZ_DRIVE` period 64 µs high 24 µs, `VERT_DRIVE` low 1.28 ms every 16.64 ms (recorded Nets); the frame has a 320 × 200 window of dots and nothing outside it; the screen reader returns 1000 cells that match the video RAM's power-on bytes (read through `MemoryCells`), seeds 1–3.
The screen reader reads the first character set (`GRAPHIC` low, as the firmware and BASIC 1 set it).

- **Stage 2**: the banner and `READY` on the TV and in the video RAM by 1.5 s (our firmware: the RAM sizing, then the screen); the cursor blinks (the cell under it inverse in one frame, plain in a frame 1/3 s later); no snow at power-on: `SEL8` is never high while `VIDEO_ON` and `DIS_ON` are both high (recorded over the boot); `DS1` dark.
- **Stage 3**:
  - `PRINT 2+2` typed through `J5`'s queue (`MatrixKeyboard::type`) shows ` 4`, `READY`;
  - a 30-line program `LIST`ed scrolls with no snow (the `SEL8` check over the listing);
  - the self-test by a Net from `J2` pin 5 to `GND` (the netlist line rewritten, as the BW-1's strap test) shows every row `OK`;
  - with the real BASIC 1 in the sockets (`ExternalRom`, skipped without): the banner on the TV; `POKE 59458,62` then `FOR I=1 TO 20:PRINT "HELLO":NEXT`: snow (some frame differs from the video RAM's own rendering, `SEL8` high during the display), and a key typed afterwards still echoes (the retrace interrupt survives: the port's drive strength, P5q).
- **Board** (after the routing): `check-board` ok; the Machine cases built from the Board; the per-Machine Board loops; `session: bw2001 with every instrument` (`tests/session/session_test.cpp`).

Debug: a guess of 0.005–0.01× under ASan (0.24× release); phase 8 measures it and records it. Each case is kept to what it shows (stage 1 a few frames); a case that would run past about 10 minutes in debug is release-only (`#ifndef NDEBUG` → `SKIP`, ADD_MACHINE): the BASIC 1 and listing cases at least, likely the typing and self-test ones. Cases are named `bw2001 machine: stage N …`.

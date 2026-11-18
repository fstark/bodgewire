# 130 — BW-2001 — Machine Design

How the Machine in [BW2001.md](BW2001.md) is built: netlist, Board, manuals, tests. The new code it needs (the `MONITOR`, `MATRIXKB`, `IEEE488`, `USERPORT`, `CASSETTE` connectors, the `74LS139` and `MC3446`, the port drive strength, the TV's separate drives, the keyboard window, the PET grid in `bw place`, the editor's retrace wait) is P5q's: [BW2001_MACHINE_DESIGN.md](../../../docs/p5q-bw2001/BW2001_MACHINE_DESIGN.md). Recipe: [ADD_MACHINE.md](../../../docs/recipes/ADD_MACHINE.md).

## Files

Machine name `bw2001`:

```
bw2001.bwn                              design netlist (grown in three stages)
roms/bw2001_*.asm, .rom                 P5p's firmware (the editor's retrace wait, P5q)
roms/901447-08.bin                      the character ROM (P5n)
board/bw2001.place                      connectors, the PET grid, resistors beside their chips
board/bw2001.kicad_pcb, .kicad_pro      Board (placed by bw place, routed by the user)
manuals/user.md, service.md
tests/machines/bw2001_test.cpp          fault-free cases on the netlist, then the Board
tests/machines/bw2001_screen.{hpp,cpp}  reads the 40 x 25 text off a TV frame
puzzles/NN-*.bwp                        16 Puzzles, easiest first
```

## Memory map

As P5p's ([BW2001_FIRMWARE.md](../../../docs/p5p-bw2001-firmware/BW2001_FIRMWARE.md#the-pet-it-runs-on)), now with the PET's own decoding: `$0000`–`$1FFF` RAM (`UI9` 74LS139 from `SEL0_N`, `SEL1_N`); `$8000`–`$8FFF` the 1 KB video RAM four times (`SEL8`); `$C000`–`$DFFF`, `$E000`–`$E7FF`, `$F000`–`$FFFF` ROM sockets (`UG2` `SELC_N`–`SELF_N`, `BA11` on each 2316B's `CS2`); `$E810` PIA1, `$E820` PIA2, `$E840` VIA, each mirrored in `$E800`–`$EFFF` by the bits it ignores. `SEL2_N`–`SEL7_N`, `SEL9_N`–`SELB_N` go to the expansion connector, which is left out: nothing answers there (the data bus floats; with `UG5`/`UG6` turned towards the CPU, a read gives the `BD` bus's last value).

## Netlist

`machines/130-bw2001/bw2001.bwn` is the three transcribed sheets ([schematic/](schematic/)) joined by Net name, with these changes, each said in a comment where it applies:

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
| `UH2`, `UH6` | `contents=roms/empty.rom` (every word `$FF`) | our firmware leaves them empty; Commodore's BASIC 1 fills them (`--roms`, P5q2). P5q left them out, against the board: fitted again in P5q2 |
| the 2114s | `access=300ns` (the 2114-3) | at 450 ns the video path misses every 64th character (P5q plan, phase 8) |
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

`machines/130-bw2001/board/bw2001.place` (P5q1, [BW2001_BOARD.md](../../../docs/p5q1-bw2001-board/BW2001_BOARD.md)): laid out after PET-Shack's "PET CPU BOARD" drawing of a PET 2001 board. `grid pet`: every chip at its PET location, columns J (left) to A (right), rows 9 (the back) to 1 (the front), lying with the notch at the left; eight upright as drawn (`UF3`, `UB8`, `UA5`, `UG2`, `UE6`, `UA2` notch at the bottom; `UG8`, `UG9` at the top). IEEE-488 (`J1`), user port (`J2`) and cassette #2 (`J3`) down the right edge; power (`J8`), cassette #1 (`J6`), the keyboard (`J5`: inside the board on the PET), `DS1` and the monitor (`J7`) along the back. Pull-ups beside their chips, chained along the resistor bands away from the upright chips; `X1`, `UDE9` and `R99` in the empty cells nearest their Nets. 293 × 269 mm, 35.2 m of wire; routed by the user on two layers (P5q's first Board, on a guessed grid, is `1f8e110`).

Running other ROMs (P5q2): `--roms DIR`, the files named by socket (`UH1.bin` …); Commodore's BASIC 1 as `external/roms/pet-basic1/` (tests/data/pet_roms.md). `UH2` and `UH6` were imported into the routed Board and routed by the user; the two connections that rerouting broke (`BD_TO_D_N`, `CASS_WRITE`) were routed by a script on a 0.1 mm grid (P5q2 plan).

## Manuals

Written to STYLE's Manuals rules (P6k):

- User: What it does (a welcome: the PET 2001 copied chip for chip; the 6502, 8 KB, the five ROMs, the 40 × 25 screen; the banner); Using it (the keyboard and the PC mapping, BASIC, the screen editor's keys, the monitor's `.M` and `.X`, the self-test by a wire from `J2.DIAG` to `J2.GND1` or `.T`, Commodore's ROMs by `--roms`, the killer poke); How it works (the goal: two machines sharing one memory; the clock; the picture: beam counters, video address and `UE6`'s row start, screen memory, character ROM, shift register, reverse video, the output; the computer: the 6502, the buffers, the decoder with a memory map table, the memory, the ROMs, start-up, the keyboard matrix, the retrace interrupt, `DS1`; sharing the screen memory: the multiplexers, snow, the editor's wait; the unused features in a table).
- Service: Components (every reference, netlist order; formulas and test-point wording tightened); Test points (probe readings idle at `READY`, checked with `bw run --probe` at 1.5 s and 2 s, the clock and video side also at 0.2 s; `SEL8` and `RES_N` from a `--record` VCD over 3 s: `SEL8` high 2 µs every third of a second, from the cursor blink; `KIN0` with `--type 'J5:1.4s:AAAA…'` read at 1.9 s. Five logic analyzer setups, each checked with `bw_ui --clip/--group … --sample … --view … --screenshot --after 2s` and the picture viewed: a line (`--clip UC5.1Q_N:r,UC6.3Y:1,UC5.2Q,UE9.1Y --sample 100ns --view 10us:+30us`), a frame (`--clip UD8.4Y:f,UC6.3Y,UC5.1Q_N --sample 10us --view 2ms:+8ms`), the video address (`--group UC3.A0 --clip UC5.2Q:r,UC6.3Y:1 --sample 100ns --view 1us:+4us`: 000, 001…), the 6502 starting (`--group UF3.A0,UF3.D0 --clip UF3.SYNC:r,UF3.RW`: FFFD F8, F806 78, F807 D8), the 6502 writing the screen (`--clip UE5.4Y:r,UF3.RW,UC6.3Y` with both buses: 8000, `RW` low the second microsecond, `VIDEO_ON` low)); Troubleshooting (no picture; the picture rolls; every character wrong the same way; no banner; wrong bytes free; specks while printing; no cursor blink or dead keys: ordered checks, never naming a fault).

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

## Puzzles

`machine bw2001` in each file (BW2001.md, Puzzles); tests in `tests/machines/bw2001_puzzles_test.cpp` (release-only but the title and build cases), helpers in `tests/machines/bw2001_rig.{hpp,cpp}`. Each Fault was tried with `bw run --tv` and `bw_ui` before its brief was written (P5r plan notes); the dead ones on seeds 1–3.

| File | Fault line | Symptom (seed 1; seeds 1–3 the same where run) | Repair |
|---|---|---|---|
| `odd_e.bwp` | `fault cell UA2.O2 $2A 1` | every `E` with a dot at line 2, bit 2 | swap `UA2` |
| `striped_screen.bwp` | `fault crack UA2.O3 UB2.D 50%` | dot bit 3 lit on all lines of every cell | mend |
| `rolling_text.bwp` | `fault crack J7.VDRIVE UD8.4Y 50%` | the TV never locks | mend |
| `stutter.bwp` | `fault stuck UD2.3Y 0` | TV row 0 `**  WW2200  IIYYBBSSCC****` | swap `UD2` |
| `negative_screen.bwp` | `fault stuck UC1.1Q 1` | every cell reversed | swap `UC1` |
| `short_memory.bwp` | `fault column UI5.IO1 0` | `2046 BYTES FREE`; self-test `RAM BAD 0C00 01` | swap `UI5` |
| `missing_keys.bwp` | `fault crack J5.COL0 UG8.PB0 50%` | `QWERTYASDF` types `ERTYDF` | mend |
| `frozen_cursor.bwp` | `fault stuck UG8.CB1 0` | no blink, no key | swap `UG8` |
| `random_screen.bwp` | `fault stuck UDE9.RST_N 0` | the power-on screen unchanged; `RES_N` low; no self-test | swap `UDE9` |
| `snow.bwp` | `fault stuck UA5.PB5 0 input` | over 1000 screen accesses in the display over a 30-line loop; `VIDEO_ON` toggles at `UA5` | swap `UA5` |
| `bad_zero_page.bwp` | `fault column UI8.IO1 1` | cleared, under 100 stray cells, no banner; self-test `ZERO PAGE BAD 0001 01` | swap `UI8` |
| `wrong_chip.bwp` | `fault joint UH8.2A4` | as `bad_zero_page`; a `UI8` swap changes nothing | resolder `UH8` pin 17 |
| `ghost_letter.bwp` | `fault cell UC3.IO2 $00C 1` | `*** BW-2001 VINY BASIC ***`; self-test `VIDEO RAM BAD 800C 02` | swap `UC3` |
| `garbled_text.bwp` | `fault column UC3.IO1 1` | TV row 0 `+++!CW-3111!UIOY!CASIC!+++` and `!` everywhere; the self-test's row `WIEEO!SAM!CAE!…` | swap `UC3` |
| `no_clock.bwp` | `fault crack X1.OUT UE2.2B 50%` | black, unlocked; `CLK8` still, `OSC8` at `X1` running | mend |
| `joined_bits.bwp` | `fault bridge UF3.D0 UF3.D1` | the power-on screen unchanged; `RES_N` high; no self-test | cut the bridge |

Missing keys depends on two readings the transcription assumed (Netlist above): `J5`'s column order and `R18`–`R25`'s (`R18` on `KIN0`). The player's manuals never discuss the transcription, so the note is here: if the real PET differs, the Puzzle still holds on our Board, which is what the player probes.

Snow's specks are brief: a one-frame screenshot may miss them (and with no wait, printing is fast); the case's snow watch (screen accesses while the beam draws the text) is the evidence.


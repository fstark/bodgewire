# P5a Bus load spike — Design

Spec: [BUS_LOAD.md](BUS_LOAD.md). Plan: [BUS_LOAD_PLAN.md](BUS_LOAD_PLAN.md). Recipes: [ADD_PART.md](../recipes/ADD_PART.md), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) (netlist and its tests only: no Board, manuals or Puzzles).

## Files

| File | Change |
|---|---|
| `boards/footprints/DIP-40_W15.24mm.kicad_mod` | copied unchanged from KiCad 10.0.7 `Package_DIP.pretty` (ADD_PART, "A new package") |
| `src/board/kicad_netlist.cpp` | `kFootprints` row `{"DIP-40", "Package_DIP:DIP-40_W15.24mm"}`, array size 15. Not a grid package (wider than DIP-16): no `IsGridPackage` change; a future Board places it with a rule, as the DIP-24 |
| `tests/board/footprint_test.cpp`, `tests/board/kicad_netlist_test.cpp` | DIP-40 pads and courtyard (as the DIP-24 case, `19a244f`); footprint table row; the table's unknown-package check moves from `DIP-40` to `PLCC-44` |
| `src/parts/z80_stub.{hpp,cpp}` | `class Z80Stub final : public Component` (S1, below) |
| `src/parts/delays.hpp` | `kZ80…` delays (below) |
| `src/parts/CMakeLists.txt` | `z80_stub.cpp` |
| `src/parts/catalogue.cpp` | `PartInfo{"Z80STUB", "Z80 stub playing a ZX80 frame (benchmark)", "DIP-40", Chip, &CreateChip<Z80Stub, "Z80STUB">}`, after `VIDEO` (sorted) |
| `datasheets/Z80STUB.md` | Function (what it plays, that it executes nothing, inputs ignored), Pins (every name; ranges change one character only: `A0`–`A9`, `A10`–`A15`) |
| `tests/parts/z80_stub_test.cpp`, `tests/CMakeLists.txt` | S1 tests |
| `tests/parts/catalogue_test.cpp` | `Z80STUB` in `kOffCornerPower` (`VCC` pin 11, `GND` pin 29) |
| `roms/make_bus_load_rom.py`, `roms/bus_load.rom` | ROM generator and its output (below) |
| `netlists/bus_load.bwn` | S2 circuit (below) |
| `tests/machines/bus_load_test.cpp`, `tests/CMakeLists.txt` | netlist tests |
| `docs/ARCHITECTURE.md`, `docs/p1d-kernel-perf/OPTIMISATION_IDEAS.md` | S4 decision (last phase) |
| `docs/recipes/ADD_PART.md` | references: DIP-40 package, the stub as a clock-scripted Part on both clock edges |

## Z80Stub

### Pins

Z80 DIP-40, index = pin − 1: 1 `A11`, 2 `A12`, 3 `A13`, 4 `A14`, 5 `A15`, 6 `CLK`, 7 `D4`, 8 `D3`, 9 `D5`, 10 `D6`, 11 `VCC`, 12 `D2`, 13 `D7`, 14 `D0`, 15 `D1`, 16 `INT_N`, 17 `NMI_N`, 18 `HALT_N`, 19 `MREQ_N`, 20 `IORQ_N`, 21 `RD_N`, 22 `WR_N`, 23 `BUSAK_N`, 24 `WAIT_N`, 25 `BUSRQ_N`, 26 `RESET_N`, 27 `M1_N`, 28 `RFSH_N`, 29 `GND`, 30–40 `A0`–`A10`. Family TTL (the Z80's pins are TTL-compatible; no `kMosSignals` entry). `CLK` not rising-edge-only. `D0`–`D7` `PinKind::Bidirectional` (ADD_PART, shared data pins): the stub is woken by data changes like any reader and returns at once unless `CLK` rose or fell.

### Script

Built once in the constructor (allocation allowed there, never in `evaluate`): `std::vector<Cycle> frame_`, `struct Cycle { Kind kind; std::uint16_t address; std::uint8_t data; }`, kinds `Fetch`, `HaltFetch`, `Read`, `Write`, `In`, `Out`, `InterruptAcknowledge`, `Internal` with lengths 4, 4, 3, 3, 4, 4, 6, 1 T-states. A constructor `Check` asserts every line adds up to 207 T-states and the frame to 312 lines.

| Lines | Cycles (T-states) |
|---|---|
| 0 | `In` `0xFFFE` (4), 29 × [`Fetch` PC (4), `Read` RAM (3)] |
| 1–2 | 29 × [`Fetch`, `Read`] (203), `Fetch` (4) |
| 3 | 29 × [`Fetch`, `Read`] (203), `Out` `0xFFFE` data `0xFF` (4) |
| border (4–51, 244–311) | 10 × [`Fetch`, `Read` RAM, `Fetch`, `Write` page] (140), 2 × `Fetch` (8), then the tail |
| display (52–243; row = (line − 52) / 8) | 32 × `Fetch` `0xC000 + row × 33 + column` (128), `Fetch` of the NEWLINE at `+ 32` (4), 4 × `HaltFetch` at `+ 33` (16), then the tail |
| tail (from T-state 148) | `InterruptAcknowledge` at the HALT address (display) or PC (border) (6), `Internal` (1), `Write` `0x43FF` PC high, `Write` `0x43FE` PC low (6), 10 × `Fetch` `0x0038`… (40), `Read` `0x43FE`, `Read` `0x43FF` (6) |

- PC for border fetches walks `0x0100`–`0x05FF` and wraps; RAM reads take `0x4000 | (lfsr & 0x3FF)`, a 16-bit Galois LFSR (`0xB400`) stepped per read; writes walk the page cursor through the 792 display-file bytes (`0x4000 + i`, data `Page()[i]`), 1,160 writes a frame. All of it fixed at construction: every frame plays the same cycles.
- `Page()`: `static const std::array<std::uint8_t, 792>&`, row r of 33 bytes: 32 codes then `0x76`. Codes are 2513 order (ASCII & `0x3F`, `D6` = 0). Row 0: `" BODGEWIRE P5A  BUS LOAD  SPIKE "`; row 1 spaces; rows 2–23: code `(r × 32 + c) & 0x3F`, `| 0x80` (inverse) when `(r / 2)` is odd.
- `R`: 7-bit, +1 after each `Fetch`, `HaltFetch`, `InterruptAcknowledge`; refresh address `0x0E00 | R` (`I` = `0x0E`, the ZX80's character set page).
- `HALT_N` goes low at T1 of the first `HaltFetch` and high at the `Internal` cycle after `InterruptAcknowledge` (the stub keeps it low through the acknowledge, so the NOP-force gate never fires there).
- State: cycle index, T-state within the cycle, `R`, `halted`. Power-on: control outputs high, address `0x0000`, data Z, `BUSAK_N` high; the first `CLK` rise starts cycle 0 (`powerOn` does not randomise: the stub is a benchmark, deterministic). `Z80Stub` exposes `line()` and `tState()` for tests.

### Pin timing

Per cycle, edges by T-state (T1 = first). ↑ rising `CLK`, ↓ falling. Delays from `kZ80…` (Z80A datasheet maxima, which are all it publishes; stated in `delays.hpp` and the commit).

| Kind | Edges |
|---|---|
| `Fetch`, `HaltFetch` | T1↑ A = address, `M1_N` 0, `RFSH_N` 1; T1↓ `MREQ_N` 0, `RD_N` 0; T3↑ `MREQ_N` 1, `RD_N` 1, `M1_N` 1, A = `0x0E00 \| R`, `RFSH_N` 0; T3↓ `MREQ_N` 0; T4↓ `MREQ_N` 1 |
| `Read` | T1↑ A, `RFSH_N` 1; T1↓ `MREQ_N` 0, `RD_N` 0; T3↓ `MREQ_N` 1, `RD_N` 1 |
| `Write` | T1↑ A, `RFSH_N` 1; T1↓ `MREQ_N` 0, D = data; T2↓ `WR_N` 0; T3↓ `MREQ_N` 1, `WR_N` 1 |
| `In` | T1↑ A, `RFSH_N` 1; T2↑ `IORQ_N` 0, `RD_N` 0; T3↓ `IORQ_N` 1, `RD_N` 1 (T3 is the 4th T-state: one automatic wait) |
| `Out` | T1↑ A, `RFSH_N` 1; T1↓ D = data; T2↑ `IORQ_N` 0, `WR_N` 0; T3↓ `IORQ_N` 1, `WR_N` 1 |
| `InterruptAcknowledge` | T1↑ A = address, `M1_N` 0, `RFSH_N` 1; T3↓ `IORQ_N` 0; T5↑ `M1_N` 1, `IORQ_N` 1, A = `0x0E00 \| R`, `RFSH_N` 0; T5↓ `MREQ_N` 0; T6↓ `MREQ_N` 1 |
| `Internal` | T1↑ `RFSH_N` 1, `HALT_N` 1 |
| every cycle | T1↑ D released (Z) if it was driven |

| Delay | ps | Datasheet (Z80A) |
|---|---|---|
| `kZ80Address` | 110'000 | tD(AD), `CLK`↑ → address |
| `kZ80Mreq` | 85'000 | `MREQ_N` both ways |
| `kZ80Rd` | 85'000 | `RD_N` both ways |
| `kZ80Wr` | 80'000 | `WR_N` both ways |
| `kZ80Iorq` | 75'000 | `IORQ_N` both ways |
| `kZ80M1` | 100'000 | `M1_N` both ways |
| `kZ80Rfsh` | 130'000 | `RFSH_N` both ways |
| `kZ80Halt` | 300'000 | `HALT_N` |
| `kZ80Data` | 150'000 | data out valid, and float |

`M1_N` rises (100 ns) before the address leaves the fetch address (110 ns): the latch clocked by `M1_N` sees the fetched byte and `A15` (below).

## Netlist `netlists/bus_load.bwn`

Header comment as other netlists (what it is, P5a, the spec's path); no Machine number. Parts and Nets, net names as below. Spare gate inputs tied to `GND` (ADD_MACHINE, netlist step).

| Ref | Part | Role |
|---|---|---|
| X1 | OSC freq=6.5MHz | `DOT` |
| U1 | 74LS74 | 1: `CLK` = `DOT` / 2 (1D ← 1Q_N = `CLK_N`, 1PRE_N and 1CLR_N on `VCC`); 2: `HSYNC` (2PRE_N ← `INTACK_N`, 2CLR_N ← `RCO_N`, D and CLK on `GND`) |
| U2 | Z80STUB | `INT_N`, `NMI_N`, `WAIT_N`, `BUSRQ_N`, `RESET_N` on `VCC`; `BUSAK_N` unconnected (own Net) |
| U3 | 2716 contents=../roms/bus_load.rom | A0–A8 ← `RA0`–`RA8`; A9, A10 ← `A9`, `A10`; CE_N ← `A14`; OE_N ← `WR` (off while writing); VPP ← `VCC`; O0–O7 → `MD0`–`MD7` |
| U4, U5 | 2114 | A0–A9 ← `A0`–`A9`; CS_N ← `RAM_CS_N`; WE_N ← `WR_N`; IO1–IO4 ↔ `MD0`–`MD3` (U4), `MD4`–`MD7` (U5) |
| U6 | 74LS244 | read: `MD0`–`MD7` → `D0`–`D7`, both OE_N ← `RD_EN_N` |
| U7 | 74LS244 | NOP: A on `GND` → `D0`–`D7`, OE_N ← `NOP_EN_N` |
| U8 | 74LS244 | write: `D0`–`D7` → `MD0`–`MD7`, OE_N ← `WR_N` |
| U9, U10 | 74LS175 | CLK ← `M1_N` (end of each M1), CLR_N ← `VCC`. U9: `MD0`–`MD3` → `CH0`–`CH3`. U10: `MD4`, `MD5` → `CH4`, `CH5`; `MD7` → `INV` / `INV_N`; `NOP_FORCE_N` → `DISP_N` (4Q) / `DISP` (4Q_N) |
| U11–U13 | 74LS157 | S ← `RFSH_N`, G_N ← `GND`; A (refresh) `LC0`–`LC2`, `CH0`–`CH5`; B `A0`–`A8`; Y → `RA0`–`RA8`; 3 spare units on `GND` |
| U14 | 74LS165 | SH_LD_N ← `LOAD_N`, CLK ← `DOT`, CLK_INH and SER ← `GND`, A–H ← `MD0`–`MD7` (H, the first dot out, is `MD7`); QH_N → `PIX_N` |
| U15 | 74LS93 | CKA ← `HSYNC` (counts at its end), CKB ← `LC0`, R01, R02 ← `VSYNC`; QA–QC → `LC0`–`LC2` |
| U16 | 74LS161 | CLK ← `CLK`, CLR_N ← `HSYNC`, ENP, ENT, LOAD_N ← `VCC`, A–D ← `GND`; RCO → `RCO`: `HSYNC` set at the acknowledge's T3↓, cleared ≈ 14.3 T later (4.4 µs) |
| U17 | 74LS04 | `M1`, `MREQ`, `MD6_N`, `WR`, `RCO_N`; 1 spare |
| U18 | 74LS04 | `NOP_FORCE`, `RFSH`; 4 spare |
| U19 | 74LS32 | `RD_EN_N` = `RD_N` + `NOP_FORCE`; `NOP_EN_N` = `RD_N` + `NOP_FORCE_N`; `INTACK_N` = `M1_N` + `IORQ_N`; `VS_CLR_N` = `IORQ_N` + `WR_N` |
| U20 | 74LS32 | `IORD_N` = `IORQ_N` + `RD_N`; `VS_SET_N` = `IORD_N` + `A0`; 2 spare |
| U21 | 74LS00 | `RAM_CS_N` = NAND(`A14`, `MREQ`); 3 spare |
| U22 | 74LS20 | `NOP_FORCE_N` = NAND(`M1`, `A15`, `MD6_N`, `HALT_N`); `LOAD_N` = NAND(`RFSH`, `REFRESH_END`, `DISP`, `VCC`) |
| U23 | 74LS74 | 1: `INV_SHOWN` ← `INV_ON` at `RFSH_N`↑ (end of each refresh); 2: `REFRESH_END` ← `RFSH` at `MREQ_N`↑ (1 only at the end of a refresh strobe, 0 at the end of the next fetch's). PRE_N, CLR_N on `VCC` |
| U24 | 74LS02 | `INV_ON` = NOR(`INV_N`, `DISP_N`); `SYNC_N` = NOR(`HSYNC`, `VSYNC`); 2 spare |
| U25 | 74LS86 | `VIDEO_D` = XOR(`PIX_N`, `INV_SHOWN`); 3 spare |
| U26 | 74LS74 | 1: `VSYNC` (1PRE_N ← `VS_SET_N`, 1CLR_N ← `VS_CLR_N`, D and CLK on `GND`); 2: `VIDEO` re-timed on `DOT` (2PRE_N, 2CLR_N on `VCC`) |
| J1 | POWER | |
| J2 | VIDEO | `VIDEO`, `SYNC_N` |

### Timing checks (worst case, from the delays above and the Parts' own)

The first design gated the load with NOR(`RFSH_N`, `MREQ`, `DISP_N`) and applied `INV_ON` straight to the XOR. Both failed on the TV (review and phase 3): `MREQ_N` is also high between T3↑ + 85 and T3↓ + 85, so the 165 loaded the floating bus mid-character (a lit bar before every character), and `INV_ON` moves at T3↑ + 141, about 4 dots into the previous character. U23 fixes both.

- Display fetch: address at T1↑ + 110 ns; the 2114 gives data 120 ns after `CS_N`↓ (its address moved while deselected), worst case 450 ns after the address, T1↑ + 560; `NOP_FORCE_N` settles by ≈ T1↑ + 590. The latch clocks at `M1_N`↑ = T3↑ + 100 = T1↑ + 715. Narrow margins, holding in the model: `A15` leaves at T3↑ + 110 (10 ns after the latch edge) and `RAM_CS_N` rises ≈ T3↑ + 104, the RAM floating 100 ns later.
- Refresh: `RFSH_N`↓ at T3↑ + 130, mux select + 18 ns; the latch's `CH` at T3↑ + 121, mux data + 9 ns: ROM address stable by T3↑ + 150, data by T3↑ + 600.
- Load: `REFRESH_END` rises at the refresh strobe's end (`MREQ_N`↑ = T4↓ + 85 = T3↑ + 546, + 25 ns); `LOAD_N` low ≈ T3↑ + 585 until `RFSH_N`↑ (next T1↑ + 130 = T3↑ + 745) plus an inverter and the NAND, ≈ T3↑ + 770. The 165 loads asynchronously, so it ends holding the glyph line valid since T3↑ + 600. `REFRESH_END` falls at the next fetch's `MREQ_N`↑ (T3↑ + 85 + 25), before that refresh's `RFSH_N`↓ (+ 130). The mux switches back at T1↑ + 148; the 2716 holds its old data 450 ns more.
- Dots: during the load `QH` shows column 0 (blank); the shifts on `DOT`↑ at T1↓ … T4↓ show columns 1–7; column 7 lasts ≈ 0.65 dot before the next load. Columns 1–5, the glyph, are whole dots.
- Inversion: `INV_SHOWN` takes `INV_ON` at the end of each refresh (T1↑ + 130 + 20): before the character's column 1 reaches the video register (sampled at T2↑). A NEWLINE or HALT fetch clears it, so the right border of an inverted row is white.
- Write cycle: the 2114 drives `MD` from `MREQ`↓ until `WR_N`↓ + 100 ns, U8 from `WR_N`↓ + 20 ns: ≈ 80 ns of contention (X) on `MD` per write. Real on a Z80 bus without isolation; the ZX80's resistors avoid it. Counted, not hidden, in the measurement.
- Line counter: `VSYNC` holds U15 at 0 for lines 0–3; the 48 `HSYNC`s of lines 4–51 bring it back to 0 at line 52 (48 mod 8 = 0), so each text row starts at glyph line 0.
- TV: lines 0–3 have no `HSYNC` (the ZX80 has none in its vertical sync). The TV free-runs 3 lines of 66 µs there, then rejects the early `HSYNC`s (< 56 µs after its line start) until it drifts back in step: 311 lines a frame, 284 synced, locked (≥ 90 %). The text lines (from 52) are all synced.

## ROM `roms/bus_load.rom`

`roms/make_bus_load_rom.py` (Python 3, standard library only, run with the repo's venv) writes it; the file's header says so and how to rerun it.

- `@000`–`@5FF`: code-like bytes from a fixed xorshift (seed in the script), 16 hex words a line: the data bus toggles on ROM fetches as it would on real code.
- `@600`–`@7FF`: 64 glyphs × 8 lines, `.#` rows 8 wide, leftmost = `O7`. Each 2513 glyph line (5 dots, `roms/2513.rom`) sits in columns 1–5; columns 0, 6, 7 blank. Line 0 blank as in the 2513.
- ROM address during refresh: `A10 A9` = 1 1 (from `I`), `A8`–`A3` = code, `A2`–`A0` = glyph line: `0x600 + code × 8 + line`.

## Tests

`tests/parts/z80_stub_test.cpp` (ADD_PART test bench: the stub, a `ClockSource` at 307'692 ps on `CLK`, each output on its own Net, recorded):

- `parts: Z80STUB fetch` — the first display line's first fetch: address `0xC000 + 0` at T1↑ + `kZ80Address`, `M1_N` 0, `MREQ_N`/`RD_N` 0 at T1↓ + their delays; refresh address `0x0E00 | R` with `RFSH_N` 0 in T3; `R` + 1 on the next fetch.
- `parts: Z80STUB read and write` — a border line's read (strobes, no data driven) and write (`D` = the page byte at T1↓ + `kZ80Data`, `WR_N` low T2↓–T3↓, `D` Z at the next T1↑).
- `parts: Z80STUB lines and frames` — over 2 frames: an `InterruptAcknowledge` (`M1_N` and `IORQ_N` low together) every 207 T-states on lines 4–311, none on 0–3; `IN` every 312 × 207 T-states; `HALT_N` low only in display lines.
- `parts: Z80STUB writes the page` — the bytes written to `0x4000`–`0x4317` over one frame equal `Page()`.

`tests/machines/bus_load_test.cpp` (as `typewriter_test.cpp`'s `DesignCircuit` and `lockedFrames`, no Board):

- `bus load: netlist loads without warnings`.
- `bus load: the TV shows the page` `[slow]` — seeds 1, 2; 3 locked frames (≈ 80 ms simulated: the first frame does not lock); stats of the 2nd and 3rd: vertically synced, 310–312 lines, ≥ 90 % synced (Timing checks, TV); the expected 256 × 192-dot page (from `Page()` and `ReadRomContents(bus_load.rom, 2048, 8)`) found in frame 2 sampling the column under each dot's centre (one dot = 153.85 ns = 1.23 TV columns, so the left offset is searched in quarter columns), comparing glyph columns 1–5 (columns 0 and 7 are blank and partly shown); frame 3 matches at the same place.

Debug cost: ≈ 15 s a seed (release runs this netlist at ≈ 0.2×).

## Measurement method (S3)

- Release preset, plugged in, other apps quiet; three runs each of `bus_load`, `test_pattern`, `typewriter` alternated; best ratio kept with its `applied_events` and `evaluations`.
- Instructions: `/usr/bin/time -l` (OPTIMISATION_IDEAS, How to measure).
- Profile: `cmake --build build/profile` (configured in P1d; reconfigure with `-O3 -g -fno-omit-frame-pointer` if gone), `sample bw 5` on a 10 s run, aggregated by function in a scratch script (not committed).
- Events per Net group: `bw run netlists/bus_load.bwn --time 5ms --record-all --vcd <scratch>` (≈ 600 k changes, tens of MB) then a scratch script counts value changes per Net, grouped by name (`A*`, `D*`, `MD*`, `RA*`, controls, `DOT`/`CLK`, video chain). Changes ≈ applied events.
- Recording cost: `bw run … --time 2s --record A0,…,A7` against the plain run (what 8 analyzer Channels on the bus cost). `bw_ui` cannot clip a netlist-only Session (`Session::padLead` needs a Board), so instruments are measured headless.
- UI: `bw_ui netlists/bus_load.bwn --screenshot <scratch>/a.png --after 2s` (TV). The PNG viewed; ratio and UI ms recorded. `bw_ui` reports the missing `manuals/bus_load/`: expected, the netlist is not a Machine.

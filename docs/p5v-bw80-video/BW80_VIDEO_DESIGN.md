# P5v BW-80 video spike — Detailed Design

Implements [BW80_VIDEO.md](BW80_VIDEO.md): the `C` Part, memory timing from the address, `.incbin`, the ZX80's netlist, its ROM and a test ROM, and their tests. No Machine (P5x), no UI change beyond the Net panel's line for a capacitor.

Recipes: [ADD_PART.md](../recipes/ADD_PART.md) (Files table, rules; a passive Part as `R` is), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §2 (netlist, a program, a real board from its schematic, `--roms`). All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

Sources, cited by these short names:

- **ZX80**: Grant Searle's redrawn ZX80 circuit (`zx80.pdf`, issue 2, his corrections listed on the page) and "Circuit operation" (searle.x10host.com/zx80/zx80.html, plain http). The drawing's text needs a renderer with font substitution (PDFKit); poppler draws its labels as boxes.
- **LISTING**: the ZX80 ROM assembly listing (k1.spdns.de, `zx80 rom source.txt`; local copy `external/roms/zx80/zx80_rom_source.txt`): `MASK-INT` ($0038), `DISP-1`/`KEYBOARD` ($013C–$01AC), `DISP-2` ($01AD).
- **P5u**: [BW80_CHIPS_DESIGN.md](../p5u-bw80-chips/BW80_CHIPS_DESIGN.md) §6 (the bench this netlist grows from), §2 (the resistor network).

---

## 1. How the ZX80 makes a line (ZX80, LISTING)

Z80A at 3.25 MHz (T = 307.7 ns), 207 T-states a line (63.7 µs).

1. `DISP-2` loads `R`, enables interrupts and `JP (HL)` into the display file's echo at `$C000+`. The display file is in RAM at `$4000`, and `A15` high selects the echo.
2. Each character fetch (`M1`, `A15'` high, `HALT_N` high, `D6'` low) forces `NOP_N` low. IC15 then releases `NOP_EN` (R3 pulls it high), and the eight 74LS05s pull `D0`–`D7` low directly, on the CPU side. The CPU reads `$00` while the RAM keeps driving `D0'`–`D7'` behind R4–R11. IC5 is transparent while `MREQ` is low with `RFSH_N` high and `/φ` high (IC16, IC13), so it takes the code (`CH0`–`CH7`) and freezes it in the refresh.
3. In the refresh (`RFSH_N` low), IC6–IC8 switch the ROM's `A0'`–`A8'` from the CPU's `A0`–`A8` to `LC0`–`LC2` and `CH0`–`CH5`. `A9`–`A11` come from the CPU, where the refresh puts `I` = `$0E`, so the ROM gives the glyph row at `$0E00` + code × 8 + line. IC6 also holds `CS1` low through the refresh.
4. At the next rise of `φ` (T1 of the following fetch, `MREQ_N` high), C11/R25 make a short high pulse. Margin on the model: `RFSH_N` falls about 130 ns after T3's rise, the 74LS157s settle `RA` and `CS1` about 15 ns later, and the 2332's 450 ns puts the glyph on `MD` about 1213 ns after T1. The load pulse comes about 1231 ns (`PHI` leads `CLK` by the 74LS05s), so about 20 ns spare. With `CH6_N` high (a character, not the `HALT`), IC16 gives `LOAD_N` low, and IC9 loads the glyph from the CPU side of the data bus (the ROM's byte through the resistors: nothing else drives `D` then). IC9 shifts it out at each rise of `2φ`, through C9/R2. Its `QH_N` (link A–C: black ink on white) goes through IC20's XOR with the inverse latch to `VIDEO`.
5. Inverse: the same pulse strobes IC11 (pin 3) and resets the IC12 latch; IC11 (pin 6) = NAND(`LOAD` through R24, `CH7`) sets it for an inverse character. So the latch follows each loaded character and is reset at a non-loading cycle.
6. `$76` (`HALT`, bit 6 set) is not forced: the CPU halts and re-reads the `HALT` (§3.4), refreshing with `R` counting. The latch takes `$76` at each of these fetches, so nothing loads and the rest of the row is blank. When `R` bit 6 drops, `A6` in the refresh pulls `INT_N` low. The interrupt acknowledge's `IORQ_N` presets IC18's sync latch (flip-flop 1). Two rising edges of `M1` (IC15 pin 8 with R18) later, through IC19 flip-flop 2 and then flip-flop 1, IC19's `1Q` takes `SYNC` low and clears IC18. Two more edges and `SYNC` is high again: 20 T-states (6.15 µs) with `MASK-INT`'s instructions. `SYNC` also clocks the line counter IC21 (`CKB`, falling edge: `LC0`–`LC2` = `QB`–`QD`).
7. VSYNC: an `IN` from an even port (`KBD_N`) resets the IC11 NAND latch. Its pin 8 low clears IC19 flip-flop 1, holding `SYNC` low, and its pin 11 high resets IC21. An `OUT` (`IOWR_N`) sets the latch again. `KEYBOARD` holds it about 1260 T-states (388 µs, 6 lines).

## 2. `C` — a capacitor (new)

### 2.1 Catalogue and Part kind

- `PartKind::Capacitor` (catalogue.hpp, after `Resistor`). `PartInfo{"C", "capacitor", "disc", Capacitor, nullptr}` in `s_parts` (sorted: after `"BUTTON"`, before `"CASSETTE"`).
- A passive Part as `R`: no Component at `placeParts`, pins `1` and `2`, only `value=`. In the builder `IsResistor` becomes `IsPassive` (`Resistor` or `Capacitor`) where it means "pins 1 and 2, `value` only, no Component" (`placeParts`, `resolvePin`, `connectNets`, the `loaded_->parts` value text); `IsResistor` stays for `wireResistors`.
- Package `disc`: `kFootprints` row `{"disc", "Capacitor_THT:C_Disc_D3.0mm_W1.6mm_P2.50mm"}` (21 → 22), the footprint copied from the installed KiCad into `boards/footprints/` unchanged (ADD_PART, rules: a new package).
- `datasheets/C.md`: "# C — capacitor"; Function: "A capacitor passes changes, not levels. With one end on a line that a chip drives and the other on a line held by a pull-up or pull-down resistor (the ZX80's C9 with R2, C11 with R25), each edge of the driven line appears on the pulled one as a short pulse; the resistor then brings the line back. The pulse lasts about the resistor's ohms times the capacitor's farads (470 Ω × 47 pF ≈ 22 ns). Anywhere else (across the supply, between two driven lines) it does nothing the game shows. Every capacitor needs its value: `value=47p`, `value=100n`, `value=1u`." Pins: "`1`, `2`: the two ends; either way round".

### 2.2 Value — `units.{hpp,cpp}`

`std::expected<std::uint64_t, std::string> ParseFarads(std::string_view text)`: picofarads. Digits, then one of `p`, `n`, `u` (micro) and optionally more digits, the letter standing for the decimal point and the multiplier, as `ParseOhms`: `47p` → 47, `4p7` → 5 (rounded), `100n` → 100000, `4n7` → 4700, `1u` → 1000000, `2u2` → 2200000. A unit letter is required (a bare `47` is refused: farads are never written so). Error: `"bad capacitance '<text>': expected e.g. 47p, 100n, 4n7 or 1u"`. Tests in `tests/netlist/units_test.cpp`.

### 2.3 Builder — `wireCapacitors`, after `wireResistors`

`wireResistors` keeps, for each signal Net with a pull, its smallest pull in ohms (`pullOhms_`, a `std::map<NetId, std::uint32_t>`; pulls on joined Nets included). Then for each `C`:

- Errors, as a resistor's: `"C11: a capacitor needs both pins connected"`, `"C11: a capacitor needs value= (farads)"`, `"C11: <ParseFarads error>"`, `"C11: both ends on net X"`.
- **Coupling**: exactly one end's Net is a signal with a pull (the output), and the other end is on a signal Net with no pull (the input). Then `circuit.add<Capacitor>(reference, input, ohms × picofarads)` (Ω × pF = ps), its input Pin connected to the input Net, its output Pin to the output Net.
- Anything else adds nothing, with no error: both ends on rails (a decoupling capacitor, P5x), a rail and a signal, no pull on either side, or a pull on both. The datasheet says it "does nothing the game shows".

`LoadedCircuit` gets nothing new: the Component is the circuit's, as the `ResistorNetwork` is. A capacitor's two pins are `loaded_->connections` like a resistor's.

### 2.4 Component — `src/parts/capacitor.{hpp,cpp}` (new)

```cpp
// A capacitor coupling a driven Net to a pulled one (P5v): each edge of the input is repeated on
// the output, which is released `width` later for its pull resistor to bring back. Pins named
// "1" and "2" as the Part's; `input` is the index of the driven end.
class Capacitor final : public Component {
public:
    Capacitor(std::string reference, PinIndex input, Time width);
    void evaluate(EvalContext& ctx) override;
private:
    PinIndex input_, output_;
    Time width_, releaseAt_ = 0;
};
```

- Pins: the input `PinSpec{name, Input, Ttl}`, the output `PinSpec{name, Output, Ttl}`. The output starts at Z (default).
- `evaluate`: when the input changed (`read != readBefore`): drive the output to the input's new value (0, 1 or X) with `kCouplingDelay` = 1 ps, `releaseAt_ = now + width_`, `wakeAfter(width_)`. Otherwise (a wake): when `now >= releaseAt_`, drive Z with 1 ps. An earlier wake that a newer edge superseded finds `now < releaseAt_` and does nothing.
- Width under 1 ps (a tiny value): 1 ps.
- Limitation, in the class comment: the pulse's length does not depend on the edge's direction or on what else drives the output Net; the output's drive is Strong, so a chip driving that Net fights it (X) for the pulse's length, as a real capacitor would fight a real gate.

Tests `tests/parts/capacitor_test.cpp` (Catch2, tag `[parts]`), through the builder on small netlists: a 74LS04 output, `C1 value=47p`, `R1 value=470` to `GND`, the far Net into a 74LS04 input. A rising edge gives 1 on the far Net for 22.09 ns, then 0 again. A falling edge gives 0 (already 0). With the pull to `VCC`, a falling edge gives a low pulse. X gives X then the pull. A second edge inside the pulse restarts it. Nothing is added for both ends on rails, no pull, or pulls on both sides (the far Net reads only its pull). The builder's four errors. Width: `47p` and `470` → 22 090 ps.

### 2.5 Board and Net panel

- `electrical.cpp`: the resistor's two sites (`NameIslands` skips its pins; `ElectricalNetlist` keeps it between two different islands, not both rails) take any passive Part (`IsPassive`, as the builder). Test in `tests/machine/electrical_test.cpp` on P5u's hand-built Board (`series_bench`'s loader): a `C` between two islands kept, left out once bridged.
- `net_pins.{hpp,cpp}`: `PinRole::Capacitor`; `DescribeNet` gives a capacitor end with its `value`, in `drivers` when its Net has a pull resistor (the end it pulses), else in `loads`. `FormatNetPin`: `C11 pin 2 · 47p capacitor`. Test in `tests/netlist/net_pins_test.cpp`.

## 3. Memory timing from the address

### 3.1 `2114` — `ram2114.{hpp,cpp}`

New member `Time dataReady_ = 0`: when the word at the current address is valid, as far as the address goes. In `evaluate`, when any address Pin changed (`read != readBefore`): `dataReady_ = now + access_`. The output delay while selected and reading:

- the select or `WE_N` changed: `max(k2114Select, dataReady_ - now)` (`k2114Select` is tCO, 120 ns);
- else an address changed: `access_`;
- else (power-on, data pins only): `access_`, as today.

At least 1 ps. The class comment says it: "data at the later of the address's time plus `access` (tAA) and the select's time plus tCO".

Tests in `tests/parts/ram2114_test.cpp`: address set while deselected, `CS_N` low 100 ns later: the data comes at address + access (450 ns default; 300 with `access=300ns`); `CS_N` low 400 ns after the address with `access=300ns`: at select + 120.

### 3.2 ROMs — `rom.{hpp,cpp}`

The same rule for the family. `RomSpec::evaluate` gains a `Time& dataReady` argument, kept per `Rom` (member `dataReady_`). In `EvaluateRom`, a changed address Pin sets `dataReady` to the latest `now + addressDelay[i]` of the changed Pins. An enable change gives `max(enableDelay, dataReady - now)`. Otherwise the code stays as is. Tests in `tests/parts/rom_test.cpp` for the 2716 and the 2332 (the 2316B, 2513 share the template): select after the address, data at address + access when that is later.

### 3.3 Z80: halted fetches at the `HALT` — `z80_core.cpp`

`haltedFetch` puts `pc_ - 1` (the `HALT`) on the address bus, not `pc_`; PC itself is unchanged, so an accepted interrupt still pushes the address after the `HALT`. Reason in the Spec (Decided). `Z80Core: HALT` in `tests/parts/z80_core_test.cpp` checks the address; `datasheets/Z80.md`'s HALT line says the processor keeps reading the `HALT` itself.

### 3.4 Who else sees it

Every Machine with a 2114 or ROM whose select follows its address: the 080 message, 090 typewriter, 110, BW-1 (its RIOTs' ROMs are not `Rom`), BW-2001, and `bus_load*`. The full suite runs at this phase's fence. A Machine that misses a fetch with this rule had a margin only the old model gave it. Its fix (a grade, as P5q's) goes to the user before it is made.

## 4. `.incbin` — `src/asm/assembler.cpp`

`.incbin "file", OFFSET, LENGTH`: the `LENGTH` bytes of `file` from `OFFSET` are emitted at the current address, as `.byte`s. `file` resolves as `.include`'s does (against the including source's directory). `OFFSET` and `LENGTH` must be known in the first pass. Errors: `".incbin needs \"file\", OFFSET, LENGTH"`, `".incbin needs a file on disk"`, `".incbin reads past the end of <file>"`. Both CPUs (the directive is the assembler's, not the CPU's). The listing's `asm_*` ctest and `asm: every ROM program matches its committed .rom` cover the test ROM. A change to `zx80_ic2.bin` changes the test ROM's `.rom`, and that test says so. Tests in `tests/asm/asm6502_test.cpp` beside `.include`'s (bytes copied, the errors), with a small binary written by the test into a temporary directory.

## 5. `netlists/bw80.bwn`

### 5.1 Header

"BW-80 (the Sinclair ZX80), P5v (docs/p5v-bw80-video/BW80_VIDEO.md): Grant Searle's redrawn issue 2 schematic, every chip at its designator, the keyboard matrix, cassette, modulator and edge connector left out (P5x)". Then a paragraph on what each block does (§1).

### 5.2 Parts

| Ref | Part | Notes |
|---|---|---|
| X1 | `OSC freq=6.5MHz` | stands in for the ceramic resonator and IC20's units 3–4 (pins 8–13, inputs tied) |
| IC1 | `Z80` | |
| IC2 | `2332 select=LL contents=../roms/zx80_ic2.bin` | |
| IC3 | `2114 access=300ns` | bits `D7'`, `D1'`, `D0'`, `D2'` |
| IC4 | `2114 access=300ns` | bits `D4'`, `D3'`, `D5'`, `D6'` |
| IC5 | `74LS373` | data latch |
| IC6–IC8 | `74LS157` | ROM address |
| IC9 | `74LS165` | dots |
| IC10 | `74LS365` | keyboard buffer |
| IC11, IC12 | `74LS00` | |
| IC13 | `74LS04` | |
| IC14, IC15 | `74LS05` | |
| IC16 | `74LS10` | |
| IC17 | `74LS32` | |
| IC18, IC19 | `74LS74` | |
| IC20 | `74LS86` | |
| IC21 | `74LS93` | line counter |
| RST1 | `POR time=1ms` | stands in for C10 (§Spec, Reset) |
| R1 | 1k | `EAR` pull-down |
| R2 | 470 | `SHIFT_CLK` pull-down (C9) |
| R3 | 1k | `NOP_EN` pull-up |
| R4–R11 | 1k | data bus: `D4'`–`D4`, `D3'`, `D5'`, `D6'`, `D2'`, `D7'`, `D0'`, `D1'` (R7 on `D6'`: Searle's correction) |
| R12, R22, R23 | 1k | `NMI_N`, `BUSRQ_N`, `WAIT_N` pull-ups |
| R13–R17 | 47k | `KB0`–`KB4` pull-ups |
| R18 | 1k | `M1_CLK` pull-up (IC15 pin 8) |
| R19 | 2k2 | `RAM_SEL_N`–`RAM_CS_N` |
| R20 | 330 | `CLK` pull-up |
| R21 | 220k | `RESET_N` pull-up |
| R24 | 1k | `LOAD`–`LOAD_R` |
| R25 | 470 | `LOAD_PULSE` pull-down (C11) |
| R28 | 47k | `D6'` pull-up (Searle's correction: `D6'`, not `D6`) |
| C9 | `47p` | `DOT2` → `SHIFT_CLK` |
| C11 | `47p` | `PHI` → `LOAD_PULSE` |
| J1 | `POWER` | |
| J2 | `VIDEO` | |

Net names: primes as `D0'` in the docs, `MD0`–`MD7` in the netlist (a Net name has no `'`; the P5u bench's `MD` is kept), `RA0`–`RA8` for `A0'`–`A8'`, `A14B`, `A15B` for `A14'`, `A15'`.

### 5.3 Wiring (pins by the catalogue's names; ZX80 pin numbers in brackets where the drawing gives them)

**Clock.** `X1.OUT` → `DOT` → IC20 unit 1 `1B` (2), `1A` (1) on GND; `1Y` (3) → `DOT2` (2φ): IC18 `2CLK` (11), C9.1. IC18 `2D` (12) ← `PHI_N`; `2Q` (9) → `PHI`: C11.1; `2Q_N` (8) → `PHI_N`: IC18 `2D`, IC15 `5A` (11), `6A` (13), IC16 `1B` (2). `2PRE_N`, `2CLR_N` (10, 13) on VCC. IC15 `5Y`, `6Y` (10, 12) → `CLK`: IC1 `CLK`, R20.

**CPU.** IC1 `A0`–`A15`, `D0`–`D7`; `INT_N` ← `A6`; `NMI_N` ← R12; `BUSRQ_N` ← R22; `WAIT_N` ← R23; `RESET_N` ← RST1 `RST_N`, R21. `M1_N`, `MREQ_N`, `IORQ_N`, `RD_N`, `WR_N`, `RFSH_N`, `HALT_N` on their Nets; `BUSAK_N` unconnected (an output).

**Memory.** IC3, IC4 `A0`–`A9` ← `A0`–`A9`; `WE_N` ← `WR_N`; `CS_N` ← `RAM_CS_N`. IC3 `IO1`–`IO4` = `MD7`, `MD1`, `MD0`, `MD2`; IC4 = `MD4`, `MD3`, `MD5`, `MD6`. IC2 `A0`–`A8` ← `RA0`–`RA8`; `A9`–`A11` ← `A9`–`A11`; `CS1` ← `ROM_CS1`; `CS2` ← `A12`; `O0`–`O7` → `MD0`–`MD7`.

**Address multiplexers** (S on `RFSH_N`, `G_N` on GND; B = the CPU, A = the display):

| Chip | 1 (A, B → Y) | 2 | 3 | 4 |
|---|---|---|---|---|
| IC8 | `CH0`, `A3` → `RA3` | `LC1`, `A1` → `RA1` | `LC0`, `A0` → `RA0` | `LC2`, `A2` → `RA2` |
| IC7 | `CH3`, `A6` → `RA6` | `CH2`, `A5` → `RA5` | `CH1`, `A4` → `RA4` | `CH4`, `A7` → `RA7` |
| IC6 | `CH5`, `A8` → `RA8` | GND, `ROM_SEL_N` → `ROM_CS1` | `A14`, `A14` → `A14B` | `A15`, `A15` → `A15B` |

The A inputs are read from lines that cross on the drawing; they are taken in the ROM's address order (line counter at the bottom, the code above it), and the test ROM's page proves them.

**Decoding.** IC13: `1A` `MREQ_N` → `1Y` `MREQ`; `2A` `LE_N` → `LE`; `3A` `MD6` → `MD6_N`; `4A` `LOAD_N` → `LOAD`; `5A` `CH6` → `CH6_N`; `6A` `A14B` → `A14B_N`. IC12: unit 3 `MREQ`, `A14B` → `RAM_SEL_N`; unit 4 `MREQ`, `A14B_N` → `ROM_SEL_N`. R19 `RAM_SEL_N`–`RAM_CS_N`.

**Data latch.** IC16 unit 1: `RFSH_N`, `PHI_N`, `MREQ` → `LE_N`. IC5: `OE_N` on GND, `LE` ← `LE`; `1D`–`8D` = `MD4`, `MD5`, `MD0`, `MD6`, `MD7`, `MD1`, `MD2`, `MD3`; `1Q`–`8Q` = `CH4`, `CH5`, `CH0`, `CH6`, `CH7`, `CH1`, `CH2`, `CH3`.

**NOP.** IC16 unit 2: `2A` `HALT_N`, `2B` `A15B`, `2C` `MD6_N` → `NOP_COND_N`. IC17 unit 3: `NOP_COND_N`, `M1_N` → `NOP_N`. IC15 unit 1: `NOP_N` → `NOP_EN`, R3. The NOP gates (inputs on `NOP_EN`), in the drawing's order, left to right: IC14 unit 4 → `D4`, unit 3 → `D3`, unit 5 → `D5`; IC15 unit 2 → `D6`; IC14 unit 2 → `D2`, unit 1 → `D7`, unit 6 → `D0`; IC15 unit 3 → `D1`. (The drawing's crossing bus lines are read in the order the resistors and IC9 list the data lines; any order works the same.)

**Data bus.** R4–R11 as §5.2; R28 `MD6` pull-up.

**Dots.** C9.2 → `SHIFT_CLK`: IC9 `CLK`, R2. C11.2 → `LOAD_PULSE`: IC16 `3A` (9), IC11 `1B` (2), R25. IC16 unit 3: `LOAD_PULSE`, `CH6_N`, `MREQ_N` → `LOAD_N`: IC9 `SH_LD_N`, IC13 `4A`. IC9 `A`–`H` ← `D0`–`D7` (the CPU side); `SER`, `CLK_INH` on GND; `QH_N` → `PIX_N` (link A–C); `QH` unconnected.

**Inverse.** IC11 unit 1: `1A` `MREQ_N`, `1B` `LOAD_PULSE` → `INV_STROBE_N`. R24 `LOAD`–`LOAD_R`. IC11 unit 2: `2A` `CH7`, `2B` `LOAD_R` → `INV_SET_N`. IC12 unit 1: `INV`, `INV_STROBE_N` → `INV_N`; unit 2: `INV_N`, `INV_SET_N` → `INV` (cross-coupled NANDs: pins 1 ← 6, 4 ← 3). IC20 unit 2: `PIX_N`, `INV` → `VIDEO`: J2 `VIDEO`.

**I/O.** IC17 unit 1: `RD_N`, `IORQ_N` → `IORD_N`; unit 2: `WR_N`, `IORQ_N` → `IOWR_N`; unit 4: `IORD_N`, `A0` → `KBD_N`. IC10: `G1_N`, `G2_N` ← `KBD_N`; `A1` `EAR` → `Y1` `D7`; `A3` `KB0` → `D0`; `A4` `KB1` → `D1`; `A5` `KB2` → `D2`; `A2` `KB3` → `D3`; `A6` `KB4` → `D4`. R13–R17 `KB0`–`KB4` pull-ups; R1 `EAR` pull-down.

**VSYNC latch.** IC11 unit 3: `3A` `IOWR_N`, `3B` `VS_RESET` → `VS_N`; unit 4: `4A` `VS_N`, `4B` `KBD_N` → `VS_RESET`. (`VS_N` low during the frame sync: it clears IC19 flip-flop 1; `VS_RESET` high then: IC21's resets.)

**Sync.** IC15 unit 4: `M1_N` → `M1_CLK`, R18. IC18 flip-flop 1: `1D` on GND, `1CLK` on VCC, `1PRE_N` ← `IORQ_N`, `1CLR_N` ← `SYNC`; `1Q_N` → `SYNC_ARM_N`. IC19 flip-flop 2: `2D` ← `SYNC_ARM_N`, `2CLK` ← `M1_CLK`, `2PRE_N`, `2CLR_N` on VCC; `2Q` → `SYNC_NEXT`. IC19 flip-flop 1: `1D` ← `SYNC_NEXT`, `1CLK` ← `M1_CLK`, `1PRE_N` on VCC, `1CLR_N` ← `VS_N`; `1Q` → `SYNC`: J2 `SYNC_N`, IC21 `CKB`, IC18 `1CLR_N`; `1Q_N` unconnected (Searle: unused).

**Line counter.** IC21 `CKB` ← `SYNC`; `R01`, `R02` ← `VS_RESET`; `QB`, `QC`, `QD` → `LC0`, `LC1`, `LC2`; `CKA` on GND; `QA` unconnected.

**Ties.** IC20 units 3, 4 inputs (`3A`, `3B`, `4A`, `4B`) on GND. Every other unit is used. The builder warns on an unconnected input, and the load test checks there are none.

### 5.4 What the netlist cannot show

In the header: the ceramic resonator (X1), the reset RC (RST1), the modulator's mixing of `SYNC` and `VIDEO` (the `VIDEO` connector takes them apart), the cassette ports, the keyboard and its diodes, the edge connector (`RAM_CS_N` is not driven from outside), D11 (not fitted: the 50 Hz machine).

## 6. ROMs

### 6.1 `roms/zx80_ic2.bin`, `roms/zx80_ic2.md`

The user's dump, 4096 bytes, SHA1 `b6769a3197c77009e0933e038c15b43cf4c98c7a`, copied from `external/roms/zx80/zx80.rom`. `roms/zx80_ic2.md`: what it is (Sinclair's ZX80 ROM, IC2, a 2332 marked 8332 on the parts list), the SHA1, the user's decision to ship it (2026-11-16: the ZX80 ROM is distributed with emulators), the listing's URL and its local copy, the character set at `$0E00`–`$0FFF` (64 glyphs × 8 rows, code 0 space, `$30` `K`).

### 6.2 Test ROM — `roms/bw80_test.asm` → `roms/bw80_test.rom`

Our own code, to the ZX80's rules (§1). The header comment gives the machine, the spec and the memory map. Assembled by `bw asm`, `.cpu z80`, `.rom $0000, $1000`; run as IC2 through `--roms` (a directory with `IC2.rom` linked to it), as a diagnostic ROM would be.

- `$0000`: `DI`; `LD SP,$4400`; `LD A,$0E`, `LD I,A`; `IM 1`; build the display file at `$4000`: a `HALT` ($76), then 24 rows of 32 codes and a `HALT` each. Row r, column c holds `(r × 32 + c) & $3F`, with `$80` added on rows 2, 3, 6, 7, … (`r & 2`), so each code appears normal and inverse. Then the frame loop.
- `$0038` (`IM 1`): the line interrupt, the same algorithm as LISTING's `MASK-INT`: count the scan lines in `C`, re-run the row while lines remain, at a row's end take the next row from the return address the interrupt pushed, count rows in `B`, and return when they are done. Re-enable interrupts and jump to the row with `R` preset.
- Frame loop: VSYNC by `IN A,($FE)` … `OUT ($FF),A`, held for LISTING's time (≈ 1260 T-states); a top margin of 56 blank lines (a lone `HALT` executed with `C` = 56), the 24 rows of 8 lines, a bottom margin, so a frame is 312 lines (64 584 T-states, as the P5s program counted). `R` is loaded with LISTING's values (`$DD` for every line after the first, `$EC` and `$F3` before the margins), so the interrupt comes at LISTING's place in the line.
- `.org $0E00`, `.incbin "zx80_ic2.bin", $0E00, $200`: the ZX80's character set.

The numbers whose meaning is timing (the margins, the delay loop, the `R` values) are named constants with a comment pointing to the LISTING routine they match.

## 7. Tests — `tests/machines/bw80_test.cpp` (new), tag `[machines]`

Built from `netlists/bw80.bwn` (no Board yet) as `bw_ui`'s no-Board branch does: `ParseNetlist`, `ResolveContentsPaths`, `ApplyRomDirectory` for the test ROM, `BuildCircuit`. The TV reads `VIDEO` and `SYNC` (the Net on `J2.SYNC_N`). The page helpers (`ExpectedPage`, `Matches`, `FindPage`) are `bus_load_test.cpp`'s, moved to `tests/machines/tv_page.{hpp,cpp}` and taking the page and the glyph table as arguments. `bus_load_test` then uses them, with its cases unchanged.

- **"bw80: netlist loads without warnings"**.
- **"bw80: the test ROM shows every character"** `[slow]`, seeds 1–2: a `--roms`-style directory (`IC2.rom` → `roms/bw80_test.rom`, a symlink in a temporary directory, as `bw2001_test.cpp`'s `Basic1Circuit` makes it); run until 3 locked frames (cap 400 ms). Frames 2 and 3: `verticalSynced`, `syncedLines` ≥ 90 % of `lines`. The expected page (§6.2's codes through the ROM's glyphs, bit 7 inverting) is found in frame 2 and matches frame 3.
- **"bw80: a 450 ns RAM misses the NOP"** `[slow]`, seed 1: the same with IC4's `access=` removed (the netlist text edited in the test before parsing). The page is not found in the third locked frame, or the TV never locks.
- **"bw80: the ZX80 ROM shows its K cursor"** `[slow]`, seed 1: the netlist as committed. 3 locked frames (cap 1 s: the ROM's RAM fill takes about 160 ms first); in the last, the inverse `K` (the glyph at `$0E00 + $30 × 8`) is found, and inside the picture area no other ink. The picture area is the 32 cells to the right of the `K`'s left edge, over the 24 rows that can hold it (from 23 rows above it to 23 below, clipped to the frame). The margins are not checked: the TV draws without blanking, so the dots loaded in the sync periods (the interrupt routine's and the keyboard scan's opcodes with bit 6 clear) show there.

Every case's simulated time and its debug wall time go in the plan's notes. A case over about a minute in debug becomes release-only (`#ifndef NDEBUG` → `SKIP`), as the BW-2001's long cases are (ADD_MACHINE §2). The fence then runs it in release.

## 8. Release

- `bw run netlists/bw80.bwn --time 2s` (the ZX80 ROM) and with `--roms` (the test ROM), median of 3: the ratio and `applied_events`. `bus_load_z80` side by side as the reference.
- `bw_ui netlists/bw80.bwn --screenshot /tmp/bw80.png --after 1s`, and with `--roms`: viewed.
- ARCHITECTURE: the P5v row done; the P5w row skipped (the user, 2026-11-16: the ZX80 ROM ships); P5x's row: "the ZX80's ROM" in place of "our ROM", P5y's likewise; the Performance paragraph's BW-80 line with the measured ratio.
- ARCHITECTURE's P5 row: the BW-80 runs the ZX80's own ROM, shipped in `roms/` (with the PET's character ROM, the exceptions to "our own firmware").
- Recipes: ADD_PART (a passive Part: `C`, its reference commit; memory timing from the address; `disc` is not a grid package, so `bw place` needs a rule for a capacitor: P5x); ADD_MACHINE §2 (`.incbin` for a ROM's data from a shipped dump; the ZX80's NOP margin as an example of a grade).
- GLOSSARY: **Capacitor** (as above), **Line counter** if missing.

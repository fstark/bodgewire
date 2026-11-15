# P5n BW-2001 chips — Detailed Design

Implements [BW2001_CHIPS.md](BW2001_CHIPS.md): binary ROM contents, the `2316B` with mask-set selects, a helper for the real ROMs in tests, six TTL Parts, the `6520` and `6522`, and a bench. No Machine, no UI change.

Recipes: [ADD_PART.md](../recipes/ADD_PART.md) (Files table, Which kind, rules; each Part below names only what differs). All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

Sources, cited by these short names:

- **TI76**: TI *The TTL Data Book*, 2nd ed., 1976, section 7 (bitsavers).
- **NS154**: National DM74LS154 data sheet.
- **TI17**, **TI21**, **TI145**: TI SN7417 (rev. H), SN74LS21 (SDLS139), SN74LS145 data sheets.
- **SY2316**: Synertek SY2316B data sheet; **MOS2316**: MOS 2316/2316A (Feb. 1979).
- **MOS6520**, **R6520**: MOS MCS6520 and Rockwell R6520 data sheets.
- **MOS6522**: MOS MCS6522 preliminary (Nov. 1977); **R6522**: Rockwell R6522; **RVD**: Rockwell "VIA differences" note (6502.org `documents/datasheets/`).
- **PET**: schematics 320082 and 320130 and the parts cross reference (zimmers.net).
- **MAME** `6522via.cpp`, **VICE** `viacore.c`, where the data sheets leave a cycle open.

---

## 1. Binary contents — `src/parts/rom_contents.{hpp,cpp}`

`ReadRomContents(path, size, width)`: a path whose extension is `.bin` (any case) is a raw dump. Its bytes are the words, one byte a word:

- the file must hold exactly `size` bytes, else `"<file>: 1024 bytes, expected 2048"`;
- `width` must be 8, else `"<file>: a .bin holds 8-bit words"` (the 2513 is 5 bits wide);
- a file that cannot be opened gives the existing `cannot read '<path>'`.

Every other extension is parsed as text, as today. The header comment of `ReadRomContents` says so. `ResolveContentsPaths` is unchanged: a relative path is taken from the netlist's directory, and `external/` is already git-ignored.

## 2. `2316B` — `src/parts/rom.{hpp,cpp}`, `catalogue.cpp`

### 2.1 Select polarities

`RomEnable` keeps `{pin, delay}`, and its comment becomes "active low unless the Part's `select=` says otherwise". `EvaluateRom<L>` gains a run-time argument:

```cpp
// activeHigh: bit i set when enable i is active high (2316B select=); 0 for the 2716 and 2513.
void (*evaluate)(EvalContext& ctx, std::span<const std::uint8_t> words, unsigned activeHigh);
```

In the enable loop, `off |= v == (activeHigh >> i & 1 ? Zero : One);`. `unknownEnable` and the delays are unchanged. `Rom` stores `unsigned activeHigh_`; its constructor gains `unsigned activeHigh = 0` as the last argument.

`RomSpec` gains `bool selectable;  // select= sets each enable's polarity (2316B)` as its last field. The 2716 and 2513 rows pass `false`, the 2316B row `true`.

### 2.2 Pins and timing

The pinout is MOS2316 p. 1, the 2716's except for the selects:

| Pins | Names |
|---|---|
| 1–8 | `A7`–`A0` |
| 9–11, 13–17 | `O0`–`O7` (the sheet's O1–O8; named as the 2716's) |
| 12, 24 | `GND`, `VCC` |
| 18, 20, 21 | `CS2`, `CS1`, `CS3` |
| 19, 22, 23 | `A10`, `A9`, `A8` |

The enables in the spec are listed in the order `CS1`, `CS2`, `CS3`, so `select=` reads in that order.

`delays.hpp` takes SY2316 p. 2-3, maximums (no typicals are published, as for the 2716):

```cpp
inline constexpr Time k2316Access = 450'000;  // address → O valid (tACC)
inline constexpr Time k2316Select = 120'000;  // CS active → O valid (tCO)
inline constexpr Time k2316Float = 100'000;   // CS inactive → O off (tDF)
```

### 2.3 Catalogue

The catalogue row is `PartInfo{"2316B", "2K x 8 mask ROM, mask-set chip selects", "DIP-24", Chip, &CreateRom<"2316B">}`.

`CreateRom` accepts `select=` when `spec.selectable`, and requires it there:

| Case | Error |
|---|---|
| missing | `2316B needs select=<CS1 CS2 CS3: L or H each>, e.g. select=LLH` |
| not three of `L`/`H` | `2316B: bad select 'LX' (three of L or H)` |
| on a 2716 | `2716: unknown parameter 'select'` (the existing text) |

Each letter maps to `activeHigh` bit 0 (`CS1`) to bit 2 (`CS3`). The pin names drop `_N` because their polarity depends on the mask. The datasheet file says so.

### 2.4 Parameter lists in tests and the manual

Only `catalogue_test.cpp` (pinouts) builds every ROM; it also passes `select=LLH` when `RomSpec::selectable`. `PartPinNames` (`manual.cpp`) takes a ROM's names from its spec and needs nothing.

### 2.5 `datasheets/2316B.md`

The datasheet file covers:

- the function in plain words;
- the three selects, whose levels are chosen when the chip is made, with the PET's wiring: `CS1` low on its 4 KB block, `CS2` on `A11` low for the lower 2 KB and high for the upper, `CS3` tied high;
- the delays;
- the pins.

## 3. Real ROMs in tests — `tests/netlist/external_rom.hpp` (new, header only)

`bw_tests` has no include path for `tests/`, so the header sits beside its users and is included by a relative path (as `mos6502_cases.hpp`). Its functions are `inline`; it also lists the seven files by socket (`kPetBasic1`). `tests/netlist/external_rom_test.cpp` reads each through `ReadRomContents` and checks BASIC 1's reset vector (`$FD38` at `$FFFC`), SKIPping without them.

```cpp
// A real ROM dump the user keeps in git-ignored external/roms/ (P5n): its path, or nullopt when
// it is missing (the test then SKIPs). `name` as "pet/rom-1-c000.901447-01.bin".
inline std::optional<std::filesystem::path> ExternalRom(std::string_view name);
```

It returns `BW_ROOT_DIR/external/roms/<name>` if that is a regular file. The caller SKIPs with `"no external/roms/pet: see tests/data/pet_roms.md"`.

`tests/data/pet_roms.md` (new) lists the expected files, their sockets and their SHA1s. These are MAME's `pet2001` set, checked against the user's files on 2026-11-15:

| File | Socket | SHA1 |
|---|---|---|
| `rom-1-c000.901447-01.bin` | UH1 `$C000` | `831db40324113ee996c434d38b4add3fd1f820bd` |
| `rom-1-c800.901447-02.bin` | UH5 `$C800` | `70c0f4fa67a70995b168668c957c3fcf2c8641bd` |
| `rom-1-d000.901447-03.bin` | UH2 `$D000` | `4bf2c20c51a63d213886957485ebef336bb803d0` |
| `rom-1-d800.901447-04.bin` | UH6 `$D800` | `d293972d529023d8fd1f493149e4777b5c253a69` |
| `rom-1-e000.901447-05.bin` | UH3 `$E000` | `f02f5fb492ba93dbbd390f24c10f7a832dec432a` |
| `rom-1-f000.901447-06.bin` | UH4 `$F000` | `960717282878e7de893d87242ddf9d1512be162e` |
| `rom-1-f800.901447-07.bin` | UH7 `$F800` | `d440f2510bc52e20c3d6bc8b9ded9cea7f462a9c` |

The character ROM (901447-08) is P5q's. The test checks each file's size (2048 bytes) through `ReadRomContents`, not its hash.

## 4. TTL Parts

### 4.1 `74LS154` — `src/parts/ls154.{hpp,cpp}`

The class is `Ls154`, the catalogue row `PartInfo{"74LS154", "4-to-16 line decoder", "DIP-24", Chip, …}`. The PET's parts list fits the LS part; the schematic draws a 74154.

- **Pins** (TI76 p. 7-171): 1–11 `Y0_N`–`Y10_N`, 12 `GND`, 13–17 `Y11_N`–`Y15_N`, 18 `G1_N`, 19 `G2_N`, 20 `D`, 21 `C`, 22 `B`, 23 `A`, 24 `VCC`.
- **Logic**: the 74LS138's evaluation (`ba95150`) over 4 select bits and two active-low enables. X folds over the selects, and an X enable makes the selected outputs X. The delay depends on the input that changed.
- **Delays** (NS154, typicals): `kLs154Select{23'000, 23'000}` (A–D → Y), `kLs154Enable{19'000, 19'000}` (G → Y).
- **Package**: a DIP-24 chip. `bw place` needs a rule for it (recipe, new package note); the bench is not placed.

### 4.2 `74177` — `src/parts/ttl177.{hpp,cpp}`

The class is `Ttl177`, the catalogue row `PartInfo{"74177", "presettable binary counter/latch", "DIP-14", Chip, …}`. It is plain TTL, as the PET's UD6 and UD7.

**Pins** (TI76 p. 7-259):

| Pin | Name | Pin | Name |
|---|---|---|---|
| 1 | `CT_LOAD_N` | 8 | `CLK1` |
| 2 | `QC` | 9 | `QB` |
| 3 | `C` | 10 | `B` |
| 4 | `A` | 11 | `D` |
| 5 | `QA` | 12 | `QD` |
| 6 | `CLK2` | 13 | `CLR_N` |
| 7 | `GND` | 14 | `VCC` |

**Function** (TI76 p. 7-260):

- Flip-flop A toggles on a falling `CLK1`. B, C and D form a ripple ÷8 on a falling `CLK2`; `QA` wired to `CLK2` makes a ÷16.
- `CLR_N` low forces every Q low. It is asynchronous and wins.
- `CT_LOAD_N` low makes each Q follow its data input, asynchronously; that is the latch.
- Counting happens only while both `CLR_N` and `CT_LOAD_N` are high.

**Evaluation**:

- **Clear** (`CLR_N` low): all four bits 0, after `kTtl177Clear`.
- **Load** (`CT_LOAD_N` low, `CLR_N` high): each bit follows its data input (X kept), after `kTtl177Data` when a data input changed and `kTtl177Load` when `CT_LOAD_N` fell.
- **`CLK1` fell**: A toggles, after `kTtl177Clk1`.
- **`CLK2` fell**: B, C, D count as a ripple. B toggles; C toggles when B went 1 → 0; D toggles when C went 1 → 0. Each changed output is driven with its own delay from the clock: `kTtl177Qb`, `kTtl177Qc` or `kTtl177Qd`.
- **X**: X on a clock or on a control input makes the bits it could change X. `fell` sees only clean edges.

The state is `std::uint8_t count_`, plus a `known_` mask of the bits that are not X. Power-on draws 4 random bits. None of the Pins is sampled.

**Delays** (TI76 p. 7-262, typicals):

| Constant | Rise, fall (ps) | Path |
|---|---|---|
| `kTtl177Clk1` | 8'000, 11'000 | `CLK1` → `QA` |
| `kTtl177Qb` | 11'000, 17'000 | `CLK2` → `QB` |
| `kTtl177Qc` | 27'000, 34'000 | `CLK2` → `QC` |
| `kTtl177Qd` | 44'000, 50'000 | `CLK2` → `QD` |
| `kTtl177Data` | 19'000, 31'000 | data → Q |
| `kTtl177Load` | 29'000, 32'000 | load → Q |
| `kTtl177Clear` | 32'000, 32'000 | clear → Q |

### 4.3 `74100` — `src/parts/ttl100.{hpp,cpp}`

The class is `Ttl100`, the catalogue row `PartInfo{"74100", "8-bit bistable latch", "DIP-24", Chip, …}`.

- **Pins** (TI76 p. 7-113): 1 `NC1`, 2 `1D1`, 3 `1D2`, 4 `1Q2`, 5 `1Q1`, 6 `NC6`, 7 `GND`, 8 `2Q1`, 9 `2Q2`, 10 `2D2`, 11 `2D1`, 12 `2G`, 13 `NC13`, 14 `NC14`, 15 `2D3`, 16 `2D4`, 17 `2Q4`, 18 `2Q3`, 19 `1Q3`, 20 `1Q4`, 21 `1D4`, 22 `1D3`, 23 `1G`, 24 `VCC`.
- **Power**: `GND` is pin 7, so the 74100 joins `kOffCornerPower` in `catalogue_test.cpp`.
- **Logic**: each enable `nG` controls 4 latches. G high is transparent (Q = D); G low holds D's level from just before G fell (`readBefore` on the falling edge). An X on G makes Q X wherever D and the held bit differ.
- **Delays** (TI76 p. 7-114, typicals): `kTtl100Data{16'000, 14'000}`, `kTtl100Enable{16'000, 7'000}`.
- **State and Pins**: power-on draws 8 random bits. The D Pins are not sampled (the latch is transparent).

### 4.4 `7417` and `74LS21` — `gate_package.cpp`

`GatePackageSpec` gains `bool openCollector = false;` as its last field: the unit outputs are `PinKind::OpenCollector`. `PackagePins` uses it; `DriveOf` already maps 1 to no drive.

- **`7417`**: hex buffer, open collector (TI17). Pins: 1 `1A`, 2 `1Y`, 3 `2A`, 4 `2Y`, 5 `3A`, 6 `3Y`, 7 `GND`, 8 `4Y`, 9 `4A`, 10 `5Y`, 11 `5A`, 12 `6Y`, 13 `6A`, 14 `VCC` (the 74LS04's names). Units are `GateKind::Buffer`, delays `kTtl17{6'000, 20'000}` (TI17 typicals), `openCollector = true`. Catalogue: `"hex buffer, open collector"`.
- **`74LS21`**: dual 4-input AND (TI21). Pins as the 74LS20's: 1 `1A`, 2 `1B`, 3 `NC3`, 4 `1C`, 5 `1D`, 6 `1Y`, 7 `GND`, 8 `2Y`, 9 `2A`, 10 `2B`, 11 `NC11`, 12 `2C`, 13 `2D`, 14 `VCC`. Units are `And`, delays `kLs21{8'000, 10'000}`.

The `GatePackage` comment lists them. `gate_package_test.cpp` gets one row each in its truth-table cases. The 7417's 1 is checked as released: a Net with only the 7417 floats, and with a pull-up `R` it reads 1.

### 4.5 `74LS145` — `ttl145.{hpp,cpp}`

`Ttl145(std::string reference, const Delays& delays = delays::kTtl145)` stores the delays. The catalogue gets `CreateLs145` (a lambda-free function beside `CreateChip`) that passes `delays::kLs145`. The row is `PartInfo{"74LS145", "BCD-to-decimal decoder/driver, open collector", "DIP-16", Chip, &CreateLs145}`.

TI145 gives only maximums (50 ns both ways for both parts). `kLs145{25'000, 20'000}` is the same assumed typical as `kTtl145`; the commit says so. `datasheets/74LS145.md` copies `74145.md` with the LS line. `ttl145_test.cpp` runs its cases over both Parts (`GENERATE`).

## 5. `6520` — `src/parts/mos6520.{hpp,cpp}`

The class is `Mos6520`, created by `&CreateChip<Mos6520, "6520">`. The catalogue row is `PartInfo{"6520", "peripheral interface adapter (PIA)", "DIP-40", Chip, …}`. It is NMOS, so it goes in `kMosSignals` and `kOffCornerPower` (GND pin 1, VCC pin 20).

### 5.1 Pins (MOS6520 p. 1)

Index = pin − 1:

| Pins | Names |
|---|---|
| 1 | `GND` |
| 2–9 | `PA0`–`PA7` |
| 10–17 | `PB0`–`PB7` |
| 18, 19 | `CB1`, `CB2` |
| 20 | `VCC` |
| 21 | `RW` |
| 22, 23, 24 | `CS0`, `CS2_N`, `CS1` |
| 25 | `PHI2` (the sheet's φ2/E) |
| 26–33 | `D7`–`D0` |
| 34 | `RES_N` |
| 35, 36 | `RS1`, `RS0` |
| 37, 38 | `IRQB_N`, `IRQA_N` |
| 39, 40 | `CA2`, `CA1` |

All signal Pins are `Family::Mos`. Their kinds:

- `PHI2`: `Input`, not sampled. It wakes the chip on both edges (the 6530's rule).
- `RW`, `CS0`, `CS1`, `CS2_N`, `RS0`, `RS1`, `RES_N`, `CA1`, `CB1`: `Input`, sampled.
- `D0`–`D7`: `Bidirectional`, sampled.
- `PA0`–`PA7`, `CA2`: `PullUp`, sampled (R6520 p. 9: passive pull-ups, kept in input mode).
- `PB0`–`PB7`, `CB2`: `Bidirectional`, sampled (push-pull, three-state as inputs).
- `IRQA_N`, `IRQB_N`: `OpenCollector` (open drain).

The control inputs are read at each `φ2` edge. An edge on `CA1` is therefore seen within half a cycle, as the chip's own synchronisation does. The data sheet allows rise and fall times up to 1 µs, so nothing faster than a cycle matters.

### 5.2 Registers (R6520 Table 2)

The state is `or_[2]`, `ddr_[2]` and `cr_[2]` for side A and side B (bits 0–5 of `cr_`), the flags `irq1_[2]` and `irq2_[2]`, the last sampled `c1_[2]` and `c2_[2]`, the output levels `c2Out_[2]`, and `driving_`.

The register is chosen by `RS1 RS0`:

| `RS1 RS0` | Register |
|---|---|
| 00 | side A: port when `cr_[0]` bit 2 is 1, else `DDRA` |
| 01 | `CRA` |
| 10 | side B: port when `cr_[1]` bit 2 is 1, else `DDRB` |
| 11 | `CRB` |

The chip is selected when `CS0`, `CS1` are high and `CS2_N` is low.

Reads:

- port A gives the `PA` pins (`readBit`), even for output bits;
- port B gives, bit by bit, the `ORB` bit where `DDRB` is 1 and the pin where it is 0;
- `CRx` gives `irq1` in bit 7, `irq2` in bit 6 (bit 6 is 0 when `CRx` bit 5 makes `C2` an output), and bits 5–0.

Writes to `CRx` store bits 5–0; the flags are read-only.

### 5.3 Control lines (MOS6520 p. 2)

- **`C1` flag**: `CRx` bit 1 chooses the active edge (0 falling, 1 rising). The edge sets `irq1`, whatever the enable says.
- **`C2` as input** (`CRx` bit 5 is 0): bit 4 chooses the edge and sets `irq2`; bit 3 enables its IRQ.
- **IRQ outputs**: `IRQx_N` is low while (`irq1` and bit 0) or (`irq2` and bit 3 and C2 is an input).
- **Clearing**: a read of the port (not the direction register) clears both flags of its side. Writes clear nothing.
- **`C2` as output** (bit 5 is 1), by bits 4 3:

| Bits 4 3 | `CA2` | `CB2` |
|---|---|---|
| 00 handshake | low after a read of port A, at that cycle's fall; high at the next active `CA1` edge | low after a write of port B, at the next rise; high at the next active `CB1` edge |
| 01 pulse | low for one cycle after a read of port A, from that cycle's fall to the next fall | low for one cycle after a write of port B, from the next rise to the one after |
| 10 | low | low |
| 11 | high | high |

`CA2` as an output is driven 0 or released to its pull-up's 1 (`PullUp`). `CB2` is driven 0 or 1. As an input, `CB2` is `Z`.

### 5.4 Cycle

`evaluate` acts on `rose(kPhi2)` or `fell(kPhi2)`, as the 6530 does.

**Rise**:

1. Sample `CA1`, `CB1`, and the C2s that are inputs; set flags on their active edges; end a `CA2` handshake on an active `CA1` edge (and `CB2` on `CB1`).
2. `RES_N` low: reset (§5.5) and return.
3. Decode with `readBit`. On a selected read, drive the byte after `kMos6520DataOut`; a port read clears its side's flags.
4. A `CB2` pulse started at the last rise ends now; then a `CB2` write strobe pending from the last fall starts (§5.3).

**Fall**:

1. Release `D` after `kMos6520DataHold`.
2. Sample the control inputs again (step 1 of the rise).
3. A selected write (`readBitBefore`) stores its register. An `ORx` or `DDRx` write drives the port's changed Pins after `kMos6520Port`.
4. `CA2` read strobes and pulse ends, as §5.3.

**After every cycle**: `IRQA_N` and `IRQB_N` are driven when their value changes, after `kMos6520Irq`. `C2` outputs are driven when they change, after `kMos6520Control`.

Every evaluation is O(1) and allocates nothing.

### 5.5 Reset and power-on

`RES_N` low clears every register and flag (R6520 p. 9). Every port bit becomes an input: `PA` released to its pull-up, `PB` to `Z`. `CA2` and `CB2` become inputs; the IRQs are released.

Power-on draws random `OR`, `DDR` and `CR` bytes for both sides (6 × 8 draws, bit 0 first) until `RES_N`, as the 6530 does, then sets the Pins from them.

### 5.6 Delays — `delays.hpp`

The values are MOS6520 p. 4 at 1 MHz:

```cpp
inline constexpr Time kMos6520DataOut = 395'000;  // φ2 rise → D valid (tDDR, max)
inline constexpr Time kMos6520DataHold = 10'000;  // φ2 fall → D released (tHR)
inline constexpr Time kMos6520Port = 1'000'000;   // φ2 fall → PA, PB valid (tPDW, max)
inline constexpr Time kMos6520Control = 400'000;  // φ2 edge → CA2, CB2 (assumed; max 1.0 µs)
inline constexpr Time kMos6520Irq = 200'000;      // deciding φ2 edge → IRQ_N (assumed)
```

The control delay is shorter than the maximum (1.0 µs) on purpose. Drives are inertial, so a one-cycle pulse whose two edges were both 1 µs out would have its second drive land at the instant the first applies. Below a half cycle, each pulse edge applies before the next one is scheduled. The CA1-to-IRQ delay is not in the MOS or Rockwell sheets (the MC6821 gives 1.0 µs max). The commit and the datasheet file call both values assumed.

### 5.7 `datasheets/6520.md`

The datasheet file covers:

- the registers as §5.2;
- the control modes as §5.3, in plain words;
- the port electrical difference (`PA` pull-ups, `PB` three-state);
- the open-drain IRQs and reset;
- the PET's use: keyboard rows on PIA1 `PA0`–`PA3`, columns on `PB`, the retrace on `CB1`.

## 6. `6522` — `src/parts/mos6522_core.{hpp,cpp}`, `src/parts/mos6522.{hpp,cpp}`

The VIA has many interacting modes, so its logic is a core apart from the Pins, as the 6502's (ADD_PART, CPU): `Mos6522Core` holds no simulator types and is tested cycle by cycle. `Mos6522` is the Part around it.

### 6.1 Core

```cpp
// The 6522's registers and logic, one φ2 cycle at a time (P5n). No simulator types.
class Mos6522Core {
public:
    struct Inputs {          // pin levels sampled at this cycle's φ2 rise
        std::uint8_t pa, pb; // PB6 counts T2 pulses; PB7 is read as a pin like the others
        bool ca1, ca2, cb1, cb2;
    };
    struct Outputs {         // what the chip drives after this cycle
        std::uint8_t paDrive, pbDrive;  // 1 bits: drive the bit of paLevel/pbLevel
        std::uint8_t paLevel, pbLevel;
        bool ca2Drive, ca2, cb1Drive, cb1, cb2Drive, cb2;
        bool irq;            // IRQ_N low
    };
    void reset();                              // RES_N low (§6.6)
    void randomise(std::span<const std::uint8_t, 16> bytes);  // power-on
    // One cycle: inputs at the rise, an optional access, then the clocked state at the fall.
    void rise(const Inputs& in);
    std::uint8_t read(unsigned reg);           // in the rise, after rise(); side effects (§6.3)
    void write(unsigned reg, std::uint8_t v);  // at the fall, before fall()
    void fall();                               // timers, shift register, strobes (§6.4, §6.5)
    Outputs outputs() const;
    // Tests: the T1 and T2 counters and the shift register.
    std::uint16_t t1() const; std::uint16_t t2() const; std::uint8_t sr() const;
};
```

One cycle is `rise(inputs)`, then `read` (a selected read), then `write` (a selected write, at the fall), then `fall()`. `outputs()` is valid after `rise` and after `fall`.

### 6.2 Registers (R6522 Table 1)

| # | Write | Read |
|---|---|---|
| 0 | `ORB` | `IRB`: `ORB` bits where `DDRB` is 1, else the pins or the latch (`ACR` bit 1) |
| 1 | `ORA`, with handshake | `IRA`, with handshake: the pins or the latch (`ACR` bit 0) |
| 2, 3 | `DDRB`, `DDRA` | the same |
| 4 | T1 latch low | T1 counter low; clears `IFR` bit 6 |
| 5 | T1 latch high, latch → counter, clears `IFR` 6, starts T1 | T1 counter high |
| 6, 7 | T1 latch low, high (7 also clears `IFR` 6: VICE, MOS and Synertek parts) | the latches |
| 8 | T2 latch low | T2 counter low; clears `IFR` 5 |
| 9 | T2 counter high, low latch → counter, clears `IFR` 5, starts T2 | T2 counter high |
| A | `SR`; clears `IFR` 2, starts a shift | `SR`; the same |
| B, C | `ACR`, `PCR` | the same |
| D | `IFR`: each 1 bit clears that flag | `IFR`, bit 7 = any (`IFR` & `IER` & `$7F`) |
| E | `IER`: bit 7 1 sets the 1 bits, 0 clears them | `IER` with bit 7 read as 1 (R6522; MAME) |
| F | `ORA`, no handshake | `IRA`, no handshake |

The MOS sheet reads `IER` bit 7 as 0, per a VICE comment. We follow R6522 and MAME, and the datasheet file says so.

When `ACR` bit 7 is set, `PB7` is an output showing T1's level instead of `ORB` bit 7, whatever `DDRB` bit 7 says (R6522 p. 7: PB7 is an output if DDRB7 or ACR7 is 1; MAME's `output_pb`).

### 6.3 Handshakes, edges and flags (MOS6522 pp. 24–25, R6522 Figs. 29–30)

`IFR` bits: 7 IRQ, 6 T1, 5 T2, 4 `CB1`, 3 `CB2`, 2 SR, 1 `CA1`, 0 `CA2`.

- **`CA1`, `CB1` edge**: `PCR` bit 0 for `CA1`, bit 4 for `CB1` (0 falling, 1 rising). The active edge sets the flag. With `ACR` latching on, it also latches `PA` (on `CA1`) or `PB` (on `CB1`).
- **`CA2` mode** (`PCR` bits 3–1) and **`CB2` mode** (bits 7–5):

| Mode | Behaviour |
|---|---|
| 000 | input, falling edge |
| 001 | independent input, falling edge |
| 010 | input, rising edge |
| 011 | independent input, rising edge |
| 100 | handshake output: low after an access, high at the next active `C1` edge |
| 101 | pulse output: low for one cycle after an access |
| 110 | low |
| 111 | high |

- **Which access**: for `CA2` in 100 and 101, a read or a write of register 1 (not F). For `CB2`, a write of register 0. A strobe after a read starts at that cycle's fall; after a write, at the next rise (MOS6522 timing, write handshake from φ2 rise). A pulse lasts one cycle.
- **Flag clearing**: `CA1`, and `CA2` unless it is independent, clear on a read or a write of register 1; `CB1` and `CB2` the same on register 0. `IFR` bit 2 clears on an `SR` access; T1's on a read of register 4 or a write of 5 or 7; T2's on a read of register 8 or a write of 9.
- **`IRQ_N`** is low while `IFR` & `IER` & `$7F` is not 0.

### 6.4 Timers (R6522 pp. 6–7, Figs. 15–16; VICE for the cycle detail)

The write of register 5 happens in cycle `w`, at its fall. Then:

- **T1 counting**: the counter is N, and `fall()` skips the decrement in cycle `w`. Reads in the following cycles see N, N−1, …, 0. The `fall()` of the cycle that reads 0 makes it `$FFFF` and **sets `IFR` bit 6**: `IRQ_N` falls N + 1 cycles after the write's fall (N + 1.5 after its middle, as R6522 says). The next cycle reads `$FFFF` with bit 6 set.
- **`PB7`** (`ACR` bit 7): low from the fall of cycle `w` in both modes.
- **Free-running** (`ACR` bit 6): the `fall()` after the `$FFFF` cycle reloads N, so the period is N + 2 cycles. `PB7` toggles each time the flag sets.
- **One-shot**: the counter reloads from the latch too (MAME and VICE, "contrary to the datasheet"), but the flag sets only once per register 5 write. `PB7` goes high at the timeout.
- **T2 one-shot** (`ACR` bit 5 is 0): counted as T1. Register 9 loads the counter, and the flag sets once at the timeout. After that the counter keeps decrementing from `$FFFF`, with no reload.
- **T2 pulse counting** (`ACR` bit 5 is 1): T2 decrements at each rise where `PB6` reads 0 after reading 1 at the last rise. The flag sets once, on the decrement that reaches 0 (R6522: an interrupt after N pulses).

### 6.5 Shift register (R6522 pp. 11–13)

`ACR` bits 4–2 choose the mode. The register shifts MSB first. Shifting out puts `SR` bit 7 on `CB2` and rotates it into bit 0; shifting in moves `CB2` into bit 0.

| Mode | Shift | Clock | `CB1` | `CB2` |
|---|---|---|---|---|
| 0 | none (MOS behaviour; Rockwell parts shift on `CB1` here: not modelled) | — | `PCR` | `PCR` |
| 1 | in | T2 | out | in |
| 2 | in | φ2 | out | in |
| 3 | in | external `CB1` | in | in |
| 4 | out, free-running, no flag | T2 | out | out |
| 5 | out | T2 | out | out |
| 6 | out | φ2 | out | out |
| 7 | out | external `CB1` | in | out |

- **Starting**: an `SR` access starts a shift of 8 bits and clears the flag. Mode 4 runs all the time; mode 5's first bit waits for the next T2 half-period.
- **Under T2** (modes 1, 4, 5): the `CB1` half-period is T2 latch low + 2 cycles (`N + 2`). T2's low counter counts the half-period down and reloads from the low latch; it never sets `IFR` 5 in these modes.
- **Under φ2** (modes 2, 6): one bit every 2 cycles. `CB1` goes low for one cycle and high for one.
- **Edges**: an output mode changes `CB2` as `CB1` falls; an input mode samples `CB2` as `CB1` rises.
- **External clock** (modes 3, 7): `CB1` is an input. Mode 3 shifts `CB2` in at the cycle after a rising `CB1`; mode 7 puts the next bit on `CB2` when `CB1` falls.
- **End of a shift**: after 8 bits, `IFR` bit 2 sets and the `CB1` clock output stops, high. Mode 7 sets the flag every 8 bits and keeps shifting; mode 4 sets no flag.
- **In the SR modes**: `CB2` is an output in 4–7 and an input in 1–3, so the `PCR` `CB2` mode does not apply there.

**Not modelled**: the NMOS bug in modes 3 and 7, where a `CB1` edge within a few ns of φ2 falling is lost. Edges here are seen at φ2 edges only, so no edge is lost; MAME and VICE do the same. The datasheet file says so.

### 6.6 Reset and power-on

`RES_N` low clears every register except the T1 and T2 counters and latches and `SR` (R6522 p. 2). The ports become inputs, the timers stop interrupting, the shift register is off, and the interrupts are disabled.

Power-on fills, from 16 random bytes, `ORA`, `ORB`, `DDRA`, `DDRB`, `ACR`, `PCR`, the T1 counter and latch (4 bytes), the T2 counter and latch (3 bytes), `SR`, `IFR` and `IER`. The timers are then counting. Real parts power up undefined until `RES_N` (VICE: the latches "do not contain 0 at powerup").

### 6.7 Part — `mos6522.{hpp,cpp}`

The class is `Mos6522`, created by `&CreateChip<Mos6522, "6522">`. The catalogue row is `PartInfo{"6522", "versatile interface adapter (VIA)", "DIP-40", Chip, …}`. It goes in `kMosSignals` and `kOffCornerPower`.

**Pins** (R6522 p. 1):

| Pins | Names |
|---|---|
| 1 | `GND` |
| 2–9 | `PA0`–`PA7` |
| 10–17 | `PB0`–`PB7` |
| 18, 19 | `CB1`, `CB2` |
| 20 | `VCC` |
| 21 | `IRQ_N` |
| 22 | `RW` |
| 23, 24 | `CS2_N`, `CS1` |
| 25 | `PHI2` |
| 26–33 | `D7`–`D0` |
| 34 | `RES_N` |
| 35–38 | `RS3`–`RS0` |
| 39, 40 | `CA2`, `CA1` |

**Kinds**:

- `PA0`–`PA7`, `PB0`–`PB7`, `CA2`, `CB1`, `CB2`: `PullUp`, sampled. RVD: the NMOS R6522's PB and `CB1` have passive pull-ups too, and MOS6522 lists the same input current for PA and PB.
- `CA1`, `RW`, `CS1`, `CS2_N`, `RS0`–`RS3`, `RES_N`: `Input`, sampled.
- `D0`–`D7`: `Bidirectional`, sampled.
- `IRQ_N`: `OpenCollector`.
- `PHI2`: not sampled.

A `PullUp` Pin drives 0 strongly and 1 through its pull-up, so a PB output's 1 is weaker than the chip's real one. Nothing in the BW-2001 needs a stronger 1, and the datasheet file says so.

A pin the core does not drive (a `0` bit of `paDrive`, `pbDrive`, or `cb1Drive` false) is driven `One`, its pull-up's weak 1, as the 6530's ports: never `Z`.

**Cycle**, the 6520's (§5.4), but the inputs are sampled at the rise only (the core takes `Inputs` in `rise()`), with:

- **Rise**: sample `Inputs` with `readBit`; `core_.rise(in)`; `RES_N` low → `core_.reset()`; a selected read drives `core_.read(reg)` after `kMos6522DataOut`.
- **Fall**: release `D`; a selected write → `core_.write(reg, byte)`; then `core_.fall()`.
- **After each edge**: compare `core_.outputs()` with the last driven and drive only what changed:
  - port bits after `kMos6522Port`, except `PB7` from T1, which comes after `kMos6522Shift`;
  - `CB1` and `CB2` from the shift register after `kMos6522Shift`;
  - `CA2` and `CB2` strobes and manual levels after `kMos6522Control`;
  - `IRQ_N` after `kMos6522Irq`.

The chip is selected when `CS1` is high and `CS2_N` low, and the register comes from `RS3`–`RS0`. Every evaluation is O(1) and allocates nothing.

**Delays** (MOS6522 p. 22, 1 MHz):

```cpp
inline constexpr Time kMos6522DataOut = 395'000;  // φ2 rise → D valid (max)
inline constexpr Time kMos6522DataHold = 10'000;  // φ2 fall → D released
inline constexpr Time kMos6522Port = 1'000'000;   // φ2 fall → PA, PB valid (max)
inline constexpr Time kMos6522Shift = 300'000;    // φ2 fall → CB2 shift data (max); CB1, PB7 too
inline constexpr Time kMos6522Control = 400'000;  // CA2, CB2 strobes (assumed; max 1.0 µs)
inline constexpr Time kMos6522Irq = 200'000;      // deciding φ2 edge → IRQ_N (assumed)
```

The control delay stays under half a cycle for the reason in §5.6. `kMos6522Shift` is the data sheet's 300 ns; `CB1` and `PB7` reuse it (assumed).

### 6.8 `datasheets/6522.md`

The datasheet file covers:

- the registers;
- the timers, with the N, …, 0, `$FFFF` rule and the N + 2 period;
- `PB7` and `PB6`;
- the shift register modes;
- the handshakes and the flag rules;
- reset;
- what is assumed (the delays, `IER` bit 7, mode 0, the CB1 bug not modelled);
- the PET's use: user port on `PA`, IEEE and cassette lines on `PB`, the retrace on `PB5`, the character set on `CA2`.

## 7. Bench — `tests/data/bw2001_bench.bwn`, `roms/bw2001_bench.{asm,rom}`

The bench is the PET's bus at its own reference designators where it has them (PET), and `U5x` for bench-only glue. There is no Board and no video.

### 7.1 Chips

| Ref | Part | Role |
|---|---|---|
| X1 | `OSC freq=1MHz` | `PHI0` (the PET divides 8 MHz: P5q) |
| X2 | `OSC freq=60Hz` | `RETRACE`, a stand-in for P5q's video (the 6502's own `SYNC` pin stays open) |
| UF3 | `6502` | |
| UG2 | `74LS154` | `A`–`D` = `A12`–`A15`, `G1_N` and `G2_N` on `GND`: `SEL0_N`–`SELF_N` |
| UE5 | `74LS04` | `RW_N`, `SELE`, `A15_N` |
| UG3 | `74LS00` | `WE_N` = NAND(`RW_N`, `PHI2`); `IO_N` = NAND(`SELE`, `A11`) |
| U50 | `74LS138` | RAM 1 KB blocks: `A`–`C` = `A10`–`A12`, `G2A_N` = `A13`, `G2B_N` = `A14`, `G1` = `A15_N`; `Y0_N`–`Y7_N` = `RAM0_N`–`RAM7_N` |
| UI1–UI8, UJ1–UJ8 | `2114` | `$0000`–`$1FFF`: UI*n* `IO1`–`IO4` on `D0`–`D3`, UJ*n* on `D4`–`D7`, `CS_N` = `RAM`*n−1*`_N` |
| UC3, UC4 | `2114` | video RAM `$8000`–`$83FF` (mirrored to `$8FFF`), `IO1`–`IO4` on `D0`–`D3` and `D4`–`D7`, `CS_N` = `SEL8_N` |
| UH1 … UH7 | `2316B` | `CS1` = `SELx_N`, `CS2` = `A11`, `CS3` = `VCC`; lower halves `select=LLH`, upper halves `select=LHH` (§7.2) |
| UG8 | `6520` | PIA1 `$E810`: `CS0` = `A4`, `CS1` = `VCC`, `CS2_N` = `IO_N`, `RS0`/`RS1` = `A0`/`A1` |
| UB8 | `6520` | PIA2 `$E820`: `CS0` = `A5`, `CS1` = `VCC`, `CS2_N` = `IO_N`, `RS0`/`RS1` = `A0`/`A1`; result byte on `PB` |
| UA5 | `6522` | VIA `$E840`: `CS1` = `A6`, `CS2_N` = `IO_N`, `RS0`–`RS3` = `A0`–`A3`; `PB5` = `RETRACE` |
| UG9 | `74LS145` | keyboard rows from PIA1 `PA0`–`PA3` |
| SW1, SW2 | `KEY` | row 3 (`Y3_N`) to PIA1 `PB5`; row 8 (`Y8_N`) to `PB0` |
| R18–R25 | `R value=10k` | pull-ups on PIA1 `PB0`–`PB7` (PET) |
| U51 | `POR time=1ms` | `RES_N` (the PET's 555) |
| R1–R3 | `R value=3k3` | `RES_N`, `IRQ_N`, `NMI_N` |
| J1 | `POWER` | |

**Wiring**:

- `PHI2` is the 6502's output; it goes to both PIAs, the VIA and UG3.
- `IRQ_N` joins the 6502, `IRQA_N` and `IRQB_N` of both PIAs, and the VIA's `IRQ_N`.
- All 18 2114s take `A0`–`A9` and `WE_N`.
- `RETRACE` goes to PIA1 `CB1` and VIA `PB5`, as the PET's retrace does (PET).
- For the 1 ms before `RES_N` rises, random power-on directions may drive VIA `PB5` against X2, or PIA ports against the keys: the Nets read X until the reset. Harmless; said here so nobody chases it in a VCD.
- `RDY` and `SO` are on `VCC`.
- Unused TTL inputs are tied, so the bench loads without warnings. Unused outputs stay open.
- There are no data buffers: P5q brings the PET's 74LS244s.

### 7.2 ROM sockets

| Socket | Address | `select=` | Contents |
|---|---|---|---|
| UH1 | `$C000` | `LLH` | `empty.rom` |
| UH5 | `$C800` | `LHH` | `empty.rom` |
| UH2 | `$D000` | `LLH` | `empty.rom` |
| UH6 | `$D800` | `LHH` | `empty.rom` |
| UH3 | `$E000` | `LLH` | `empty.rom` |
| UH4 | `$F000` | `LLH` | `empty.rom` |
| UH7 | `$F800` | `LHH` | `../../roms/bw2001_bench.rom` |

`empty.rom` is `tests/data/empty.rom`, all `$FF`. UH3's `CS2` low keeps it out of `$E800`–`$EFFF`, where `IO_N` selects the I/O.

The boot test (§7.4) replaces the seven contents with the files of §3. It does this in the parsed `Netlist` before `ResolveContentsPaths`, with absolute paths.

### 7.3 Program — `roms/bw2001_bench.asm`

The file starts with `.rom $F800, $800`, has a header comment as `bw1_bench.asm`, and is kept equal to its `.rom` by an `asm_bw2001_bench` ctest. The result byte goes on PIA2 `PB` (`DDRB` = `$FF`): `$01` RAM, `$02` video RAM, `$55` pass.

1. **`reset`**: `sei`, `cld`, `ldx #$FF`, `txs`. Then PIA2 `CRB` = 0, `DDRB` = `$FF`, `CRB` = `$04`, `ORB` = `$00`; PIA2 `CRA` = 0, `DDRA` = `$FF`, `CRA` = `$3C` (`CA2` high).
2. **RAM test**, writing `X ^ page ^ $A5` then reading it back:
   - page 0 by `abs,X`, so no RAM is in use while it is tested;
   - pages `$01`–`$1F` through a pointer at `$00`/`$01` (page 0 already tested);
   - a mismatch sends `$01` to PIA2 `PB` and stops (`jmp *`).
3. **Video RAM test**: `$8000`–`$83FF` the same way; a mismatch sends `$02`.
4. **PIA1**: `DDRA` = `$0F`, `CRA` = `$04`, `PA` = 3 (row 3). `CRB` = `$05`: `ORB` selected, `CB1` IRQ on the falling edge.
5. **VIA**:
   - `DDRA` = `$FF` (the jiffy count) and `DDRB` = `$80` (`PB7`, the T1 output);
   - `ACR` = `$D0`: T1 free-running on `PB7`, `SR` mode 4 under T2;
   - T2 latch low = 8 (`CB1` half-period 10 cycles), `SR` = `$0F`;
   - T1 latch = 998, written low then high: half-period 1000 cycles, square wave 2 ms;
   - `IER` = `$C0` (T1).
6. **Pass**: `$55` to PIA2 `PB`, then `cli`.
7. **Main loop**, forever (PIA2 `DDRA` = `$FF` set at step 1):
   - PIA1 `PA` = 3 (row 3), then copy PIA1 `PB` to PIA2 `PA`, so `SW1` shows on PIA2 `PA5`;
   - PIA1 `PA` = 8 (row 8), then read PIA1 `PB`; bit 0 (`SW2`) sets PIA2 `CA2` by writing PIA2 `CRA` as manual high (`$3C`) or manual low (`$34`).
8. **`irq`**:
   - `pha`;
   - `bit` VIA `IFR`; if bit 6 is set, `lda` register 4 (clears T1);
   - `lda` PIA1 `CRB`; if bit 7 is set, `lda` PIA1 `ORB` (clears the flag), `inc JIFFY`, copy `JIFFY` to VIA `ORA` (each `RETRACE` fall);
   - `pla`, `rti`.
9. **`nmi`**: `rti`. The vectors are at `$FFFA`.

### 7.4 Tests — `tests/netlist/bw2001_bench_test.cpp` (`[slow]`, in `tests/CMakeLists.txt`)

The `Bench` fixture is the P5j one: the netlist is parsed, `ResolveContentsPaths` applied, built with `cells`, and checked to load with no warnings. In a debug build the bench runs some 20–30 simulated ms a second, so the program's checks are **one run per seed** (seeds 1–3), all in one test case as `bw1_bench_test` does:

- **Pass**: PIA2 `PB` reads `$55` by `kDone`. The RAM loops are 36 pages × 256 × about 28 cycles ≈ 260 ms; `kDone` = 300 ms, the measured time in the plan's notes.
- **T1**: VIA `PB7` toggles every 1000 µs (± 1 µs), 5 times after `kDone`; each toggle is followed within 2 µs by an `IRQ_N` fall (the T1 interrupt), and `IRQ_N` is high again within 30 µs (the handler).
- **Shift register**: VIA `CB1` toggles every 10 µs; `CB2` shows `$0F` rotating (4 bits low, 4 high, every 160 µs).
- **Retrace**: VIA `PA` (the jiffy count) steps by one at each `RETRACE` fall: three steps in the 55 ms after `kDone`.
- **Keys**: `SW1` held → PIA2 `PA5` 0 within 200 µs, released → 1. `SW2` held → PIA2 `CA2` 0 within 200 µs.

The stuck RAM cells run on seed 1 only, each to `kDone`: `StuckCell{"UI5", IO1, $123, 0}` (the pattern writes `$97` there) gives `$01`; `StuckCell{"UC4", IO4, $010, 1}` (pattern `$25`) gives `$02`.

The BASIC 1 case (seed 1) uses `ExternalRom` for the seven files and SKIPs without them:

- it runs to 600 ms;
- it decodes video RAM, `UC3.word(a) | UC4.word(a) << 4` for `$8000` + `a`, as PET screen codes (`$01`–`$1A` letters, `$20`–`$3F` as ASCII);
- row 0 must be `*** COMMODORE BASIC ***`, row 2 ` 7167 BYTES FREE`, row 4 `READY.`;
- on failure it prints the 25 rows (INFO).

The facts it rests on:

- the banner and `READY.` come from 901447-05 and -01: `$E174`, `$C299`;
- every screen write waits for VIA `PB5` low (`$E7AD`), which X2 supplies;
- RAM sizing from `$0401` stops at `$2000`, where nothing answers: the bus there holds the last byte the CPU read, the pointer's high byte, never the `$92` and `$24` it tests;
- the boot takes about 0.33 s.

### 7.5 `tests/data/pet_roms.md`

The table of §3, how to place the files (copy or symlink to `external/roms/pet/`), and the test that uses them.

## 8. Docs

- [ADD_PART.md](../recipes/ADD_PART.md):
  - reference commits for the 2316B (mask-set select polarities), 74LS154, 74177 (falling-edge ripple counter with an asynchronous transparent load), 74100, 7417 and 74LS21 (an open-collector gate package), 74LS145 (a delays variant sharing code), 6520, and 6522 (a peripheral with its logic in a core);
  - Which kind, ROM: `select=`; contents: `.bin` raw dumps;
  - rules:
    - a real ROM in tests through `ExternalRom`, SKIPped without it, never committed;
    - a Pin that pulses for one cycle needs a delay under half a cycle, because drives are inertial.
- [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md): the cell Fault chip list gains the 2316B.
- [ARCHITECTURE.md](../ARCHITECTURE.md): the P5n row done, with links.

## 9. Tests (unit)

Each new file is added to `tests/CMakeLists.txt`, sorted.

- **`rom_contents_test.cpp`**: a `.bin` of 2048 bytes is read byte for byte (written by the test into a temporary directory); 2047 bytes; width 5; an uppercase `.BIN`.
- **`rom_test.cpp`**, 2316B:
  - for each of the 8 `select=` values, the outputs drive only at that combination of `CS1`–`CS3` and float otherwise;
  - `k2316Access`, `k2316Select`, `k2316Float`;
  - an X on a select gives X;
  - the parameter errors of §2.3;
  - a stuck cell.
- **`ls154_test.cpp`**: the 16 outputs, each enable, X folding over the selects, an X enable, the two delays.
- **`ttl177_test.cpp`**:
  - ÷2 on `CLK1`, ÷8 on `CLK2`, ÷16 with `QA` wired to `CLK2`;
  - the ripple delays in order (QB before QC before QD on 0111 → 1000);
  - clear over load over count, and the load transparent while low;
  - an X clock;
  - the random power-on.
- **`ttl100_test.cpp`**: transparent, hold (the D before G fell), the two halves independent, an X enable, the delays.
- **`gate_package_test.cpp`**: the 7417 (released 1, pull-up) and the 74LS21.
- **`ttl145_test.cpp`**: both Parts by `GENERATE`, each with its delays.
- **`mos6520_test.cpp`**: a bench of one 6520, with a `Script` of bus cycles at 1 MHz (`mos6530_test.cpp`'s helper, copied: `PHI2` from a `Stimulus`, address and control 100 ns after each fall, write data from 150 ns after the rise). Each port and control Pin gets its own Net with an optional outside `Stimulus`. Cases:
  - the pins, the catalogue row, and every signal Pin sampled but `PHI2`; the datasheet file's pin ranges ascending (`D0`–`D7`, `RS0`–`RS3`: the manual check expands only ascending ranges);
  - `DDRA`/port A selection by `CRA` bit 2, and the same for B;
  - a port A read gives the pins even for output bits; a port B read gives the latch for output bits;
  - `PA` input alone reads 1 (pull-up); `PB` input alone floats;
  - `CA1` falling and rising per `CRA` bit 1 set `CRA` bit 7; `IRQA_N` low only with bit 0; a port A read clears the flag, a `DDRA` read does not;
  - `CA2` as input sets bit 6, with its IRQ by bit 3;
  - `CA2` handshake: low `kMos6520Control` after the port A read's fall, high after the next active `CA1` edge;
  - `CA2` pulse: one cycle low; `CB2` handshake and pulse after a port B write, from the next rise;
  - manual levels;
  - `RES_N`: every register 0, the Pins released;
  - side B's IRQ independent of side A's;
  - each rule checked to fail on a mutation (the plan's notes list them).
- **`mos6522_core_test.cpp`**: the core alone, cycle by cycle (`rise`, `read`/`write`, `fall`):
  - every register read and write;
  - T1 one-shot: N = 3 reads 3, 2, 1, 0, `$FFFF`, with `IFR` 6 set in the `$FFFF` cycle and the IRQ out after the `fall()` of the 0 cycle; no second flag; a register 4 read clears it;
  - T1 free-running: N = 3 has a period of 5 cycles; `PB7` low from the write, then toggling each period; `PB7` follows T1 with `DDRB` bit 7 at 0;
  - one-shot `PB7`: low from the write, high at the timeout;
  - a latch-only write changes the next period;
  - T2 one-shot, and T2 counting 3 `PB6` falls then flagging;
  - `SR`: `CB1` and `CB2` directions per mode (§6.5's table); modes 2 and 6 at 2 cycles a bit; 1, 4 and 5 at 2 × (N + 2); 3 and 7 on outside `CB1` edges; `IFR` 2 after 8 bits; mode 4 recirculating with no flag; mode 0 not shifting;
  - latching on `CA1` (`ACR` 0) and `CB1` (`ACR` 1);
  - each `PCR` mode of `CA2` and `CB2`; the independent modes not cleared by an `ORA` access; register F's access neither clearing nor strobing;
  - `IFR` write-1-to-clear, `IFR` bit 7, `IER` set and clear, `IER` read bit 7 = 1;
  - reset keeping the T1, T2 and `SR` values;
  - each rule checked to fail on a mutation.
- **`mos6522_test.cpp`**: the Part on a bench like the 6520's:
  - the pins, the catalogue row, the sampled Pins;
  - a register read at `kMos6522DataOut` and released at `kMos6522DataHold`;
  - a port write at `kMos6522Port`;
  - `PB7` from T1 at `kMos6522Shift`;
  - `IRQ_N` low then released;
  - `CB1` and `CB2` driving and releasing as the SR mode changes;
  - `RES_N`.

## 10. Release check

- `bw run tests/data/bw2001_bench.bwn --time 400ms` headless ratio, and the BASIC 1 boot's (a copy of the bench with the external contents, in the scratchpad, not committed), recorded in the plan's notes.
- `led_computer.bwn`, `bus_load.bwn` and `bw1.bwn` headless ratios, median of 3, before phase 1 and after phase 6: within noise. Phase 1 adds an argument to the ROM's hot path.
- There is no UI change. A `bw_ui` start of the 080 (2716 and 2513 ROMs) with `--screenshot` is taken and viewed: its text is unchanged.

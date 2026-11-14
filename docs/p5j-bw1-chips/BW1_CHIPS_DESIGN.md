# P5j BW-1 chips — Detailed Design

Implements [BW1_CHIPS.md](BW1_CHIPS.md): ROM and RAM blocks under the 2716, 2513 and 2114, the memory keyword in cell Faults, a pull-up Pin kind, the `6530`, `6102` and `74125` Parts, and a bench. No Machine, no UI change.

Recipes: [ADD_PART.md](../recipes/ADD_PART.md) (Files table, rules), [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md) (Fault lines). All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md). Sources: the MCS6530 preliminary data sheet (MOS, August 1975, `6502.org/documents/datasheets/mos/mos_6530_rriot_preliminary_aug_1975.pdf`, cited "DS p. N", the PDF's page), the KIM-1 User Manual (`6502.org/documents/books/kim1_user_manual.pdf`, cited "KIM Fig. N").

---

## 1. Memory blocks — `src/parts/memory_block.{hpp,cpp}` (new)

```cpp
// The cells of a ROM: words of up to 8 bits, from a contents file. A stuck cell changes them.
class RomBlock {
public:
    explicit RomBlock(std::vector<std::uint8_t> words);
    std::size_t size() const { return words_.size(); }
    std::uint8_t operator[](std::size_t word) const { return words_[word]; }
    std::span<const std::uint8_t> words() const { return words_; }
    void stick(std::size_t word, unsigned bit, bool one);  // Check()s word < size(), bit < 8

private:
    std::vector<std::uint8_t> words_;
};

// The cells of a static RAM of `width` bits a word: random at power-on; a stuck bit is forced
// on every store and at power-on.
class RamBlock {
public:
    RamBlock(std::size_t size, unsigned width);
    void powerOn(PowerOnContext& ctx);  // `width` draws a word, bit 0 first, word 0 first
    std::size_t size() const { return words_.size(); }
    std::uint8_t operator[](std::size_t word) const { return words_[word]; }
    void store(std::size_t word, unsigned data);            // masked to `width` bits
    void stick(std::size_t word, unsigned bit, bool one);   // Check()s; forces the word at once

private:
    std::vector<std::uint8_t> words_, stuckMask_, stuckValue_;
    unsigned width_;
};
```

- `RamBlock::powerOn` is the 2114's loop today (for each word, `width` draws ORed in from bit 0, then `store` of every word), so a 2114 draws the same bits in the same order: every seed's power-on state is unchanged (the 110's Blackout brief, P5g's tests).
- `store`: `words_[w] = ((data & ~stuckMask_[w]) | stuckValue_[w]) & ((1u << width_) - 1)`, parenthesised (`-Wbitwise-op-parentheses`).
- The vectors are allocated once at construction; `store` and `operator[]` do not allocate (hot path).
- `src/parts/CMakeLists.txt`: `memory_block.cpp`.

### 1.1 Users

- `Rom` (`rom.{hpp,cpp}`): `words_` becomes `RomBlock block_`; `evaluate` passes `block_.words()` to `spec_.evaluate` (`EvaluateRom` unchanged); `size()` → `block_.size()`; `stick` → `block_.stick`. The constructor's `Check` on the word count stays.
- `Ram2114`: `words_`, `stuckMask_`, `stuckValue_` become `RamBlock cells_{1024, 4}`; `powerOn` → `cells_.powerOn(ctx)`; `store` → `cells_.store`; `word(a)` → `cells_[a]`.

### 1.2 `MemoryCells` — `memory_cells.hpp`

A chip may hold more than one memory:

```cpp
class MemoryCells {
public:
    virtual ~MemoryCells() = default;
    // Its memories' names: {""} for a chip with one (2114, ROMs, 6102); {"rom", "ram"} (6530).
    virtual std::span<const std::string_view> memories() const;
    virtual std::size_t wordCount(std::size_t memory) const = 0;
    // Bit of a word stored for data Pin `pin`; nullopt if `pin` is not a data Pin.
    virtual std::optional<unsigned> dataBit(PinIndex pin) const = 0;
    // From now on bit `bit` of word `word` of memory `memory` reads `one`.
    virtual void stick(std::size_t memory, std::size_t word, unsigned bit, bool one) = 0;
};
```

`memories()` has a default in the header returning a `static constexpr std::array<std::string_view, 1>{""}`. The 2114 and `Rom` overrides take `memory` and `Check` it is 0.

## 2. Memory keyword in cell Faults

### 2.1 Puzzle — `src/machine/puzzle.{hpp,cpp}`

`CellDecl` gains `std::string memory = {};  // "" when not written` as its **last** field (existing initialisers keep compiling).

- `fault cell REF.PIN [MEMORY] WORD 0|1`: 5 or 6 tokens; with 6, `tokens[3]` is the memory: one or more lowercase letters, else the syntax error. Error text `"expected: fault cell REF.PIN [MEMORY] WORD 0|1"`.
- `fault column REF.PIN [MEMORY] 0|1`: 4 or 5 tokens; error `"expected: fault column REF.PIN [MEMORY] 0|1"`.
- `bad word '…'` unchanged.

### 2.2 Builder — `src/netlist/builder.hpp`

`StuckCell` gains `std::size_t memory = 0;` as its **last** field (existing `{reference, pin, word, value}` initialisers keep compiling). `stickCells` checks `memory < memories().size()` and `word < wordCount(memory)`, then calls `stick(memory, …)`.

### 2.3 Machine — `src/machine/machine.{hpp,cpp}`

The per-cell checks of `LoadMachine` move into a free function, so they can be tested on a 6530 before a Machine has one:

```cpp
// A cell or column Fault on `component` (Part `part`, "6530"), checked: the StuckCell, or the
// error text after "cell U2.D3 ram $3F: " / "column U2.D0 rom: ".
std::expected<StuckCell, std::string> ResolveCell(const CellDecl& decl, const Component& component,
                                                  std::string_view part);
```

`LoadMachine` keeps `known()` and the Component lookup, then calls it and reports `written + ": " + error`. `written` is `"cell U2.D3 ram $3F"` or `"column U2.D0 rom"` (the memory only when given). Errors, in order:

| Case | Text |
|---|---|
| not `MemoryCells` | `RAM and ROM chips only` (as today) |
| not a data Pin | `not a data pin` (as today) |
| several memories, none given | `U2 (6530) has memories rom and ram: name one` |
| a name the chip lacks | `U2 (6530) has no memory 'xyz'` |
| a name on a chip with one | `U3 (2114) has one memory: no name` |
| word out of range | `U3 (2114) has 1024 words` / `U2 (6530) ram has 64 words` |

The reference in the texts is `decl.pin.reference`. Swap and `build()` are unchanged.

## 3. Pull-up Pin — `src/sim/`

### 3.1 `logic.hpp`

```cpp
// PullUp: an NMOS port pin (6530): drives 0 Strong, 1 Weak (its own pull-up); reads its Net.
enum class PinKind : std::uint8_t { Input, Output, OpenCollector, Bidirectional, Power, NoConnect,
                                    Switched, PullUp };
```

- `IsDriver` and `IsReader`: true for `PullUp`.
- `DriveOf(PullUp, family, v)`: `Z` → `{Z, None}`; `One` → `{One, Weak}`; `Zero`, `X` → `{v, Strong}`. A pull-up Pin and a resistor pull-up are the same Weak 1: a key to an open-collector 0, a TTL output or a stuck tie beats it; nothing else on the Net reads 1, not floating (S2 table, `Resolver`).

### 3.2 `simulator.cpp` and `circuit.cpp`

- `powerOn`: `PullUp` joins `Bidirectional` and `Switched`: released (`Z`) until the Part's `powerOn` or first evaluation, **no random draw**, so adding the kind changes no existing seed.
- `Circuit::buildTables`: `netSingleDriver_` stays for `Output` only (a `PullUp` Net has resolution work to do).

### 3.3 Builder — `builder.cpp`

The stuck-Pin tie for a reader (§ "A stuck bidirectional Pin forces its Net…"): `Supply` for `Bidirectional` **and** `PullUp` (the chip's own strong 0 must not fight a stuck 1 into X), `Strong` otherwise. `warnUnconnectedInputs` is unchanged (`PullUp` Pins are MOS).

No other `PinKind` switch exists outside `logic.hpp` (`grep "case PinKind"`).

## 4. `6102` — `src/parts/ram6102.{hpp,cpp}`

`class Ram6102 final : public Component, public MemoryCells`, `&CreateChip<Ram6102, "6102">`, `PartInfo{"6102", "1K x 1 static RAM", "DIP-16", Chip, …}`.

- Pinout (KIM Fig. 3.4: address pins, `R/W` 3, `DIN` 11, `DOUT` 12, `CE` 13, `VSS` 9, `VCC` 10): 1 `A6`, 2 `A5`, 3 `RW`, 4 `A1`, 5 `A2`, 6 `A3`, 7 `A4`, 8 `A0`, 9 `GND`, 10 `VCC`, 11 `DIN`, 12 `DOUT`, 13 `CE_N`, 14 `A9`, 15 `A8`, 16 `A7`. `RW` as the 6502's (`R/W`, high reads). Off-corner power: `kOffCornerPower`.
- `kRw = 2, kDin = 10, kDout = 11, kCeN = 12`; `kAddress{7, 3, 4, 5, 6, 1, 0, 15, 14, 13}` (`A0`..`A9`).
- Pins MOS: inputs `Input`, `DIN` `Input` with `sampled = true` (read only when a write is in progress or ends, at a wake by another Pin), `DOUT` `Output` (tri-state through `Z`). Catalogue test: `kMosSignals`.
- `RamBlock cells_{1024, 1}`; `powerOn`: `cells_.powerOn(ctx)`, `setOutput(kDout, Z)`.
- `evaluate`, the 2114's with one bit: the write held just before now (`CE_N`, `RW` low before: `store` at the address and `DIN` before); `selected = !readBit(CE_N)`, `writing = selected && !readBit(RW)`; writing stores `readBit(DIN)`; deselected or writing: `DOUT` `Z` after `k6102Off`; else the bit after `k6102Access` (an address Pin changed), `k6102Select` (`CE_N` or `RW` changed, the longer of the two if both), `k6102Access` otherwise.
- Assumptions, said in the datasheet file and the commit: `DOUT` floats while writing (as the 2114; the KIM buffers `DOUT` and enables the buffer only on reads, so the two choices look the same there). `delays.hpp`: `k6102Access = 500'000` (the KIM parts list: "500ns Acc"), `k6102Select = 500'000` (chip enable to output: the 2102 family's equals its access time, assumed), `k6102Off = 100'000` (as the 2114's, assumed). Maximums.
- `MemoryCells`: one memory of 1024 words; `dataBit(DIN)` and `dataBit(DOUT)` are 0.
- `datasheets/6102.md`: function in plain words (MOS's 2102 equivalent; KIM use), pins.

## 5. `74125` — `src/parts/ttl125.{hpp,cpp}`

`class Ttl125 final : public Component`, `&CreateChip<Ttl125, "74125">`, `PartInfo{"74125", "quad bus buffer, tri-state", "DIP-14", Chip, …}`; plain TTL as the KIM's U13, U14 (Appendix A), named like `Ttl145`.

- Pinout (TI): 1 `1OE_N`, 2 `1A`, 3 `1Y`, 4 `2OE_N`, 5 `2A`, 6 `2Y`, 7 `GND`, 8 `3Y`, 9 `3A`, 10 `3OE_N`, 11 `4Y`, 12 `4A`, 13 `4OE_N`, 14 `VCC` (KIM Fig. 3.4: 9→8 enabled by 10, 12→11 by 13, 5→6 by 4, 2→3 by 1). Enables as the 74LS244's `1OE_N`.
- `evaluate`: the 74LS244's per buffer (`a52b167`), each with its own enable: `OE_N` 1 → `Z`, 0 → `A` (X → X), X → X; delay by the transition (`driven_[i]` Z → enable, to Z → disable, else data). `powerOn`: outputs `Z`.
- `delays.hpp`: `kTtl125Data{8'000, 12'000}`, `kTtl125Enable{14'000, 16'000}`, `kTtl125Disable{5'000, 7'000}` (rise = to or from 1, as `kLs244*`): assumed typicals, the TI sheet not fetched (recipe), said in the commit.
- `datasheets/74125.md`.

## 6. `6530` — `src/parts/mos6530.{hpp,cpp}`

### 6.1 Masks and catalogue

```cpp
// A KIM-1 mask (BW1_CHIPS.md, "The 6530 as the KIM uses it"): ROM while RS0 is low; RAM and I/O
// while RS0 is high, CS1 low and A9-A6 equal `ram` or `io`.
struct Mos6530Mask {
    std::string_view name;  // "002"
    unsigned ram;           // A9-A6: 002 0b1111 ($17C0), 003 0b1110 ($1780)
    unsigned io;            // A9-A6: 002 0b1101 ($1740), 003 0b1100 ($1700)
};
const Mos6530Mask* FindMos6530Mask(std::string_view name);  // nullptr if unknown
```

`PartInfo{"6530", "ROM, RAM, I/O and timer (KIM mask 002 or 003)", "DIP-40", Chip, &CreateMos6530}`. `CreateMos6530` (catalogue.cpp, after `CreateRom`'s shape): `mask=` and `contents=` both required; errors `"6530 needs mask=002|003"`, `"6530: unknown mask '004' (002|003)"`, `"6530 needs contents=<file>"`, any other name `"6530: unknown parameter 'freq'"` (the ROMs' style); contents by `ReadRomContents(path, 1024, 8)`. `ResolveContentsPaths` already resolves `contents=`.

`PartPinNames` (manual.cpp) returns the names of `Mos6530::Pins()` for `"6530"` (it cannot build one without a file), as it does for ROMs; `tests/manual/manual_test.cpp` checks it has 40 names (recipe: a Part with a required parameter). `catalogue_test` passes `mask=002` and `tests/data/empty.rom`; `kMosSignals` and `kOffCornerPower` gain `6530` (GND pin 1, VCC pin 20). The placer test is unaffected (DIP-40 exists; a 6530 needs a placement rule, as the 6502).

### 6.2 Pins

Index = pin − 1 (DS p. 12): 0 `GND`, 1 `PA0`, 2 `PHI2`, 3 `RS0`, 4 `A9`, 5 `A8`, 6 `A7`, 7 `A6`, 8 `RW`, 9 `A5`, 10 `A4`, 11 `A3`, 12 `A2`, 13 `A1`, 14 `A0`, 15 `RES_N`, 16 `PB7`, 17 `CS1`, 18 `PB5`, 19 `VCC`, 20 `PB4`, 21 `PB3`, 22 `PB2`, 23 `PB1`, 24 `PB0`, 25 `D7` … 32 `D0`, 33 `PA7`, 34 `PA6` … 39 `PA1`.

Pin 18 is named `CS1` and pin 19 `PB5`: on both KIM masks pin 18 is a chip select and pin 19 a port pin, so the names do not depend on the mask. The datasheet file says the pin is `PB6` on other masks.

```cpp
static constexpr PinIndex kPhi2 = 2, kRs0 = 3, kRw = 8, kResN = 15, kPb7 = 16, kCs1 = 17;
static constexpr std::array<PinIndex, 10> kAddress{14, 13, 12, 11, 10, 9, 7, 6, 5, 4};  // A0..
static constexpr std::array<PinIndex, 8> kData{32, 31, 30, 29, 28, 27, 26, 25};          // D0..
static constexpr std::array<PinIndex, 8> kPortA{1, 39, 38, 37, 36, 35, 34, 33};          // PA0..
static constexpr PinIndex kNoPin = 0xFFFF;
static constexpr std::array<PinIndex, 8> kPortB{24, 23, 22, 21, 20, 18, kNoPin, 16};  // PB0..PB7
```

All signal Pins MOS. `PHI2`: `Input`, not sampled, both edges. `RS0`, `CS1`, `RW`, `RES_N`, `A0`–`A9`: `Input`, `sampled`. `D0`–`D7`: `Bidirectional`, `sampled`. `PA0`–`PA7`, `PB0`–`PB5`, `PB7`: `PullUp`, `sampled` (port inputs are read at the `φ2` rise, TPCR). Only `PHI2` wakes it, as the 6502's `PHI0` (ADD_PART, CPU).

### 6.3 State

```cpp
const Mos6530Mask& mask_;
RomBlock rom_;                       // 1024 x 8
RamBlock ram_{64, 8};
std::array<std::uint8_t, 2> port_{}, direction_{};  // A, B: data and direction registers
std::uint64_t pulse_ = 0;            // φ2 rises since power-on: pulse k of DS Fig. 5
std::uint64_t timerStart_ = 0;       // pulse of the last timer write
std::uint64_t interruptAt_ = 0;      // timerStart_ + N·P: the flag's pulse
std::uint8_t timerCount_ = 0;        // N
unsigned timerShift_ = 10;           // P = 1 << shift: 0, 3, 6, 10
bool flagPending_ = false;           // the flag will set at interruptAt_
bool flag_ = false, irqEnabled_ = false, driving_ = false;
std::array<std::uint8_t, 2> driven_{};  // last value driven on each port's Pins, bit by bit
```

Class: `Mos6530(std::string reference, const Mos6530Mask& mask, std::vector<std::uint8_t> rom)` (`Check`s 1024 words) and `static std::vector<PinSpec> Pins()` (the 40 Pins; `PartPinNames` takes their names). `kPortB` is indexed by bit: index 6 (`PB6`, the chip select) is `kNoPin`, index 7 is `PB7`.

### 6.4 Evaluation

`evaluate`: `rose(kPhi2)` → `rise`, `fell(kPhi2)` → `fall`, otherwise nothing (power-on evaluation).

**Rise** (decode with `readBit`, X → random as a MOS input):

1. `++pulse_`.
2. `RES_N` low: `port_`, `direction_` = 0, `irqEnabled_ = false`; every port Pin driven 1 after `kMos6530Port`; `PB7` from §6.5; return (no read).
3. `rom = !RS0`; `selected = RS0 && !CS1`; `a96 = A9..A6`; `ram = selected && a96 == mask_.ram`; `io = selected && a96 == mask_.io`.
4. `RW` high and one of them: the byte below, driven on `D0`–`D7` after `kMos6530DataOut`; `driving_ = true`.
   - ROM: `rom_[A9..A0]`. RAM: `ram_[A5..A0]`.
   - I/O, `A2` low (`A1 A0`): 0 port A, 1 direction A, 2 port B, 3 direction B. A port read is, per bit, the data bit where the direction bit is 1 and the Pin (`readBit`) where it is 0. Port B bit 6 (no Pin): the data bit.
   - I/O, `A2` high, `A0` low: the timer (§6.5); then `irqEnabled_ = A3`; a flag whose pulse is past (`pulse_ > interruptAt_`) is cleared; `PB7` updated after `kMos6530Irq` from this rise.
   - I/O, `A2` high, `A0` high: the flag on `D7`, `D6`–`D0` 0 (DS Fig. 4 shows only `D7`; the others assumed 0).

**Fall** (decode again with `readBitBefore`, the values held through `φ2`):

1. `driving_`: `D0`–`D7` `Z` after `kMos6530DataHold`; `driving_ = false`.
2. `RES_N` high and `RW` low and selected: the byte on `D0`–`D7` (`readBitBefore`) is written:
   - RAM: `ram_.store(A5..A0, byte)`.
   - I/O `A2` low: the register; the port's Pins driven after `kMos6530Port` (§6.5).
   - I/O `A2` high: `timerCount_ = byte`, `timerShift_ = {0, 3, 6, 10}[A1 A0]`, `timerStart_ = pulse_`, `interruptAt_ = pulse_ + (N << shift)`, `flagPending_ = true`, `flag_ = false`, `irqEnabled_ = A3`; `PB7` updated after `kMos6530Irq`.
   - ROM: nothing.
3. `flagPending_ && pulse_ + 1 >= interruptAt_`: `flag_ = true`, `flagPending_ = false`, `PB7` updated after `kMos6530Irq`. So `PB7` falls 300 ns after the `φ2` fall ending pulse `N·P − 1`, 200 ns before pulse `N·P` (TIC); `N = 0` sets the flag at the write's own fall.

Every evaluation is O(1) and allocates nothing.

### 6.5 Timer and Pins

- Timer value at pulse `c` (DS Fig. 5): `c < interruptAt_`: `N − 1 − ((c − timerStart_) >> shift)`; else `(0xFF − (c − interruptAt_)) & 0xFF`. It counts on after a cleared flag, as the chip's does.
- A port Pin's drive: `Zero` when its direction bit is 1 and its data bit 0, else `One` (the Weak pull-up). `PB7`: also `Zero` while `irqEnabled_ && flag_`.
- Port Pins are driven only when their value changes (a write, a reset, the flag), tracked in `std::array<std::uint8_t, 2> driven_` (one bit a Pin), so the many evaluations drive nothing.
- `powerOn` (random state until `RES_N`; a real NMOS chip powers up undefined): `ram_.powerOn(ctx)` (512 draws), then 8 draws each for port A, direction A, port B, direction B and `timerCount_`, bit 0 first; `timerShift_ = 10`, `timerStart_ = 0`, `interruptAt_ = N << 10`, `flagPending_ = true`, `flag_ = irqEnabled_ = false`; `setOutput` of every port Pin from the registers. `D0`–`D7` start `Z` (simulator).

`delays.hpp` (DS p. 3, 1 MHz; maximums, no typicals published):

```cpp
inline constexpr Time kMos6530DataOut = 395'000;   // φ2 rise → D valid (TCDR)
inline constexpr Time kMos6530DataHold = 10'000;   // φ2 fall → D released (THR)
inline constexpr Time kMos6530Port = 1'000'000;    // φ2 fall → PA, PB valid (TCPW)
inline constexpr Time kMos6530Irq = 300'000;       // φ2 fall → PB7 low: TIC 200 ns at 1 MHz
```

The 6502 bus at 1 MHz (`kMos6502Address` 130 ns after `PHI0` falls, data read as `PHI0` falls): a 6530 read is valid 425 ns after `PHI0` rises, 75 ns before the 6502 reads it.

### 6.6 `MemoryCells`

`memories()` `{"rom", "ram"}`; `wordCount(0)` 1024, `wordCount(1)` 64; `dataBit(D0..D7)` 0–7; `stick(0, …)` → `rom_.stick`, `stick(1, …)` → `ram_.stick`.

### 6.7 `datasheets/6530.md`

Function in plain words: the four parts, the KIM masks' map, the ports and pull-ups, the timer as Fig. 5 with its rule and the data sheet's off-by-one, `RES`, the delays. Pins as §6.2; pin 18 `CS1` (`PB6` on other masks), pin 17 `PB7` doubling as `IRQ`.

## 7. Bench — `tests/data/bw1_bench.bwn`, `roms/bw1_bench.{asm,rom}`

The KIM's bus at its own reference designators where it has them (KIM Figs. 3.2–3.4), no Board:

| Ref | Part | Role |
|---|---|---|
| X1 | `OSC freq=1MHz` | `PHI0` |
| U1 | `6502` | |
| U2 | `6530 mask=002 contents=../../roms/bw1_bench.rom` | the program; `CS1` = `K5`, `RS0` = `K7` |
| U4 | `74145` | `A`–`C` = `A10`–`A12`, `D` = `GND` (A13–A15 undecoded); `Y0_N` = `K0`, `Y5_N` = `K5`, `Y7_N` = `K7` |
| U5–U12 | `6102` | `D7`…`D0` (U5 is `D7`, KIM Fig. 3.4); `CE_N` = `K0`, `RW` = `RAM_RW` |
| U13, U14 | `74125` | `DOUT` → `D`: U14 for U5–U8, U13 for U9–U12, pins as Fig. 3.4; enables `BUF_N` |
| U15 | `74LS00` | `RAM_RW` = NAND(`RW_N`, `PHI2`); `BUF_N` = NAND(`RW`, `K0_N`) |
| U16 | `74LS04` | `RW_N`, `K0_N` |
| U25 | `POR time=1ms` | `RES_N` (the KIM's 556) |
| SW1 | `BUTTON` | `PA7` to `GND` |
| R1–R6 | `R value=3k3` | pull-ups on `K0`, `K5`, `K7`, `RES_N`, `NMI_N`, `IRQ_N` |
| J1 | `POWER` | |

Nets named: `PHI2` is `U1.PHI2` (the 6502's output, 30 ns after `PHI0`) to `U2.PHI2` and `U15.1B`; `RES_N` is `U25.RST_N` (open collector), `R4`, `U1.RES_N`, `U2.RES_N`; `IRQ_N` is `U1.IRQ_N`, `U2.PB7` and `R6`; `RDY`, `SO` on `VCC`. The unused gates' inputs (U15 gates 3–4 on `VCC`, U16 inverters 3–6 on `GND`) are tied, so the bench builds without warnings. The KIM's 74LS145 (U4) is our `74145` (the same function, open collector), the KIM's 7400/7404 our LS ones: P5l chooses the Machine's.

Program (`bw asm`, `.rom $1C00, $400`, header comment as `led_computer.asm`; the `asm_bw1_bench` ctest keeps the `.rom` equal):

1. `reset`: `sei`, `cld`, `ldx #$FF`, `txs`; port A direction `$7F` (`PA7` an input), port B direction `$3F` (`PB7` an input: the interrupt), port A `$00`, port B `$00`.
2. 6102 test, no RAM used meanwhile: for X = 0..255, write `X`, `X^$FF`, `X^$55`, `X^$AA` to `$0000,X`, `$0100,X`, `$0200,X`, `$0300,X`; then compare all four; a mismatch writes `$01` to port A and stops (`jmp *`).
3. 6530 RAM test: X = `$3F`..0, `X^$A5` to `$17C0,X`, compare; a mismatch: `$02`, stop.
4. Pass: `$55` to port A; `count` (`$00`) = 0; the timer written at `$174D` (÷8, `A3` = 1) with `N = 100`; `cli`.
5. Main loop: port B = `count & $1F`, bit 5 from `PA7` (port A read, bit 7), forever.
6. `irq`: `pha`, the timer written again (`$174D`, 100: clears the flag), `inc count`, `pla`, `rti`. `nmi`: `rti`. Vectors at `$1FFA`.

A test edits nothing in the files: Faults come through `BuildCircuit`'s `cells`.

## 8. Docs

- [ADD_PART.md](../recipes/ADD_PART.md): reference commits (6530: a bus peripheral woken only by `φ2`, mask and contents parameters, pull-up ports, two memories; 6102; 74125); "Which kind": memory chips keep their words in a `RomBlock` / `RamBlock` (`memory_block.hpp`) and implement `MemoryCells` (one memory, or named ones); rules: `PinKind::PullUp` for NMOS port pins; a Part whose pin names `PartPinNames` cannot build (6530) returns them from a static.
- [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md), Faults list: `fault cell U2.D3 ram $3F 0`, the memory name required on a chip with several; the chip list gains 6102 and 6530.
- [GLOSSARY.md](../GLOSSARY.md), Stuck Cell: "in a chip with several memories (the 6530's ROM and RAM), one of them, named". New **Pull-up Pin**: a Pin that drives 0 and holds its Net at a weak 1 otherwise (an NMOS port pin); anything driving beats its 1.
- [ARCHITECTURE.md](../ARCHITECTURE.md): the P5j row done, with links.

## 9. Tests

`tests/parts/memory_block_test.cpp` (new): `RamBlock` draws for seeds 1–4 equal a copy of the old 2114 loop (two test Components in two Circuits powered on with the same seed: `PowerOnContext` is private to the Simulator); the seed-dependent Machine tests (the 110's Blackout, P5g's cells) in phase 1's full suite are the guard on the real 2114; stuck bits hold through `store` and `powerOn`; width masking; `RomBlock::stick` both ways; the `Check`s.

`tests/sim/logic_test.cpp`: `DriveOf(PullUp, …)` for 0, 1, X, Z; `IsDriver`, `IsReader`. `tests/sim/simulator_test.cpp`: a test Component with one `PullUp` Pin: alone driving 1 → One, not floating; with a `Stimulus` driving 0 on the Net → Zero; power-on `Z` (Net floating before its first drive). `tests/netlist/builder_test.cpp`: a stuck `PullUp` Pin (a 6530's `PA0` stuck at 1, the program driving it 0): the listed Net reads 1, and the chip's own `"U2.PA0 stuck"` Net reads 1, not X (the Supply tie of §3.3 against the chip's strong 0).

`tests/parts/ram6102_test.cpp` (copy of `ram2114_test`'s bench, one data bit and a `DOUT` Net): pins and catalogue row (`DIN` the only `sampled` Pin); write then read back for several addresses; `CE_N` high and writing float `DOUT`; the held write (address changes after `RW` rises are not written); delays (`k6102Access`, `k6102Select`, `k6102Off`); a stuck cell; random power-on differs by seed.

`tests/parts/ttl125_test.cpp`: each buffer (`GENERATE`) enabled, disabled, X enable, X data; enable, disable and data delays.

`tests/parts/mos6530_test.cpp`: a `Bench` of one 6530 with a `Stimulus` per input; a `Script` of bus cycles (`{address, read|write, data}` plus `RS0`, `CS1`, `RES_N` per cycle) turned into `Stimulus` steps at 1 MHz (`PHI2` high 500 ns from 1 µs, address and control 100 ns after each fall, write data from 150 ns after the rise to 30 ns after the fall); each port Pin on a Net of its own with an optional `Stimulus` (Z unless listed, MOS, Strong) for outside drives. Cases:
- pins, catalogue row, parameter errors (§6.1); every signal Pin `sampled` but `PHI2`;
- ROM read (`RS0` low) at any `CS1`: the byte, valid `kMos6530DataOut` after the rise, `Z` `kMos6530DataHold` after the fall;
- RAM write then read at `$3C0`–`$3FF` for 002 and `$380`–`$3BF` for 003, and not selected at the other mask's pattern, at `CS1` high, or at `RS0` low (the ROM answers);
- ports: direction 0 → the Pin reads 1, not floating, and reads 0 when driven low from outside; direction 1 data 0 → 0 after `kMos6530Port`; direction 1 data 1 with an outside 0 → the Net 0, the read gives 1 (the register);
- `RES_N` low for one pulse: registers 0, every port Pin 1, a read returns nothing (bus floating);
- timer, Fig. 5: `N = 52`, ÷8, written in pulse 0: reads 51 (pulse 1), 25 (213), 0 (415), `$FF` (416), `$AB` (500); ÷1, ÷64, ÷1024 with `N = 3`; `N = 0`;
- `PB7`: with `A3` = 1, low at `kMos6530Irq` after the fall ending pulse 415, not before; with `A3` = 0 never; a timer read in pulse 416 leaves it low, one after releases it `kMos6530Irq` after that read's rise; a flag read gives `$00` before the flag and `$80` after the pulse-416 timer read;
- `MemoryCells`: memories, word counts, `dataBit`, a stuck `rom` cell read through the pins, a stuck `ram` cell surviving a write.

`tests/machine/puzzle_test.cpp`: `fault cell U2.D3 ram $3F 0` and `fault column U2.D0 rom 1` parsed (memory `"ram"`, `"rom"`); the old forms keep `""`; errors: `Ram`, `r4m`, 7 tokens, the new catch-all strings.

`tests/machine/machine_test.cpp`: `ResolveCell` on a `Mos6530` built in the test (§2.3's table: no name, `xyz`, `ram $40`, success with memory 1) and on the 110's `U3` with `ram` (`InlinePuzzle`: "has one memory: no name").

`tests/netlist/bw1_bench_test.cpp` (`[slow]`), seeds 1–3, the bench built by `BuildCircuit` with `ResolveContentsPaths`, no warnings:
- by 28 ms: `PA0`–`PA6` read `$55` (about 23 ms: POR 1 ms, the 6102 loops about 20 ms, the 6530's about 1.3 ms); `PB7` falls at least 5 times between 28 and 33 ms, each 800 µs plus the handler's latency after the last: from `PB7` falling (300 ns after a `φ2` fall) through the instruction under way and possibly one more (the main loop's longest is 4 cycles), the 7-cycle interrupt sequence, `pha` (3), `lda #` (2) and `sta abs` (4), about 16 to 24 µs, so the test accepts 810 to 830 µs; `PB0`–`PB4` step by one between falls;
- `SW1` held: `PB5` 0 within 50 µs; released: 1;
- `StuckCell{"U5", DIN, $123, Zero}` (word `$123` holds `$23^$FF = $DC`, bit 7 of `D7` is 1): `$01`;
- `StuckCell{"U2", D3, $10, One, memory 1}` (`$10^$A5 = $B5`, bit 3 is 0): `$02`.

`tests/CMakeLists.txt`: the new test files. The 110, 080 and every other Machine test pass unchanged (phase 1's full suite).

## 10. Release check

- `bw run netlists/led_computer.bwn`, `netlists/bus_load.bwn`, `netlists/message.bwn` headless ratios (median of 3) before phase 1 and after phase 2: within noise.
- `bw run tests/data/bw1_bench.bwn --time 100ms` headless ratio, recorded in the plan's notes (the first measure of the BW-1's bus; P5l measures the Machine).
- No UI change, so no screenshot beyond a `bw_ui` start of the 110 (unchanged look) with `--screenshot`, viewed.

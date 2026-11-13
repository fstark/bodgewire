# P5c — 6502 Part — Design

Spec: [MOS6502.md](MOS6502.md). Plan: [MOS6502_PLAN.md](MOS6502_PLAN.md). Recipes: [ADD_PART.md](../recipes/ADD_PART.md) (a chip with state, a new power pin name), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) (100 Heartbeat, in [HEARTBEAT_DESIGN.md](../machines/100-heartbeat/HEARTBEAT_DESIGN.md)).

Sources: MOS 6500 data sheet, Nov. 1985 (6502.org, `mos_6500_mpu_nov_1985.pdf`): pinout p. 10, 1 MHz timing p. 5, signal descriptions p. 7. Cycle-by-cycle bus: "64doc" (John West, Marko Mäkelä, 1994), Instruction timing. Interrupt and BRK details: the Visual 6502 wiki ("6502 Timing of Interrupt Handling", "6502 BRK and B bit"). Decimal mode: Bruce Clark, "Decimal Mode" (6502.org tutorials), Appendix A. Undocumented opcodes: Tom Harte's data is the reference; "No More Secrets" (NMOS 6510 unintended opcodes) for names.

## Files

| File | Change |
|---|---|
| `src/parts/mos6502_core.{hpp,cpp}` | `Mos6502Core`: the instruction core, no simulator includes (only `<array>`, `<cstdint>`) |
| `src/parts/mos6502.{hpp,cpp}` | `class Mos6502 final : public Component`: the core behind the Pins |
| `src/parts/delays.hpp` | `kMos6502…` delays (Pin timing) |
| `src/parts/CMakeLists.txt` | `mos6502.cpp`, `mos6502_core.cpp` (sorted) |
| `src/parts/catalogue.{hpp,cpp}` | `PartInfo{"6502", "8-bit microprocessor (NMOS)", "DIP-40", Chip, &CreateChip<Mos6502, "6502">}` between `2716` and `74LS00`; `PowerPinVolts`: `GND2` → 0 V; new `std::string_view PowerPinRail(std::string_view pin)`: the rail Net a power Pin belongs on (`GND2` → `GND`, otherwise the pin's own name) |
| `src/netlist/builder.cpp` | the power-pin check (`can only be on net …`, line 234) and `addImplicitPower` (line 256) use `PowerPinRail`, so `GND2` joins `GND` and never looks up a missing Net |
| `tests/netlist/builder_test.cpp` | a Part with `GND2`: implicit on `GND`; `net GND U1.GND2` accepted; `net VCC U1.GND2` refused naming `GND` |
| `datasheets/6502.md` | Function (plain words: what a CPU cycle is, `SYNC`, `RDY`, interrupts, reset, the random power-on state), Pins (`A0`–`A9`, `A10`–`A15`, `D0`–`D7`, every other name) |
| `tests/parts/catalogue_test.cpp` | `6502` in `kMosSignals` and `kOffCornerPower` (`GND` pin 1, `VCC` pin 8, `GND2` pin 21); power-pin names `GND`, `GND2`, `VCC` |
| `tests/parts/mos6502_core_test.cpp` | hand-written core cases (Tests) |
| `tests/parts/mos6502_cases.{hpp,cpp}` | reads the compact case files, runs one case on a core over a 64 KiB array, reports the first difference |
| `tests/parts/mos6502_conformance_test.cpp` | Harte subset, full Harte, Dormann |
| `tests/parts/mos6502_test.cpp` | the Part at its Pins |
| `tests/CMakeLists.txt` | `parts/mos6502_cases.cpp`, `parts/mos6502_conformance_test.cpp`, `parts/mos6502_core_test.cpp`, `parts/mos6502_test.cpp`; `BW_6502_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data/6502"`, `BW_6502_EXTERNAL_DIR="${PROJECT_SOURCE_DIR}/external/6502"` |
| `tests/data/6502/harte/00.txt` … `ff.txt` | the committed subset, 100 cases an opcode (the first 100 of each file of `SingleStepTests/65x02`, folder `6502/v1`, not `nes6502`), 4.8 MB; written by `fetch.sh --subset` from each JSON before it is deleted |
| `tests/data/6502/README.md`, `LICENSE-harte` | where the data comes from (repository, commit hash), the format, how to refresh; Harte's MIT licence text |
| `tests/data/6502/convert_harte.py` | JSON → compact format (`--cases N` keeps the first N); standard library only, run with `venv/bin/python` |
| `tests/data/6502/fetch.sh` | full Harte set (256 files, each fetched, converted into `external/6502/harte/`, deleted) and Dormann's `6502_functional_test.bin` and `.lst` into `external/6502/dormann/`; the commit hashes fetched written to `external/6502/SOURCES` |
| `.gitignore` | `external/` |
| `docs/recipes/ADD_PART.md` | a CPU kind (core apart from the Pins, conformance data), `GND2`, reference commits |
| 100 Heartbeat | [HEARTBEAT_DESIGN.md](../machines/100-heartbeat/HEARTBEAT_DESIGN.md) |

## Instruction core

### Interface

```cpp
struct Mos6502Registers {
    std::uint16_t pc;
    std::uint8_t a, x, y, s, p;  // p: N V 1 B D I Z C; bit 5 reads 1, B (bit 4) only as pushed
};

struct Mos6502Bus {
    std::uint16_t address;
    std::uint8_t data;  // written; meaningless on a read
    bool read;          // RW high
    bool sync;          // opcode fetch
};

class Mos6502Core {
public:
    void start(const Mos6502Registers& registers);  // next cycle: an opcode fetch at registers.pc
    const Mos6502Bus& bus() const { return bus_; }  // the cycle in progress
    void step(std::uint8_t data);                   // ends it; `data` read (ignored on a write)
    Mos6502Registers registers() const;             // as at the end of the last instruction
    bool jammed() const;

    // Input levels for the next step(), true = asserted (pin low). NMI latches on the
    // deasserted → asserted change between two steps.
    void setIrq(bool asserted);
    void setNmi(bool asserted);
    void setReset(bool asserted);
    void setOverflow();  // SO's falling edge: V = 1 now
};
```

No allocation, no virtual calls: `step` is a `switch` on the addressing mode and the cycle number within the instruction. `registers()` returns the programmer's registers; when an instruction's last effect lands in the next fetch on the real chip (ADC's A), the core applies it at the end of the instruction's last cycle instead. Nothing on the Pins can tell.

### Opcode table

`constexpr std::array<Opcode, 256> kOpcodes`, `struct Opcode { Mode mode; Access access; Operation operation; }`.

- `Mode`: `Implied`, `Accumulator`, `Immediate`, `ZeroPage`, `ZeroPageX`, `ZeroPageY`, `Absolute`, `AbsoluteX`, `AbsoluteY`, `IndirectX`, `IndirectY`, `Relative`, `JumpAbsolute`, `JumpIndirect`, `Call` (JSR), `Return` (RTS), `ReturnFromInterrupt`, `Break` (BRK, IRQ, NMI, reset), `Push`, `Pull`, `Jam`.
- `Access`: `Read` (the operand is read once), `Write` (stored once), `Modify` (read, written back unchanged, written changed), `None`.
- `Operation`: one per instruction effect, the documented and the undocumented (`Slo` = ASL then ORA, `Lax` = LDA and LDX, …). A read operation takes the byte; a write operation gives the byte to store (`Sha` and its family also see the address); a modify operation maps the old byte to the new one and sets flags (and A, for the combined ones).

The table follows the NMOS opcode matrix; Harte's subset checks every row, so a wrong row fails at once.

### Cycles (64doc)

T1 is the opcode fetch (`sync`, read at PC, PC + 1). "Dummy" reads are real bus cycles whose data is thrown away. `EA` is the effective address.

| Mode | Cycles after T1 |
|---|---|
| `Implied`, `Accumulator` | T2 dummy read at PC |
| `Immediate` | T2 operand at PC++ |
| `ZeroPage` | T2 `ADL` at PC++; T3 read or write at `ADL`. Modify: T3 read, T4 write old, T5 write new |
| `ZeroPageX`/`Y` | T2 `ADL`; T3 dummy read at `ADL`; T4 at `(ADL + X) & $FF`. Modify: T4–T6 |
| `Absolute` | T2 `ADL`, T3 `ADH`; T4 at `EA`. Modify: T4–T6 |
| `AbsoluteX`/`Y` | T2 `ADL`, T3 `ADH`; T4 at `ADH:(ADL + X)` (low byte only). Read: that is the operand unless the add carried; then T5 at `EA`. Write and Modify: T4 always a dummy read, T5 at `EA` (Modify: T5 read, T6 write old, T7 write new) |
| `IndirectX` | T2 pointer `ZP`; T3 dummy read at `ZP`; T4 `ADL` at `(ZP + X) & $FF`; T5 `ADH` at `(ZP + X + 1) & $FF`; T6 at `EA`. Modify: T6–T8 |
| `IndirectY` | T2 `ZP`; T3 `ADL` at `ZP`; T4 `ADH` at `(ZP + 1) & $FF`; T5 at `ADH:(ADL + Y)`; Read: operand unless carried, then T6 at `EA`. Write/Modify: T5 dummy, T6 at `EA` (Modify: T6–T8) |
| `Relative` | T2 offset at PC++. Not taken: done. Taken: T3 dummy read at PC, PCL += offset; no carry: done; carry: T4 dummy read at the unfixed PC, PCH fixed |
| `JumpAbsolute` | T2 `ADL`, T3 `ADH` → PC |
| `JumpIndirect` | T2 `IAL`, T3 `IAH`; T4 PCL at `IAH:IAL`; T5 PCH at `IAH:(IAL + 1)` (no carry into `IAH`) |
| `Call` | T2 `ADL` at PC++; T3 dummy read at `$0100 + S`; T4 push PCH; T5 push PCL; T6 `ADH` at PC → PC |
| `Return` | T2 dummy read at PC; T3 dummy read at `$0100 + S`; T4 PCL, T5 PCH (pulled); T6 dummy read at PC, PC + 1 |
| `ReturnFromInterrupt` | T2 dummy at PC; T3 dummy at the stack; T4 P, T5 PCL, T6 PCH (pulled) |
| `Push` | T2 dummy at PC; T3 push (`PHP`: P with B and bit 5 set) |
| `Pull` | T2 dummy at PC; T3 dummy at `$0100 + S`; T4 pull |
| `Break` | T2 read at PC (BRK: PC + 1; interrupt and reset: PC kept); T3 push PCH, T4 push PCL, T5 push P (B set for BRK only); T6 vector low (I set), T7 vector high. Reset: T3–T5 are reads at the same stack addresses, S still decremented by 3. Vector: reset `$FFFC`, NMI `$FFFA`, otherwise `$FFFE`, chosen at the end of T4: an NMI latched by then takes over a BRK or an IRQ, and its latch is cleared |
| `Jam` | T2 read at PC (PC + 1), then reads at `$FFFF`, `$FFFE`, `$FFFE`, `$FFFF` and `$FFFF` for ever (Harte's 11 listed cycles, then more of the last); only reset leaves it; IRQ and NMI ignored |

### Interrupts and reset

- At the end of every cycle (`step`), the core computes `wanted = nmiLatched || (irq && !I)` and keeps it for the next step (`pollLast_`); the value at the end of each fetch is also latched (`pollFetch_`). When an instruction ends, it takes an interrupt if `wanted` was true at the end of its second-to-last cycle (the fetch, for a 2-cycle instruction). Exception: a taken branch that does not cross a page ends at T3 but uses `pollFetch_`. `CLI`, `SEI` and `PLP` change I at their last cycle, after the poll, so the instruction after them still runs under the old I; `RTI` pulls P at T4, before its poll.
- Taking one: the next T1 fetch happens as usual (`sync`, the byte read is thrown away, PC not incremented), then `Break` with the interrupt's push and vector.
- `setReset(true)` seen by `step`: the instruction is abandoned; each later cycle reads at PC (`sync` low) until a `step` sees it false; the following cycle is the `Break` T2 of a reset (T1 is the cycle in which the release was seen). The JAM state and the NMI latch are cleared.
- `setOverflow()`: V set at once; an instruction that writes V later in the same instruction wins.

### Decimal mode

`ADC`, `SBC` and the combined `RRA`, `ISC` with D set follow Clark's NMOS rules: the result in BCD with the low nibble's half-carry, C from the BCD sum, Z from the binary sum, N and V from the intermediate after the low-nibble fix (ADC); SBC's flags all from the binary difference. `ARR` with D set follows Harte's data. Invalid BCD digits are not rejected.

### Unstable opcodes

| Opcode | Rule (all of Harte's cases agree) |
|---|---|
| `ANE` `$8B` | A = (A \| `$EE`) & X & imm |
| `LXA` `$AB` | A = X = (A \| `$EE`) & imm |
| `SHA` `$9F`, `$93` | stores A & X & (H + 1), H the high byte of the base address; when the index carries, the high byte of the address written is that value |
| `SHX` `$9E`, `SHY` `$9C` | X (or Y) & (H + 1), the same address rule |
| `TAS` `$9B` | S = A & X, stores S & (H + 1), the same address rule |
| `LAS` `$BB` | A = X = S = memory & S |

## Pins

`Mos6502` (index = pin − 1), from the data sheet p. 10:

| Pin | Name | Kind |
|---|---|---|
| 1 | `GND` | power |
| 2 | `RDY` | input, sampled |
| 3 | `PHI1` | output |
| 4 | `IRQ_N` | input, sampled |
| 5 | `NC1` | no-connect |
| 6 | `NMI_N` | input, sampled |
| 7 | `SYNC` | output |
| 8 | `VCC` | power |
| 9–20 | `A0`–`A11` | outputs |
| 21 | `GND2` | power |
| 22–25 | `A12`–`A15` | outputs |
| 26–33 | `D7`–`D0` | bidirectional, sampled |
| 34 | `RW` | output |
| 35, 36 | `NC2`, `NC3` | no-connect |
| 37 | `PHI0` | input (both edges) |
| 38 | `SO` | input, sampled |
| 39 | `PHI2` | output |
| 40 | `RES_N` | input, sampled |

Every pin but the power pins `Family::Mos` (NMOS: no pull-up, bus charge, X read as a random bit), `NC1`–`NC3` included (the catalogue test's `kMosSignals` rule). `PHI0` is the only Pin that wakes the Part; the data pins being `sampled` also spares the evaluation their own drive would cause (ADD_PART, sampled inputs; `Circuit::buildTables`).

### Per edge

- **`PHI0` rises** (phase 2 starts): `PHI1` → 0, `PHI2` → 1. `SO` sampled (`readBitBefore`; data sheet: on the trailing edge of φ1): a 1 → 0 change since the last rise calls `setOverflow()`. A write cycle drives `D0`–`D7` with `bus().data`.
- **`PHI0` falls** (the cycle ends): `PHI1` → 1, `PHI2` → 0. `RES_N`, `NMI_N`, `IRQ_N`, `RDY` sampled (`readBitBefore`) and passed to the core. Then:
  - a read cycle with `RDY` low: nothing else; the same cycle runs again (the Pins already show it);
  - otherwise: a read cycle reads `D0`–`D7` (`readBitBefore`: the bus as it stood while `PHI2` was high); a write cycle releases them (Z); `step(data)`; the new `bus()` drives `A0`–`A15`, `RW` and `SYNC`.
- `drive` is called for every address bit each cycle; the kernel's comparison with the pending value drops the unchanged ones.
- **Power-on**: `Mos6502Registers` from `randomBit()` (P bit 5 set), `start(…)`; `setOutput` the address of PC, `RW` 1, `SYNC` 1, `PHI1` 1, `PHI2` 0. The data pins start released (Z) by the kernel. If `PHI0` powers on high, `PHI1`/`PHI2` disagree with it until its first edge: harmless, nothing samples them before.
- X never reaches the core: inputs go through `readBitBefore`.

### Pin timing

The data sheet measures from φ2 (out); `PHI2` follows `PHI0` by `kMos6502Phi2`, so each delay below is from the `PHI0` edge. It gives no φ0 → φ1/φ2 delay: 20–40 ns is assumed and keeps the outputs non-overlapping (TD ≥ 5 ns).

`kMos6502Phi1` and `kMos6502Phi2` are `Delays{rise, fall}`; the others are one `Time`, as `kZ80…`.

| Delay | ps (rise, fall) | Data sheet, 1 MHz |
|---|---|---|
| `kMos6502Phi1` | 40'000, 20'000 | none published: rises after `PHI2` falls, falls before it rises |
| `kMos6502Phi2` | 30'000, 30'000 | none published |
| `kMos6502Address` | 130'000 | φ2 + tADS 100 ns typical |
| `kMos6502ReadWrite` | 130'000 | φ2 + tRWS 100 ns typical |
| `kMos6502Sync` | 390'000 | φ2 + tSYNC 360 ns maximum (no typical) |
| `kMos6502DataOut` | 180'000 | φ2 + tMDS 150 ns typical |
| `kMos6502DataHold` | 90'000 | φ2 + tHW 60 ns typical (data released) |

Read data is taken at the `PHI0` fall, 30 ns before the φ2 fall the data sheet measures tDSU (100 ns) and tHR (10 ns) against. A memory meeting tACC (575 ns from the address) has its data on the bus well before either.

## Test data

### Format

One file per opcode, `harte/<hh>.txt` (two lower-case hex digits). Comment lines start with `#`. Each case is three lines, every number hex:

```
I <pc> <s> <a> <x> <y> <p> <address>:<value> …     initial registers and RAM
F <pc> <s> <a> <x> <y> <p> <address>:<value> …     final registers and RAM
C <address>:<value>:<r|w> …                        bus cycles in order
```

`convert_harte.py SRC OUT [--cases N]` writes it from Harte's JSON (`initial`, `final`, `cycles`); the subset is `--cases 100`. The header comment of each file names the source repository and commit.

### Runner (`mos6502_cases`)

`ReadMos6502Cases(path)` → `std::vector<Mos6502Case>`, `std::expected` with the file and line on a bad line (`from_chars` over `string_view`; one file at a time). `RunMos6502Case(const Mos6502Case&)` → `std::optional<std::string>` (the first difference, else nothing): a 64 KiB array with the initial RAM (the full run reuses one array and clears only the cells a case touched), `start(initial)`, then for each listed cycle `bus()` compared (address, R/W, the data on a write), the RAM read or written, `step(byte)`; then `registers()` and every final RAM entry compared. An access to an address not in the initial RAM is allowed (it reads 0); the cycle comparison catches it.

### Dormann

`external/6502/dormann/6502_functional_test.bin` loaded at `$0000` (64 KiB), `start` at `$0400` with S `$FF`, P `$24`. The case steps until an opcode fetch (`sync`) is at the address of the previous one (a `jmp *` or `bne *` trap), then checks that address is the success trap. The success address is read from the `.lst` by `fetch.sh`: `success` is a macro, so it takes the expanded `jmp *` line marked `;test passed, no errors` (`3469 : 4c6934 > jmp *` at commit `7954e2d`), into `external/6502/dormann/success`; `fetch.sh` fails if it finds none. `SOURCES` keeps the commits fetched. Cap: 200 M cycles.

## Tests

Test names: `"6502 core: …"` for the core, `"6502: …"` for the Part (as `"2114: …"`).

`tests/parts/mos6502_core_test.cpp` (`[parts]`):

- NOP: two cycles, `sync` on the first, the second a dummy read at PC + 1.
- Reset: from a random state and mid-instruction; while asserted, reads at PC; after release the 7 cycles of the table, I set, fetch at the vector; S − 3.
- IRQ: taken after the current instruction when I is clear; masked when set; asserted only during the last cycle → after the next instruction; `CLI` then `NOP` with IRQ held → taken after the `NOP`; the taken-branch delay; B clear in the pushed P, `$FFFE` vector.
- NMI: once per falling edge, held low → once; ignores I; takes over a BRK whose T4 has not ended (vector `$FFFA`, B set in the pushed P).
- JAM: the bus of Harte's case, then `$FFFF` reads; IRQ and NMI ignored; reset recovers.
- SO: V set between instructions.

`tests/parts/mos6502_conformance_test.cpp`:

- `6502 core: Harte subset` (`[parts]`): `GENERATE` over the 256 files of `BW_6502_DATA_DIR/harte`; each file's cases run; the first failure printed with its opcode and case name.
- `6502 core: Harte full` (`[parts][slow]`): the same over `BW_6502_EXTERNAL_DIR/harte`; `SKIP` when the folder is missing.
- `6502 core: Dormann functional test` (`[parts][slow]`): as above; `SKIP` when missing.

`tests/parts/mos6502_test.cpp` (the Z80STUB bench shape: every Pin on its own Net, a `ClockSource` of 1 µs on `PHI0`, a `Stimulus` per input; a test-only `BenchMemory` Component, 64 KiB, woken by the address, `RW` and `PHI2`: while `RW` is high it drives `D0`–`D7` with the byte at the address 200 ns after the address last changed, while `RW` is low it releases them and stores the data at `PHI2`↓; each case loads a small program and the vectors into it):

- `6502: pins`: count, names, kinds, `sampled` on every input and data pin, `PHI0` not.
- `6502: clocks and bus timing`: `PHI1`/`PHI2` edges at their delays and never both high; address, `RW` at `PHI0`↓ + 130 ns, `SYNC` at + 390 ns.
- `6502: reads before the edge`: a data Net changed at the `PHI0` fall itself is not seen; one changed before is.
- `6502: runs a program`: reset, then a short program (`LDA #`, `STA abs`, `PHA`, `JSR`/`RTS`, a loop) ends with the expected bytes in `BenchMemory`.
- `6502: writes`: during a `PHA`, `D` driven at `PHI0`↑ + 180 ns, released at `PHI0`↓ + 90 ns, `RW` low for the cycle.
- `6502: RDY`: low on a read cycle repeats it (address unchanged over two cycles); low on a write cycle is ignored, the next read stalls.
- `6502: reset and interrupts through the pins`: `RES_N` low 2 cycles → the reset sequence on the address pins; `IRQ_N`, `NMI_N` as in the core cases, one each.
- `6502: X on the data bus`: an opcode read as X runs (no crash, no X on any output).
- `6502: wakes only on PHI0`: a bench of the CPU, the `ClockSource` and `Stimulus` inputs without steps (no `BenchMemory`, the data Nets tied): `SimStats::evaluations` (a total) minus the clock's own (one a half period) equals the `PHI0` edges.

Debug cost, measured before the docs were committed: the subset 17 s, Dormann 3.7 s (the whole test passes). Full Harte runs more than 10 minutes in debug with the first runner (a zeroed 64 KiB array per case); with the runner clearing only the cells a case touched it is measured again in phase 2. Over 2 minutes, it gets Catch2's hidden tag (`[.][slow]`, run by name: `bw_tests "6502 core: Harte full"`, once in release at the end of phase 2), so it never joins the full debug suite.

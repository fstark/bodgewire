# P5d — ROM programs + first computer — Design

Spec: [ROM_PROGRAMS.md](ROM_PROGRAMS.md). Plan: [ROM_PROGRAMS_PLAN.md](ROM_PROGRAMS_PLAN.md). Recipes: [ADD_PART.md](../recipes/ADD_PART.md) (three chips with state or special logic), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) (110 LED computer, in [LED_COMPUTER_DESIGN.md](../machines/110-led-computer/LED_COMPUTER_DESIGN.md)).

Sources: MOS 6500 data sheet, Nov. 1985 (the instruction set and addressing, p. 2–4; the 1 MHz timing already in `delays.hpp`). TI SN74LS138 and SN74LS273 data sheets: typical delays as commonly quoted (the PDFs cannot be fetched, ADD_PART), said so in the commits.

## Files

| File | Change |
|---|---|
| `src/parts/mos6502_core.{hpp,cpp}` | `Mos6502Operand`, `Mos6502Instruction`, `Mos6502Decode(std::uint8_t)` (below); the table and the core unchanged |
| `src/asm/asm6502.{hpp,cpp}` | `Assemble6502`: source text → ROM bytes and `.rom` text |
| `src/asm/CMakeLists.txt` | `add_library(bw_asm STATIC asm6502.cpp)`, `PUBLIC bw_parts`, `PRIVATE bw_warnings` |
| `CMakeLists.txt` | `add_subdirectory(src/asm)` after `src/parts` |
| `src/cli/main.cpp`, `src/cli/CMakeLists.txt` | `bw asm <file.asm> -o <file.rom>`; `bw` links `bw_asm`; usage text and the "expected …" message list it |
| `src/parts/ls138.{hpp,cpp}` | `class Ls138 final : public Component` |
| `src/parts/ls273.{hpp,cpp}` | `class Ls273 final : public Component` |
| `src/parts/power_on_reset.{hpp,cpp}` | `class PowerOnReset final : public Component` |
| `src/parts/delays.hpp` | `kLs138Select`, `kLs138EnableG2`, `kLs138EnableG1`, `kLs273ClockToQ`, `kLs273Clear` |
| `src/parts/CMakeLists.txt` | `ls138.cpp`, `ls273.cpp`, `power_on_reset.cpp` (sorted) |
| `src/parts/catalogue.cpp` | rows `74LS138` (after `74LS107`), `74LS273` (after `74LS27`), `POR` (between `PADDLE` and `POWER`); `CreatePowerOnReset` (parameter `time=`) |
| `datasheets/74LS138.md`, `74LS273.md`, `POR.md` | Function and Pins (ADD_PART) |
| `tests/parts/ls138_test.cpp`, `ls273_test.cpp`, `power_on_reset_test.cpp` | Part cases (spec, Tests) |
| `tests/parts/mos6502_core_test.cpp` | decode cases |
| `tests/asm/asm6502_test.cpp` | assembler cases; every `roms/*.asm` against its `.rom` |
| `tests/asm_cli.cmake`, `tests/CMakeLists.txt` | the three Part tests and the asm test in `bw_tests`; `bw_asm` linked; `BW_ROM_DIR="${PROJECT_SOURCE_DIR}/roms"`; ctest `asm_<name>` per `roms/*.asm` running `bw asm` and comparing |
| `src/manual/manual.cpp`, `tests/manual/manual_test.cpp` | `PartPinNames` builds `POR` with `time=1ms`; its 8 names checked |
| `docs/NAMING.md` | `asm` (assembler, assembly) allowed as an abbreviation: the command is `bw asm` |
| `docs/recipes/ADD_MACHINE.md`, `ADD_PART.md` | ROM programs; the new kinds of Part, reference commits |
| 110 LED computer | [LED_COMPUTER_DESIGN.md](../machines/110-led-computer/LED_COMPUTER_DESIGN.md) |

## Decode (`mos6502_core.hpp`)

```cpp
// How an instruction's operand is written in assembly.
enum class Mos6502Operand : std::uint8_t {
    None, Accumulator, Immediate, ZeroPage, ZeroPageX, ZeroPageY, Absolute, AbsoluteX,
    AbsoluteY, Indirect, IndirectX, IndirectY, Relative
};

struct Mos6502Instruction {
    std::string_view mnemonic;  // upper case, "LDA"; undocumented ones by their usual names
    Mos6502Operand operand;
    bool documented;
};

Mos6502Instruction Mos6502Decode(std::uint8_t opcode);
```

In `mos6502_core.cpp`, from `kOpcodes`:

- the operand form from the `Mode`: `Implied`, `Return`, `ReturnFromInterrupt`, `Break`, `Push`, `Pull`, `Jam` → `None`; `JumpAbsolute`, `Call` → `Absolute`; `JumpIndirect` → `Indirect`; the others by name;
- the mnemonic from a `constexpr std::array<std::string_view, …>` indexed by `Op` (one name an `Op`; `Jam` is `JAM`);
- documented: the `Op` is a documented operation (`Lda` … `Brk`: not `Lax`, `Las`, `Anc`, `Alr`, `Arr`, `Sbx`, `Ane`, `Lxa`, `Sax`, `Sha`, `Shx`, `Shy`, `Tas`, `Slo`, `Rla`, `Sre`, `Rra`, `Dcp`, `Isc`, `Jam`), not `Nop` unless the opcode is `$EA`, and the opcode is not `$EB`. This rule gives exactly 151 (a test checks the count; the table's undocumented NOPs and `$EB` `SBC #` are the only documented operations at undocumented opcodes).

The enum `Op` stays private to the `.cpp`; the hot path is not touched.

## Assembler (`src/asm/asm6502.hpp`)

```cpp
struct AssembledRom {
    std::uint16_t base = 0;          // CPU address of the ROM's word 0 (.rom)
    std::vector<std::uint8_t> words; // the ROM, unwritten words 0xFF
    std::string text;                // the .rom file
};

// A 6502 program (docs/p5d-rom-programs/ROM_PROGRAMS.md, What is new 1) assembled into a ROM image.
// `file` names the source in errors ("<file> line N: <what>") and in the .rom header.
std::expected<AssembledRom, std::string> Assemble6502(std::string_view source, std::string_view file);

// The same from a file; "cannot read '<path>'"; errors name the file's filename.
std::expected<AssembledRom, std::string> Assemble6502File(const std::filesystem::path& path);
```

The header comment of `asm6502.hpp` repeats the syntax in short (the reference a ROM author reads).

### Structure

- **Lines → statements.** Each line: optional label (`name:` or `@name:`), then a constant (`NAME = expr`), a directive (`.org`, `.rom`, `.byte`, `.word`) or an instruction, then an optional `;` comment (outside a `"` string or `'c'`). Symbols: letters, digits, `_`, starting with a letter or `_`, case-sensitive; mnemonics, `A`, `X`, `Y` case-insensitive. A local `@name` is stored as `<last global label>@name`; one before any global label is an error.
- **Expressions.** A small precedence-climbing parser over a token list: primaries (number, `'c'`, symbol, `*`, parenthesised), unary `-` `<` `>` `~`, then the binary levels `* /`, `+ -`, `<< >>`, `&`, `^`, `|`. Values are `std::int64_t`. Evaluating returns "known" or "unknown" (a symbol not yet defined); pass 2 makes unknown an error.
- **Operand form.** After the mnemonic: empty → `None` (or `Accumulator` when only that form exists: `ASL` alone is `ASL A`); `A` → `Accumulator`; `#e` → `Immediate`; `(e,X)` → `IndirectX`; `(e),Y` → `IndirectY`; `(e)` → `Indirect`; `e,X` / `e,Y` → zero-page or absolute indexed; `e` → zero page, absolute or `Relative` (branches). Where both forms exist, zero page when `e` is known in pass 1 and in `0`–`255`; else absolute. The form is fixed in pass 1 (the statement's length with it) and reused in pass 2, so a label's address never moves between passes.
- **Encoding table.** Built once from `Mos6502Decode`: a map from (mnemonic, form) to opcode, documented opcodes only. An instruction with no entry for its form: "`LDX` has no `(zp,X)` form" (the operand form named as written).
- **Pass 1** assigns addresses and defines symbols (a label: the address; a constant: its value, which must be known at that point); **pass 2** evaluates and emits. Range checks in pass 2: immediate and zero-page operands `-128`–`255` (a negative one as its byte), absolute `0`–`$FFFF`, branch offset `-128`–`127` from the next instruction, `.byte` `-128`–`255`, `.word` `0`–`$FFFF`.
- **ROM window.** `.rom BASE, SIZE` must come before the first byte and only once; `SIZE` a power of two from 16 to 64K; `BASE + SIZE` ≤ `$10000`. Every emitted byte's address must be inside it and not written before.
- **Text.** Header: `; Generated by bw asm from <file>: do not edit. Edit the .asm and run bw asm again.`, `<file>` the source's filename only (`Assemble6502File` passes `path.filename()`), so the `.rom` is the same whatever path `bw asm` was given. Then every source line in order: a line with bytes → `@OOO HH HH HH` (offset with as many hex digits as `SIZE − 1` needs, at least 3), padded to a column, `; FFFF  <source line>`; a line with no bytes → `; <source line>` (an empty line stays empty). A `.byte` line with more than 16 bytes continues with `@OOO` lines of 16, without a comment. The text ends with a newline.

### `bw asm`

`int AsmCommand(std::span<char*> args)` in `main.cpp`: exactly `asm <in> -o <out>`, otherwise the usage error (exit 2); `Assemble6502File` error → `bw: <message>` and exit 1; writes the text, prints `<out>: N bytes at $BASE-$END`.

## 74LS138 (`ls138.hpp`)

Pins (TI): 1 `A`, 2 `B`, 3 `C`, 4 `G2A_N`, 5 `G2B_N`, 6 `G1`, 7 `Y7_N`, 8 `GND`, 9 `Y6_N`, 10 `Y5_N`, 11 `Y4_N`, 12 `Y3_N`, 13 `Y2_N`, 14 `Y1_N`, 15 `Y0_N`, 16 `VCC`. No state, no sampled pins.

`evaluate`: enable = `G1 ∧ ¬G2A_N ∧ ¬G2B_N` in three-valued logic (a 0 on `G1` or a 1 on a `G2` disables, whatever the others). For each output `k`: the set of select values the inputs allow (an X bit allows both); output 0 if enabled and the selects are exactly `k`; 1 if disabled or `k` is not among the allowed values; else X. Delay by what changed at this timestamp, as the 74LS157 (`rose` or `fell`, so a change to or from X counts as a select change): `G1` → `kLs138EnableG1`, a `G2` → `kLs138EnableG2`, otherwise `kLs138Select` (the power-on evaluation: select). Allocation-free: `std::array`.

Delays (TI typicals, ps; rise = output to 1): `kLs138Select{21'000, 21'000}` (3 levels), `kLs138EnableG2{12'000, 20'000}`, `kLs138EnableG1{17'000, 26'000}`.

## 74LS273 (`ls273.hpp`)

Pins (TI): 1 `CLR_N`, 2 `1Q`, 3 `1D`, 4 `2D`, 5 `2Q`, 6 `3Q`, 7 `3D`, 8 `4D`, 9 `4Q`, 10 `GND`, 11 `CLK`, 12 `5Q`, 13 `5D`, 14 `6D`, 15 `6Q`, 16 `7Q`, 17 `7D`, 18 `8D`, 19 `8Q`, 20 `VCC`. `CLK` rising-edge-only; `1D`–`8D` sampled; `CLR_N` an ordinary input (asynchronous).

As `Ls175` with eight units and no `Q_N`: `CLR_N` 0 → all 0 (`kLs273Clear`); X → all X; else on `rose(CLK)` each `Q` takes `readBefore(D)` (`kLs273ClockToQ`). Power-on: each `Q` random. Delays: `kLs273ClockToQ{17'000, 18'000}`, `kLs273Clear{18'000, 18'000}` (only the fall is used).

## POR (`power_on_reset.hpp`)

Pins: 1 `RST_N` (`PinKind::OpenCollector`, TTL), 2 `NC2`, 3 `NC3`, 4 `GND`, 5 `NC5`, 6 `NC6`, 7 `NC7`, 8 `VCC`. `PowerOnReset(std::string reference, Time hold)`.

- `powerOn`: `setOutput(kRstN, Zero)`, so `RES_N` is low from time 0 (the Net resolves from the outputs at power-on).
- `evaluate`: called once, at power-on (no inputs, so nothing wakes it again): `drive(kRstN, One, hold)`, an open-collector release exactly `hold` later. No wake and no state; each power-on starts over.
- Pins `NC2`, `NC3`, `NC5`–`NC7` are `PinKind::NoConnect` with `Family::Ttl` (the catalogue test checks every non-MOS chip's family). Row: `PartInfo{"POR", "power-on reset", "DIP-8", Chip, &CreatePowerOnReset}`.
- `CreatePowerOnReset`: `time=` required, `DurationFromText` (`parts/paddle.hpp`, as `PADDLE`; `ParseDuration` is in `bw_netlist`, above `bw_parts`), at least 1 µs; errors as `PADDLE`'s (`POR needs time=<duration>`, `POR: unknown parameter 'x'`, `POR: time must be at least 1us`).
- Datasheet `POR.md`: what a power-on reset does (holds the processor in reset until the supply has settled, then lets go), that the real one is a resistor, a capacitor and a Schmitt trigger, open collector so a Reset button can share the line.

## Catalogue tests

`tests/parts/catalogue_test.cpp`: the three new Parts need nothing extra (TTL pins; power at the DIP corners: 74LS138 pins 8/16, 74LS273 10/20, POR 4/8). The POR needs a parameter wherever every Part is built, as `OSC` needs `freq=`: `catalogue_test.cpp` (chip pinouts, beside the `OSC` case) and `PartPinNames` in `src/manual/manual.cpp` (beside `OSC` and `PADDLE`; without it the POR's datasheet pins go unchecked, silently), with a size check in `tests/manual/manual_test.cpp` (`manual: part pin names`).

## Tests in CMake

```
add_test(NAME asm_${name}
         COMMAND ${CMAKE_COMMAND} -DBW=$<TARGET_FILE:bw> -DSOURCE=${asm}
                 -DEXPECTED=${PROJECT_SOURCE_DIR}/roms/${name}.rom
                 -DOUT=${CMAKE_CURRENT_BINARY_DIR}/asm_${name}.rom
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/asm_cli.cmake)
```

one per `roms/*.asm` (`file(GLOB … CONFIGURE_DEPENDS)`), outside the `BW_KICAD_CLI` block; `asm_cli.cmake` runs `bw asm`, fails on a non-zero exit, and compares with `cmake -E compare_files`.

## Recipes

- ADD_MACHINE step 2, ROMs: a program is `roms/<m>.asm` (header comment: Machine, spec path, memory map); `./build/release/src/cli/bw asm roms/<m>.asm -o roms/<m>.rom`; commit both; the asm test and `asm_<m>` check they agree. Reference commits.
- ADD_PART: a decoder (74LS138, delay by the input that changed, X folded over the selects), an octal register (74LS273 as 74LS175), a Part acting by itself after power-on (`POR`: an output set in `powerOn`, one delayed drive from the power-on evaluation); a required parameter means `catalogue_test.cpp` and `PartPinNames` pass one.

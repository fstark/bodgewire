# P5t — Z80 assembler

Roadmap step P5t ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): `bw asm` for the Z80, the documented opcodes, enough for test ROMs and the BW-80 ROM. Design: [Z80_ASSEMBLER_DESIGN.md](Z80_ASSEMBLER_DESIGN.md). Plan: [Z80_ASSEMBLER_PLAN.md](Z80_ASSEMBLER_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

The BW-80 (P5u–P5y) runs a Z80. Its video spike (P5v) needs a test ROM and its firmware (P5w) a 4K ROM of our own: both are Z80 programs too long to hand-assemble as P5s did for the bus load (`roms/make_bus_load_z80_rom.py`, bytes written beside their mnemonics). `bw asm` already assembles the 6502 ROMs of the BW-1 and BW-2001 (P5d, P5k, P5p); this step gives it the Z80, in the same source language, so a ROM author learns one tool.

## User decisions (2026-11-15)

| Question | Decision |
|---|---|
| Dialect | Zilog's mnemonics and operands (`LD A,(HL)`, `JP (IX)`, `EX AF,AF'`, `ADD A,B` but `SUB B`), with the 6502 side's expressions and directives unchanged (`$hex`, `%bin`, `*` the statement's address, `.byte`, `.word`, `NAME = expr`): one language for both CPUs. No `DB`/`DEFB`/`EQU`/`0FFh`/`$` dialect. An operand wholly inside parentheses is memory (or a port) |
| Instructions | Zilog's documented set only, as the 6502 side: no `IXH`/`IXL`, `SLL`, `OUT (C),0`, `IN F,(C)` |
| Bus-load ROM | moved to `roms/bus_load_z80.asm`, assembled by `bw asm` to exactly the bytes the script writes today; the script is retired |
| Checking | a hand table of every documented form's bytes from Zilog's manual, and each documented opcode run on the P5s core (`Z80Core`), which must take exactly its bytes; self-contained, every build |

Decided while designing (2026-11-15), for the user to overrule:

- The CPU is chosen in the source: `.cpu z80` (or `.cpu 6502`, the default, so every existing `.asm` is unchanged), before the first instruction, once. `bw asm` keeps its command line.
- The public API becomes CPU-neutral: `Assemble`, `AssembleFile` in `src/asm/assembler.hpp` (in place of `Assemble6502`, `Assemble6502File`); its callers (the CLI and three tests) follow.
- Beside the hand table, the exhaustive check is a round trip through a test-only disassembler written from the Z80's decoding by opcode fields (Cristian Dinu, "Decoding Z80 Opcodes"), not from the assembler's table: every documented opcode disassembles to text that assembles back to the same bytes. The Z80 core has no mnemonic table to build the encodings from (the 6502 side's `Mos6502Decode`), so the two must be kept from agreeing by construction.
- Zilog's operand forms are strict: `ADD A,B`, `ADC A,B`, `SBC A,B` need the `A,`; `SUB`, `AND`, `XOR`, `OR`, `CP` take one operand. The error names the form that exists.

## What is new

1. **`.cpu z80`** in `bw asm`: a two-pass assembler for Zilog's documented Z80 instruction set (UM0080's Instruction Set chapter), in the 6502 side's source language ([ROM_PROGRAMS.md](../p5d-rom-programs/ROM_PROGRAMS.md), What is new 1, with `.checksum` and `.include` of P5k and P5p):
   - mnemonics and register names in either case; operands as Zilog writes them, separated by commas: registers `A B C D E H L I R`, pairs `AF BC DE HL SP IX IY`, `AF'`; `(HL)`, `(BC)`, `(DE)`, `(SP)`, `(C)`; `(IX+d)`, `(IX-d)`, `(IX)` (the same as `(IX+0)`), and `IY` likewise; conditions `NZ Z NC C PO PE P M` as the first operand of `JP`, `CALL`, `RET` and (`NZ Z NC C` only) `JR`; an operand wholly inside one pair of parentheses is an address in memory (or a port for `IN`/`OUT`): `LD A,(BUF+1)` reads memory, `LD A,(1+2)*3` loads 9;
   - every instruction size known in the first pass (none depends on a value), so a forward reference costs nothing;
   - value checks in the second pass: 8-bit data −128..255, 16-bit data −32768..65535, addresses and ports in range, an index displacement −128..127, `JR`/`DJNZ` targets within −128..127 of the next instruction, `BIT`/`SET`/`RES` bit 0–7, `IM` 0–2, `RST` one of `$00 $08 … $38`;
   - errors in the 6502 side's form, naming the file and line: an unknown instruction (a 6502 mnemonic under `.cpu z80` is one), an operand list the instruction has no form for (`LD has no form 'B,(SP)'`), the ranges above; a symbol named like a register or condition reads as one when it is the whole operand (a note in the recipe, no error);
   - the `.rom` text exactly as for the 6502.
2. **`.cpu NAME`**: `6502` or `z80`; once, before any instruction; without it, the 6502.
3. **`roms/bus_load_z80.asm`**: P5s's display program and data in Z80 source, assembled to the same 2 KB as `roms/make_bus_load_z80_rom.py` wrote; the script is deleted and the netlist's comment names the `.asm`.

## Tests

- **Every form, by hand** (`tests/asm/asmz80_test.cpp`): one row per documented form of UM0080 (about 130: each register group, each index form, each condition kind, every block and I/O instruction), its text and its bytes as the manual gives them, `(IX+d)` and `(IY+d)` with a negative displacement, the `DD CB d op` order.
- **Every opcode, round trip:** a test-only disassembler (`tests/asm/z80_disassembler.{hpp,cpp}`, by opcode fields) enumerates every documented opcode (all 252 unprefixed ones, 248 `CB`, the documented `ED` ones, the `DD`/`FD` index forms, `DD CB`/`FD CB` with a displacement) with fixed operand bytes; each disassembles to text that assembles to the same bytes.
- **Every opcode, on the core:** each of those byte sequences run on `Z80Core` (registers pointing away from it) reads exactly its own bytes, from its first to its last, before the instruction ends.
- **Syntax and errors:** memory and expression parentheses; `(IX-1)`, `(IX)`; `JR *`; forward references; `AF'` before a comment; a label named like a register reads as the register; ranges; `.cpu` twice, after an instruction, unknown; a 6502 mnemonic under `.cpu z80` and a Z80 one under the 6502; the 6502 side's tests unchanged.
- **Bus load:** `roms/bus_load_z80.asm` assembles to the words of the `.rom` the script wrote (checked once against the old file); `asm: every ROM program matches its committed .rom` and the ctest `asm_bus_load_z80` keep it so; the `bus load` tests show the page with the same line and frame counts.
- Each hand rule is checked against a mutation of the assembler (the index displacement after the `CB`, a condition code off by one, `JR`'s offset from the instruction's start).

## Later

- The undocumented Z80 opcodes, if a test ROM ever needs them (`.byte` meanwhile).
- Macros and repeat blocks: not needed by any ROM yet.
- A listing of T-states per line: the bus-load program's budget is checked by its test on the simulator.

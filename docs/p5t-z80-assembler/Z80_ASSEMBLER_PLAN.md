# P5t Z80 assembler — Implementation Plan

Design: [Z80_ASSEMBLER_DESIGN.md](Z80_ASSEMBLER_DESIGN.md)
Spec: [Z80_ASSEMBLER.md](Z80_ASSEMBLER.md)

On branch `p5t-z80-asm` (a worktree), while P5q1 runs on `main`.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `3aadd06` |
| 1 | Front end apart from the 6502; `Assemble`, `.cpu` | `733124e` |
| 2 | Z80 instructions; hand table, disassembler round trip, core lengths | |
| 3 | `roms/bus_load_z80.asm`, the script retired | |
| 4 | Release check, recipes, roadmap | |

## Phase 1: front end

- `src/asm/instruction_set.hpp`, `src/asm/assembler.{hpp,cpp}` as the design's Front end; `asm6502.{hpp,cpp}` cut down to the 6502's `InstructionSet`; `memo()` for its forms.
- `.cpu 6502|z80` (the Z80 set an empty stub refusing every mnemonic until phase 2, so `.cpu z80` parses).
- Callers: `src/cli/main.cpp`, `tests/asm/asm6502_test.cpp`, `tests/machines/bw1_model_test.cpp`, `tests/machines/bw2001_model_test.cpp`.
- Tests: `.cpu` unknown, twice, after an instruction; `.cpu 6502` explicit gives the same ROM.

Fence: debug build; `ctest --preset debug -j 12 -R "asm|bw1 model|bw2001 model"` (the callers; the Puzzle and session tests reach no changed code but the assembled ROMs, which `asm_*` holds) (every `asm_*` unchanged: no `.rom` differs); `git status` shows no `roms/*.rom` changed. Commit: `asm: the front end apart from the 6502; Assemble, .cpu (P5t)`.

## Phase 2: Z80 instructions

- `src/asm/asmz80.{hpp,cpp}`: operands, the form table, values (design, Z80 instructions); the `assembler.hpp` header comment gains the Z80 syntax.
- `tests/asm/z80_disassembler.{hpp,cpp}`, `tests/asm/asmz80_test.cpp` (design, Tests); `tests/CMakeLists.txt`.
- Mutations (design, Tests), each run once and reverted; noted below.

Fence: `ctest --preset debug -j 12 -R "asm"` (the Z80 cases are named `asm z80: …`); 696 opcodes round-trip and run on the core; the mutations noted. Commit: `asm: the Z80's documented instructions, checked by hand table, round trip and core (P5t)`.

## Phase 3: bus load in Z80 source

- `roms/bus_load_z80.asm` (design, Bus load); `bw asm roms/bus_load_z80.asm -o roms/bus_load_z80.rom` with `build/debug/src/cli/bw` (the same output; the release build comes in phase 4).
- Words check: `git show HEAD:roms/bus_load_z80.rom` to the scratchpad, both read with `ReadRomContents` (a throwaway test run, or a short program): every word equal.
- Delete `roms/make_bus_load_z80_rom.py`; the netlist's comment.

Fence: `ctest --preset debug -j 12 -R "asm|bus load"` (re-run `cmake --preset debug` for the new `asm_bus_load_z80`); the words check passed. Commit: `bus load: the Z80 display program in bw asm source, the script retired (P5t)`.

## Phase 4: finish

- Release: `cmake --preset release && cmake --build --preset release`; `bw asm` on every `roms/*.asm` gives the committed `.rom`s; `bw run netlists/bus_load_z80.bwn --time 2s` ratio as P5s (about 0.43×); `bw_ui netlists/bus_load_z80.bwn --screenshot … --after 1s`, viewed: the page.
- Full debug suite (shared code: the assembler every Machine's ROM goes through).
- `docs/recipes/ADD_MACHINE.md` (design, Recipes); ARCHITECTURE row P5t done with the date and links; the P5d row's mention of `asm6502` left as history.
- This plan cut down to the phase table and notes. Rebase on `main` before the finish if it moved.

Fence: release check done, full suite passes (or its failures shown not P5t's). Commit: `docs: P5t implemented`.

## Notes

- Review (before the commit of the docs), fixed in them: expressions from the original text (symbols case-sensitive); `Cond` tried before `Reg8` in the first operand of `JP`/`CALL`/`JR`/`RET`; the Z80 test helpers read the `.rom` text's rows (the 6502 helper strips real `$FF`s and starts at the window); no "register as a value" error (a register name inside an expression is a symbol, as noted); why `ED 63`/`ED 6B` leave the round trip; the shared helpers listed; test names `asm z80: …` for `-R`; narrower phase 1 filter; phase 3 with the debug `bw`; the disassembler's types; `im` apart from `m`; the 6502's error order kept.
- Phase 1 was written while the review ran, then checked against it.
- Phase 2: the form table needs no separate `(HL)`/`(XY+d)` rows: one pattern `M` (`r`, `(HL)` as code 6, `(XY+d)` as 6 behind its prefix) covers each 8-bit row, a memory-to-memory pair refused; `LD r,r'` is `LD M,M`. The opcode list's operand bytes: after DD/FD, nn is `$5AFD` (d and n read as one word). A first count of 698: the unprefixed loop fed `DD 34 12` and `FD 34 12` as plain opcodes (decoded as `INC (IX+18)`); the loop now skips the four prefix bytes. Mutations, each failing cases: the `DD CB d op` order swapped (3 cases), `JP cc`'s condition shifted to bit 4 (3), `JR`'s offset from the instruction's start (5). Debug: `ctest -R asm` 29 tests, 0.6 s.
- The `.rom` listing's comment column is padded for three bytes (the 6502's longest); a 4-byte Z80 instruction pushes its comment right. Left so: widening it would change every committed 6502 `.rom`.

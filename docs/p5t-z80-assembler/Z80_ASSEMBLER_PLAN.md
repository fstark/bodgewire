# P5t Z80 assembler — Implementation Plan

Design: [Z80_ASSEMBLER_DESIGN.md](Z80_ASSEMBLER_DESIGN.md)
Spec: [Z80_ASSEMBLER.md](Z80_ASSEMBLER.md)

Implemented 2026-11-15 on branch `p5t-z80-asm` (a worktree), while P5q1 and P5q2 ran on `main`; rebased on `main` at P5q2's `3dbd22c` before the finish.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `ca57b52` |
| 1 | Front end apart from the 6502; `Assemble`, `.cpu` | `9c04edf` |
| 2 | Z80 instructions; hand table, disassembler round trip, core lengths | `cfab52e` |
| 3 | `roms/bus_load_z80.asm`, the script retired | `55e9cca` |
| 4 | Release check, recipes, roadmap | this commit |

## Notes

- Review (before the commit of the docs), fixed in them: expressions from the original text (symbols case-sensitive); `Cond` tried before `Reg8` in the first operand of `JP`/`CALL`/`JR`/`RET`; the Z80 test helpers read the `.rom` text's rows (the 6502 helper strips real `$FF`s and starts at the window); no "register as a value" error (a register name inside an expression is a symbol, as noted); why `ED 63`/`ED 6B` leave the round trip; the shared helpers listed; test names `asm z80: …` for `-R`; narrower phase 1 filter; phase 3 with the debug `bw`; the disassembler's types; `im` apart from `m`; the 6502's error order kept.
- Phase 1 was written while the review ran, then checked against it. Debug: `ctest -R "asm|bw1 model|bw2001 model"` 27 tests, every `.rom` unchanged.
- Phase 2: the form table needs no separate `(HL)`/`(XY+d)` rows: one pattern `M` (`r`, `(HL)` as code 6, `(XY+d)` as 6 behind its prefix) covers each 8-bit row, a memory-to-memory pair refused; `LD r,r'` is `LD M,M`. The opcode list's operand bytes: after DD/FD, nn is `$5AFD` (d and n read as one word). A first count of 698: the unprefixed loop fed `DD 34 12` and `FD 34 12` as plain opcodes (decoded as `INC (IX+18)`); the loop now skips the four prefix bytes. Mutations, each failing cases: the `DD CB d op` order swapped (3 cases), `JP cc`'s condition shifted to bit 4 (3), `JR`'s offset from the instruction's start (5). Debug: `ctest -R asm` 29 tests, 0.6 s.
- The `.rom` listing's comment column is padded for three bytes (the 6502's longest); a 4-byte Z80 instruction pushes its comment right. Left so: widening it would change every committed 6502 `.rom`.
- Phase 3: the data sections of `roms/bus_load_z80.asm` (the image's `.byte` rows, one comment a text row; the glyphs as `.byte %0xxxxx00`, one comment a code) were written by a throwaway script from the old script's own functions, the program by hand. Words check: a throwaway Catch case read the old `.rom` (from the commit before) and the new with `ReadRomContents`: all 2,048 words equal. Debug: `ctest -R "asm|bus load"` 33 tests, 75 s (the TV case 75 s).
- Phase 4, release: `ctest --preset release -R asm` (30: every `roms/*.asm` through `bw asm` gives its committed `.rom`). `bw run --time 2s`, alternated, 3 runs each: `bus_load` 0.370–0.378×, `bus_load_z80` 0.406–0.420×; both about 5 % under P5s's figures with the same ROM bytes, so machine load (P5q work ran on `main` at the same time), not P5t. `bw_ui netlists/bus_load_z80.bwn --after 1s`: 0.41×, UI 0.1 ms, the page on the TV (screenshot viewed).
- Phase 4, full debug suite on the rebased tree: 1,238 of 1,238 pass (810 s); 10 skipped, as always without `external/` (the worktree has none: the real PET ROMs, the full 6502 sets) or in debug (release-only cases).

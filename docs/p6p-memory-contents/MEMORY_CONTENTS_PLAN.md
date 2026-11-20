# P6p — Memory contents — Implementation Plan

Spec: [MEMORY_CONTENTS.md](MEMORY_CONTENTS.md). Design: [MEMORY_CONTENTS_DESIGN.md](MEMORY_CONTENTS_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | The read and the defaults: `MemoryCells::read`, `Machine::contents`, `roms/contents.txt` and its parser | |
| 2 | The views' logic: the Z80 disassembler moved, the 6502's, `memory_contents` | |
| 3 | The window and the page: the click, the Contents divider, the three views, `--contents` | |
| 4 | Docs, release check, full release suite | |

The row's split ("the page, hex and graphics; then the disassembly") is kept in the other order: the logic, the disassemblers included, before the page, so the page never ships with a placeholder Code view (design review, 2026-11-20).

## Phase 1 — The read and the defaults

Design §1, §2.

1. `MemoryCells::read`, `wordBits`; `Rom`, `Mos6530`, `Ram2114`, `Ram6102`.
2. `ChipContents`, `Machine::contents` (ROM chips only).
3. `src/view/contents_defaults.{hpp,cpp}`: `Cpu`, `CharacterSet`, `ChipDefaults`, `ContentsDefaults`, `ParseContentsDefaults`.
4. The six `roms/contents.txt` (design §2's table).
5. Tests: `rom:`, `6530:`, `2114:`, `6102:` (`read` through a `stick`, `wordBits`); `machine: contents …` (Odd E's UA2 `$2A` reads `$44`, `$40` after the swap; a column; a RAM and an unknown reference nullopt); `contents defaults: …` (the parser; every Machine's file against its netlist, ROM chips by `FindRomSpec` or `6530`).

**Fence**: release build; `ctest --preset release -j -R "^(rom|6530|2114|6102|machine|contents defaults): "`; debug build and the same `-R` in debug (Parts and Machine C++; the anchored names reach no BW-2001 case); the full release suite (shared code: `MemoryCells`, `Machine`). Commit: `parts, machine, view: a ROM's contents as held now; roms/contents.txt on six Machines (P6p phase 1)`.

## Phase 2 — The views' logic

Design §6, §7.

1. `git mv tests/asm/z80_disassembler.{hpp,cpp} src/asm/`; `src/asm/disassembled.hpp` (`Z80Text` renamed `Disassembled`); `Z80DocumentedOpcodes` to `tests/asm/z80_opcodes.{hpp,cpp}`.
2. `Mos6502OperandBytes` in `mos6502_core.*`, `asm6502.cpp` on it; `Disassemble6502`.
3. `src/view/memory_contents.{hpp,cpp}`: settings, `DefaultSettings`, `SizeText`, `CharacterOf`, `HexColumns`, `HexLine`, `GlyphLayout`, `GlyphDot`, `GlyphCount`, `CodeLine`, `Disassembly`; `bw_view` links `bw_asm`.
4. Tests: `disassembler 6502: …`; `asm z80: …` as today; `contents: …`.

**Fence**: release build; `ctest --preset release -j -R "^(asm|disassembler 6502|contents): "`; debug the same `-R`; the full release suite (shared code: `mos6502_core`, the assembler). Commit: `asm, view: the 6502 and Z80 disassemblers in bw_asm; the Contents views' logic (P6p phase 2)`.

## Phase 3 — The window and the page

Design §3, §4, §5, §8.

1. `src/view/chip_body.{hpp,cpp}`: the body moved from `board_look.cpp` (`BodyBox`), `MemoryLook`, `MemoryWindowOf`, `MemoryWindowAt`; `board_look.cpp` on them. Screenshot: the BW-80's IC2 x-rayed and the LED Computer's U2 look as before.
2. `Binder`: `Divider::Contents`, `openContents`, `contents()`.
3. `BoardView`: the window's press, `takeContentsClick`, the tooltip, the double-click fit off windows; `main.cpp`'s handling.
4. `src/ui/contents_page.{hpp,cpp}` with the three views; `ManualWindows`' tab, page, `openContents`, `setContentsDefaults`, `PushPaper`'s widget colours; `main.cpp` reads `roms/contents.txt`.
5. `--contents REF[:view]`.
6. Tests: `chip body: …`, `binder: …`.
7. Screenshots, viewed: the BW-80's IC2 in Code (`$0038` on the first screen) and `IC2:glyphs`; the BW-2001's `UH4:hex` (PETSCII) and UA2 on Odd E (`machines/130-bw2001/puzzles/01-odd-e.bwp --contents UA2`); the Message's U15, the Typewriter's U23 (5-bit glyphs); the BW-1's `U2:hex` on Odd seven (`$1FEE` reads `8F`); the BW-1's U2 in Code at `$1C00`; the LED Computer's U2.

**Fence**: release build; `ctest --preset release -j -R "^(chip body|binder|contents|contents defaults): "`; screenshots viewed. Commit: `ui: a memory chip's window opens the binder at its contents: Hex, Code and Glyphs (P6p phase 3)`.

## Phase 4 — Docs and release check

Design §9.

1. The six User Manuals (end of Using it), GLOSSARY, ADD_MACHINE, ADD_PUZZLE.
2. The manual tests (`ctest -R "^manual"`).
3. The release check: the BW-2001 running with the binder out at Contents (speed ratio and UI ms printed by `--screenshot`); the full release suite.
4. ARCHITECTURE's row done with the date and links; this plan cut down to the phase table and notes.

**Fence**: `ctest --preset release -j` (full). Commit: `docs: P6p done, the Contents divider in the manuals and recipes (P6p phase 4)`.

## Implementation notes

- Review (2026-11-20), fixed: Odd seven's byte is `$87` (reads `$8F`); phase 1's test finds ROM chips without `MemoryLookOf` (phase 3); `CharacterOf` returns UTF-8 (`£`), the font checked in `src/ui`; anchored `-R` fences, the new cases named; the full release suite at the shared-code fences (1, 2); the head link frames the chip without selecting it; `openContents` sets `shown_` only while away; `PushPaper` gains the widgets' colours; 8 words a row at the default width, no narrow-binder screenshot (no width flag); plain `x` and `-` (no `×`, `–` in the UI font); the phases reordered (no placeholder); `Machine::contents` ROMs only, `Session::contents` dropped (no Board without a Machine); `ManualWindows::openContents(k, view)`, `setContentsDefaults`; `ChipContents` in `bw`; the User Manuals' sentence in Using it; absolute 6502 operands 4 digits, Z80 displacements decimal; `Disassembled` defined once; the glyph grid at least 3 px a dot; Go to outside the chip ignored; `Ram2114`/`Ram6102` `read` tested; `BodyBox`, not a public `Box`.

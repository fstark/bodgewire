# P6p — Memory contents — Implementation Plan

Spec: [MEMORY_CONTENTS.md](MEMORY_CONTENTS.md). Design: [MEMORY_CONTENTS_DESIGN.md](MEMORY_CONTENTS_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | The read and the defaults: `MemoryCells::read`, `Machine::contents`, `roms/contents.txt` and its parser | `6d1619a` |
| 2 | The views' logic: the Z80 disassembler moved, the 6502's, `memory_contents` | `f38727a` |
| 3 | The window and the page: the click, the Contents divider, the three views, `--contents` | `97a3420` |
| 4 | Docs, release check, full release suite | this commit |

The row's split ("the page, hex and graphics; then the disassembly") is kept in the other order: the logic, the disassemblers included, before the page, so the page never ships with a placeholder Code view (design review, 2026-11-20).

## Implementation notes

- Review (2026-11-20), fixed: Odd seven's byte is `$87` (reads `$8F`); phase 1's test finds ROM chips without `MemoryLookOf` (phase 3); `CharacterOf` returns UTF-8 (`£`), the font checked in `src/ui`; anchored `-R` fences, the new cases named; the full release suite at the shared-code fences (1, 2); the head link frames the chip without selecting it; `openContents` sets `shown_` only while away; `PushPaper` gains the widgets' colours; 8 words a row at the default width, no narrow-binder screenshot (no width flag); plain `x` and `-` (no `×`, `–` in the UI font); the phases reordered (no placeholder); `Machine::contents` ROMs only, `Session::contents` dropped (no Board without a Machine); `ManualWindows::openContents(k, view)`, `setContentsDefaults`; `ChipContents` in `bw`; the User Manuals' sentence in Using it; absolute 6502 operands 4 digits, Z80 displacements decimal; `Disassembled` defined once; the glyph grid at least 3 px a dot; Go to outside the chip ignored; `Ram2114`/`Ram6102` `read` tested; `BodyBox`, not a public `Box`.
- Phase 1 (2026-11-20): `Machine::contents` reads only a `Rom` or a `Mos6530` of the design build; its test uses the LED Computer's U2 (a cell and a column), the BW-2001 left to the phase 3 screenshot (its Machine loads slowly in debug). The defaults test skips non-directories under `machines/` (`.DS_Store`). Checks: release and debug `-R "^(rom|6530|2114|6102|machine|contents defaults): "` (78), full release suite 1472/1472.
- Phase 2 (2026-11-20): `DefaultSettings` takes the word width only (the chip's size is not needed for a default). `HexLine` and `CharacterOf` give UTF-8; `src/ui` swaps `£` for `.` if the font lacks it. The Z80 test of the real ROM: `$0038` reads `DEC C`, `JP NZ,$0045`. Checks: release and debug `-R "^(asm|asm z80|disassembler 6502|contents): "` (29), full release suite 1479/1479.
- Phase 3 (2026-11-20): `PopupBg` (the paper) is pushed around each combo only, not in `PushPaper`: `PageTooltip` draws light text on the tooltip's dark background, which a paper `PopupBg` would hide. Address is hidden in the Glyphs view (glyphs count the chip's own words); Go to starts at the chip's address. The open view's button is framed in ink. `board_look.cpp` keeps its `Box` as an alias of `BodyBox`. Screenshots viewed: the BW-80's IC2 in Code (`$0000` `LD HL,$7FFF`, `$0038` on the second screen) and Glyphs at `$0E00` (the ZX80's set); UA2 on Odd E (`05` with its dot); UH4 in Hex PETSCII; the Message's U15 and the Typewriter's U23 (5-bit, 16 a row); the BW-1's U2 in Hex and in Code (`SAVE`, `JMP ($17FA)`); the LED Computer's U2; IC2 x-rayed on the Board, the window as before. Checks: release `-R "^(chip body|binder|contents|contents defaults): "` (17).
- Phase 4 (2026-11-20): the six User Manuals end Using it (the Message, What it does: it has no Using it) with the window, pointed at first on a mask ROM, addresses plain (ADD_MACHINE's rule); GLOSSARY (Contents, Equipment's binder); ADD_MACHINE (`roms/contents.txt`), ADD_PUZZLE (the views and a ROM Fault). Release check: the BW-2001 on Odd E, 4 s, 0.36× and UI 0.9 ms with the binder at UH1's Code, 0.37× and 0.9 ms without. Full release suite 1482/1482.
- For the user to check (a screenshot cannot click or type): the window's click under each tool, the tooltip, Go to in Hex and Code, a Combo's list on the paper, the settings kept per chip, the head's link framing the chip, 16 words a row in a widened binder.
- After the implementation (2026-11-20), the user's request: no Bits setting in Glyphs; a glyph is always 8 dots wide, bit 7 the leftmost, so a 2513's 5-bit glyphs sit in the right five columns (which bits reach the screen is the circuit's business). `glyphBits` and `DefaultSettings`' word width removed, `kGlyphDots`. Checks: release `-R "^(contents|contents defaults|chip body|binder): "` (17); the Message's U15 viewed.

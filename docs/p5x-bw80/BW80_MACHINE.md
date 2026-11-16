# P5x — BW-80 (ZX80)

Roadmap step P5x ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), after P5v's video spike ([BW80_VIDEO.md](../p5v-bw80-video/BW80_VIDEO.md)). It makes Machine 140 BW-80 ([BW80.md](../machines/140-bw80/BW80.md)) out of P5v's `netlists/bw80.bwn`: the ZX80's keyboard, its expansion edge connector, a Board laid out as the ZX80's own, manuals, and the tests that type on it. The firmware is the ZX80's ROM (P5v; P5w skipped), so the editor is Sinclair's: one keyword a key, the bottom-line editor and its inverse `K` cursor.

## Why

P5v's ZX80 shows its `K` cursor and waits for a key it cannot get. The keyboard is the last of the ZX80's tricks worth keeping: no decoder and no port chip, the address lines `A8`–`A15` select the rows themselves during an `IN`, and IC10 puts the five columns on `D0`–`D4`. With it the BW-80 is a computer the player can program, and P5y can break any link of that scan (the pull-ups, IC10, the copper, the connector). The edge connector gives the player the bus to probe, as the PET's user port does. The Board is where Faults live; laid out as the ZX80's, it reads against photographs of the real thing.

## Decided

With the roadmap row (2026-11-15): the keys, cassette and edge on P5v's netlist; the ZX80's ROM and its editor given; keyboard on the Board or floating, PC typing and paste as key presses, the editor's form, all decided at the start.

Asked at the start of the step (the user, 2026-11-16):

- **The keyboard floats**: a drawn ZX80 membrane keyboard in its own window, behind a `MEMBRANEKB` Part that reads `A8`–`A15` and pulls `KB0`–`KB4` low itself, the ZX80's isolating diodes D3–D10 inside it; faultless, as the BW-2001's (the user asked whether floating is the more efficient choice: it is, in simulation, where the Part costs nothing until a key is held, where eight diode Parts on the Board would put a Net event on every change of `A8`–`A15`; and in work, where the BW-2001's keyboard chain is reused almost whole). Faults stay on the Board side: IC10, the 47k pull-ups, the copper, the header. Offered instead: 40 membrane keys and the diodes as Parts on the Board, per-key faults possible.
- **The cassette is left out**: no `MIC`; `EAR` stays as P5v left it, pulled down through R1 into IC10. `SAVE` and `LOAD` run and wait, as on a ZX80 with no recorder. Offered instead: the two jacks as passive connectors; a tape recorder device that records `SAVE` and plays it back.
- **The Board after the ZX80's own PCB**: every chip where Sinclair put it on the issue 2 board, standing upright as there, the connectors at the back where the ZX80's sockets are; the keyboard area (the lower third of the ZX80's board, printed contacts under the membrane) left off since the keyboard floats. `bw place` gets a rule for the Board's size and one for a part at a given place; the user routes. Offered instead: `bw place`'s annealing; a Board authored from a KiCad schematic (the "schematic reconsidered" of the roadmap row, a larger step of its own).
- **Typing by character, paste tokenized**: a PC key presses the ZX80 key that types its character, with SHIFT where the ZX80 needs it (`"` is SHIFT+Y); Enter is NEW LINE, Backspace RUBOUT, the arrows SHIFT+5–8, Home HOME, Escape BREAK (the SPACE key, which the ROM reads without SHIFT while a program runs), Shift+Enter EDIT. Paste reads a BASIC listing: a keyword at a statement's start (or after THEN) becomes its key, THEN, TO, AND, OR, NOT and `**` their SHIFT keys, everything else letter by letter, so a pasted program runs. Offered instead: by position, paste plain.
- **The edge connector** (the row): `EXPANSION`, the ZX80's 2 × 23 expansion bus as a passive connector with the ZX80's pin names, nothing plugged in.

Decided in this step's design (not the user's):

- **`MEMBRANEKB`'s pins**: `A8`–`A15` are Inputs that wake the Part (an `IN` holds the row pattern about 1 µs, the Part must answer inside it), `KB0`–`KB4` open-collector outputs (a diode pulls a column low, nothing pulls it high: the Board's 47k do). A key held on a row whose address line is floating or X does nothing.
- **One interface for both keyboards**: the BW-2001's `MatrixKeyboard` and the new `MembraneKeyboard` share `KeyMatrix` (held keys, a queue of presses, the typed press for drawing) and `PressQueue` (the SPSC queue and the hold/gap timing, moved out of `MatrixKeyboard` unchanged). The Session, the CLI, the window and the capture work on `KeyMatrix`, and tell the two apart only by a kind (the look, the key map).
- **A ZX80 press is held 60 ms and released 120 ms** (the BW-2001's 40/40): the ROM's `KEYBOARD` reads the keys once a frame (20 ms), takes a key only when two consecutive reads agree, and accepts a new key only after five frames that read no key (its shift counter in `RESULT_lo`); three frames down and six up leave a margin each way.
- **The reset stays `RST1 POR time=1ms`** (P5v): the ZX80's R21/C10 hold it about 0.2 s, which only delays the first picture.
- **The edge connector's lower pin 6 carries `PHI_N`** (the ZX81 documents call it Φ, "inverted to make the CPU clock", which is what IC15's gates do here); lower 23 (the ZX81's ROMCS) is not connected on the ZX80, nor the two SLOT pads and the 9 V pad. The ZX80's data bus pads carry the memory side, `MD0`–`MD7`, and `RAM_CS_N` is the resistor's far side, as Sinclair wired them.
- **The Board is 155.5 × 142 mm** (`size` in the rules file): the ZX80's width and its component area, the chips' courtyard centres at Searle's measured positions ([BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md#board)).

## New

1. **`MEMBRANEKB`**, the BW-80 keyboard (Design §2): the Part, its key table (40 keys, legend, shifted legend, keyword), the character map and the paste tokenizer; `KeyMatrix` and `PressQueue` shared with the BW-2001's keyboard.
2. **`EXPANSION`**, the ZX80's edge connector (Design §3).
3. **Session, CLI, window** (Design §4): the Session and `--type` on any `KeyMatrix`; the keyboard window draws the ZX80's membrane for a `MEMBRANEKB`; the capture maps PC keys by kind; paste through the tokenizer.
4. **`bw place`: `size W H` and `at REF X Y`** (Design §5), the Board's size and a part at a given place on it, for a layout copied from a real board.
5. **Machine 140 BW-80** (Design §6, [BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md)): the netlist with `J3` and `J4`, the Board, the manuals, the tests, the Session case.
6. **Docs** (Design §8): recipes, GLOSSARY, GAME_DESIGN's ladder, the roadmap rows.

## Tests

- `MEMBRANEKB`: the catalogue row and pins; on a bench with the 47k pull-ups and a row driven as an address line, a held key pulls its column low while its row is low and not otherwise; two keys on one column and two rows (SHIFT and a letter) never join the rows (the diodes); a typed press held 60 ms then released 120 ms; the queue dropped at power-off; the character map for every ZX80 key, shifted ones included; the tokenizer on a program listing (keywords at a statement's start and after THEN, `**`, a string keeping its spaces, a word that is no keyword typed letter by letter, `GO TO` and `GOTO`).
- `KeyMatrix`: the BW-2001's cases unchanged after the refactor.
- `EXPANSION`: 46 pins, their names and pads, no power pin, never woken.
- Window layout: the ZX80 caps cover every key once, inside the panel, none overlapping.
- `bw place`: `size` fixes the outline; `at` puts a part's courtyard centre where it says; `next` works beside it; a free resistor lands near its chip; a header on the back edge lies along it; refusals (a part placed twice, a part off the Board, `at` without `size`, `at` with `grid pet`, a bad number).
- Machine (release-only but the first): the netlist loads without warnings; the `K` cursor screen (P5v, kept); `PRINT 2+2` NEW LINE typed through `J3` prints `4` and the report `0/0`, read from the display file in RAM and matched on the TV; a two-line program entered and run; the SHIFT symbols (`"`, `+`) through the tokenizer; a key held on a floating row does nothing. After the Board: the same cases built from the Board, `check-board` ok, the per-Machine Board loops, `session: bw80 with every instrument`.
- Release: `bw run netlists/bw80.bwn --time 2s` (the ratio against P5v's 0.25×); `bw_ui` on the Board with every instrument (ratio, UI ms); screenshots viewed: the `K` cursor, a typed line, the keyboard window.

Design: [BW80_MACHINE_DESIGN.md](BW80_MACHINE_DESIGN.md). Plan: [BW80_MACHINE_PLAN.md](BW80_MACHINE_PLAN.md).

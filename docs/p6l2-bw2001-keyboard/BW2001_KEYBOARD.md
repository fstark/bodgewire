# P6l2 — BW-2001 keyboard

Roadmap step P6l2 ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the BW-2001's keyboard window drawn like a real PET 2001's, matched to a photograph the user supplied. It follows P6l, the BW-80's keyboard ([BW80_KEYBOARD.md](../p6l-bw80-keyboard/BW80_KEYBOARD.md)). A UI change only:
- `src/ui/matrix_keyboard_window.cpp`;
- `src/ui/pet_graphics.hpp`, new;
- the layout in `src/view/matrix_keyboard_layout`.

The key matrix (`Bw2001Keys`), the Part and the typing are unchanged. Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

P5q drew the 2001 as a typewriter: staggered chiclets with RVS left of Q and a five-wide SPACE. The real 2001's keyboard is a point-of-sale keyboard (Commodore bought a POS company to make the PET, the user): a strict grid of square keys, each printed with the graphic that SHIFT types, with the special keys in colour.

## User decisions (2026-11-18)

| Question | Decision |
|---|---|
| Layout | the photograph's grid, the key matrix unchanged |
| Graphics on the keys | drawn from the PET's character ROM (`machines/130-bw2001/roms/901447-08.bin`) |
| Roadmap | a new row after P6l, this note only, no design, plan or review |

## What changed

- **Layout**: an 11-column main block with no stagger. Rows: `! " # $ % ' & \ ( ) ←`; Q–P, ↑; A–L, `:`, RETURN two rows high; Z–M, `, ; ?`; SHIFT, OFF/RVS, `@ [ ]`, SPACE two keys wide, `< >`, RUN/STOP, SHIFT. The numeric pad is unchanged, one key from the block. `MatrixCap` gained a `height` in rows (RETURN's 2), and the layout test checks heights. `kMatrixKeyboardWidth` is now 16 and `kMatrixPadX` 12.
- **Keys**: a cream body around a rounded top. The tops are grey on the main block and near-white on the pad. The coloured tops carry white words: red SHIFT, SPACE, RETURN (its letters stacked), CLR/HOME and INST/DEL (the photograph's paler two are worn stickers under the lighting, the user), and blue OFF/RVS, RUN/STOP and the two CRSR keys (arrows above and below). The keys nearly touch. The legends are black and bold (the keycap font struck twice a device pixel apart), and the pad's 0 is slashed.
- **Graphics**: above each legend, the character SHIFT and the key type, drawn from the character ROM's 8 × 8 bitmap. A key types its character c and SHIFT sets bit 7, which the screen shows as code c (letters, `@ [ ]`, ← ↑) or c + 0x40 (digits and symbols): A ♠, ↑ π, the pad's digits the box corners. `kPetGraphics` holds screen codes 0x40–0x7F, copied from the ROM. Each glyph pixel is a whole number of device pixels, so the checks stay even.

## Checked

- Screenshots of `machines/130-bw2001/bw2001.bwn` viewed and compared with the photograph.
- `ctest --preset release -R "matrix|keyboard|layout|manual"`, then the full release suite.

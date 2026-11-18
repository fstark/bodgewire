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
- **Case**: white-painted metal (the user, from a second photograph), the black plates under the keys kept.
- **Keys**: symmetrical, from the user's close-up: a cream body, a raised rim, and in its recess a centred foil label, lighter at the top, with a shadow along its top and left edge. The graphic sits above the label's centre and the legend below it. The labels are silver on the main block and near-white on the pad. The coloured labels carry white words: red SHIFT, SPACE, RETURN (its letters stacked), CLR/HOME and INST/DEL (the photograph's paler two are worn stickers under the lighting, the user), and blue OFF/RVS, RUN/STOP and the two CRSR keys (arrows above and below). The keys nearly touch. The legends are black, in Saira Expanded Bold (below), and the pad's 0 is slashed. The words on the coloured labels are sized to fill 86 % of the label's width.
- **Graphics**: above each legend, the character SHIFT and the key type, drawn from the character ROM's 8 × 8 bitmap. A key types its character c and SHIFT sets bit 7, which the screen shows as code c (letters, `@ [ ]`, ← ↑) or c + 0x40 (digits and symbols): A ♠, ↑ π, the pad's digits the box corners. `kPetGraphics` holds screen codes 0x40–0x7F, copied from the ROM. Each glyph pixel is a whole number of device pixels, so the checks stay even. A very thin square, the character cell's outline, is printed around each graphic but the four card suits (A, S, Z, X), as in the user's close-ups. Sizes measured on those close-ups: the graphics about 0.18 of the key, the capitals 0.17 high.
- **Font** (the user, 2026-11-19, from a list of free fonts): Saira, SIL OFL, cut to a static Expanded Bold (width 125, weight 700) standing in for the 2001's Eurostile Bold Extended. Embedded like Roboto: `src/ui/fonts/` (the font, its licence, how it was cut), `pet_font.cpp.in`, `kPetFont`; `MatrixKeyboardWindow` takes it as a second font.

## Checked

- Screenshots of `machines/130-bw2001/bw2001.bwn` viewed and compared with the photograph.
- `ctest --preset release -R "matrix|keyboard|layout|manual"`, then the full release suite.

## Left for later

- The printed graphics are smooth drawings, not the ROM's pixels: the ♣ on X shows the difference most (the user, 2026-11-19). A roadmap row, *Smooth PET key graphics*.

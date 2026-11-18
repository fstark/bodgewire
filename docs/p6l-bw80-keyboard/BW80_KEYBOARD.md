# P6l — BW-80 keyboard

Roadmap step P6l ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the BW-80's keyboard window drawn to look like a real ZX80's, matched to a photograph the user supplied. A UI change only: `src/ui/matrix_keyboard_window.cpp` and the ZX80 layout constants in `src/view/matrix_keyboard_layout.hpp`. The key table (`Zx80Keys`), the Part and the typing are unchanged. Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

P5x's membrane was a first sketch, and it was off in ways that looked ugly next to the real thing. The blue was too harsh and the black too deep. The arrows touched the keys, the labels were too small, and BREAK and SHIFT were the wrong colour. Every graphic key showed the same mosaic, and the keys were square, with the wrong row stagger.

## User decisions (2026-11-18)

| Question | Decision |
|---|---|
| The surround | the ZX80's cream lip around the black membrane, with two screws in the membrane's bottom corners |
| The `BW-80 · J3` engraving | moulded into the cream bottom lip; the membrane itself carries nothing the real one does not |
| The pound sign | may stay different from the original (the user) |
| Docs | this note only, no design, plan or review (the user) |

## What changed, measured on the photograph

- **Keys**: wider than tall, 0.80 × 0.66 of the key pitch, rounded, with no outline. Rows 1.03 pitches apart, staggered 0, ½, ¾ and ¼ of a key: the Q row is half a key in, the A row a further quarter, and the SHIFT row back to a quarter. The user remembered half a key per row; the photo shows ¼ between the Q and A rows (`kZx80*` in `matrix_keyboard_layout.hpp`).
- **Colours**: mid-blue keys (about `(30, 82, 200)`) with a gentle gradient, and a soft black panel `(26, 24, 29)`. Letters are pale blue-white and keywords white. Shifted words above the digits and EDIT are muted gold. Symbols on the keys and SHIFT's legend are a paler gold. BREAK is dark blue on the black.
- **Legends**: keywords at 0.22 of the pitch, centred over their key. Digits are centred on the key, letters at its lower left. TO is now above the 4, not on it.
- **Arrows**: hollow block arrows outlined in gold, above 5–8, clear of the keys.
- **Graphics**: each key has its own mosaic in a pale frame (the dark line inside it on the photograph is shadow or dirt, the user: none drawn): Q ▌, W ▄, E ▘, R ▝, T grey bottom half, A grey, S ▞, D ▖, F ▗, G grey top half (each drawn from its 8 × 8 bitmap in the ROM's character set, the greys a one-pixel check of gold and the key's blue).
- The cream top lip leaves room for the put-away ×. The window keeps its proportions (`KeepAspect`), now 12.05 × 5.55 pitches.

## Checked

- Screenshots of `machines/140-bw80/bw80.bwn` viewed: idle, and with the keys given to the Machine (gold frame on the case). The graphics were zoomed in and compared with the photograph key by key.
- `ctest --preset release -R "matrix|keyboard|zx80|membrane|manual"`: the layout test now bounds the caps by `kZx80CapHeight`.
- The BW-80 User Manual now says the shifted symbols are printed in gold, not yellow.
- The keycap font was baked at 1× and stretched on a Retina display, blurring the legends (here, on the BW-2001's keyboard and on the Board's keys). ImGui bakes a font at the window's pixel density only for the current font, and the legends are drawn with `AddText` without being pushed. `SharpFont` (`src/ui/bench`) sets the density before drawing, in all three places.

# MEMBRANEKB — the BW-80 keyboard

## Function

The BW-80's keyboard, the ZX80's 40-key membrane: a printed flexible sheet whose contacts lie on the main board's own tracks. There is no decoder and no port chip. The eight rows are the address lines `A8`–`A15`: the ROM reads an even I/O port with one of them low and the others high, and the five columns `KB0`–`KB4`, each held high by a 47k pull-up on the board, come onto `D0`–`D4` through the 74LS365 buffer. A held key carries its row's low to its column. Each row reaches the keys through a diode (the ZX80's D3–D10, inside this Part), so that two keys on one column, SHIFT and a letter, never join two address lines; a key on a row whose line is floating does nothing.

| Row | Line | `KB0` | `KB1` | `KB2` | `KB3` | `KB4` |
|---|---|---|---|---|---|---|
| 0 | `A8` | SHIFT | Z `:` | X `;` CLEAR | C `?` CLS | V `/` GOSUB |
| 1 | `A9` | A LIST | S STOP | D DIM | F FOR | G GOTO |
| 2 | `A10` | Q NEW | W LOAD | E SAVE | R RUN | T CONT |
| 3 | `A11` | 1 NOT | 2 AND | 3 THEN | 4 TO | 5 ← |
| 4 | `A12` | 0 RUBOUT | 9 HOME | 8 → | 7 ↑ | 6 ↓ |
| 5 | `A13` | P `*` | O `)` PRINT | I `(` INPUT | U `$` IF | Y `"` REM |
| 6 | `A14` | NEW LINE EDIT | L `=` | K `+` LET | J `-` RAND | H `**` POKE |
| 7 | `A15` | SPACE £ BREAK | . `,` | M `>` | N `<` NEXT | B OR RET |

Each cell: the key, its shifted symbol or word, and the keyword it types alone while the cursor is `K` (BREAK is SPACE itself, read without SHIFT while a program runs). The ZX80's ROM reads the keyboard once a frame, takes a key read twice in a row, and accepts a new key only after five frames with no key; a typed key is held 60 ms and released 120 ms.

The keyboard is drawn in its own window. Keys are pressed with the mouse or typed on the PC's keyboard by character; a pasted listing is typed keyword by keyword.

## Pins

- `A8`, `A9`, `A10`–`A15`: the rows (pads 1–8), the address lines, read
- `KB0`–`KB4`: the columns (pads 9–13), each pulled low through a diode by a held key on a low row, otherwise released to the board's pull-up

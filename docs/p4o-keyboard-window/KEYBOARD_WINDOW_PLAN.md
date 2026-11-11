# P4o Keyboard window — Implementation Plan

Design: [KEYBOARD_WINDOW_DESIGN.md](KEYBOARD_WINDOW_DESIGN.md)
Spec: [KEYBOARD_WINDOW.md](KEYBOARD_WINDOW.md)

Done 2026-11-11, checked by the user.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Control codes through the Session, datasheet | `23a4d87` |
| 2 | Keyboard layout | `5ef5183` |
| 3 | Keyboard window, font, capture by focus | `785e7a7` |
| 4 | Release check, recipes, docs | this commit |

## Implementation notes

- Phase 3 look, tuned from screenshots: the first version's pressed caps (sunk 10 %, darkened 15 %) were hard to see, and the case was flat. Now a pressed cap's top drops 12 % into its skirt and its face darkens to 72 %. The case has a vertical gradient (46 → 28 grey) and a near-black well behind the keys, with a shadowed top lip. The skirt is 28 grey, so the caps stand out from the well. The shifted symbol on `K`–`P` sits higher, because `_` over `O` read as a macron. The engraving is lighter (128 grey). The other values are as in the design.
- Release check: `bw_ui tests/data/ram_bench.bwn --type 'J1:100ms:!!!!!!!!!!!!!!!'` at 1.00× shows `1` and both SHIFTs down, no amber edge, no Typing banner. `--type 'J1:100ms:THE QUICK BROWN FOX'` draws the window at the bottom of the Board area (at 1 s the key sent is the space). `puzzles/gibberish.bwp` has no keyboard window and the same UI font as before. On 070 (`long_paddle.bwp`, probe, 8 clips, 2 hooks, meter): 1.00×, UI 1.4 ms (1.2 ms in P4n). Debug tests: 763 passed.
- User check (done 2026-11-11, all passed): `bw_ui tests/data/ram_bench.bwn`:
  - F2 focuses the keyboard: the case glows amber and the banner says Typing.
  - Typing `HELLO` moves the caps down and up.
  - `!` typed on the PC draws SHIFT and `1` down.
  - Ctrl-G, Ctrl-Shift-2 and Escape go out (Debug, Keys sent: `^G`, `^@`, `[ESC]`).
  - Clicking caps with the mouse types them; SHIFT latches.
  - Clicking the Board stops typing; F2 starts and stops it.
  - `U` does not open the manual while typing.
  - The window resizes with its caps, and closing it and pressing F2 reopens it focused.
- Docs review (before phase 1), fixed:
  - `NoFocusOnAppearing` (else typing at launch) and `NoCollapse`.
  - Ctrl codes from the shifted key, so `^@ ^^ ^_` are reachable; Ctrl with a digit sends the digit.
  - The title bar counted in the window height, and `u` taken from both sides.
  - The default font chosen by pixel density, as ImGui would choose it lazily.
  - Legend sizes in whole pixels.
  - The window drawn outside the Machine-only block.
  - F2 handled before the early return; `capture.stop` handled after `Begin`.
  - A surround colour, so the case's corners show.
  - The SHIFT latch kept until the clicked cap is released.
  - Modifiers read from `SDL_GetModState`.

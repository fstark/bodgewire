# P4o Keyboard window — Implementation Plan

Design: [KEYBOARD_WINDOW_DESIGN.md](KEYBOARD_WINDOW_DESIGN.md)
Spec: [KEYBOARD_WINDOW.md](KEYBOARD_WINDOW.md)

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Control codes through the Session, datasheet | |
| 2 | Keyboard layout | |
| 3 | Keyboard window, font, capture by focus | |
| 4 | Release check, recipes, docs | |

## Phase 1 — Control codes

Design §2: `kKeyboardLastCode`, `Session::typeCode`, `type` through it, `bw_ui --type` on `typeCode` (the flash comes in phase 3), `KEYBOARD.md`. Test `session: typing control codes` (§9).

Fence: `cmake --build --preset debug && ctest --preset debug -j 12` green; `clang-format` on the changed C++ files.
Commit: `session: control codes to the keyboard (CTRL, ESC)`

## Phase 2 — Keyboard layout

Design §3: `src/view/keyboard_layout.{hpp,cpp}` in `src/view/CMakeLists.txt`, `tests/view/keyboard_layout_test.cpp` (§9) in `tests/CMakeLists.txt`.

Fence: debug build and tests green; clang-format.
Commit: `view: bit-paired ASCII keyboard layout`

## Phase 3 — Keyboard window

Design §4–§7: the embedded font (CMake, template, the default font added first by pixel density), `KeyboardWindow`, `KeyboardCapture` reworked, `kBoardShare` to `toolbar.hpp`, side panel Debug line, banner, main wiring and `--type` flashes.

Fence: debug build and tests green; clang-format; release build; screenshots viewed (release only: the flashes need 1.00×, the ratio printed):
- `build/release/src/ui/bw_ui tests/data/ram_bench.bwn --type 'J1:100ms:THE QUICK BROWN FOX' --screenshot <scratch>/kb.png --after 1s`: the window at the bottom of the Board area, every legend legible, one cap down, no amber edge and no Typing banner (the window does not take focus by appearing). The look is judged at this point, and the colours, insets, legend sizes and widths are tuned until it reads as a vintage keyboard (zoom by cropping the PNG).
- `--type 'J1:100ms:!!!!!!!!!!!!!!!'`: `1` and both SHIFTs down.
- `puzzles/gibberish.bwp`: no keyboard window, UI otherwise unchanged against a screenshot taken before the phase, compared by eye (the TV and the time differ between runs), the UI font in particular (§5).

Commit: `ui: the keyboard window`

## Phase 4 — Release check and docs

- Release: `ctest --preset debug -j 12` green; the phase 3 screenshots again; 070 with every instrument at 1.00× in `bw_ui` (UI ms noted: the window draws about 60 caps only when a keyboard exists, so 070 is unchanged).
- Docs (design §8): GLOSSARY Typing, ADD_CONNECTOR_DEVICE (UI row, capture bullet, reference commits), ARCHITECTURE P4o row done with the date and links, this plan cut to the status table and notes ("user check pending").

Commit: `docs: P4o implemented, user check pending`

## Implementation notes

- User check (pending): `bw_ui tests/data/ram_bench.bwn`: F2 focuses the keyboard and the case glows, the banner says Typing; typing `HELLO` moves the caps down and up; `!` typed on the PC draws SHIFT and `1` down; Ctrl-G and Escape go out (Debug: `^G`, `[ESC]`); clicking caps with the mouse, SHIFT latching; clicking the Board stops typing; F2 starts and stops it; `U` does not open the manual while typing.
- Docs review (before phase 1), fixed: `NoFocusOnAppearing` (else typing at launch) and `NoCollapse`; Ctrl codes from the shifted key (`SDL_GetKeyFromScancode` with Shift) so `^@ ^^ ^_` are reachable, Ctrl with a digit sends the digit; the title bar in the window height and `u` from both sides; the default font chosen as ImGui's lazy choice by pixel density; legend sizes in whole pixels; the window drawn outside the Machine-only block; F2 before the early return and `capture.stop` handled after `Begin`; a surround colour so the case's corners show; the SHIFT latch kept until the clicked cap is released; modifiers from `SDL_GetModState`; screenshots in release at 1.00×.

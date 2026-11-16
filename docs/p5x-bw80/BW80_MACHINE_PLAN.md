# P5x BW-80 (ZX80) — Implementation Plan

Spec: [BW80_MACHINE.md](BW80_MACHINE.md). Design: [BW80_MACHINE_DESIGN.md](BW80_MACHINE_DESIGN.md). Machine: [BW80.md](../machines/140-bw80/BW80.md), [BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md).

Every phase ends with its fence: `cmake --build --preset debug` clean, `clang-format` on the C++ files touched, the tests the phase names green in debug (and the release-only cases in release where it says so), one commit of this step's own paths with the message given. Machine cases are `[slow]`.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `4b19397` |
| 1 | `PressQueue`, `KeyMatrix`: the BW-2001's keyboard refactored | `cf1630e` (with 2) |
| 2 | ZX80 keys, the tokenizer, `MEMBRANEKB`, `EXPANSION` | `cf1630e` |
| 3 | Session, CLI, capture, the window's ZX80 look | `101eeb4` |
| 4 | Netlist with `J3`, `J4`; manuals; the typing cases | `13f4b0e` |
| 5 | `bw place`: `size`, `at` | this commit |
| 6 | Board: placed, routed by the user, wired into the tests | |
| 7 | Session case, release check, recipes, docs, full suite | |

## Phases

### Phase 0 — Docs reviewed

A fresh sub-agent reviews the spec, design, plan and Machine docs against the code (AGENTS.md step 3); every must-fix goes into the docs. Commit: `docs: P5x spec, design and plan; Machine 140 BW-80 docs`.

### Phase 1 — `PressQueue`, `KeyMatrix` (Design §2.1)

- `src/parts/press_queue.{hpp,cpp}`: the queue and phases out of `MatrixKeyboard`, with `kMatrixHold`, `kMatrixGap`, `kMatrixWaiting`.
- `src/parts/key_matrix.{hpp,cpp}`: `MatrixPress`, the named-key characters, `KeyMatrixKind`, `KeyMatrix`.
- `MatrixKeyboard` on both; `type(std::u32string_view)` beside `type(char32_t)`; `KeyMatrixOf` in the builder.
- `Session` on `KeyMatrixOf` (`matrixKind`, `typeText`), `bw run` and `bw_ui` `--type` through the text form (`Typing::text`), `MatrixCapture`'s paste through `typeText`.
- Tests: `tests/parts/press_queue_test.cpp` (registered in `tests/CMakeLists.txt`); the BW-2001's cases unchanged; `tests/machine/typing_test.cpp`'s refusal of `{` becomes a check of `text` and `untypable`.

Fence: `ctest --preset debug -j 12 -R "matrix|press queue|session|typing|bw2001"`; release `ctest --preset release -R "bw2001 machine: stage 3"` (typing through `J5`). Commit: `parts: PressQueue and KeyMatrix, the BW-2001 keyboard refactored for a second kind (P5x)`.

### Phase 2 — ZX80 keys, the tokenizer, `MEMBRANEKB`, `EXPANSION` (Design §2.2, §2.3, §3)

- `src/parts/zx80_keys.{hpp,cpp}`: `Zx80Keys`, `Zx80KeyAt`, `Zx80KeyFor`, `Zx80Presses`.
- `src/parts/membrane_keyboard.{hpp,cpp}`, the catalogue rows, `ConnectorPins` for both, the two footprints copied into `boards/footprints/` and listed in `kFootprints`, `datasheets/MEMBRANEKB.md`, `datasheets/EXPANSION.md`; the sources and tests in the CMake lists.
- Tests: `zx80_keys_test.cpp`, `membrane_keyboard_test.cpp`, `connector_test.cpp` (`EXPANSION`), `catalogue_test.cpp` if it counts rows, `manuals_test.cpp` (the datasheets' pin names).

Fence: `ctest --preset debug -j 12 -R "zx80 keys|membrane keyboard|connector|catalogue|manuals|footprint"`. Commit: `parts: MEMBRANEKB, the BW-80 keyboard with the ZX80's keys and a paste tokenizer; EXPANSION, the ZX80's edge connector (P5x)`.

### Phase 3 — Session, CLI, capture, window (Design §4)

- `Session::matrixKeyboards` lists both kinds; `matrixKind`.
- `MatrixCapture` by kind; `Zx80Caps` in the layout; the window's ZX80 look.
- Tests: `matrix_keyboard_layout_test.cpp` (`Zx80Caps`); `session_test.cpp` `session: typeText on a MEMBRANEKB` on a bench netlist (`tests/data/bw80_keys.bwn`: the Membrane keyboard, pull-ups, rows on ties, as the Part test's bench).
- Release: `bw_ui tests/data/bw80_keys.bwn --screenshot` with the window: viewed and tuned until it reads as the ZX80's keyboard (memory: a drawn device, iterate on screenshots).

Fence: `ctest --preset debug -j 12 -R "layout|session|matrix"`; the screenshot viewed. Commit: `ui: the BW-80 keyboard window, PC typing and paste by kind (P5x)`.

### Phase 4 — Netlist, manuals, the typing cases ([BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md))

- `netlists/bw80.bwn`: header, `J3`, `J4`, `BUSAK_N`.
- `manuals/bw80/user.md`, `service.md`; `"bw80"` in `manuals_test.cpp`.
- `tests/machines/bw80_screen.{hpp,cpp}` (registered in `tests/CMakeLists.txt`); the cases "PRINT 2+2 typed through J3", "a program is entered and run" (release-only), "a key on a floating row does nothing" (debug).

Fence: `ctest --preset debug -j 12 -R "bw80|manuals"`; release `ctest --preset release -R "bw80"` (all the picture and typing cases). Commit: `bw80: the keys and the edge connector on the ZX80's netlist, manuals, typing on the ZX80 ROM (P5x)`.

### Phase 5 — `bw place`: `size`, `at` (Design §5)

- `place_rules.{hpp,cpp}`: `size`, `AtRule`; `placer.cpp`: `runFixed`, the outline from `size`, parts at their courtyard centres, `placeFree` over a grid, headers along the back and front edges under both fixed layouts.
- Tests: `place_rules_test.cpp`, `placer_test.cpp` (the `size` and `at` cases); the PET cases unchanged.

Fence: `ctest --preset debug -j 12 -R "place"`. Commit: `place: size and at, a Board and its parts where a real board has them (P5x)`.

### Phase 6 — Board

- `boards/bw80/bw80.place` as the Machine design; `./build/release/src/cli/bw place netlists/bw80.bwn boards/bw80/bw80.place -o boards/bw80/bw80.kicad_pcb`; the wire length and the Board viewed (`bw_ui netlists/bw80.bwn --screenshot`).
- **Stop for the user**: route in KiCad (ADD_MACHINE §3); the layout constraints: none from this step (P5y's crack Puzzles come later and use whatever route order the user chose).
- After the routing: `bw check-board` ok; `"bw80"` in `boards_test.cpp`, `electrical_test.cpp`, `board_scene_test.cpp`; `bw80_test.cpp` built from the Board (`BoardCircuit`), the netlist kept for "loads without warnings" and the text variants.

Fence: `ctest --preset debug -j 12 -R "bw80|boards|electrical|board_scene|drc_bw80|place_drc_bw80"`; release `ctest --preset release -R "bw80"`. Commit: `bw80: Board after the ZX80 issue 2 layout, routed by the user; Machine cases on the Board (P5x)`.

### Phase 7 — Session case, release check, docs

- `session: bw80 with every instrument`.
- Release: `bw run netlists/bw80.bwn --time 2s` (median of 3, with `bus_load_z80` beside it); `bw_ui netlists/bw80.bwn --type "J3:1s:PRINT 2+2\n" --screenshot` at 1 s and 3 s, with every instrument (`--probe`, `--clip`, `--hook`, `--meter`): the ratio and UI ms; the `K` cursor, the typed line, the keyboard window viewed.
- Recipes (Design §8), GLOSSARY, GAME_DESIGN row 140, ARCHITECTURE's P5x row done (date, ratios, links); this plan cut to the status table and notes.
- The full debug suite.

Fence: `ctest --preset debug -j 12` all green. Commit: `docs: P5x done, the BW-80 is a Machine`.

## Implementation notes

- Phases 1 and 2 written while the review ran and committed together: phase 2's Part is what `KeyMatrixOf` and `matrixKeyboards()` name, so phase 1 alone would not build. The review's findings went into the code before the docs commit: the ROM accepts a new key only after five no-key frames (`kMembraneGap` 120 ms), BREAK is SPACE alone, `MEMBRANEKB` sorts after `MC3446`, the datasheets' pin ranges, `PressQueue::step` returning the delay instead of taking an `EvalContext` (none can be made in a test). The tokenizer's "after a keyword" state is tracked, not read back from the last press (a letter key has a keyword too); strings keep their spaces. Fences: phase 1 `matrix|press queue|session|typing|bw2001` green in debug, the BW-2001's typing case in release; phase 2 `zx80 keys|membrane keyboard|connector|catalogue|manuals|footprint` green in debug.
- Phase 3: the window draws by the capture's kind; the cap loop (buttons, latch, the Session's held keys) is one template over both looks. The ZX80 look after Searle's overlay: black panel, blue caps, white letters and keywords above, yellow shifted legends (words above the digits and NEW LINE, symbols on the cap, graphics as a mosaic, arrows drawn). The Session case runs netlist-only (`Session(std::nullopt, …)`, as `bw_ui` runs a bench: `LoadNetlistMachine` wants a Board) and checks the press after the run pauses. Screenshot on `tests/data/bw80_keys.bwn` viewed: every key with its three legends, at 1.00×.
- Phase 4: `J3` and `J4` on the netlist (46 + 13 pins, `BUSAK_N` a Net now); the ROM's report after a direct command reads `0/-2` (code 0, line -2: the edit line), not `0/0` as the Machine docs guessed (fixed there); after `RUN` of a two-line program `0/20`. The TV needs about 100 ms to lock again once the picture returns, so the PRINT case allows 600 ms. The display-file reader (`Bw80DisplayRows`) reads D_FILE out of IC3 and IC4. The Machine case for a floating keyboard row was dropped: the ROM scans the keys only once its RAM is cleared (160 ms, minutes in debug), and the Part's bench already proves the diode. Release: `PRINT 2+2` 6.8 s, the program 10 s, the four P5v cases unchanged; debug `bw80|manuals` green.
- Phase 5: `size` and `at` as designed; a free part under `size` searches a 2.54 mm grid of the outline inside the bare margin for the spot nearest its Nets' parts (or the centre) whose courtyard, with the part gap around it, overlaps nothing placed; the edge groups go inside the outline with no widening (a group that does not fit "falls off" its edge). `next` beside an `at` part is deferred as beside a PET chip. 39 placement cases green in debug.

# P4l ROM Parts — Implementation Plan

Design: [ROM_PARTS_DESIGN.md](ROM_PARTS_DESIGN.md)
Spec: [ROM_PARTS.md](ROM_PARTS.md); Machine: [MESSAGE.md](../machines/080-message/MESSAGE.md), [MESSAGE_DESIGN.md](../machines/080-message/MESSAGE_DESIGN.md)

Every phase ends with its fence: `cmake --build --preset debug` clean (`-Werror`), `clang-format` on the C++ files touched, `ctest --preset debug -j 12` green (the `[slow]` cases included unless the phase says otherwise), then one commit with the message given. Only this step's own paths are committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Contents format | |
| 2 | DIP-24 package | |
| 3 | ROM Parts (2716, 2513), contents paths | |
| 4 | 74LS165 | |
| 5 | Netlist, ROM files, fault-free test | |
| 6 | Manuals | |
| 7 | Board (user routing), per-Machine loops | |
| 8 | Session test, release check, recipes, docs | |

## Phase 1 — Contents format

- `src/parts/rom_contents.{hpp,cpp}` (design §2), `src/parts/CMakeLists.txt`.
- `tests/parts/rom_contents_test.cpp` (design §8), `tests/CMakeLists.txt`.
- Fence: the new cases green. Commit `parts: ROM contents format`.

## Phase 2 — DIP-24 package

- Copy `DIP-24_W15.24mm.kicad_mod` from the installed KiCad 10 into `boards/footprints/` unchanged; `kFootprints` row (design §5).
- Tests: `kicad_netlist_test` DIP-24; the footprint loads with 24 pads (`LoadFootprints` asked for `DIP-24`). Nothing in the catalogue uses it yet.
- Fence as above. Commit `board: DIP-24 package`.

## Phase 3 — ROM Parts, contents paths

- `src/parts/rom.{hpp,cpp}` with the 2716 and 2513 specs (design §3), delays, `CreateRom` and the two catalogue rows; datasheets `2716.md`, `2513.md`.
- `ResolveContentsPaths` in the builder, called from `LoadNetlistFile` and `LoadMachine` (design §6).
- `PartPinNames` takes ROM names from `FindRomSpec` (design §3); `tests/data/empty.rom`.
- Tests: `tests/parts/rom_test.cpp`; `catalogue_test` (pin count from the package, ROMs with `contents=`, `2513` in `kOffCornerPower`); the builder cases; `placer_test`: a 2716 with no rule refused ("needs a placement rule"), with a `row front middle` rule placed inside its band (design §7).
- Fence: `manuals: every catalogue Part has a Datasheet` green with the new rows, their pins covered; `footprint: every catalogue package loads` green. Commit `parts: 2716 and 2513 ROMs with contents files` (the commit says the 2716 delays are Intel's maximums, from known values, and the 2513's come from the 1972 Signetics MOS data book).

## Phase 4 — 74LS165

- `src/parts/ls165.{hpp,cpp}` (design §4), delays, catalogue row, `datasheets/74LS165.md`, `tests/parts/ls165_test.cpp`.
- Fence as above. Commit `parts: 74LS165` (TI typicals from known values, the PDF not fetched).

## Phase 5 — Netlist, ROM files, fault-free test

- `roms/2513.rom` and `roms/message.rom` (drafted while the docs were reviewed: check them against MESSAGE_DESIGN.md § ROM contents and commit them here), `netlists/message.bwn` (MESSAGE_DESIGN.md § Netlist, with the header comment).
- `tests/machines/message_test.cpp` on the netlist (design §8).
- Release speed: `build/release/src/cli/bw run netlists/message.bwn --time 2s` on an idle machine; record it. Gate: ≥ 1.5× headless (060: 2.12×, 070 at 1.46× still gave 1.00× in `bw_ui`); below that, stop and tell the user before the Board.
- A first look: `build/release/src/ui/bw_ui netlists/message.bwn --screenshot /tmp/message.png --after 1s` (no Board yet: the TV alone), view the PNG.
- Fence: the message cases green, release speed recorded. Commit `message: netlist, ROM contents, fault-free test`.

## Phase 6 — Manuals

- `manuals/message/user.md`, `service.md` (MESSAGE_DESIGN.md § Manuals); `message` in `tests/machines/manuals_test.cpp`.
- Fence: manuals cases green. Commit `message: manuals`.

## Phase 7 — Board

- `boards/message/message.place` (MESSAGE_DESIGN.md § Board), then `build/release/src/cli/bw place netlists/message.bwn boards/message/message.place -o boards/message/message.kicad_pcb`; record the grid and wire length; reconfigure so `place_drc_message` appears and passes.
- **Stop for the user**: route `boards/message/message.kicad_pro` in KiCad (KICAD_GUIDE.md steps 7–8, F.Cu/B.Cu, no zones, DRC 0 errors). Placement is not theirs to change.
- After: `bw check-board netlists/message.bwn boards/message/message.kicad_pcb` prints `ok`; `drc_message` passes; `message` added to the per-Machine loops (ADD_MACHINE.md step 3), with `ResolveContentsPaths` in `electrical_test`'s `LoadDesign` (design §7); `message_test.cpp` builds the page cases from the Board (`BoardCircuit`).
- Fence: everything green. Commit `message: Board` (only `message.place`, `.kicad_pro`, `.kicad_pcb` and the test changes).

## Phase 8 — Session test, release check, docs

- `session: message with every instrument` (8 clips, 2 hooks, probe, meter).
- Release: `bw_ui netlists/message.bwn --screenshot /tmp/message.png --after 1s --probe … --clip … --hook … --meter …`: 1.00×, UI time under ~1 ms; view the PNG (the page, sharp, a border all round).
- Recipes: ADD_PART (ROM family, file parameter, DIP-24), ADD_MACHINE (080 commits, `roms/`); GLOSSARY **ROM Contents**.
- ARCHITECTURE row P4l done with the date and links; this plan cut to the status table and implementation notes, "user check pending".
- Fence: everything green. Commit `docs: P4l implemented, user check pending`.

## Implementation notes

- Docs review (before phase 1): the netlist loads without warnings and the circuit holds (six dots F5–F1, 0 a character; blanking in step). Fixed: the text starts at TV column ~44, not 48 (`U17` re-times sync one dot late, as on 060); the page search accepts a run of up to 4 horizontal offsets (a dot is 4 columns) and stops each offset at its first mismatch; DIP-24 before the ROM rows (the footprint test loads every catalogue package); the catalogue test derives the pin count from the package and gives ROMs `contents=` (`tests/data/empty.rom`); `PartPinNames` takes ROM names from the spec so the datasheets are checked; `electrical_test` resolves `contents=`; no X on the 74LS165's edge-only `CLK`; `;` inside a string is not a comment; the ROM delay simplification documented.

# P4l ROM Parts — Implementation Plan

Done 2026-11-11, checked by the user.

Design: [ROM_PARTS_DESIGN.md](ROM_PARTS_DESIGN.md)
Spec: [ROM_PARTS.md](ROM_PARTS.md); Machine: [MESSAGE.md](../machines/080-message/MESSAGE.md), [MESSAGE_DESIGN.md](../machines/080-message/MESSAGE_DESIGN.md)

Every phase ends with its fence: `cmake --build --preset debug` clean (`-Werror`), `clang-format` on the C++ files touched, `ctest --preset debug -j 12` green (the `[slow]` cases included unless the phase says otherwise), then one commit with the message given. Only this step's own paths are committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Contents format | `04293c0` |
| 2 | DIP-24 package | `19a244f` |
| 3 | ROM Parts (2716, 2513), contents paths | `9cd6bcb` |
| 4 | 74LS165 | `8b587d1` |
| 5 | Netlist, ROM files, fault-free test | `babed7b` |
| 6 | Manuals | `1635989` |
| 7 | Board (user routing), per-Machine loops | `3dc540c` |
| 8 | Session test, release check, recipes, docs | this commit |

## Implementation notes

- Docs review (before phase 1): the netlist loads without warnings and the circuit holds (six dots F5–F1, 0 a character; blanking in step). Fixed: the text starts at TV column ~44, not 48 (`U17` re-times sync one dot late, as on 060); the page search accepts a run of up to 4 horizontal offsets (a dot is 4 columns) and stops each offset at its first mismatch; DIP-24 before the ROM rows (the footprint test loads every catalogue package); the catalogue test derives the pin count from the package and gives ROMs `contents=` (`tests/data/empty.rom`); `PartPinNames` takes ROM names from the spec so the datasheets are checked; `electrical_test` resolves `contents=`; no X on the 74LS165's edge-only `CLK`; `;` inside a string is not a comment; the ROM delay simplification documented.
- Fences: the full debug suite (`ctest`, about 9 minutes, no `[slow]` label to exclude) ran after phases 1 and 5; phases 2–4 were committed on a clean build and their own tests (footprint, kicad netlist, ROM, catalogue, builder, placer, manuals, 74LS165), which the phase 5 run then covered (711/711).
- Phases 1–4: the code worked as designed; the ROM, contents and 74LS165 tests passed on the first build.
- Phase 5: the page drew right first time (`bw_ui` screenshot: the poem, a dark border). Release headless 2.19× with the machine under load (load average 7; 060 measured 2.12× idle); `bw_ui` 1.00×, UI 0.3 ms. `message: the TV shows the page` 11.6 s in debug for three seeds.
- Phase 6: the design's Service Manual bullets grouped references (`U2`, `U3`; `U6`–`U8`), but the Chip panel finds a role by the bullet's first reference: one bullet each, design fixed.
- Phase 7: `bw place` (seed 1): a 6 × 3 grid, 2.24 m of wire; the ROMs side by side on the front edge. `place_drc_message` passes. The user routed it with Freerouting, which left one connection: `F1`, `U16.14` to `U15.4`, the font ROM's pad boxed in by `F2`–`F4` and `VCC` in the 3.8 mm channel between the ROMs and by B.Cu tracks across `U15`. The agent added it (F.Cu, a via, B.Cu under `VIDEO_D`, a via, F.Cu down inside `U15`), checked on a scratch copy with `kicad-cli pcb drc` (0 violations, 0 unconnected) and `check-board` (`ok`). Functional-group placement should keep the font ROM next to the 74LS165.
- Phase 8: `session: message with every instrument` (8 clips on the ROM outputs and the shift register, hooks on `U1.RCO` and `U16.CLK`, probe on `U16.QH`, meter on `U15.VCC`) 31 s in debug. Release `bw_ui netlists/message.bwn` with the same instruments: 1.00×, UI 0.6 ms; the screenshot shows the poem, `RCO` once every six `CLK` periods, the meter at 5.00 V. ADD_PART.md gained the ROM kind, the file parameter and new packages; ADD_MACHINE.md the 080 commits, `roms/` and the connection Freerouting leaves; GLOSSARY **ROM Contents**.

# P5q2 Commodore's ROMs on the BW-2001 — Plan

Spec: [BW2001_ROMS.md](BW2001_ROMS.md). Design: [BW2001_ROMS_DESIGN.md](BW2001_ROMS_DESIGN.md).

Implemented 2026-11-15; user check pending.

Each phase ended with its fence: `cmake --build --preset debug` clean, `clang-format` on the C++ touched, the named tests green, one commit.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | `ApplyRomDirectory`, `--roms DIR` | `3dbd22c` |
| 2 | `UH2`, `UH6`: netlist, Board (user routes), BASIC 1 on the Board, docs | this commit |

## Implementation notes

- Docs review (after the first docs commit, which went in before it: fixed here), with the user's `--roms DIR` design (2026-11-15, files named by reference) replacing `--rom`/`--roms NAME`: the BASIC 1 case changes only once the Board has `UH2`/`UH6`; no `bw run` option tests exist (the function is tested); the BASIC 1 table stays in the test header; `.bwp` takes the flag too; a `.rom` holds one chip; the directory made absolute (re-read at every build); KiCad 10's import is File → Import → Netlist, its two replace/delete options off, and the tracks through H2 and H6 rerouted; the routing stop inside one phase.
- Phase 1: green after one test fix (a resistor with no Nets in the test's netlist). An error from the directory is reported as a load error (exit as other netlist errors), not a usage error: `bw run` and `bw_ui` print it with the netlist's name.
- Phase 2: the netlist's `UH2`/`UH6` are `UH1`'s and `UH5`'s Nets with their `CS1` on `SELD_N`; our firmware unchanged (it never reads `$D000`–`$DFFF`). The user imported the KiCad netlist and routed the two chips (2026-11-15); `check-board` then found two connections the rerouting had broken (`BD_TO_D_N`: `UE5` pin 6 to the data buffers' `2OE_N`; `CASS_WRITE`: `J6` pad 5 to the rest), which Freerouting failed to add and which were too crowded to route by hand. A script routed them on a scratch copy (A* on a 0.1 mm grid over both layers around the two ends, every other Net's tracks, pads and vias as obstacles inflated by the Board's 0.2 mm clearance plus 0.05 mm, vias where both layers allow, the last point snapped onto the Net's copper): `BD_TO_D_N` 38 segments and 3 vias, `CASS_WRITE` 60 segments and 10 vias across the Board; KiCad DRC 0 unconnected, no errors (the two silkscreen warnings), `check-board` ok; copied back with KiCad closed and the Board unchanged since. The BASIC 1 case now runs on the Board through `--roms` (a temporary directory of links). `external/roms/pet-basic1/` made for the user; the board-less `external/bw2001_basic1.bwn` removed. Release `bw_ui netlists/bw2001.bwn --roms external/roms/pet-basic1`: the BASIC 1 banner on the Board (screenshot viewed).
- `tools/route_missing.py` (the user's request): the router as a repository tool, every open connection `check-board` reports routed in turn, then KiCad's DRC on the copy; checked on the Board as it was before the fix (both connections, 53 s, check-board ok, DRC 0 unconnected); ADD_MACHINE §3 says when to use it. Fence `bw2001|boards|electrical|scene|session|drc|manuals|place|rom` 198/198 in debug.

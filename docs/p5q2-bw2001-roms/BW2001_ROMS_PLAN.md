# P5q2 Commodore's ROMs on the BW-2001 — Plan

Spec: [BW2001_ROMS.md](BW2001_ROMS.md). Design: [BW2001_ROMS_DESIGN.md](BW2001_ROMS_DESIGN.md).

Each phase ends with its fence: `cmake --build --preset debug` clean, `clang-format` on the C++ touched, the named tests green, one commit.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | `ApplyRomDirectory`, `--roms DIR` | this commit |
| 2 | `UH2`, `UH6`: netlist, Board (user routes), BASIC 1 on the Board, docs | |

### 1. `ApplyRomDirectory`, `--roms DIR` (design §1, §2, §4 first item)

Fence: `-R "rom|machine|external"`; by hand in release: `bw run netlists/bw2001.bwn --roms <a directory with a bad file>` exits 2 with the message. Commit `machines: ROMs chosen at run time from a directory, --roms (P5q2)`.

### 2. `UH2`, `UH6`: netlist, Board, BASIC 1 on the Board, docs (design §3, §4 second item)

The netlist, `roms/empty.rom`, the manuals; the KiCad netlist exported to a scratch path. **Stop for the user** (import, place, reroute, route; design §3). Nothing is committed during the stop (the Board loops and `drc_bw2001` are red until the routing). Then `check-board`; `-R "bw2001|boards|electrical|scene|session|drc|manuals|place|rom"`; the release-only BASIC 1 case on the Board; `bw_ui netlists/bw2001.bwn --roms external/roms/pet-basic1` screenshot; the docs (design §3). Commit `bw2001: UH2 and UH6, Commodore's ROMs on the Board (P5q2)`.

## Implementation notes

- Docs review (after the first docs commit, which went in before it: fixed here), with the user's `--roms DIR` design (2026-11-15, files named by reference) replacing `--rom`/`--roms NAME`: the BASIC 1 case changes only once the Board has `UH2`/`UH6`; no `bw run` option tests exist (the function is tested); the BASIC 1 table stays in the test header; `.bwp` takes the flag too; a `.rom` holds one chip; the directory made absolute (re-read at every build); KiCad 10's import is File → Import → Netlist, its two replace/delete options off, and the tracks through H2 and H6 rerouted; the routing stop inside one phase.
- Phase 1: green after one test fix (a resistor with no Nets in the test's netlist). An error from the directory is reported as a load error (exit as other netlist errors), not a usage error: `bw run` and `bw_ui` print it with the netlist's name.

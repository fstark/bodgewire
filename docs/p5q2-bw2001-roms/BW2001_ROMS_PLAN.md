# P5q2 Commodore's ROMs on the BW-2001 — Plan

Spec: [BW2001_ROMS.md](BW2001_ROMS.md). Design: [BW2001_ROMS_DESIGN.md](BW2001_ROMS_DESIGN.md).

Each phase ends with its fence: `cmake --build --preset debug` clean, `clang-format` on the C++ touched, the named tests green, one commit.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | ROM overrides, `--rom`, `--roms` | |
| 2 | `UH2`, `UH6` in the netlist and manuals; the Board's import (user routes) | |
| 3 | The Board, BASIC 1 on it, release check, docs | |

### 1. ROM overrides, `--rom`, `--roms` (design §1, §2, §4)

Fence: `-R "rom|cli|machine|bw2001 bench|external"`. Commit `machines: ROMs chosen at run time, --rom and --roms (P5q2)`.

### 2. `UH2`, `UH6` in the netlist and manuals; the Board's import (design §3)

The netlist, `roms/empty.rom`, the manuals; the KiCad netlist exported. Nothing committed until the Board matches (the Board loops and `drc_bw2001` are red meanwhile). **Stop for the user**: import, place `UH2`/`UH6` at H2/H6, route them.

### 3. The Board, BASIC 1 on it, release check, docs

`check-board`; `-R "bw2001|boards|electrical|scene|session|drc|manuals|place"`; the release-only BASIC 1 case on the Board; `bw_ui netlists/bw2001.bwn --roms pet-basic1` screenshot; ADD_MACHINE (running a Machine on other ROMs), BW2001 docs, ARCHITECTURE row. Commit `bw2001: UH2 and UH6, Commodore's ROMs on the Board (P5q2)`.

## Implementation notes

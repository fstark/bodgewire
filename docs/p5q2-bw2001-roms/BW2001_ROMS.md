# P5q2 — Commodore's ROMs on the BW-2001

Roadmap step P5q2 ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), after P5q ([BW2001_MACHINE.md](../p5q-bw2001/BW2001_MACHINE.md)) and P5q1. The BW-2001 was meant to run Commodore's own ROMs as the reference and ours as the firmware we can distribute (P5q's row: "the real PET ROMs run on it as the reference"). P5q left the two sockets our firmware does not use (`UH2` at `$D000`, `UH6` at `$D800`) out of the netlist and the Board, so BASIC 1, which fills all seven, ran only on a board-less copy of the netlist. This step puts them back and lets the player choose the ROMs at run time, on the Board.

## Decided

Asked 2026-11-15 (the user: "the goal was to make a machine that ran with the Commodore ROM, then write a custom one to be able to distribute"):

- **`UH2` and `UH6` fitted**: 2316Bs like the other five; with our firmware they hold an empty ROM (every word `$FF`, what an empty socket reads). The Board gains their two footprints at cells H2 and H6, free in P5q1's layout; **the user routes them** on the routed Board.
- **ROMs chosen at run time**, the Board unchanged (contents are data, not circuit), in two forms: `--rom UH1=FILE,…` (any socket, any file) and `--roms pet-basic1` (Commodore's BASIC 1, the seven files of `external/roms/pet/`, tests/data/pet_roms.md), in `bw run` and `bw_ui`. Without them: our firmware.

Chosen here (not asked): a missing file of a named set is an error naming it and pointing at tests/data/pet_roms.md; `--rom` and `--roms` may be combined (the set first, then each `--rom` over it); a socket named that is not a ROM Part is an error.

## Tests

- Overrides: a socket's contents replaced and its `offset=` dropped; an unknown reference, a Part that is not a ROM, a file that cannot be read, each refused with its message; the named set expands to the seven sockets; a missing file of it named.
- CLI: `--rom`, `--roms` parsed and refused as their errors say.
- The Machine: our firmware unchanged with `UH2`/`UH6` empty (the P5q cases); BASIC 1 by `--roms pet-basic1` on the Machine built from the Board (the killer-poke case, which built a text copy until now); the Board checks after the user's routing.
- Release: `bw_ui netlists/bw2001.bwn --roms pet-basic1` on the Board, its banner viewed.

Design: [BW2001_ROMS_DESIGN.md](BW2001_ROMS_DESIGN.md). Plan: [BW2001_ROMS_PLAN.md](BW2001_ROMS_PLAN.md).

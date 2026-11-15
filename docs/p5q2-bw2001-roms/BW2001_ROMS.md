# P5q2 — Commodore's ROMs on the BW-2001

Roadmap step P5q2 ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), after P5q ([BW2001_MACHINE.md](../p5q-bw2001/BW2001_MACHINE.md)) and P5q1. The BW-2001 was meant to run Commodore's own ROMs as the reference and ours as the firmware we can distribute (P5q's row: "the real PET ROMs run on it as the reference"). P5q left the two sockets our firmware does not use (`UH2` at `$D000`, `UH6` at `$D800`) out of the netlist and the Board, so BASIC 1, which fills all seven, ran only on a board-less copy of the netlist. This step puts them back and lets the player choose the ROMs at run time, on the Board.

## Decided

Asked 2026-11-15 (the user: "the goal was to make a machine that ran with the Commodore ROM, then write a custom one to be able to distribute"; "I thought you were saying there would be no ROM in the socket, but did not realise we would not put the socket"):

- **`UH2` and `UH6` fitted**: 2316Bs like the other five; with our firmware they hold an empty ROM (every word `$FF`, what an empty socket reads). The Board gains their two footprints at cells H2 and H6; **the user routes them** on the routed Board (the tracks now crossing those cells moved).
- **ROMs chosen at run time by a directory**, the Board unchanged (contents are data, not circuit): `--roms DIR` in `bw run` and `bw_ui`; in `DIR` a ROM Part's file is named after its reference (`UH1.bin` or `UH1.rom`), and replaces its `contents=`; a ROM Part with no file there keeps the netlist's. Without the flag: our firmware. Exposing the choice in the game is for later (P7).
- **P7**: a Machine's own `roms/` directory, files named the same way, will hold its default ROMs (the netlist then names none).

Chosen here (not asked): Commodore's set is `external/roms/pet-basic1/`, seven symbolic links named `UH1.bin` … `UH7.bin` to the user's files in `external/roms/pet/` (tests/data/pet_roms.md says how to make it); a `.rom` text file in `DIR` holds one chip (2048 words), a `.bin` exactly the chip's size; a directory that does not exist, or with a file matching no ROM Part (`UX9.bin`), is an error; a Machine keeping a real board's layout keeps every chip location, even one its firmware leaves unused (added to ADD_MACHINE).

## Tests

- The directory: the files by reference replace their Parts' contents (`offset=` dropped); `.bin` and `.rom` both; a Part with no file unchanged; a missing directory, a file matching no ROM Part, two files for one Part (`UH1.bin` and `UH1.rom`) each refused with its message.
- The Machine: our firmware unchanged with `UH2`/`UH6` empty (the P5q cases); BASIC 1 by `--roms` on the Machine built from the Board (the killer-poke case, which built a text copy of the netlist until now; its directory made by the test from tests/data/pet_roms.md's files, skipped without them); the Board checks after the user's routing.
- Release: `bw_ui netlists/bw2001.bwn --roms external/roms/pet-basic1` on the Board, its banner viewed.

Design: [BW2001_ROMS_DESIGN.md](BW2001_ROMS_DESIGN.md). Plan: [BW2001_ROMS_PLAN.md](BW2001_ROMS_PLAN.md).

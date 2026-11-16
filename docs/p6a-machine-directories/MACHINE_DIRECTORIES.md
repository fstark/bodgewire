# P6a — Machine directories

Roadmap step P6a ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), done 2026-11-16 (`3c2ebdd`, the conformance split `84c9752`). Written afterwards, on 2026-11-17, from the move as made: the layout of a Machine on disk and the rules that load it. The code is the truth; this is the map.

## Why

Each Machine was spread over six top-level directories (`netlists/`, `boards/`, `manuals/`, `roms/`, `puzzles/`, `docs/machines/`), joined only by its name. The game shell (P7) lists Machines and their Puzzles, and will show a screenshot of each: a Machine has to be one thing on disk first. Asked by the user at the start of P6 (2026-11-16), before the UX work, so that work is built on the layout it keeps.

## Decided with the user (2026-11-16)

- One directory per Machine, numbered; the number orders them and is dropped when shown to the player.
- In it: the working Machine (its netlist), its development docs, its ROMs, its Puzzles in order.
- `docs/` there are development docs, never shown to the player; the player's are `manuals/`.
- Puzzles numbered easiest first; the `machine` line kept in each Puzzle file.
- `datasheets/` stays shared.
- No shared ROMs: a ROM two Machines use is copied into each.
- A short row: no spec, design or plan before the move, no review agent (this document came after).

Chosen in the move (not asked): the `board/` subdirectory for the placement and KiCad files; the numbers and names of the old `docs/machines/` directories (`020-led-chaser`); the Puzzle order of each Machine doc's Puzzle list; the bench and test ROM programs kept with their Machine.

## The layout

```
machines/
  NNN-name/                 010-counter … 140-bw80, the numbers of the tutorial ladder
    <m>.bwn                 the design netlist; <m> is the Machine's name (counter, chaser, bw2001)
    board/
      <m>.place             bw place rules (Boards placed by bw place)
      <m>.kicad_pro         KiCad project
      <m>.kicad_pcb         the Board, routed by the user
                            (KiCad's local files beside them, git-ignored: .kicad_prl, .net, .dsn…)
    manuals/
      user.md               the player's User Manual
      service.md            the player's Service Manual
    docs/
      NAME.md               the Machine's spec: lesson, circuit, when it works, Puzzles
      NAME_DESIGN.md        netlist, Board, Puzzle files, manuals; its file table relative to here
      schematic/            a real board's transcription (the BW-2001)
    roms/                   only Machines with ROMs
      *.asm, *.inc          ROM programs and their includes
      *.rom, *.bin          the contents the netlist names, and the assembled programs
    puzzles/
      NN-name.bwp           easiest first: 01-dark-led.bwp, 02-frozen-led.bwp
```

Shared, outside `machines/`:

| Directory | Holds |
|---|---|
| `datasheets/` | one Markdown datasheet per catalogue Part, for every Machine |
| `footprints/` | the vendored KiCad footprints `bw place` uses (was `boards/footprints/`) |
| `bench/` | the kernel benchmarks: `bus_load.bwn`, `bus_load_z80.bwn` and their ROMs, not Machines |
| `tests/data/` | test netlists (benches of a Machine's chips), test-only ROMs (`empty.rom`) |

## Names and numbers

- **The directory** is `NNN-name`: the ladder number (three digits, `045` between `040` and `050`) and a lowercase hyphenated name. The name need not be the Machine's: `020-led-chaser` holds `chaser.bwn`.
- **The Machine's name** `<m>` is its netlist's name. Puzzles name it (`machine chaser`), the netlist and the Board are found by it, tests find the directory by it.
- **A Puzzle file** is `NN-name.bwp`: two digits for its place among the Machine's Puzzles, easiest first, in the order of the Machine doc's Puzzle list, then its name, lowercase with hyphens. Inserting a Puzzle renumbers the later ones. The player sees its `title`, never the file name.
- **Shown to the player**, a number is dropped: the directory `010-counter` is the Counter, the file `01-dark-led.bwp` is "Dark LED".

## Loading

- A Puzzle's Machine directory is its directory's parent (`MachineDir` in `src/machine/machine.hpp`); a netlist's is its own directory.
- `LoadMachine(puzzle, dir)` reads `dir/<m>.bwn` and `dir/board/<m>.kicad_pcb`, `<m>` from the Puzzle's `machine` line. A Puzzle in the wrong Machine's `puzzles/` finds no netlist by that name: "unknown machine".
- `HasBoard(netlist)`: `board/<stem>.kicad_pcb` beside the netlist. `bw_ui` opens a netlist with a Board as a fault-free Machine, one without as a bare circuit.
- `contents=` in a netlist is relative to the netlist's directory: `contents=roms/bw1_002.rom`. `--roms DIR` (P5q2) is unchanged.
- `LoadManuals(dir, datasheets, design)` (`src/manual/manual.hpp`): `dir/manuals/{user,service}.md` and `datasheets/<Part>.md`; `DatasheetDir(dir)` is the repository's `datasheets/`. Problems name `manuals/user.md`, `datasheets/74LS00.md`.

## Tests and tools

- Tests find files by name, never by path: `tests/paths.hpp` gives `test::MachineDir("chaser")` (the directory whose `chaser.bwn` exists), `NetlistPath`, `BoardPath`, `RomPath(machine, file)`, `BenchPath(file)`, and `PuzzlePath("dark_led")` (any `machines/*/puzzles/NN-dark-led.bwp`; `_` and `-` alike). Only `BW_ROOT_DIR` and `BW_FOOTPRINT_DIR` remain as compile definitions.
- ctest globs: `drc_<m>` on `machines/*/board/*.kicad_pcb`, `place_drc_<m>` on `machines/*/board/*.place` with the netlist one directory up, `asm_<name>` on `machines/*/roms/*.asm` and `bench/*.asm`, each checked against the `.rom` beside it.
- `bench/measure.sh` runs every `machines/*/*.bwn` and `bench/*.bwn`; `bench/trace_check.sh` finds a netlist in either layout, so a reference commit from before P6a still records.
- `.gitignore` keeps KiCad's local files out under `machines/*/board/`.

## Not changed

- **One ROM file per socket** (P7): a Machine's netlist still names its ROM files with `contents=`, and the BW-2001's BASIC and kernel still span two sockets by `offset=`. Its bench and test programs (`bw1_bench`, `bw2001_bench`, `bw80_bench`, `bw80_test`) sit in its `roms/` until then.
- **Finished milestones** (`docs/p*/`): history, their links to moved files left broken.

## Recipes

[ADD_MACHINE.md](../recipes/ADD_MACHINE.md) and [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md) give the paths for a new Machine and a new Puzzle (the file name rule is in ADD_PUZZLE); this document is the reference they rely on.

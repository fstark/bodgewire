# P5q2 Commodore's ROMs on the BW-2001 — Design

Implements [BW2001_ROMS.md](BW2001_ROMS.md). Recipes: [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §2 (netlist, manuals), §3 (Board).

## 1. A ROM directory — `src/netlist/rom_directory.{hpp,cpp}` (bw_netlist)

```cpp
// Replaces the contents= of each ROM Part (a Part with contents=) that has a file named after
// its reference in `directory` (UH1.bin or UH1.rom), and drops its offset= (P5q2). The
// directory is made absolute here: the files are read again at every build. Errors: the
// directory missing; a file matching no ROM Part; two files for one Part.
std::expected<void, std::string> ApplyRomDirectory(Netlist& netlist,
                                                   const std::filesystem::path& directory);
```

Called after `ResolveContentsPaths`, before `BuildCircuit`. The file is read by the Part, as any `contents=`: a `.bin` of the wrong size or a `.rom` past one chip gives the existing errors (`rom_contents.cpp:197`; `CreateRom` refuses `offset=` with a `.bin`, so `offset=` must go). Only file names `REF.bin` / `REF.rom` count; other files are ignored (a README may sit there).

## 2. Loading — `src/machine/machine.{hpp,cpp}`, `src/cli/main.cpp`, `src/ui/main.cpp`

- `LoadMachine(puzzle, root, std::optional<std::filesystem::path> roms = {})` applies it to the parsed design after `ResolveContentsPaths` (the `friend` declaration in `machine.hpp:85` updated); `LoadNetlistMachine(path, roms)` and `LoadPuzzleFile(path, roms)` pass it. The Machine keeps the changed design, so every rebuild (power-on after a repair: `Machine::build`, `machine.cpp:441`, from `design_`) has the same ROMs.
- `bw run` and `bw_ui`: `--roms DIR` for a `.bwn` and a `.bwp` alike; `bw run` on a netlist parses, resolves, applies and builds (in place of `LoadNetlistFile`); `bw_ui` passes it to `LoadNetlistMachine` / `LoadMachine`, or applies it on its board-less path. Usage lines; an error from `ApplyRomDirectory` is a usage error (exit 2) with its message.

## 3. Netlist, Board, manuals

- `netlists/bw2001.bwn`: `part UH2 2316B select=LLH contents=../roms/empty.rom`, `part UH6 2316B select=LHH contents=../roms/empty.rom`, on the Nets of `UH1` and `UH5`, their `CS1` on `SELD_N` (sheet2.bwn:31–32, 46–74; `SELD_N` exists: `UG2.Y13_N UG4.2C`). `roms/empty.rom`: a copy of `tests/data/empty.rom` (every word `$FF`). Our firmware never reads `$D000`–`$DFFF`.
- Board: `bw export-kicad netlists/bw2001.bwn -o <scratch>/bw2001.net` (not committed). **The user**: in KiCad's PCB editor File → Import → Netlist, with "Replace footprints with ones from the netlist" and "Delete footprints with no symbols" unchecked (the existing footprints match by reference and timestamp: `kicad_netlist.cpp:86`, `kicad_pcb_writer.cpp:139`, so only `UH2`, `UH6` are added); place `UH2` in cell H2, `UH6` in H6, lying as their neighbours (90°); rip up and reroute the tracks now crossing those cells (about 21 segments and a via at H2, 29 segments at H6, measured by the review), then route the two chips; DRC 0 errors. `boards/bw2001/bw2001.place` needs no change (`grid pet` places every PET chip).
- Manuals: the service manual's `UH2` "ROM socket $D000–$D7FF: empty with our firmware (BASIC 1's 901447-03 in Commodore's set)", `UH6` likewise; the user manual: running Commodore's ROMs (`--roms DIR`, files named `UH1.bin` …).
- `tests/data/pet_roms.md`: the `pet-basic1` directory of links (`cd external/roms && mkdir pet-basic1 && ln -s ../pet/rom-1-c000.901447-01.bin pet-basic1/UH1.bin` …, one per socket).
- Docs: BW2001.md and BW2001_DESIGN.md (the "left out" row for `UH2`, `UH6` flipped); ADD_MACHINE: every chip location of a real board kept, and running a Machine on other ROMs; ARCHITECTURE: the P7 row's ROM directory.

## 4. Tests

- `tests/netlist/rom_directory_test.cpp` (a temporary directory, small ROM Parts): replaced by `.bin` and by `.rom`, `offset=` dropped, a Part without a file unchanged, a missing directory, a stray `UX9.bin`, `UH1.bin` with `UH1.rom`, a README ignored. No test of `bw run`'s option parsing exists (`src/cli` has none); the option is a thin call of this function, checked by hand in the release check.
- `tests/machines/bw2001_test.cpp`, after the Board has `UH2`/`UH6` (phase 2's end): `Basic1Circuit` makes a temporary directory of links named by socket to `kPetBasic1`'s files (`tests/netlist/external_rom.hpp`, unchanged: `external_rom_test`, the bench and this test use it), then `LoadNetlistMachine(path, dir)` and `build()` (SKIP without the files); the netlist-text copy (`VariantCircuit` for BASIC 1) goes.

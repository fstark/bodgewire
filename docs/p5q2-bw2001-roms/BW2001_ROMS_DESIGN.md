# P5q2 Commodore's ROMs on the BW-2001 — Design

Implements [BW2001_ROMS.md](BW2001_ROMS.md). Recipes: [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §2 (netlist, manuals), §3 (Board).

## 1. ROM overrides — `src/netlist/rom_override.{hpp,cpp}` (bw_netlist)

```cpp
// A ROM socket's contents chosen at run time (P5q2): the Board unchanged.
struct RomOverride {
    std::string reference;          // "UH1"
    std::filesystem::path contents; // a .rom or .bin, absolute or relative to the current directory
};

// "UH1=path,UH5=path" → overrides; error text otherwise.
std::expected<std::vector<RomOverride>, std::string> ParseRomOverrides(std::string_view text);

// A named ROM set ("pet-basic1": the seven BASIC 1 files of external/roms/pet/ by socket,
// tests/data/pet_roms.md) under `root` (the repository: MachineRoot). Error naming the set's
// missing files, or an unknown name.
std::expected<std::vector<RomOverride>, std::string> RomSet(std::string_view name,
                                                            const std::filesystem::path& root);

// Replaces each named Part's contents= (and drops its offset=) in a parsed netlist, after
// ResolveContentsPaths. Errors: unknown reference; a Part without contents= (not a ROM).
std::expected<void, std::string> ApplyRomOverrides(Netlist& netlist,
                                                   std::span<const RomOverride> overrides);
```

The file is read by the Part when the netlist is built, as any `contents=` (size and format checked there: a `.bin` of the wrong size gives the existing error). The BASIC 1 table moves from `tests/netlist/external_rom.hpp` (`kPetBasic1`) to `rom_override.cpp` (`RomSet`); the test header keeps `ExternalRom` and uses the table through the new header.

## 2. Loading — `src/machine/machine.{hpp,cpp}`, `src/cli/main.cpp`, `src/ui/main.cpp`

- `LoadNetlistMachine(path, std::span<const RomOverride> roms = {})`: `LoadMachine` gains the overrides and applies them to the parsed design after `ResolveContentsPaths`, before `BuildCircuit`. The Machine keeps the overridden design, so every rebuild (power-on after a repair) has the same ROMs. The Board check is unchanged (it compares Nets, not contents).
- `bw run` and `bw_ui`: `--rom REF=FILE,…` and `--roms NAME`; the set first, then each `--rom`. `bw run` (netlist only) applies them in its own load (`LoadNetlistFile` becomes parse + resolve + apply + build there); `bw_ui` passes them to `LoadNetlistMachine`, or applies them on its board-less path. Usage lines and their errors as the other options'.
- `.bwp` Puzzle files: no ROM line (not needed by P5r yet).

## 3. Netlist, Board, manuals

- `netlists/bw2001.bwn`: `part UH2 2316B select=LLH contents=../roms/empty.rom`, `part UH6 2316B select=LHH contents=../roms/empty.rom`, on the Nets of `UH1` and `UH5` but their `CS1` on `SELD_N` (as the transcription: SHEET2.md). `roms/empty.rom`: a copy of `tests/data/empty.rom` (every word `$FF`), committed for Machines.
- Board: `bw export-kicad netlists/bw2001.bwn -o …/bw2001.net`; **the user** imports it into the routed `bw2001.kicad_pcb` (KiCad: File → Import → Netlist, or Tools → Update PCB from Netlist), places `UH2` in cell H2 and `UH6` in cell H6 lying as their neighbours (90°), and routes them. `boards/bw2001/bw2001.place` needs no change (`grid pet` places every PET chip; `place_drc_bw2001` then has the two).
- Manuals: service manual `UH2` "ROM socket $D000–$D7FF: empty with our firmware, BASIC 1's 901447-03 with Commodore's", `UH6` likewise `$D800`; the user manual: how to run Commodore's ROMs (`--roms pet-basic1`, the files in `external/roms/pet/`).
- `docs/machines/130-bw2001/BW2001.md`, `BW2001_DESIGN.md`: the sockets and the ROM choice.

## 4. Tests

- `tests/netlist/rom_override_test.cpp`: parse (one, several, a missing `=`, an empty file); apply (contents replaced, `offset=` dropped, unknown reference, not a ROM); `RomSet("pet-basic1")` (seven, by socket; a missing file named, with the files absent: a temporary root); unknown set.
- `tests/cli/…` (where `bw run`'s options are tested): `--rom`, `--roms` accepted and refused.
- `tests/machines/bw2001_test.cpp`: `Basic1Circuit` becomes `LoadNetlistMachine(path, RomSet("pet-basic1"))->build()` (skipped without the files); the P5q cases unchanged.
- `tests/data/bw2001_firmware.bwn`, `bw2001_bench_test.cpp`: unchanged (the bench keeps its own replacement, or uses `ApplyRomOverrides`).

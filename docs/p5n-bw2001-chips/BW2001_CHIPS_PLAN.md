# P5n BW-2001 chips — Implementation Plan

Design: [BW2001_CHIPS_DESIGN.md](BW2001_CHIPS_DESIGN.md)
Spec: [BW2001_CHIPS.md](BW2001_CHIPS.md)

Each phase ends with a fence: its checks, then one commit staged by path (the user edits docs in parallel). The test subsets follow AGENTS.md. Phases 1 and 2 touch shared Parts (ROM, gate packages, 74145), so they run the full debug suite.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `cf0564a` |
| 1 | Binary contents, 2316B, `ExternalRom` | `77fa639` |
| 2 | TTL: 74LS154, 74177, 74100, 7417, 74LS21, 74LS145 | `ca0624a` |
| — | Character ROM 901447-08 shipped (user, 2026-11-15) | `2b19976` |
| 3 | 6520 | `78dc33d` |
| 4 | 6522 core and Part | `2230bf5` |
| 5 | Bench, its program, BASIC 1 boot | this commit |
| 6 | Release check, recipes, roadmap | |

## Phase 1 — Binary contents, 2316B, `ExternalRom`

Design §1–§3.

1. Add `.bin` reading to `ReadRomContents`, with its tests.
2. Add `activeHigh` to `EvaluateRom`, `selectable` to `RomSpec`, and `select=` to `CreateRom`.
3. Add the `2316B` spec row, delays, catalogue row, `datasheets/2316B.md`, and its `rom_test.cpp` cases (design §9).
4. Pass `select=LLH` in `catalogue_test.cpp` (§2.4).
5. Add `tests/netlist/external_rom.hpp`, `tests/netlist/external_rom_test.cpp` (in `tests/CMakeLists.txt`) and `tests/data/pet_roms.md`: the seven files read when present, SKIPped otherwise.

Before the change: measure the 110, `bus_load` and BW-1 headless ratios (release, median of 3) and note them.

**Fence**:
- build debug and release with no warnings;
- `ctest --preset debug -j 12` full: the 080 and every ROM user unchanged;
- clang-format the C++ files.

Commit: `parts: 2316B mask ROM with mask-set selects, raw .bin contents, real ROMs for tests`

## Phase 2 — TTL

Design §4.

1. Add `74LS154` and `74177`.
2. Add `74100` (in `kOffCornerPower`: `GND` pin 7).
3. Add the gate-package `openCollector` field, then `7417` and `74LS21`.
4. Pass delays to `Ttl145`, then add `74LS145`.

Each Part gets its datasheet file, its catalogue row (sorted) and its test.

**Fence**: full debug suite (shared gate package and 74145 code), no warnings, clang-format.

Commit: `parts: 74LS154, 74177, 74100, 7417, 74LS21, 74LS145`

## Phase 3 — 6520

Design §5.

1. Add `Mos6520`, its delays, its catalogue row, `kMosSignals` and `kOffCornerPower`.
2. Add `datasheets/6520.md`.
3. Add `mos6520_test.cpp`. Check each rule's test against a mutation and list them in the notes.

**Fence**: `ctest --preset debug -R "6520|catalogue|manual|footprint"`, no warnings, clang-format.

Commit: `parts: 6520 PIA`

## Phase 4 — 6522

Design §6.

1. Add `Mos6522Core` and `mos6522_core_test.cpp`. Check each rule against a mutation and record them.
2. Add `Mos6522`, its delays, its catalogue row and the catalogue lists.
3. Add `datasheets/6522.md` and `mos6522_test.cpp`.

**Fence**: `ctest --preset debug -R "6522|catalogue|manual|footprint"`, no warnings, clang-format.

Commit: `parts: 6522 VIA, its logic in a core`

## Phase 5 — Bench

Design §7.

1. Write `roms/bw2001_bench.asm` and its `.rom` (`bw asm`); the `asm_bw2001_bench` ctest comes from the `roms/*.asm` glob, nothing to add.
2. Write `tests/data/bw2001_bench.bwn`.
3. Write `tests/netlist/bw2001_bench_test.cpp` (and its `tests/CMakeLists.txt` entry): one run a seed for the program's checks, the stuck cells on seed 1, and BASIC 1. Measure `kDone` and each case's debug time; record them in the notes.

If BASIC 1 does not reach `READY.`, find the chip at fault with the logic analyser's VCD (`bw run --vcd`) and MAME's behaviour. Fix it in that chip's phase files, with a test for the rule, in this commit, and say so in the notes.

**Fence**:
- `ctest --preset debug -R "bw2001|asm|6520|6522|2316|rom"`;
- BASIC 1 passing here, not skipped (the files are present on this machine);
- no warnings, clang-format.

Commit: `bw2001: bench on the PET's bus, our program and BASIC 1 to READY.`

## Phase 6 — Release check and docs

Design §8, §10.

1. Do the release build.
2. Measure the bench's headless ratio, idle and booting BASIC 1.
3. Measure the 110, `bus_load` and BW-1 ratios against phase 1's baseline.
4. Take a `bw_ui` screenshot of the 080 and view it.
5. Update ADD_PART and ADD_PUZZLE.
6. Set the ARCHITECTURE row to done, with the date and links.
7. Cut this plan down to its table and notes.

**Fence**: the full debug suite, then the commit.

Commit: `docs: P5n implemented`

## Notes

- Review (before code), fixed in the docs: `kDone` from 36 pages, not 9 (300 ms; `bw run --time 400ms`); the 6522's power-on bytes 16; the 74100 in `kOffCornerPower`; only `catalogue_test` builds every ROM; the external-ROM header beside its users in `tests/netlist/` (no include path for `tests/`); the 6522 samples its inputs at the rise only; `CB1` and `CB2` direction per SR mode; `PB7` under T1 by `ACR` bit 7 alone, low from the T1 write in both modes; undriven pull-up pins driven `One`; the retrace Net named `RETRACE` (the 6502 has a `SYNC` pin); PIA2's and the 2114s' wiring spelled out; the T1 IRQ checked on the bench; one bench run a seed, stuck cells on seed 1; fence regexes by test name (`[parts]` is a tag); the `CB2` pulse end in the 6520's rise; pin ranges ascending in the datasheet files; the power-on contention before `RES_N` noted; the retrace window 55 ms.
- Phase 1 and 2 code was written while the review ran, then checked against its findings (74100 already in `kOffCornerPower`, `external_rom.hpp` already in `tests/netlist/`).
- Baselines before phase 1 (release, headless, median of 3): 110 5.48×, `bus_load` 0.397×, BW-1 3.04×.
- Phases 1 and 2 were tested together: one full debug suite on the tree with both, 1080/1080 in 645 s. Each was then committed alone by staging its own version of the shared files (catalogue, delays, CMake, catalogue test), and that snapshot was built in a scratch tree and its fence subset run there.
- Test helpers: `tests/parts/chip_bench.hpp` (one chip, a Stimulus on every input) for the TTL Parts; `tests/parts/bus_bench.hpp` (the 6530 test's scripted 1 MHz bus, made general) for the 6520 and 6522.
- 6520 mutations, each failing a test: a port read keeping the flags; a DDR read clearing them; the CB2 pulse never ending; C1 on either edge; no CB2 write strobe.
- 6522 core mutations, each failing a test: T1 decrementing in its write cycle; T1 not reloading; `IER` bit 7 read as 0; mode 1 at the φ2 rate; independent C2 modes cleared; PB7 needing `DDRB`; T2 reloading; reset clearing T1; no PA latching. The shift register starts from the cycle after the `SR` access (as T1 from the cycle after its write).
- Bench, against the design: the keys are read in the retrace interrupt, as the PET does, not in the main loop (reading PIA1's port B clears the retrace flag, so a main-loop read could take the interrupt away before the CPU saw it); key checks therefore allow 20 ms. The T1 IRQ check is by level at each PB7 turn, skipping turns within 300 µs of a retrace. `kDone` measured: `$55` at 349.5 ms (37 cycles a byte over 35 pages), so 400 ms, not the review's 300.
- BASIC 1 boots to `*** COMMODORE BASIC ***`, ` 7167 BYTES FREE`, `READY.` on the bench with no change to any chip.
- Debug times: the program case (3 seeds) 82 s, the stuck cells 39 s, BASIC 1 34 s; release 2.4 s for all three. Headless release ratio of the bench 1.31× (450 ms).

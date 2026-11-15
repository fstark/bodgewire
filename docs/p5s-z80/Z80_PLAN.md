# P5s Z80 Part — Implementation Plan

Design: [Z80_DESIGN.md](Z80_DESIGN.md)
Spec: [Z80.md](Z80.md)

Work on branch `p5s-z80` (worktree `.claude/worktrees/p5s-z80`), P5q running on `main` in parallel; rebased on `main` before the merge back, and the ratios measured again on the rebased tree (P5q may change shared code). Fence test subsets by name (`-R`), never the full debug suite until phase 6. The worktree has no `venv`: the Python scripts are standard library only and run with `python3`.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `170f3d2` |
| 1 | Test data: converter, fetch script, committed subset | `1a8a98d` |
| 2 | Core: every opcode, the runner, the Harte subset | `707d412` |
| 3 | Core: interrupts, HALT, reset; hand cases; full Harte, ZEXDOC, ZEXALL in release | `87e1e25` |
| 4 | `Z80` Part, datasheet, catalogue | `ac34721` |
| 5 | Bus load with the Z80: ROM, netlist, test, ratios | this commit |
| 6 | Release check, recipes, roadmap | |

## Phase 1 — Test data

- `tests/data/z80/convert_harte.py`, `fetch.sh`, `README.md` (the Harte and ZEXALL commits from `external/z80/SOURCES`), `LICENSE-harte` (Design, Test data); `fetch.sh --subset` run (1.37 GB, written to `external/z80/` and the 25-case subset).

Fence: `python3 tests/data/z80/convert_harte.py --help` runs; `head -7 tests/data/z80/harte/00.txt` shows the header and an `I`/`F`/`C` case; `ls tests/data/z80/harte | wc -l` is 1604; `du -sh tests/data/z80` under 25 MB; `git status --short` lists only `tests/data/z80/` (`external/` ignored).
Commit: `z80: Harte's Z80 test data, the converter and fetch script (P5s)`

## Phase 2 — Core, every opcode

- `z80_core.{hpp,cpp}`: the interface (Design), the resumable sequencer, the ALU, flags with X/Y, WZ, Q; the five opcode tables; `HALT` setting the halted state; `EI`/`DI`.
- `z80_cases.{hpp,cpp}`, `z80_conformance_test.cpp` (the subset case; the full and ZEX cases skip without data).
- CMake: `z80_core.cpp` in `src/parts/CMakeLists.txt`; `parts/z80_cases.cpp`, `parts/z80_conformance_test.cpp` and the two `BW_Z80_…` definitions in `tests/CMakeLists.txt`.
- Debug time of the subset measured.

Fence: `cmake --build --preset debug`; `ctest --preset debug -R "Z80 core"`; `clang-format --dry-run --Werror` on the new C++ files.
Commit: `z80: the instruction core, every opcode, checked on Harte's data (P5s)`

## Phase 3 — Core, interrupts and the full suites

- Interrupts (NMI, IM 0/1/2, the EI shadow, prefixes, `LD A,I` P/V), HALT's halted Fetches, `reset()` (Design, Interrupts and HALT).
- `z80_core_test.cpp` with the design's hand cases, each with its mutation tried and noted; `parts/z80_core_test.cpp` in `tests/CMakeLists.txt`.
- `Z80 core: Harte full`, `ZEXDOC`, `ZEXALL` run once each in release by name; times and the ZEX output's last lines recorded.

Fence: `cmake --build --preset debug`; `ctest --preset debug -R "Z80 core"`; `cmake --build --preset release`; `build/release/tests/bw_tests "Z80 core: Harte full"`, `"Z80 core: ZEXDOC"`, `"Z80 core: ZEXALL"` pass.
Commit: `z80: interrupts, HALT and reset; ZEXDOC, ZEXALL and the full Harte set pass (P5s)`

## Phase 4 — The Part

- `z80.{hpp,cpp}`: pins, the T-state sequencer (Design, Pins), waits, bus grant, reset, power-on. `delays.hpp`: `kZ80Busak`, `kZ80Float` from the AC table, the comment widened; the reset-to-first-fetch count checked against UM0080's Reset figure and written in the notes.
- Catalogue row, `datasheets/Z80.md`, `catalogue_test.cpp` lists; `z80.cpp` in `src/parts/CMakeLists.txt`, `parts/z80_test.cpp` in `tests/CMakeLists.txt`.
- `z80_test.cpp` (Design, Tests).

Fence: `cmake --build --preset debug`; `ctest --preset debug -R "Z80|catalogue|manuals|footprint"`.
Commit: `z80: the Z80 Part, cycle-exact per CLK edge (P5s)`

## Phase 5 — Bus load with the Z80

- `roms/make_bus_load_z80_rom.py` with `--check`; `roms/bus_load_z80.rom`; `netlists/bus_load_z80.bwn`; `bus_load_test.cpp` over both netlists, the ROM per netlist, the RAM copy case.
- Debug time of the bus load tests measured.
- Release: the two netlists' ratios (Design, Measurements); `bw_ui` screenshot viewed.

Fence: `python3 roms/make_bus_load_z80_rom.py --check`; `cmake --build --preset debug`; `ctest --preset debug -R "bus load|Z80"`; release measurements in the notes.
Commit: `bus load: the real Z80 on the P5a circuit, our display program, measured (P5s)`

## Phase 6 — Finish

- Rebase on `main` (P5q's work); build both presets; the full debug suite; the release ratios again on the rebased tree.
- `docs/recipes/ADD_PART.md`: reference commits for the Z80 core and Part, the M-cycle core variant (a resumable function, Internal cycles); fixes found on the way.
- ARCHITECTURE: P5s row done with the date, figures and links; Performance gets the ratio.
- This plan cut to the status table and notes.

Fence: `ctest --preset debug -j 12` passes; `cmake --build --preset release`; screenshots viewed.
Commit: `docs: P5s implemented`

## Notes

- Review (before commit of the docs), fixed in them: `step()` and the INT sampling made one rule (inputs at the cycle's last rising edge, `step()` there or at the later data edge), internal T-states as their own cycles, bus request and reset in the Part only; `RFSH_N` not tri-state, reset floats only A and D with the controls high; power pins `Ttl`; `NMI_N` read at every rising edge; RAM read with `Ram2114::word`; the pulse rule per pin; file names with `_`; Harte's sampling written in; hand cases for the prefix chains and `ED` holes Harte lacks; the bus-load tail invariant and `--check`; the wake test in the 6502's shape; the CMake work in its phases; `python3` for the scripts.
- The core, the runner and the data were written while the review ran (phases 1–2 code), then checked against its findings. Release: the Harte subset passes (0.5 s); the full Harte set passes (1,604,000 cases, 4.7 s); ZEXDOC and ZEXALL pass, 46,734,977,142 T-states each, 44 s and 46 s. Fixes found by the data: Q cleared by a `DD`/`FD` prefix (`dd 37`), WZ = PC + 1 on a repeating `INIR`/`OTIR` family (`ed b2`).
- Phase 2: the core was written whole (interrupts, HALT and `reset()` included); phase 3 adds their hand cases. Debug: the Harte subset 2.7 s (the estimate of 60–90 s was far off: the runner reuses its RAM and the core is small per cycle).
- Phase 3 mutations of the core, each failing a hand case: the EI shadow ignored; IM 0 counting PC; NMI on its level (the first try survived: the case gave NMI once, not at every cycle as the Part does; fixed); no P/V loss; R's bit 7 counted; a halted fetch counting PC; `DD ED` keeping the `DD` (the first try survived: `ED 4B` loads BC whatever the prefix; now `ED 6B`); an `ED` hole running `NEG`; an interrupt taken after a prefix; IM 2 clearing the vector's bit 0.
- Release, phase 3: `Z80 core: Harte full` 4.7 s; `ZEXDOC` and `ZEXALL` "Tests complete" with every test OK, 46,734,977,142 T-states each, 44 s and 46 s.
- Phase 4: `kZ80Float` 90 ns and `kZ80Busak` 100 ns are the Z80A's usual published maxima, not read from a scan of UM0080 here (said in `delays.hpp`). The reset-to-first-fetch count was not checked against UM0080's Reset figure either: the model starts T1 of the fetch at `$0000` at the second rising edge after `RESET_N` goes high (the first sees it high). Both are open for a check against the manual.
- Phase 4, the bench: a write ends at the first of its strobes to rise (`WR_N` before `MREQ_N`, `IORQ_N` before `WR_N` by the data sheet's delays), so `BenchMemory` stores at `WR_N`'s rise and records an `OUT` at `IORQ_N`'s. "Reads before the edge" probes the timing on the same circuit as it measures (a Stimulus on the data, its step far away): a tied bus draws other power-on bits, so other timing. Debug: `ctest -R "Z80|catalogue|manuals|footprint"` 80 tests, 4.3 s with `-j 12`.
- Phase 5, the program against the design's first sketch: the handler is on the ZX80's balanced pattern (`DEC C`, `JP NZ`; `POP`, `DEC B`, `RET Z`, `SET 3,C`, or `POP`, `RET Z`, `JR`), 54 T-states on both paths, not 42; so a line has 35 Fetches after `EI` (one halted Fetch on a display line, not four) and A = `$5D`. Border rows are runs of `HALT`s in RAM around the page (a `HALT`'s pushed PC is the next row), so one `JP (HL)` serves both. The design was updated to the program as built. The page came up on the first run, 3.7 µs right of the stub's (71 T-states from HSYNC to the first character, the stub's 59).
- Phase 5, the TV lock: with the 100 ms cap no locked frame came. The Z80's first VSYNC comes after the POR and the copy (6.2 ms), and the TV (free-running at 21 ms, accepting a vertical sync only 16–21 ms into its frame) falls in step after about 6 frames; the stub's first VSYNC is at once. The Z80 netlist's cap is 250 ms rather than a POR tuned to the TV.
- Phase 5, debug: `bus load: the TV shows the page` 80 s (both netlists, both seeds), the copy case 1.3 s. Release, `bw run --time 2s`, alternated, 3 runs each: `bus_load` 0.383–0.389× (239 M applied events, 228 M evaluations), `bus_load_z80` 0.428–0.431× (236 M applied events, 221 M evaluations): the real CPU is no dearer than the stub, slightly cheaper (its border lines are halted fetches where the stub played ROM and RAM cycles). `bw_ui netlists/bus_load_z80.bwn --after 1s`: 0.43×, UI 0.0 ms, the page on the TV (screenshot viewed).

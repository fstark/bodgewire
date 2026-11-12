# P5a Bus load spike — Implementation Plan

Spec: [BUS_LOAD.md](BUS_LOAD.md). Design: [BUS_LOAD_DESIGN.md](BUS_LOAD_DESIGN.md).

Every phase ends with its fence: `cmake --build --preset debug` clean (`-Werror`), `clang-format` on the C++ files touched, the tests the phase names green in debug, then one commit with the message given (the tests run named in it). Only this step's own paths are committed.

## Phase 1 — DIP-40 package

ADD_PART, "A new package": footprint copied from KiCad 10.0.7, `kFootprints` row, footprint and table tests (DIP-24 cases as the model).

Fence: `ctest --preset debug -j 12 -R "footprint|kicad|board"`. Commit `board: DIP-40 package`.

## Phase 2 — Z80STUB Part

`src/parts/z80_stub.{hpp,cpp}`, `kZ80…` delays, catalogue row, `datasheets/Z80STUB.md`, `kOffCornerPower`, `tests/parts/z80_stub_test.cpp` (design, Tests).

Fence: `ctest --preset debug -j 12 -R "Z80STUB|catalogue|manuals|footprint|placer|kicad"` (placer and pcb writer load every catalogue package). Commit `parts: Z80STUB, a stub Z80 playing a ZX80 frame`.

## Phase 3 — ROM and netlist

`roms/make_bus_load_rom.py`, `roms/bus_load.rom`, `netlists/bus_load.bwn`, `tests/machines/bus_load_test.cpp`. Check by hand first: `bw run netlists/bus_load.bwn --time 60ms --tv <scratch>/p.bmp` (viewed), and a VCD of `M1_N,RFSH_N,MREQ_N,LOAD_N,HSYNC,VSYNC,LC0` over one line, against the design's timing checks; differences go into the design or the netlist, noted below.

Fence: `ctest --preset debug -j 12 -R "bus load|Z80STUB|rom"`. Commit `bench: bus_load netlist, a ZX80-like load around the stub Z80`.

## Phase 4 — Measure and decide

Design, Measurement method. Results table and decision into the spec's Results; S4 into ARCHITECTURE (Performance; P5a row done with the date and links; the row that carries kernel work, if any) and OPTIMISATION_IDEAS (Target with the measured gap, Next steps, the benchmark netlist). Release screenshots viewed. ADD_PART references (DIP-40, the stub). This plan cut down to the phase table and notes.

Fence: full `ctest --preset debug -j 12`. Commit `docs: P5a measured, <decision in a few words>`.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | DIP-40 package | |
| 2 | Z80STUB Part | |
| 3 | ROM and netlist | |
| 4 | Measure and decide | |

## Implementation notes

- Docs review (before phase 1), fixed: the T3 load glitch and the early inversion (both also seen on the TV in phase 3: U23 74LS74 added, the 74LS27 dropped); the TV's line counts (311 lines, ≈ 284 synced, locked: no HSYNC during VSYNC, as on the ZX80); `bw_ui --clip` needs a Board, so the recording cost is measured headless; spare flip-flop inputs named; the footprint table's unknown-package check; HSYNC ≈ 4.4 µs; `InterruptAcknowledge` spelled out; the phase 2 test filter; timing margins; the debug cost (≈ 15 s a seed); VCD size.

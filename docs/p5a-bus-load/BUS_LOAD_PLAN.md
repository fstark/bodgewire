# P5a Bus load spike — Implementation Plan

Spec: [BUS_LOAD.md](BUS_LOAD.md). Design: [BUS_LOAD_DESIGN.md](BUS_LOAD_DESIGN.md).

Done 2026-11-12, user check pending.

Every phase ended with its fence: `cmake --build --preset debug` clean, `clang-format` on the C++ files touched, the tests the phase names green in debug (full suite at phase 4), one commit. Only this step's own paths were committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | DIP-40 package | `3ed4d69` |
| 2 | Z80STUB Part | `94c25ae` |
| 3 | ROM and netlist | `2b297b6` |
| 4 | Measure and decide | this commit |

## Implementation notes

- Docs review (before phase 1), fixed: the T3 load glitch and the early inversion (both also seen on the TV in phase 3: U23 74LS74 added, the 74LS27 dropped); the TV's line counts (311 lines, ≈ 284 synced, locked: no HSYNC during VSYNC, as on the ZX80); `bw_ui --clip` needs a Board, so the recording cost is measured headless; spare flip-flop inputs named; the footprint table's unknown-package check; HSYNC ≈ 4.4 µs; `InterruptAcknowledge` spelled out; the phase 2 test filter; timing margins; the debug cost (≈ 15 s a seed); VCD size.
- Phases 1–2: footprint, kicad, board 69 green; Z80STUB, catalogue, manuals, footprint, placer, kicad green. `manuals: every catalogue Part has a Datasheet` first failed on `` `A0`–`A15` `` (ranges expand one character only): split, and ADD_PART says so.
- Phase 3: the first TV picture showed a lit bar before every character (the T3 load) and was fixed as in the review note above; the first test search in whole TV columns missed the page, which sits at a half column (left 150.5, top 51), so it searches in quarter columns. Debug: `bus load` cases ≈ 15 s a seed.
- Phase 4, release, alternated runs: bus_load 0.219, 0.214, 0.204×; test pattern 5.41, 4.92, 5.27×; typewriter 0.93, 0.91, 0.93× (the machine busier than at the step's start: typewriter 0.99× then). `/usr/bin/time -l --time 1s`: bus_load 76.9 G instructions for 119.5 M applied events (644 each), test pattern 4.62 G for 6.02 M (768). Profile (`build/profile`, `sample` 8 s): `EventQueue::pop` 1786, `runUntil` 1717, `EventQueue::push` 716, `Rom::evaluate` 440, `GatePackage::evaluate` 344, `Ls157` 245, `GateFunction` 231, `driveChanged` 180, `Ls74` 175, `markNetDirty` 162, `readBit` 138, `Ls244` 134, the stub 138, `Ram2114` 87, `Ls165` 79, `Ls175` 60 (of ≈ 6,700). Net changes per µs from a 25 ms `--record-all` VCD (last 5 ms): `DOT` 13, `CLK` and `CLK_N` 6.5 each, `MREQ_N` and `MREQ` 2.9 each, each `D` line ≈ 2.2. `--record A0,…,A7` 0.21× as without. `bw_ui netlists/bus_load.bwn --after 2s`: 0.21×, UI 0.0 ms, the page on the TV (viewed); it also reports the missing manuals, as expected.
- Full debug suite: 809 tests green.
- Decision recorded in the spec, ARCHITECTURE (Performance, P5a done, P5b inserted, later rows one letter on) and OPTIMISATION_IDEAS.
- User check pending: `bw_ui netlists/bus_load.bwn` shows the page; `bw run netlists/bus_load.bwn --time 2s` ratio on your machine; the new P5b row and its letter.

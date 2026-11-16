# P5z Kernel for the BW-2001 — Implementation Plan

Spec: [KERNEL_BW2001.md](KERNEL_BW2001.md). Design: [KERNEL_BW2001_DESIGN.md](KERNEL_BW2001_DESIGN.md).

Every phase ends with its fence:
- `cmake --build --preset release` and `--preset debug` clean;
- `clang-format` on the C++ files touched;
- `bench/trace_check.sh` identical;
- `bench/measure.sh` logged in the idea log, and any netlist slower by more than 5 % than the previous row shown to the user before the idea is kept (spec, Guard 3);
- the tests the phase names;
- one commit of this step's own paths, whose message names the tests run.

An idea that is dropped is reverted before the next one starts, and its row is still logged.

## Status

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Docs reviewed | done |
| 1 | Guards and baseline | done |
| 2 | Z1: falling-edge-only and sampled Pins | |
| 3 | Z2: quiet Pins (kernel, then 2114, then ROM) | |
| 4 | Z3: specialised Parts, from the profile | |
| 5 | Z4: run loop, from the profile | |
| 6 | Finish | |

## Phases

### 1. Guards and baseline

- `bench/trace_check.sh` and `bench/measure.sh` as designed. The typed text and times for the long runs are chosen with the reference build and `--tv`, the BMP viewed, and written in the notes. VCD sizes logged.
- The check is run twice and passes. It is then shown to fail by flipping one value in a reference VCD, as P5b did.
- Baseline: `bench/measure.sh` twice, logged as row 0.
- Fence tests: none beyond the scripts. Commit: `bench: trace check and measurement on the computers (P5z)`.

### 2. Z1

- `PinSpec::fallingEdgeOnly` (appended after `sampled`), `FanOut` edge mask, the `runUntil` test.
- Tests: the wake cases in `tests/sim/simulator_test.cpp`; `tests/sim/circuit_test.cpp`'s `risingEdgeOnly` checks rewritten for the mask, plus merge cases.
- The Parts (74LS107, 74LS93, 74LS90), each checked against its `evaluate` as the design says. The 74LS107's `J`/`K` become sampled. The 74177 is not converted.
- Measured as one idea (the spec's Guard 3).
- Fence tests: the full debug suite (kernel change), and release `bw2001|bw80|bw1`. ADD_PART: the lines the design names. Commit: `sim: falling-edge-only Pins, the dot-clock chain's clocks (P5z Z1)`.

### 3. Z2

1. Kernel only: `quiet`, `lastChange`, `pinFanOut_`, `fanOutLoud_`, `pinQuiet_`, `tests/sim/quiet_test.cpp` (added to `tests/CMakeLists.txt`). Measured with no Part using it: the cost of the extra test on every netlist, logged as row Z2.0. Not committed alone.
2. The 2114, and its tests. Measured.
3. The ROM, and its tests. Measured.

The bar applies to steps 1–3 together against Z1. Fence tests: the full debug suite, and release `bw2001|bw80|bw1|bus load|typewriter`. ADD_PART: Quiet Pins. One commit: `sim: quiet Pins, deselected 2114s and ROMs sleep (P5z Z2)`.

### 4. Z3

- Profile on Z2 (`build/profile`, by function), logged in the notes.
- One idea per row, hottest Part first. The design's starting points come first, and the row's own design is written in the notes.
- Each idea is kept on the bar, one commit each: `parts: <Part> specialised (P5z Z3.n)`.
- Fence tests: that Part's tests and every Machine using it (`ctest -R`), in debug; release `bw2001|bw80`.
- The phase ends when no Part reaches the bar.

### 5. Z4

- A line profile of `runUntil` (P5b notes: `sample` with offsets, self time per line).
- One idea per row, as for Z3, each kept on the bar.
- Fence tests: the full debug suite per kept idea.

### 6. Finish

- Final `bench/measure.sh` (last row of the log), and the trace check.
- Spec Results, ARCHITECTURE Performance and row P5z (done, date, links), OPTIMISATION_IDEAS (Target, History, Next steps with the options left).
- `bw_ui` screenshots of a BW-2001 and a BW-80 Puzzle viewed, with the ratio and UI ms noted.
- Full debug suite. Plan cut down to the phase table and notes.
- Commit: `docs: P5z done, kernel for the BW-2001`.

## Idea log

| # | Idea | bw2001 ratio | Instr./event | Cycles/event | Eval./event | bw80 | bus_load | Kept | Commit |
|---|------|--------------|--------------|--------------|-------------|------|----------|------|--------|
| 0 | baseline (`f849080`) | 0.210× | 856 | 156 | 2.14 | 0.242× | 0.373× | — | `f849080` |

## Implementation notes

- Docs review (2026-11-16), fixed before the docs commit:
  - the 74177 left out of Z1 (an X clock makes its counts X);
  - the 74LS90 kept (not on the BW-2001; ADD_PART's rule) and "dot-clock chain" corrected;
  - a 2114 quiets only on a definite `CS_N` 1;
  - `fallingEdgeOnly` appended after `sampled` (positional `PinSpec`s);
  - `bus load` as the test name pattern;
  - `circuit_test`'s `risingEdgeOnly` checks rewritten;
  - ADD_PART's actual lines named (line 38 said falling clocks stay unmarked);
  - wake tests in `simulator_test.cpp`; `quiet_test.cpp` in `tests/CMakeLists.txt`;
  - kernel details (reset before the power-on evaluations, entry index accessors, `quiet` on a Pin without an entry, count `Check`, mask with quiet Pins);
  - quiet toggled only on change;
  - eval./event in `measure.sh` and the slowdown fence;
  - trace check globbing, cache keyed on the options, typing pace and times, real Net names.
- Measured while writing the spec (2026-11-16, before phase 1): see the spec's Why. `bw_ui` on healthy BW-2001 Puzzles: 0.20×, UI 2.5 ms. On `bad_zero_page`: 0.62×, because its Fault stops the CPU. On the BW-80 (`q_everywhere`'s family): 0.25×, UI 0.9 ms.
- Phase 1 (2026-11-16):
  - Typed text, chosen with the reference build and `--tv`, BMPs viewed:
    - BW-2001: `J5:1600ms:PRINT7\n` in a 2.5 s run; the screen shows `PRINT7`, ` 7`, `READY`.
    - BW-80: `J3:400ms:P` in a 1 s run; the edit line changes.
  - VCD sizes on each side:
    - bus_load 55 MB, test pattern 8 MB, typewriter 147 MB;
    - bw2001 214 MB, bw2001 long **706 MB** (over the 300 MB aimed for: the video RAM buses and `TV_VIDEO` at 8 MHz; kept, since disk and time allow);
    - bw80 178 MB, bw80 long 303 MB, bw1 34 MB.
  - A full check takes 80 s with the reference recorded, 45 s without.
  - Shown to fail: one value flipped in the reference test pattern VCD gave line 5001, its `#time` and both values.
  - Baseline, `bench/measure.sh` twice: bw2001 0.212 / 0.210×, 856 instructions, 155–156 cycles, 2.14 evaluations an applied event; bw80 0.243 / 0.242×; bus_load 0.372 / 0.373× (458 instructions, 97 cycles, 0.96 evaluations).
  - Others, best of the two: ball 2.59×, bus_load_z80 0.42×, bw1 2.57×, chaser 14.0×, counter 12.7×, dice 33.0×, heartbeat 8.5×, knob 14.8×, led_computer 5.1×, message 2.73×, stopwatch 10.7×, tennis 1.91×, test pattern 6.98×, typewriter 1.30×.
  - **Noise:** the small, fast netlists move up to ±7 % between two runs of the same build (dice −5.8 %, stopwatch +6.7 %). A netlist `measure.sh` marks SLOWER is measured again before it is shown to the user.
  - The script's output is kept in `build/measure/` and passed as the previous run for the next comparison.

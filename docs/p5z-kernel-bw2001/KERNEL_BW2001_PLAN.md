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
| 2 | Z1: falling-edge-only and sampled Pins | done: kept |
| 3 | Z2: quiet Pins (kernel, then 2114, then ROM) | done: kept |
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
| Z1 | `PinSpec::fallingEdgeOnly` on the 74LS107, 74LS93, 74LS90 clocks; the 107's `J`, `K` sampled | 0.222× | 830 | 146–148 | 1.95 | 0.244× | 0.382× | yes: +5.7 % ratio, −5…6 % cycles (two runs) | this row's commit |
| Z2.0 | quiet Pins in the kernel, no Part using them | 0.219× | 845 | 151 | 1.95 | 0.241× | 0.371× | measured only: the test's cost, 1–3 % on every netlist | — |
| Z2 | quiet Pins: deselected 2114s (`CS_N` a definite 1) and ROMs (off) sleep through their address and data | 0.315–0.320× | 580 | 107 | 1.18 | 0.247× | 0.384–0.387× | yes: +44 % ratio, −27 % cycles against Z1 (two runs) | this row's commit |

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
- Phase 2, Z1 (2026-11-16):
  - Traces identical. Evaluations in the trace runs: bw2001 35.1 M → 31.4 M (−11 %), bw2001 long 714 M → 652 M; the others within a few thousand.
  - Two measurements: bw2001 0.222× both times, 830 instructions, 146–148 cycles, 1.95 evaluations an event. Just over the bar on both, so kept.
  - Others against row 0, best of 3; none slower: ball 2.70×, bw1 2.62×, chaser 13.7×, counter 12.5×, dice 33.5×, knob 19.0× (+28 %: its 74LS93s), stopwatch 11.8×, tennis 2.04×, test pattern 7.20×, typewriter 1.32×.
  - The 74LS90 checked against its code: its clocks enter only through `fell`, and its R0/R9 X paths are idempotent.
  - Full debug suite 1361/1361 in 806 s; release `bw2001|bw80|bw1` 156/156.
- Phase 3, Z2 (2026-11-16):
  - **One change from the design:** the Simulator keeps its own copy of the fan-out entries, whose new `pins` byte counts the loud Pins, in place of a separate `fanOutLoud_` array. The run loop then reads no extra cache line; `pinQuiet_` is as designed.
  - The kernel alone (Z2.0) costs 1–3 % everywhere, as the test of the count that no Part changes yet.
  - Traces identical. Evaluations in the trace runs: bw2001 31.4 M → 23.4 M, bw2001 long 652 M → 376 M (−42 %), bus_load 5.68 M → 5.22 M, bw80 18.15 M → 17.43 M.
  - Others against Z1, none slower beyond noise: led_computer +19 % (its 2114 and ROM), the rest within ±2.5 %.
  - Test fix: the 2114's X-select count case first held `WE_N` high, so an X-selected chip drove its data Pins and was woken by its own drives (15 evaluations, not 10). `WE_N` low keeps its outputs off.
  - Full debug suite 1372/1372 in 680 s (Z1: 806 s); release `bw2001|bw80|bw1|bus load|typewriter` 178/178.
  - Profile on Z2 (`build/profile`, 6,700 samples by function): `runUntil` 25 %, `resolveNet` 11.5 %, the queue 15 %, `GateFunction` 8 %, **74LS244 7.2 %** (the hottest Part), `driveChanged` 6 %, 74LS93 3.2 %, 74LS107 2 %, 74LS157 2 %, 2114 1.4 %, ROM 1.3 %.

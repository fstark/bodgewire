# P5z Kernel for the BW-2001 — Implementation Plan

Spec: [KERNEL_BW2001.md](KERNEL_BW2001.md). Design: [KERNEL_BW2001_DESIGN.md](KERNEL_BW2001_DESIGN.md).

Implemented 2026-11-16; user check pending. The phases and fences are in git history (`2f422b7`); results in the spec (Results).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `2f422b7` |
| 1 | Guards and baseline | `bf382ac` |
| 2 | Z1: falling-edge-only and sampled Pins | `5ce7af3` |
| 3 | Z2: quiet Pins (kernel, 2114, ROM) | `4916ff2` |
| 4 | Z3: Parts from the profile: 74LS244 quiet halves kept (Z3.1); unchanged buffers skipped dropped (Z3.2) | `158e181` |
| 5 | Z4: run loop: plain buses kept (Z4.1); fan-out masks dropped (Z4.2) | `eba14fa` |
| 6 | Finish | this commit |

## Idea log

| # | Idea | bw2001 ratio | Instr./event | Cycles/event | Eval./event | bw80 | bus_load | Kept | Commit |
|---|------|--------------|--------------|--------------|-------------|------|----------|------|--------|
| 0 | baseline (`f849080`) | 0.210× | 856 | 156 | 2.14 | 0.242× | 0.373× | — | `f849080` |
| Z1 | `PinSpec::fallingEdgeOnly` on the 74LS107, 74LS93, 74LS90 clocks; the 107's `J`, `K` sampled | 0.222× | 830 | 146–148 | 1.95 | 0.244× | 0.382× | yes: +5.7 % ratio, −5…6 % cycles (two runs) | `5ce7af3` |
| Z2.0 | quiet Pins in the kernel, no Part using them | 0.219× | 845 | 151 | 1.95 | 0.241× | 0.371× | measured only: the test's cost, 1–3 % on every netlist | — |
| Z2 | quiet Pins: deselected 2114s (`CS_N` a definite 1) and ROMs (off) sleep through their address and data | 0.315–0.320× | 580 | 107 | 1.18 | 0.247× | 0.384–0.387× | yes: +44 % ratio, −27 % cycles against Z1 (two runs) | `4916ff2` |
| Z3.1 | 74LS244: a half whose `OE_N` is a definite 1 quiets its `A` Pins (quiet Pins on a two-way bus buffer) | 0.333–0.335× | 550 | 101–103 | 1.07 | 0.250× | 0.395× | yes: +5.2 % ratio, −5.2 % instructions, −4.7 % cycles (two runs), kept as part of the quiet-Pins idea (Guard 3: an idea counts with every Part it is applied to) | `158e181` |
| Z4.1 | plain buses (untied, no network Pin, two or more Output or Bidirectional Drivers) resolved from a count of their driving Drivers and the sum of their PinIds, when 0 or 1 | 0.364–0.365× | 509 | 95 | 1.07 | 0.246–0.247× | 0.413× | yes: +9 % ratio, −7 % cycles, −7.5 % instructions (two runs) | `eba14fa` |
| Z3.2 | 74LS244: a buffer whose `OE_N` and `A` did not change at this timestamp is skipped | 0.362× | 511 | 97 | 1.07 | 0.250× | 0.408× | no: equal | — |
| Z4.2 | fan-out masks per Net (loud entries, entries per change kind): the run loop visits only loud entries that wake on the change | 0.344× | 495 | 100 | 1.07 | 0.241× | 0.400× | no: −2.7 % instructions, +5 % cycles, −6 % ratio | — |
| end | final `bench/measure.sh` (on Z4.1) | 0.362× | 509 | 95 | 1.07 | 0.246× | 0.411× | — | — |

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
- Phase 4, Z3.1 (2026-11-16):
  - The 74LS244 was the hottest Part on Z2. The BW-2001's six are paired back to back on two-way buses (`SD`↔`BD`, `D`↔`BD`, `BD`↔`BBD`), so the disabled half's `A` Pins see the enabled half's own outputs, and every drive woke the chip again. A disabled half drives Z whatever `A` does and keeps no timing state, so quieting its `A` Pins is exact.
  - Traces identical. Evaluations: bw2001 23.4 M → 21.5 M, bw2001 long 376 M → 341 M, bus_load 5.22 M → 4.97 M.
  - Just under the bar on cycles alone (−4.7 %), over it on ratio and instructions. Kept because it is the quiet-Pins idea of Z2 applied to one more Part, which Guard 3 measures as a whole.
  - Others against Z2: all equal or up to +3 %.
  - Full debug suite 1373/1373 in 675 s; release `bw2001|bw80|bus load` 101/101.
- Z4.1 (2026-11-16), taken before the rest of Z3 because the Z2 profile put `resolveNet` at 11.5 %:
  - The BW-2001's RAM data bus `BBD` has sixteen 2114s and two 74LS244 halves on each line, and `BD`, `SD` and `D` are shared too. Each change scanned every Driver through `Resolver`.
  - A Net with no tie, no network Pin and only Output or Bidirectional Drivers resolves exactly as `Resolver` does when none drives (floating: the TTL pull-up or the last driven value) or one drives (its value). The Simulator keeps each Net's count of Drivers not at Z and the sum of their PinIds: with one, the sum names it. Both are updated where an output's value changes (event apply) and recounted at power-on after `setOutput`.
  - Traces identical. Others against Z3.1: bw1 +13 % (its 6530 and RAM buses), bus_load_z80 +5.5 %, the rest within ±2.6 %.
  - Full debug suite 1374/1374 in 671 s; release `bw2001|bw80|bw1|bus load|typewriter` 178/178.
  - Profile on Z4.1 with LTO (`build/profile-lto`, 6,700 samples by function): `runUntil` 31 %, the queue 21 %, 74LS244 7.4 %, `resolveNet` 5.3 %, 74LS00 packages 4.9 %, 74LS93 4.6 %, 74LS08 3.0 %, 74LS107 2.9 %, 2114 2.7 %, ROM 2.5 %, 74LS157 2.5 %. The non-LTO profile showed `GateFunction` at 9.8 %; with LTO it is inlined into the packages. Profiles from here come from `build/profile-lto`.
- Z3.2: what is left of the 74LS244 after Z3.1 is the reads and drives of an enabled half, not the buffers that did not move. Equal, as P5b's 4.4 found for its reads.
- Z4 line profile on Z4.1 (`build/profile`, self time by source line through `sample`'s call tree): the fan-out loop (`simulator.cpp` 311–314) about 10 %, the queue's `openRun` and `takeNext` lines 2–4 % each, `driveChanged` three lines of about 2 % each; no other line over 3 %.
- Z4.2: the fan-out loop's 10 % was mostly quiet entries on the address and data buses. A 64-bit loud mask per Net (bits cleared and set by `setQuiet`) and three fixed masks of the entries waking on a rise, a fall, another change; the loop walks `loud & wakes[change]` in entry order (evaluation order unchanged), wider Nets the plain loop. Traces identical, instructions down, cycles up: the bit loop's dependent `countr_zero` steps and the masks' loads cost more than the short, predictable walk.
- Phase 6 (2026-11-16):
  - Final `bench/measure.sh` in the log's last row. Others against row 0: ball 2.69×, bus_load_z80 0.46×, bw1 2.96×, chaser 13.8×, counter 12.6×, dice 32.6×, heartbeat 8.4×, knob 19.4×, led_computer 6.49×, message 2.76×, stopwatch 11.7×, tennis 2.04×, test pattern 7.12×, typewriter 1.31×. None slower.
  - Trace check identical (8 runs).
  - Screenshots viewed: `bw_ui puzzles/ghost_letter.bwp` (BW-2001: the "VINY BASIC" banner, `READY`; 0.36×, UI 2.4 ms) and `puzzles/q_everywhere.bwp` (BW-80: the Q-filled page and junk margins; 0.23×, UI 0.9 ms).
  - The full debug suite last ran on Z4.1's code (1374/1374 in 671 s); nothing in `src` or `tests` changed after it.
- User check (pending):
  - `bw_ui` on a BW-2001 Puzzle: the speed shown at about 0.36×, the picture and keyboard as before;
  - `bench/trace_check.sh` (45 s once the reference is recorded) and `bench/measure.sh` on your machine.

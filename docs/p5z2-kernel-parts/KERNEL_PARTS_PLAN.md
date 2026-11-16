# P5z2 Kernel: Parts — Implementation Plan

Spec: [KERNEL_PARTS.md](KERNEL_PARTS.md). Design: [KERNEL_PARTS_DESIGN.md](KERNEL_PARTS_DESIGN.md).

Every phase ends with its fence:
- `cmake --build --preset release` and `--preset debug` clean;
- `clang-format` on the C++ files touched;
- `bench/trace_check.sh` identical;
- `bench/measure.sh build/measure/p5z2-<prev>.txt | tee build/measure/p5z2-<phase>.txt` (the baseline `p5z2-0.txt` taken in phase 1 before A, on `f010fb8`'s code), and the BW-80's evaluations over 1 s (`bw run netlists/bw80.bwn --time 1s`, the `evaluations` line), logged in the idea log with the lines the idea adds and its verdict (spec, Terms);
- the full debug suite (gate packages and the 74LS373 are shared Parts), and release `bw2001|bw80|bw1|bus load|typewriter`;
- one commit of this step's own paths, whose message names the tests run.

A dropped idea is reverted before the next, its row still logged.

## Status

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Docs reviewed | done |
| 1 | A: gate packages unit by unit | |
| 2 | B: 74LS93 and 74LS107 fast paths | |
| 3 | C: quiet D on the 74LS373 | |
| 4 | Finish | |

## Phases

### 1. A

- Baseline: `bench/measure.sh > build/measure/p5z2-0.txt` and the BW-80's evaluations, before any change.
- `EvalContext::readChanged`, `GatePackage::powerOn`, `first_`, `GatePackageSpec::evaluate(ctx, all)`, `EvaluatePackage`'s skip.
- ADD_PART: the gate package row.
- Tests as the spec says.
- Measured; logged.
- Commit: `parts: gate packages evaluate only the units whose inputs moved (P5z2 A)`.

### 2. B

- The 74LS93's reset read, then the 74LS107's known path, measured together as one idea and logged.
- The 74LS90's reset form only if the 93's showed a gain.
- Commit: `parts: 74LS93 and 74LS107 known-value paths (P5z2 B)`.

### 3. C

- The 74LS373's quiet `D`, and its tests.
- ADD_PART: the Quiet Pins rule reworded (design, ADD_PART).
- C's expected gain (2–3 % on the BW-80) is inside the ratio's noise: the BW-80's evaluation count and its instructions per event (`/usr/bin/time -l bw run netlists/bw80.bwn --time 1s`) decide.
- Measured; logged.
- Commit: `parts: 74LS373 D quiet while the latch is closed (P5z2 C)`.

### 4. Finish

- Final `bench/measure.sh` and the trace check.
- Spec Results; ARCHITECTURE Performance and row P5z2 (done, date, links); OPTIMISATION_IDEAS History.
- `bw_ui` screenshots of a BW-2001 and a BW-80 Puzzle viewed, ratio and UI ms noted.
- Plan cut down to the phase table, log and notes.
- Commit: `docs: P5z2 done, kernel Parts`.

## Idea log

| # | Idea | bw2001 ratio | Instr./event | Cycles/event | Eval./event | bw80 | bw80 evaluations (1 s) | bus_load | Lines | Kept | Commit |
|---|------|--------------|--------------|--------------|-------------|------|------------------------|----------|-------|------|--------|
| 0 | P5z's end (`f010fb8`) | 0.362× | 509 | 95 | 1.07 | 0.246× | | 0.411× | — | — | `f010fb8` |

## Implementation notes

- Docs review (2026-11-16), fixed before the docs commit:
  - ADD_PART's Quiet Pins rule reworded for a closed latch, not only a deselect;
  - A's pending-drive test called a regression guard;
  - ADD_PART in phase 1;
  - the BW-80's evaluations and instructions measured for C;
  - P5z's Z3.2 cited, with what makes A different and what drops it;
  - the 74LS90 in the spec and Files;
  - `measure.sh` output paths;
  - `LE`/`OE_N` loud;
  - one Net read per input (`readChanged`), not `read` and `readBefore`.

# P5b Kernel for buses — Implementation Plan

Spec: [KERNEL_BUSES.md](KERNEL_BUSES.md). Design: [KERNEL_BUSES_DESIGN.md](KERNEL_BUSES_DESIGN.md).

## Fences

Every phase that changes code ends with:

- `cmake --build --preset release` and `cmake --build --preset debug` clean; `clang-format` on the C++ files touched.
- `bench/trace_check.sh` identical (from phase 2 on).
- `bench/measure.sh`: the idea's row in the log (ratio of the three netlists, instructions per applied event on `bus_load`), against the last kept row.
- Kept (≥ 5 % on `bus_load` ratio and instructions, plain code; for phases 5–6 the bar applies to the summed conversion, spec Guard 3): the full debug suite (`ctest --preset debug -j 12`, ≈ 15 min: kernel and shared code, AGENTS.md), then one commit. Dropped: `git checkout` of the code, the log row committed with the next commit.
- Before each idea, the user is told what is tried, the gain expected and the files touched; after, the result and kept or dropped (spec, Progress). This is a message, not a stop: work goes on unless the idea needs a guard-2 exception.

Only this step's paths are committed (the user edits docs in parallel).

## Phases

### Phase 1 — Guards and baseline

- `bench/trace_check.sh`, `bench/measure.sh` (design).
- Run the trace check with the reference build: the current tree is the P5a kernel, so it must pass (it checks the script).
- Baseline: `bench/measure.sh` twice; the log's row 0, the other netlists in the notes.
- Queue numbers for idea 1: size distribution and delay spread on `bus_load` (temporary counters, not committed), in the notes.
- Fence: trace check identical; commit `bench: trace check against the P5a kernel, measurement script`.

### Phase 2 — Idea 1, the event queue

- Wheel (design) with its tests; measured; kept or dropped. If dropped, the 4-ary heap as a second row.
- Fence as above; commit `sim: event queue as a timing wheel` (or the kept variant's name).

### Phase 3 — Idea 2, the run loop

- Profile `runUntil` by line on the current best; one try per hot spot the profile shows (design, Idea 2), each its own row and commit `sim: …`.

### Phase 4 — Idea 3, LTO

- Release preset change; clean build time before and after in the notes; measured; commit `build: LTO in the release build` if kept.

### Phase 5 — Idea 4, the ROM, shown to the user

- `EvaluateRom` (design); ROM tests and the machines using ROMs green; measured.
- **Stop:** show the user the ROM's new code, its numbers, and any `EvalContext` addition it would want. Wait for the OK before phase 6. Commit `parts: ROM with specialised evaluation` when the user OKs (or drop, logged).

### Phase 6 — Idea 4, further Parts (after the OK)

- In profile order (gate packages, 74LS157, 74LS74, 74LS244, then the next in a fresh profile): one row and one commit each, `parts: <Part> with specialised evaluation`; ADD_PART updated in the first commit of this phase.
- Before the phase closes: the summed conversion (ROM and these Parts) against the last row before phase 5; below 5 %, every conversion commit is reverted together (`git revert`) and logged.

### Phase 7 — Finish

- Final `bench/measure.sh`; the spec's Results; ARCHITECTURE (Performance, row P5b done with date and links); OPTIMISATION_IDEAS (Target, History, Next steps); if short of 2×, the options left for the user.
- Release screenshots: `bw_ui netlists/bus_load.bwn --screenshot … --after 2s` and `bw_ui netlists/typewriter.bwn --screenshot … --after 2s`, viewed (pictures as before, ratio printed).
- Full debug suite; plan cut to the status table, idea log and notes ("user check pending"); commit `docs: P5b implemented, user check pending`.

## Status

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Guards and baseline | done |
| 2 | Event queue | — |
| 3 | Run loop | — |
| 4 | LTO | — |
| 5 | ROM, shown to the user | — |
| 6 | Further Parts | — |
| 7 | Finish | — |

## Idea log

| # | Idea | bus_load ratio | Instr./event | Test pattern | Typewriter | Kept | Commit |
|---|------|----------------|--------------|--------------|------------|------|--------|
| 0 | baseline (P5a kernel) | 0.225× | 643 | 5.46× | 0.997× | — | `0757c02` |

## Implementation notes

- Phase 1 (2026-11-12): `bench/trace_check.sh` identical against `0757c02` (10 s once the reference is built); checked to fail and locate a changed VCD line (one value flipped in the reference test pattern: "line 5001", the `#time` and both values printed). macOS bash is 3.2: the scripts use no associative arrays or namerefs. Baseline, `bench/measure.sh` twice: bus_load 0.225 / 0.224×, 643 instructions per applied event (76.88 G for 119.5 M events, 1 s); test pattern 5.46 / 5.42×; typewriter 0.997 / 0.986×; others: ball 2.29×, chaser 12.8×, counter 11.6×, dice 21.7×, knob 15.4×, message 2.32×, stopwatch 9.28×, tennis 1.53×.
- Queue on bus_load (100 ms, temporary counters): 13.7 M pops for 11.9 M applied events, 0.50 M stale, 1.30 M wakes. Size at pop: < 8 events 25 %, 8–15 43 %, 16–31 30 %, 32–63 2 %: **not the large queue P5a assumed**. Push delay from the last pop: < 16 ns 35 %, 16–33 ns 14 %, 66–131 ns 43 %, 131–262 ns 1 %, 262–524 ns 7 %; none beyond 0.53 µs. A heap of ≈ 16 is 4 levels deep: the cost per pop is in the comparison (three fields, branches) and sifting 16-byte events, not in depth.

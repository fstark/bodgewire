# Kernel Performance — Specification (P1d)

Opened by P1c: the test-pattern generator ran at 2.48× real time against a target of ≥ 10× ([TEST_PATTERN_PLAN.md](TEST_PATTERN_PLAN.md)). Builds on [KERNEL.md](KERNEL.md) and [PARTS.md](PARTS.md). Terminology: [GLOSSARY.md](GLOSSARY.md).

## Scope

In:

- Kernel-internal changes to make Component evaluation and Net updates cheaper.
- Gate packages using the faster path.
- Measurement method and gates (S6).

Out: multi-threading, a different engine (compiled, cycle-based), LTO or other build-flag tricks, UI and instruments.

## Starting point

Already done on `main`, measured on `netlists/test_pattern.bwn`, release build:

| Commit | Change | Ratio |
|---|---|---|
| — | P1c baseline | 2.48× |
| 543f850 | Hot `EvalContext` members, `DelayFor`, `GateFunction` inline in headers; `drive` fast path for unchanged values | 4.5× |
| ccd9c47 | Rising-edge-only clock Pins: falling and X clock changes don't wake the Component | 5.0× |
| 738d9e1 | Gate package dispatches on gate kind once per evaluate | 5.3× |

Decision: **revert 738d9e1** first. It duplicates `GateFunction`, adds a one-kind-per-package rule, and its gain is within run-to-run noise. S3 replaces it.

Profile at 5.3× (share of samples): gate packages ≈ 27%, `runUntil` own code ≈ 26%, 74LS161 ≈ 21%, event queue ≈ 17%. Counts per simulated µs: 68 Pin reads (`read`, `readBefore`, `rose`), 6 Net changes, 12 evaluations.

Diagnosis: the time goes into gathering inputs, not computing logic. Each read is pin → Net (`Circuit`) → value (`Simulator`), a chain of dependent loads, and a woken Component re-reads all its inputs though usually one changed. Pushing each Net change to the Pins that read it would cost 18 writes per µs instead of 68 reads (measured).

## Behaviour

### S1 Packed inputs (push)

- `Simulator` keeps, per Component, its input values packed into words: two bit-planes, *value* and *known* (known = 0 means X). Bit position = Pin index.
- When a Net's value changes, the Simulator updates the corresponding bits of every Component reading it, and wakes the Component (as today: rising-edge-only Pins wake on 0 → 1 only).
- Per Net, one precomputed fan-out list replaces today's readers list: `{component, word index, bit mask, rising-edge-only}`.
- Components have at most 64 Pins (`Check`ed). All planned Parts fit (largest: 40-pin CPU).

### S2 Part-facing API unchanged

- `read`, `readBefore`, `rose`, `readBit`, `drive`, `wakeAfter` keep their meaning and signatures. Existing Parts compile and behave identically.
- `read` takes its value from the packed words.
- `readBefore` and `rose` keep using Net state (`previous`, `changedAt`): they need the value before this timestamp, which the packed words no longer hold.
- New, optional: `ctx.inputs()` returns the two packed planes, for Parts that evaluate with bit operations. In P1d only `GatePackage` uses it.

### S3 Gate packages on packed inputs

- `GatePackage::evaluate` computes all its gates from the packed planes with bit operations (three-valued: X propagates as today).
- It calls `drive` only for outputs whose value changed since its last evaluation.
- The 74LS161 stays on `read`/`rose`: one Component, and its work is real (counting).

### S4 Derived data, rebuilt in one place

- The fan-out lists and word layout are derived from the topology by one function, run by `Circuit::finalize()`.
- It must be safe to re-run later: P2 changes wiring at runtime (broken trace, chip swap, trace repair). A Pin that loses its Net (broken leg) moves to a private Net, as unconnected Pins already do.

### S5 State stays in `Simulator`

- Packed words live in one flat array owned by the `Simulator`, indexed (not pointers into Components). Two Simulators on one Circuit stay independent (golden board vs faulted board, P3).

### S6 Measurement and gates

- Benchmark: `bw run netlists/test_pattern.bwn --time 5s`, release preset, best of 3 runs. Recorded in the plan after each phase, with `applied_events` and `evaluations`.
- Behaviour must not change: same `applied_events` and `evaluations` as before each phase (except where a phase removes evaluations on purpose, stated in the plan), P1c golden check passes, all tests pass in debug (sanitizers) and release.
- VCD of `netlists/test_pattern.bwn` over 40 ms, seed 1, is byte-identical before and after P1d.
- Profiling: optimised build with symbols in `build/profile` (`-O3 -g -fno-omit-frame-pointer`, not a preset), sampled with macOS `sample`.
- Pass: ≥ 10× real time on the test pattern.
- If S1–S3 land below 10×: profile again; the next candidate is the event queue (binary heap → bucket queue by time), as a further phase of P1d.

## Done when

- S6 pass, or S1–S3 and the event-queue phase done and the shortfall recorded with a profile.
- [KERNEL_DESIGN.md](KERNEL_DESIGN.md) updated to describe packed inputs and fan-out lists.

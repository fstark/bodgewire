# P5b Kernel for buses — Specification

Roadmap row P5b ([ARCHITECTURE.md](../ARCHITECTURE.md), Roadmap). Design: [KERNEL_BUSES_DESIGN.md](KERNEL_BUSES_DESIGN.md). Plan: [KERNEL_BUSES_PLAN.md](KERNEL_BUSES_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

P5a measured a ZX80-shaped load, `netlists/bus_load.bwn`, at **0.22×** real time headless: 120 applied events per simulated µs at 644 instructions each ([BUS_LOAD.md](../p5a-bus-load/BUS_LOAD.md), Results). Below 1×, the user's threshold puts kernel work in its own step before the 6502. The profile on that load: event queue `pop` 27 % and `push` 11 % (1,786 and 716 of ≈ 6,700 samples), `runUntil`'s own code 25 %, Parts ≈ 30 % (ROM 7 %, gate packages with `GateFunction` 9 %).

## Aim and terms (set 2026-11-12, ARCHITECTURE row P5b)

- **Aim:** as close to 2× on `bus_load` headless as the kernel allows *without bending its design*. The step may end short; it then ends with the measured ratio and the options left, for the user to choose.
- **Guard 1, the trace check:** every Net of three runs (below), recorded over a fixed time and seed, byte-compared with the P5a kernel's. Every idea keeps them identical. A difference that comes only from reordered simultaneous events is shown to the user and explained before the idea is kept. The debug ASan/UBSan suite stays green.
- **Guard 2, the design stays:** the Part API (`EvalContext`), Board, Faults, Puzzles and instruments unchanged; no Part-specific code in the kernel, no second kernel path, no undefined behaviour or unsafe shortcuts, no cuts to timing or to which Nets are simulated (OPTIMISATION_IDEAS ideas 2–4 are out). An idea that needs any of these stops and goes to the user.
- **Guard 3, one idea at a time:** one commit each, kept only if it gains ≥ 5 % on `bus_load` (ratio and instructions retired) and its code reads as plainly as what it replaces; otherwise reverted and logged. Other Machines may get slower if they stay correct; measured and reported each time (every `netlists/*.bwn`). For specialised evaluation (S3.4) the unit is the conversion, not the Part: the ROM is kept on the user's OK as the pattern; further Parts are each committed and logged, and the 5 % bar applies to the summed conversion (ROM plus the Parts after it, against the last row before the ROM), measured before phase 6 closes; if it falls short, the whole conversion is reverted together.
- **Progress:** before each idea the user is told what is tried, the gain expected and the files touched; after measuring, the result (ratio, instructions, other netlists) and kept or dropped. The plan's idea log has one row per idea.

## User decisions (2026-11-12)

| Question | Decision |
|---|---|
| How the P5a kernel's traces are kept | an on-demand script: it builds the P5a commit (`0757c02`) in a git worktree, records the reference VCDs once, and byte-compares fresh ones. Nothing checked in; the script stays in `bench/` after the step |
| What the traces cover | `bus_load`, the test pattern and the typewriter (with typing). This narrows the row's "each Machine": other Machines are guarded by their own tests and the debug suite |
| LTO | tried in the release build only; debug (ASan/UBSan) unchanged |
| Specialised evaluation, first Part | the ROM (`Rom`, 2716 and 2513): the hottest single Part on `bus_load`, and the shape later memories and CPUs follow. Shown to the user, who OKs it before any other Part is converted |

## Behaviour

Nothing a player, a Puzzle or an instrument sees changes. What is new:

### S1 Trace check

`bench/trace_check.sh` records, with `bw run … --seed 1 --record-all --vcd`:

| Run | Time | Input |
|---|---|---|
| `netlists/bus_load.bwn` | 50 ms (two and a half frames: the page drawn) | none |
| `netlists/test_pattern.bwn` | 100 ms | none |
| `netlists/typewriter.bwn` | 300 ms | `--type J3:100ms:HI` |

The reference side runs once per reference commit (cached under `build/trace-ref/`); the current side runs on `build/release`. The check passes when the three VCDs are byte-identical and each run prints the same `applied_events` and `evaluations`. On a difference it prints the first differing VCD line and its time.

### S2 Measurement

`bench/measure.sh`, release build, the machine otherwise idle:

1. Ratio: `bw run X --time 2s`, best of 3, every `netlists/*.bwn` alternated (`bus_load`, the test pattern and the typewriter in the log's columns, the others in the notes).
2. Instructions: `/usr/bin/time -l bw run netlists/bus_load.bwn --time 1s`, instructions retired, and per applied event.
3. Where time goes (when an idea needs it): `build/profile` (`-O3 -g -fno-omit-frame-pointer`) sampled with `sample`, shares by function.

Baseline taken in phase 1 on the same day as the ideas, since the machine's load moves the ratio (P5a: typewriter 0.99× at the start, 0.93× at the end).

### S3 Ideas, in profile order

1. **The event queue:** on `bus_load` it holds many pending events (address and data bits on 100–450 ns delays). A bucket queue or timing wheel, rejected in P1d on a heap of 8–15 events, is measured again; it pops in exactly the `(time, target, stamp)` order of today, so traces stay identical.
2. **The run loop's own code** (`runUntil`, `markNetDirty`, `driveChanged`): what a fresh profile of its lines shows.
3. **LTO** in the release build.
4. **Specialised evaluation** (OPTIMISATION_IDEAS idea 1) on the ROM, within today's `EvalContext`. If the gain needs an addition to `EvalContext` (for instance reading several Pins at once), that is a Part API change: it is shown to the user with the ROM, not made silently. This stop is the likely outcome: idea 1's real lever (Net indices precomputed per Pin, no generic `read`/`drive`) needs more than today's `EvalContext`; within it, only loops and constants can be specialised. After the user's OK, other Parts in profile order (gate packages, 74LS157, 74LS74, 74LS244), one idea each, and ADD_PART describes the new way.

Each idea's design is written in the plan's idea log when it is tried, not ahead: the next idea depends on the last profile.

## Tests

- `bench/trace_check.sh` passes after every kept idea (S1).
- Event queue changes keep `tests/sim/event_queue_test.cpp` (pop order `(time, target, stamp)`) and gain cases for whatever the new queue adds (events far in the future, held in the overflow; a push from outside `runUntil` earlier than the event `top()` last showed, as `Simulator::wake(…, 0)` makes between two `runUntil` calls).
- A converted Part keeps its existing tests unchanged; they are its behaviour check.
- The full debug suite after each kept idea (kernel and shared Parts: AGENTS.md).

## Results

Filled at the end: baseline and final ratio and instructions on the three netlists, the idea log's summary, what is left.

## Done when

- The ideas of S3 tried, each logged, kept ones committed under the three guards.
- Final measurement recorded here, in ARCHITECTURE (Performance, row P5b) and OPTIMISATION_IDEAS (Target, History, Next steps); ADD_PART updated if Parts are written differently.
- If short of 2×: the ratio and the options left written for the user.
- Full debug suite green; release screenshots of `bus_load` and the typewriter viewed (pictures unchanged).

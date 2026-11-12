# P5b Kernel for buses — Implementation Plan

Spec: [KERNEL_BUSES.md](KERNEL_BUSES.md). Design: [KERNEL_BUSES_DESIGN.md](KERNEL_BUSES_DESIGN.md).

## Fences

Every phase that changes code ends with:

- `cmake --build --preset release` and `cmake --build --preset debug` clean; `clang-format` on the C++ files touched.
- `bench/trace_check.sh` identical (from phase 2 on).
- `bench/measure.sh`: the idea's row in the log (ratio of the three netlists, instructions and cycles per applied event on `bus_load`), against the last kept row, with nothing else running.
- Kept (≥ 5 % on `bus_load` ratio and cycles, plain code; for phases 5–6 the bar applies to the summed conversion, spec Guard 3): the full debug suite (`ctest --preset debug -j 12`, ≈ 15 min: kernel and shared code, AGENTS.md), then one commit. Dropped: `git checkout` of the code, the log row committed with the next commit.
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

### Phase 5a — A second look (the user's OK, 2026-11-12)

Four ideas found after phase 5 by reading the code and counting on `bus_load`, each measured first on a scratch copy, then applied in this order, one commit each, with the fences above:

- 1f, the event queue as runs of same-time events (design, Idea 1f): `sim: event queue as runs of same-time events`.
- 5, sampled inputs (design, Idea 5; the trace check stops comparing `evaluations`): `sim: sampled inputs never wake their Component`.
- 4.2, gate packages (design, Idea 4.2): phase 6's first Part, with ADD_PART: `parts: gate packages with specialised evaluation`.
- 2b, two small changes to the run loop (design, Idea 2b): `sim: readBit inline, the Net of each Pin kept by the Simulator`.

Ideas 5 and 2b gain less than 5 % alone: kept on the user's decision (spec, User decisions).

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
| 2 | Event queue | done: 1a kept |
| 3 | Run loop | 2a dropped; nothing in the line profile reaches 5 % alone |
| 4 | LTO | done: kept |
| 5 | ROM, shown to the user | done: kept (the user's OK, 2026-11-12) |
| 5a | A second look: 1f, 5, 4.2, 2b | 1f, 5 done |
| 6 | Further Parts | — |
| 7 | Finish | — |

## Idea log

| # | Idea | bus_load ratio | Instr./event | Cycles/event | Test pattern | Typewriter | Kept | Commit |
|---|------|----------------|--------------|--------------|--------------|------------|------|--------|
| 0 | baseline (P5a kernel) | 0.225× | 643 | 165 | 5.46× | 0.997× | — | `0757c02` |
| 1a | one 128-bit key for `(time, target, stamp)` in the heap | 0.244× | 640 | 149 | 5.50× | 1.013× | yes: −10 % cycles, +8 % ratio; others equal or faster | this commit |
| 1b | timing wheel, 1,024 slots of 1,024 ps, overflow heap | 0.239× | 710 | — | 4.27× | 0.780× | no: +10 % instructions, others 15–40 % slower | — |
| 1b′ | same, 256 slots of 4,096 ps, no sort of one-event slots | 0.239× | 705 | — | 4.37× | 0.775× | no | — |
| 1c | 4-ary heap, hand-written | 0.202× | 657 | — | 5.43× | 0.959× | no: slower | — |
| 1d | binary heap, hand-written, child chosen without a branch (on 1a) | 0.244× | 636 | 149 | 5.54× | 0.991× | no: cycles unchanged | — |
| 1e | sorted vector, earliest last (on 1a) | 0.230× | 626 | 158 | 5.55× | 0.995× | no: +6 % cycles | — |
| 2a | `markNetDirty`, `markComponentDirty` inline in the header | 0.245× | 623 | 148 | 5.58× | 1.020× | no: −1 % cycles | — |
| 3 | LTO, release preset | 0.256× | 584 | 141 | 5.73× | 1.020× | yes: −5…6 % cycles, +5.0…5.3 % ratio (three runs) | `cd6a085` |
| 4.1 | ROM: `EvaluateRom<Layout>`, compile-time pin layout per Part, reached through `RomSpec::evaluate` | 0.279× | 577 | 133 | 5.99× | 1.047× | yes, on the user's OK: −5…6 % cycles, +7…9 % ratio (two runs) | `45f0d40` |
| 1f | pending events as runs of same-time events, a take is a whole timestamp (on 4.1) | 0.354× | 503 | 103 | 5.76× | 1.071× | yes: −24 % cycles, +34 % ratio against 4.1 measured the same hour (0.265×, 578, 136) | `dca233b` |
| 5 | sampled inputs: `PinSpec::sampled`, the `D`s of the 74LS74 and 74LS175 never wake their chip | 0.377× | 479 | 100 | 5.89× | 1.083× | yes, on the user's decision (under the bar): −3 % cycles, −5 % instructions, +6 % ratio | this commit |

## Implementation notes

- Phase 1 (2026-11-12): `bench/trace_check.sh` identical against `0757c02` (10 s once the reference is built); checked to fail and locate a changed VCD line (one value flipped in the reference test pattern: "line 5001", the `#time` and both values printed). macOS bash is 3.2: the scripts use no associative arrays or namerefs. Baseline, `bench/measure.sh` twice: bus_load 0.225 / 0.224×, 643 instructions per applied event (76.88 G for 119.5 M events, 1 s); test pattern 5.46 / 5.42×; typewriter 0.997 / 0.986×; others: ball 2.29×, chaser 12.8×, counter 11.6×, dice 21.7×, knob 15.4×, message 2.32×, stopwatch 9.28×, tennis 1.53×.
- Queue on bus_load (100 ms, temporary counters): 13.7 M pops for 11.9 M applied events, 0.50 M stale, 1.30 M wakes. Size at pop: < 8 events 25 %, 8–15 43 %, 16–31 30 %, 32–63 2 %: **not the large queue P5a assumed**. Push delay from the last pop: < 16 ns 35 %, 16–33 ns 14 %, 66–131 ns 43 %, 131–262 ns 1 %, 262–524 ns 7 %; none beyond 0.53 µs. A heap of ≈ 16 is 4 levels deep: the cost per pop is in the comparison (three fields, branches) and sifting 16-byte events, not in depth.
- Phase 2, idea 1 (2026-11-12): the queue's numbers above made the design's wheel unlikely; tried as designed anyway (1b, 1b′, 1c in the log): the trace check passed for each, and each lost. With ≈ 16 pending events a binary heap is 4 levels; the wheel pays for slot bookkeeping and bitmap scans on every pop, and on sparse netlists scans many empty words. 1a (one 128-bit key for `(time, target, stamp)`, same order) cut cycles 9.5 % (19.7 → 17.8 G, three runs each) with instructions −0.6 %: `bus_load` runs at ≈ 3.6 instructions a cycle against 5.7 on the test pattern, and the heap's three-field comparison mispredicts. The user then set the bar on ratio and cycles (spec, User decisions) and 1a was kept.
- Line profile (`sample` call tree with offsets, self time per source line, a scratch parser; `xctrace` needs full Xcode): with 1a, `std::pop_heap` is 22.5 % of all time on a heap of ≈ 16; `runUntil`'s own 26 % is spread, the largest line being Net resolution (5.8 %). 1d and 1e show the heap's cost is the chain of dependent loads per level, not branches or depth: neither a branchless child choice nor a sorted vector shortens it.
- Phase 3: `markNetDirty` was out of line (2.8 %); inlining gained 1 % (2a). No other line of `runUntil` exceeds 2 %: no run-loop try reaches the bar alone, so the phase ends here.
- Phase 4, LTO: clean release build 23 s → 27 s (SDL3, imgui and Catch2 included); touching `simulator.cpp` relinks in 7 s. Other netlists: ball 2.28 → 2.22×, dice 21.7 → 21.2× (−2 %), the rest equal or faster. The release test suite passes with LTO (810); debug, which LTO does not touch, was last green at 1a. From here profiles come from `build/profile-lto`.
- Phase 5: profile on LTO (`build/profile-lto`, no `-g`: by function): `runUntil` 27.8 %, `pop` 26.2 %, `push` 9.8 %, gate packages 8.9 %, ROM 7.9 %, 74LS157 5.1 %, 74LS74 3.1 %, 2114 2.9 %, 74LS244 2.8 %, the stub 2.4 %. The ROM alone gained 6 % in cycles, more than the design's 3–4 %: the generic version looped over spans of run-time length and recomputed the longest delay each time. Trace identical; ROM, catalogue, manuals and `bus load` tests green in release (80) and debug (61).
- ROM shown to the user: the first version dispatched on the spec's place in `s_specs` (`&spec_ == &s_specs[1]`), so a new ROM Part needed a layout, a dispatch branch and a constructor check kept in step. At the user's request the specialised function moved into the spec row (`RomSpec::evaluate = &EvaluateRom<layout>`): a new ROM is its arrays, one `RomLayout` line and its row; `Rom::evaluate` is one call. Same numbers (133–135 cycles an event), trace identical.
- Phase 5a (2026-11-13), a second look. Counters on `bus_load` (100 ms, temporary): 13.7 M pops in 6.36 M timestamps, 2.16 each (one pop 55 % of timestamps, two 28 %, eight or more 7 %); 43 % of pushes carry the time of the push before; 12.9 M evaluations, 57 % of which drive nothing new and ask no wake; Net resolution by the single-driver path 65 %; 20.1 M Component marks for 11.9 M applied events, 7.2 M of them on a Component already marked. The ROM state (4.1) measured again before the first idea: 0.265×, 578 instructions and 136 cycles an event (the machine that hour; phase 5 logged 0.279×). The rows from 1f on compare with it.
- 1f: four shapes replayed on the queue's own recorded operations (20 ms of `bus_load`, 2.7 M pushes and 1.27 M takes; cycles per event popped): the heap of 1a 51; groups by time, each its own vector, 23; unordered runs scanned for the earliest time 28–30; time-ordered runs in one flat array 22, kept. A first version moved the open run in time order when `keys_` refilled, so the next push to it overran its neighbour: found by a random comparison with the heap, which is now the test "same takes as a sorted list over long random use" (it fails when the mistake is put back). A test of the size before the sort: no gain (503 instructions and 103 cycles either way), left out. Other netlists (2 s, best of 3, 4.1 → 1f): ball 2.27 → 2.35×, chaser 13.7 → 14.3×, counter 12.5 → 12.9×, dice 22.4 → 23.6×, knob 15.6 → 16.0×, message 2.36 → 2.50×, tennis 1.53 → 1.58×, stopwatch 9.87 → 9.19× (9.3–10.0× on both builds when alternated: noise). Profile-guided optimisation, on the scratch copy only: +9 % on the heap, +3 % on top of 1f, 5 and 4.2 with the typewriter and the test pattern 4–6 % slower: not pursued.
- 5: evaluations in the trace check's runs: `bus_load` 6,465,280 → 5,691,230 (−12 %), test pattern 1,224,046 → 1,204,510, typewriter 17,727,601 → 17,436,400; VCDs and applied events identical. The evaluations spared are short ones (a 74LS74 or 74LS175 finding no clock edge), hence 3 % of the cycles for 12 % of the evaluations. `bench/trace_check.sh` prints the two counts in place of comparing them. Nothing outside the kernel reads the fan-out (checked: only `runUntil`), so the Net panel and the electrical checks see the same Pins. Other netlists (1f → 5): ball 2.35 → 2.40×, chaser 14.3 → 14.7×, counter 12.9 → 13.4×, dice 23.6 → 23.4×, knob 16.0 → 16.4×, message 2.50 → 2.54×, stopwatch 9.19 → 9.42×, tennis 1.58 → 1.61×.

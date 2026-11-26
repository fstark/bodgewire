# Performance

Everything known about the simulation kernel's speed: what it is today on each host, how to measure it, what was done and kept, what was tried and dropped, and the ideas left. The milestone docs (`docs/p1d-kernel-perf/`, `p5a-bus-load/`, `p5b-kernel-buses/`, `p5z-kernel-bw2001/`, `p5z2-kernel-parts/`) hold the full logs; this page is meant to outlive them. Terms: [GLOSSARY.md](GLOSSARY.md). Design principles of the kernel: [ARCHITECTURE.md](ARCHITECTURE.md#performance).

## Where we stand

The **speed ratio** is simulated time over wall time, headless (`bw run FILE --time 2s` prints `ratio`). 1.00× is real time. The ratio is capped at 1.00× in `bw_ui`.

| Machine | M5 Max (`c271cbe`, 2026-11-25) | i7-6700K Skylake (`c271cbe`, 2026-11-26) |
|---|---|---|
| BW-80 (`machines/140-bw80/bw80.bwn`) | **0.265×**: 483 instructions, 82.5 cycles per applied event, IPC 5.8 | **0.096×**: 541 instructions, 209–214 cycles, IPC 2.5–2.6 |
| BW-2001 (`machines/130-bw2001/bw2001.bwn`) | **0.39×**: 506 instructions, 90 cycles, IPC 5.6 | **0.126×**: 534 instructions, 254 cycles, IPC 2.1 |
| `bus_load` (P5a's stub Z80) | 0.41×: 428 instructions, 87 cycles | — |
| BW-1 | 2.9× | — |
| Paddle Tennis | 2.05× | 0.76× (in `bw_ui`, P8a) |
| Typewriter, ball, message | 1.31×, 2.70×, 2.78× | — |
| Test pattern, LED computer | 7.1×, 6.3× | — |
| Counter, chaser, dice, stopwatch, knob | 12–33× | — |

- **Sources:** M5 from `bench/measure.sh` (best of 3) and alternated A/B runs; Skylake from the Linux session's `perf stat` and alternated runs (means of 5).
- **In `bw_ui`:** about 5–10 % below headless (P5z2: BW-2001 0.37×, UI 2.3 ms a frame; BW-80 0.23×, UI 0.9 ms). The UI is not the limit.
- **Web build:** about 0.4× native speed (P8b, [WEB.md](p8b-web/WEB.md)), so on an x86 laptop a computer Machine runs at a few percent of real time.
- **What real time needs:** the BW-80 applies 190 M events per simulated second, the BW-2001 124 M, `bus_load` 120 M. Real time is 3.7× away on the M5 for the BW-80 and 2.6× for the BW-2001; on Skylake, about 10× and 8×.

### The target, as it moved

- **P1c (2026-10-26):** ≥ 10× real time on the test pattern, so that full Machines would fit. P1d reached about 5×, accepted for P2–P3.
- **P5a (2026-11-12):** the first BW-80-like load ran at 0.22×. P5b's aim: 2× on `bus_load`. It reached 0.40×.
- **P5 (2026-11-15):** below 1× is measured and recorded, not a stop.
- **P5z (2026-11-16):** the BW-2001 as the target, the BW-80 measured. Since then, an idea is kept when its gain is worth the complexity it adds (the user, 2026-11-16). This replaced the 5 % bar of P5b and P5z.

## How to measure

### Rules

- **Traces exact.** Every kernel change must leave the simulation identical: `bench/trace_check.sh [commit]` byte-compares the VCDs of eight runs (short runs with every Net, long runs of the BW-2001 and BW-80 on chosen Nets) and the applied events against a reference commit. It builds the reference once in a worktree under `build/trace-ref/<commit>/`. Evaluations are printed, not compared: an idea may spare evaluations that changed nothing.
- **Alternate.** Ratios move ±1–5 % run to run, and the M5's clock varies with its power state (on 2026-11-26 it ran the BW-80 at 0.18× with the same 82 cycles per event as the day before at 0.265×). Compare two builds only by alternating them in one session: A, B, A, B, five rounds of 2 s each, and compare the means. Keep `main`'s binary in a worktree (`git worktree add --detach build/trace-ref/<commit>/src <commit>`, then `cmake --preset release` inside it).
- **Count instructions and cycles per applied event**, from one 1 s run per build. Instructions are stable to 0.01 %; cycles to about 1 %.
- **Measure on both architectures.** The M5 and Skylake disagree (next section): a change can cut instructions on both and gain on neither, or gain on arm64 and lose on x86.
- **Every netlist.** `bench/measure.sh [previous.txt]` prints the best-of-3 ratio of every netlist, and against a previous run's output each one's change, so a slowdown elsewhere stands out.
- **Keep an idea when its gain is worth its complexity.** A small gain from a few plain lines is kept. A large one that needs a second kernel path or a Part API change is the user's choice. The kernel keeps one path unless the user decides otherwise.

### On macOS (the M5)

- **Counts:** `/usr/bin/time -l build/release/src/cli/bw run FILE --time 1s` gives "instructions retired" and "cycles elapsed"; divide by `applied_events` from `bw run`'s own output. No Xcode needed.
- **Where time goes:** a build with symbols, `cmake -S . -B build/profile -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O3 -g -fno-omit-frame-pointer"`, sampled with `sample <pid> 10` while `bw run` runs. `sample` gives each address a source line, so its output can be grouped by function or by line. The profile build has no LTO, so its inlining differs a little from release (0.233× against 0.255× on the BW-80): the split between lines is indicative.
- **Instruments** (line-level, call trees) needs Xcode, which the development Mac does not have.
- **Cost of one operation:** a scratch build that does it twice (with `asm volatile("" ::: "memory")` to block merging); the instruction delta divided by the operation count.
- **Call counts:** temporary global counters printed at exit, then `git checkout`.
- **Which signals cause the work:** record every Net to a VCD (`bw run FILE --time 100ms --record-all --vcd x.vcd`) and count value changes per Net.

### On Linux (Skylake)

The Linux host's report of 2026-11-25 used these; they show what `sample` cannot:

```sh
CC=clang-23 CXX=clang++-23 cmake -S . -B build/profile -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -g -fno-omit-frame-pointer" \
  -DBW_BUILD_TESTS=OFF -DBW_BUILD_UI=OFF
cmake --build build/profile --target bw
BW="build/profile/src/cli/bw run machines/140-bw80/bw80.bwn"
perf stat -e cycles,instructions,branches -- $BW --time 1s       # at most 3 events: no multiplexing
perf stat --topdown -- $BW --time 1s                              # needs perf_event_paranoid 0
perf record -g --call-graph dwarf -- $BW --time 2s; perf annotate --stdio -l 'bw::Simulator::runUntil(long)'
perf record -e br_misp_retired.all_branches:pp -c 20011 -- $BW --time 1s   # where branches mispredict
valgrind --tool=callgrind --dump-instr=yes $BW --time 0.1s        # exact instructions per line
```

- `perf annotate` and `callgrind_annotate` charge inlined code to its innermost line (`stl_vector.h`); passing addresses through `llvm-symbolizer --inlining` charges it to the caller's line.
- The NMI watchdog holds one counter: groups of more than three events multiplex and give wrong cache-miss counts.

## What the hardware says

The two hosts limit the same code differently. Measured on the BW-80 at `c271cbe`:

| | M5 Max (arm64) | i7-6700K (x86-64) |
|---|---|---|
| Instructions per applied event | 483 | 541 |
| Cycles per applied event | 82.5 | 209 |
| IPC | 5.8 | 2.59 |
| What limits it | latency: chains of dependent loads from an event to the Part it wakes | instruction count first, then branch mispredictions and the frontend |

- **Memory is not the problem on either.** The working set is under 2 MB. On Skylake: 0.69 L1D misses and 0.0009 LLC misses per event, memory-bound 2 % of slots.
- **Skylake top-down:** 63 % retiring, 11 % bad speculation (almost all branch mispredictions, 1.1 per event), 16 % frontend bound (57 % of uops come from the legacy decoders because the hot code is larger than the 1.5 K-uop cache), 10 % backend bound.
- **On the M5, fewer instructions rarely means fewer cycles.** The `kernel-run-loop` cuts removed 4–6 % of instructions and left cycles where they were. What speeds the M5 up is fewer events, fewer evaluations, or shorter chains of dependent loads (P5b's queue runs, P5z's quiet Pins).
- **On Skylake, instructions count, but only about half of a cut reaches cycles** (IPC falls slightly as cheap instructions go).
- **Register pressure differs.** arm64 has 31 general registers, x86-64 16. Holding more values in locals across a virtual call spills them on x86 (cut 2, below). Register saves on entry to a large function cost x86 more (cut 3).
- **Players are more likely on hardware like the Skylake than like the M5**, and the web build runs at about 0.4× native on top of that.

## Where the time goes

### BW-80

On the M5 (`sample`, 10 s, about 7,800 samples, 2026-11-25, before the resistor fast paths):

| Where | Share |
|---|---|
| `Simulator::runUntil`'s own code | 31 % |
| Event queue (`openRun` 11 %, `takeNext` 8 %) | 19 % |
| `resolveNet` + `Resolver::finish` | 9 % |
| `driveChanged` | 7 % |
| `ResistorNetwork` | 6 % |
| Other `EvalContext` calls | 3.5 % |
| Chip Parts, all together | 24 %, of which the Z80 **2 %** |

`runUntil`'s own 31 % by line: applying events 8 % of the run, the fan-out loop 6 %, Net bookkeeping 5 %, `updateExternal` on resistor Nets 4.5 %, calling `evaluate` 3.3 %, unattributed 4 %.

On Skylake (callgrind, 545 instructions per event at `c271cbe`):

- **Kernel:** about 360 instructions in its own functions and 75 inlined into the Parts (`drive`, `driveChanged`, `push`, the reads). The Parts' own logic is about 110.
- **Largest blocks per event:** `runUntil` 171, `resolveNet` 62 (64 per call), `openRun` 59 (71 per call), `takeNext` 44. The event queue as a whole is about 136 instructions and 20 % of cycles.
- **Calls per applied event:** 0.82 takes (99 % one run, 78 % one key), 0.83 `openRun` (two thirds of pushes open a new run: Part delays differ, so pushes rarely share a time), 0.96 `resolveNet` (51 % single-driver, 30 % the general `Resolver` loop, 19 % networked), 0.99 evaluations.
- **Mispredictions:** `openRun`'s insertion scan 12 % of all, the indirect `evaluate` call 8 %, `drive()`'s compare 7 % (inherent to the data), the fan-out loop 21 %, the take's sort 9 %, `Ls157::evaluate` 7.5 %.
- **Why the run loop's lines all look cheap:** the loops store bytes (`componentDirty_` is `uint8_t`, `LogicValue` an `enum class : uint8_t`), which may alias anything, so the compiler reloads every vector's data pointer from `this` after each store. The cost is spread over many lines: no single line stands out (P5b had concluded "no line over 2 %").

**Which signals cause the work** (100 ms of every Net, changes counted after 50 ms):

| Nets | Rate | Share of Net changes |
|---|---|---|
| `DOT`, `DOT2`, `SHIFT_CLK` (X1, its buffer, the C9 pulse) | 13 M edges/s each | 29 % |
| `CLK`, `PHI`, `PHI_N`, `LOAD_PULSE` (÷2, IC15 onto R20, C11) | 6.5 M edges/s each | 19 % |
| Buses, `MREQ`, latches and the rest | | 52 % |

The clock tree is half the BW-80's activity. Nets that touch a resistor (`CLK` through R20, the data lines split by R4–R11 into CPU and memory sides, the RC pulses) are about 40 % of Net changes.

### BW-2001

At P5z's end (`sample`, `build/profile-lto`, 2026-11-16): `runUntil` 31 % and the queue 21 %, no line above 4.6 %; the 74LS244 7.4 %; every other Part under 5 %. Before P5z the waste was fan-out: each change of `BA0`–`BA9` woke 23 chips, nearly all deselected, which quiet Pins removed.

### Earlier loads

- **Test pattern (P1d, 2026-10-27):** per simulated µs, 6 applied events, 12 evaluations, about 8 queue pops, about 5,000 instructions in all. Heap push and pop ≤ 102 instructions; full Net resolution 77, single-driver path about 10; one gate package or 74LS74 evaluation about 306. Chip evaluations over half the total.
- **`bus_load` (P5a, 2026-11-12):** 644 instructions per event: queue `pop` + `push` 37 %, `runUntil` 25 %, Parts about 30 %. Two thirds of the events are bus lines.

## What was done and kept

Chronological. The gain is on the step's benchmark at the time, M5, release.

| Step | Change | Commit | Result |
|---|---|---|---|
| P1d | Hot `EvalContext` members, `DelayFor`, `GateFunction` inline in headers; `drive` fast path for unchanged values | `543f850` | test pattern 2.48× → 4.5× |
| P1d | Rising-edge-only clock Pins (`PinSpec::risingEdgeOnly`) | `ccd9c47` | 5.0×, evaluations −33 % |
| P1d | Single-driver fast path in Net resolution | `fe5fb17` | −8.4 % instructions |
| P1d | Fan-out lists in the Circuit (kept when packed inputs were removed) | `3326650`, `bd7c516` | — |
| P5b | One 128-bit key for `(time, target, stamp)` in the heap | `147168a` | `bus_load` 0.225× → 0.244×, −10 % cycles |
| P5b | LTO in the release build | `cd6a085` | 0.256×, −5 % cycles |
| P5b | ROM with specialised evaluation (`EvaluateRom<Layout>`) | `45f0d40` | 0.279×, −6 % cycles |
| P5b | Event queue as runs of same-time events, a take is a whole timestamp | `dca233b` | 0.354×, −24 % cycles: the largest single gain |
| P5b | Sampled inputs (`PinSpec::sampled`: the `D`s of the 74LS74 and 74LS175 never wake their chip) | `f62aa8b` | 0.377×, −5 % instructions |
| P5b | Gate packages with specialised evaluation (`EvaluatePackage<units, delays>`) | `7e26235` | 0.407×, −9 % cycles; test pattern 7.27× |
| P5b | The Net of each Pin kept by the Simulator (`pinNet_`) | `a0bc4cf` | 424 instructions, −2 % cycles |
| P5z | Falling-edge-only clocks (`PinSpec::fallingEdgeOnly`: 74LS107, 74LS93, 74LS90; the 107's `J`/`K` sampled) | `5ce7af3` | BW-2001 0.210× → 0.222× |
| P5z | Quiet Pins (`EvalContext::quiet`, `lastChange`): deselected 2114s and ROMs sleep through their buses | `4916ff2` | 0.32×, −27 % cycles, evaluations per event 1.95 → 1.18 |
| P5z | Quiet Pins on the 74LS244's disabled halves | `158e181` | 0.334× |
| P5z | Plain buses resolved from a count of their driving Drivers (`driving_`: count and PinId sum) | `eba14fa` | 0.365×, −7.5 % instructions |
| P5z2 | Gate packages evaluate only the units whose inputs moved (`EvalContext::readChanged`) | `fcb70a0` | BW-2001 0.373×, BW-80 0.253× |
| — | Resistor network fast paths: only the changed groups resolved (`EvalContext::changedNetworkPins`), a two-Net one-resistor group solved directly (9 of the BW-80's 10 groups), `externalDrives` from `driving_` when 0 or 1 other drives | `c271cbe` (2026-11-25) | BW-80 0.249× → 0.265× (+6.3 %), 502 → 483 instructions, 88 → 82.5 cycles; BW-2001 +1.5 % |

The tools: `bench/trace_check.sh` and `bench/measure.sh` (`1557465`, extended to the computers in `bf382ac`; the trace check's netlist lookup fixed for `machines/*/` in `c271cbe`), `bench/bus_load.bwn` (`2b297b6`), `bench/bus_load_z80.bwn` (`ccec7a5`), `bench/bus_groups.py`.

## What was tried and dropped

| Step | Idea | Commit or branch | Result, and why dropped |
|---|---|---|---|
| P1d | Gate package dispatches on gate kind once | `738d9e1`, reverted | 5.3×, within noise; duplicated `GateFunction` |
| P1d | Packed inputs: two 64-bit words per Component, pushed on Net change | `b041658`–`615c3e2`, removed in `bd7c516` | ≈ 0; a second copy of Net values, a 64-Pin limit |
| P1d | Gate packages skip units with unchanged inputs | — | −9 % instructions, but 74LS gates only (done generally in P5z2, `fcb70a0`) |
| P1d | Timing wheel instead of the binary heap | — | +9–12 % instructions |
| P1d | `-mcpu=native` | — | no change |
| P5b | Timing wheels (1,024 slots of 1,024 ps with an overflow heap; 256 of 4,096 ps) | — | +10 % instructions, other netlists 15–40 % slower |
| P5b | 4-ary heap, hand-written | — | slower (0.202×) |
| P5b | Binary heap with a branchless child choice | — | cycles unchanged |
| P5b | Sorted vector, earliest last | — | +6 % cycles |
| P5b | `markNetDirty`, `markComponentDirty` inline in the header | — | −1 % cycles, under the bar |
| P5b | 74LS157 copying the selected input when `S` and `G_N` are known | — | within noise |
| P5b | 74LS244 reading each `OE_N` and `A` once | — | −0.4 % instructions, cycles unchanged: the cost is the `read` and `drive` calls themselves |
| P5z | 74LS244 skipping buffers whose inputs did not change | — | equal |
| P5z | Fan-out masks per Net: the run loop visits only the loud entries that wake on the change | — | −2.7 % instructions, **+5 % cycles**, −6 % ratio |
| P5z2 | 74LS93 and 74LS107 known-value paths | — | equal: LTO had already folded `GateFunction` in |
| P5z2 | Quiet `D` on the 74LS373 while `LE` is 0 | — | 1.3 % fewer BW-80 evaluations, no time saved |
| Review D19 | A delay scale per Component in `EvalContext::driveChanged` | `013a8f3` (docs) | 10–20 instructions per event in every form tried, every Machine 2–5 % slower, for a hook nothing used |
| Review D22 | Network Pins unified into one kind | — | not done: hot path, no need yet |
| — | Skylake JCC-erratum padding (`-mbranches-within-32B-boundaries`) | — | uop-cache share 43 → 49 %, cycles unchanged |
| — | Cut 2: `runUntil` keeps its vectors' data pointers and the counters in locals | `b2920ef`, reverted by `8fd0fed` (branch `kernel-run-loop`) | M5 −7 instructions per event, cycles slightly up (BW-2001 −1.3 %); Skylake **+10.6** instructions (spills across the virtual call) |
| — | Cut 1: the one-run take inline in the header, the gather and sort out of line, the runs' capacity in a member | `bd26b35` (branch `kernel-run-loop`) | M5 −20 instructions per event, Skylake −19.8; cycles barely moved |
| — | Cut 3: `resolveNet`'s single-driver and plain-bus paths inline in the run loop, the general path out of line (`noinline`) | `a6a4766` (branch `kernel-run-loop`) | M5 about 0, Skylake −12.4 instructions per event |

The `kernel-run-loop` branch, cuts 1 and 3 (head `8fd0fed`), measured against `main` (`c271cbe`), alternated, means of 5:

| | M5 | Skylake |
|---|---|---|
| BW-80 | +0.4 %, 483 → 461 instructions, cycles 82.2 → 82.3 | +0.6 %, 541 → 518 instructions, 214 → 208 cycles |
| BW-2001 | −0.6 %, 506 → 479 instructions, cycles 88.6 → 89.7 | +0.8 %, 534 → 501 instructions, 255 → 251 cycles |

Within noise on both, so not merged (2026-11-26): extra code for no measured gain. One question is open: in an earlier Skylake session, `a6a4766` (all three cuts) was +6.0 % on the BW-2001 (240 cycles per event); on the M5 in one session it was 1.3 % below `8fd0fed`. A single Skylake session with `main`, `a6a4766` and `8fd0fed` alternated would settle whether cut 2 helps x86 on the BW-2001.

## What we learned

- **Guesses are wrong; measure first.** P1d guessed wrong three times (memory chains, branch mispredictions, heap cost). P5z2's estimate of 8–15 % on the BW-80 came out at 1–2 %, because a Part's share of the profile is not what an idea can remove from it.
- **Batches are tiny and activity is low.** Nominal delays spread changes over distinct picosecond timestamps: a timestamp holds one or two changes, and a whole board changes a few Nets per µs. Event-driven is the right model; evaluating everything per step would cost thousands of times more, and there is nothing to vectorise or parallelise per timestamp.
- **The cost is per-event bookkeeping, not logic.** A NAND is one compare; each `read`, `DelayFor`, `drive` and the virtual call around them are several instructions. On the BW-80 the Parts' own logic is about 110 of 545 instructions, the Z80 2–2.6 % of the time.
- **The big wins removed work, not instructions.** Runs of same-time events, quiet Pins and edge-only clocks cut events, wakes or evaluations. Micro-optimisations of the run loop's lines have moved instructions but rarely cycles, on the M5 above all.
- **Anything added to the hot path costs 2–5 %,** even a test that is never taken (D19; Z2.0, quiet Pins with no Part using them, cost 1–3 % on every netlist until Parts used them).
- **An x86 change can be an arm64 change's opposite** (cut 2), so a kernel idea is measured on both before it is kept.
- **The BW-80 and BW-2001 differ.** The BW-2001's waste was Part-level (deselected memories on wide buses) and quiet Pins removed it. The BW-80's cost is the generic per-event path, its clock tree and its resistors.

## Ideas for the future

Ordered by expected gain for cost. Estimates are from profiles and counts, not measurements, and the lesson above applies to them. The large ones are also roadmap rows ([ARCHITECTURE.md](ARCHITECTURE.md), "Kernel: …").

### Small and exact

- **Oscillator lane** (roadmap): a fixed-period clock's self-wake kept out of the general queue. Estimated 3–6 % on the BW-2001 and BW-80. It covers only X1's self-wake, one of the seven Nets that make the BW-80's clock tree (48 % of its Net changes); a lane for the whole fixed-period tree (buffer, ÷2, the RC pulses) would save more but bends the design: each of those Nets has copper and can carry a Fault.
- **Event apply trimmed** (roadmap): `driveChanged` and the event apply's bookkeeping (generation, pending). 1–3 %; lines P5b found hard to move.
- **`openRun`'s scan:** on Skylake, 18 instructions per event and 12 % of all mispredictions. A new run usually lands 1–2 places before the tail; checking the tail first, or a structure that avoids the scan, might help. Timing wheels and heaps (P5b) were slower: only a change that keeps the runs is worth trying.
- **`resolveNet`'s general path:** 30 % of calls on the BW-80 (open-collector Nets such as `CLK`, ties, the NOP gates) go through the full `Resolver` loop at about 67 instructions. A count-based shortcut like `driving_` for the common shapes (a tie and one open-collector driver) must reproduce `Resolver::finish` exactly. A per-Net kind byte in `NetState`'s two spare bytes would replace four classification arrays. Skylake estimate 10–15 instructions per event; cut 3 suggests the M5 gains little.
- **The recorder check:** `isRecording` on every Net change costs about 1 % headless; skipping it when nothing records helps only headless (instruments record in `bw_ui`).
- **`Ls157::evaluate`:** 3.5 % of Skylake's cycles for 1.9 % of instructions and 7.5 % of mispredictions; a branch-free form might help x86.

### Multi-Pin `EvalContext` (roadmap)

Idea 1's real lever: specialised evaluation (done for ROMs and gate packages in P5b) left per-Pin bookkeeping as the Parts' cost. An `EvalContext` that reads and drives several Pins at once is a Part API change; Parts are converted one by one, traces exact. Estimated 10–20 %.

### Bus as one entity (roadmap)

The user's idea (2026-11-13), assessed, not built. A Bus is kept as one word (value, a driven mask, an unknown mask) in place of one Net per line; Parts read and drive it as a word; a Bus touched by a Fault goes back to one Net per line.

- **Counted** on `bus_load` at `a0bc4cf` with `bench/bus_groups.py` (60 ms, every Net recorded): per simulated µs, 95 Net changes, of which Buses 45, clocks 26, single lines 24. Taking a Bus's bits changing at one timestamp as one event: 45 → 12 (all changes 95 → 62, 1.54× fewer); grouping only lines that join the same chips: 45 → 26 (1.26×). Rise and fall delays differ, so one Bus transition is not always one timestamp.
- **Estimated:** 1.3–1.5× on `bus_load` with lines grouped by their driver; 1.1–1.3× on the BW-2001 since quiet Pins removed most bus fan-out. Weaker on the BW-80, whose data lines are each two Nets joined by a resistor, with the NOP forcing through that split.
- **Cost:** a second kernel path that must match the first per Pin (inertial delay, resolution, the random bit an X input draws, in Pin order); every Part with Bus Pins rewritten; a Puzzle with a Bus Fault runs that Bus per line, so that path must be fast anyway.
- **Easy:** no switch while running (each Bus chosen at build); the trace check with Buses forced per line tests divergence.
- **Recommendation (2026-11-13):** shelved. Look again only if what is left to real time is within its gain. More than half the Net changes are clocks and single lines, which no Bus scheme touches.

### Two cores (roadmap)

CPU side and video side on two threads, with conservative synchronisation in the kernel.

- **BW-2001:** 1.3–1.7× estimated, up to 2× in theory. The PET's video chain runs on its own and meets the CPU only at the video RAM arbitration, but that couples them at every access.
- **BW-80:** at most 1.1–1.15×. On the ZX80 design the video is the CPU: the Z80 fetches the display file as instructions, glyph addresses go out in its refresh cycle, and HSYNC and VSYNC come from `M1`, `IN` and `OUT`. Only the tail (the shift clock, IC9's shifting, the inverse XOR, about 10–15 % of events) is separable.
- **Lookahead:** one gate delay (about 10 ns), so the halves synchronise every few simulated nanoseconds. Threads must spin, not wait: a wake costs microseconds.
- **On the web:** threads are already Web Workers with shared memory and real atomics, so it works, but less steadily: a paused worker stalls the other, background tabs and low-power iPads throttle workers, and the thread pool (`-sPTHREAD_POOL_SIZE=12`, `src/ui/CMakeLists.txt`) needs counting.

### Generated code (roadmap)

Per Circuit, code that for "U4's output changed" runs exactly the resolution, fan-out and evaluations needed, removing all generic dispatch. Regenerated when wiring changes (Faults, Repairs: rare). Estimated 2–3×, the most uncertain and the largest change.

### Fewer events

- **Behavioural CPU, RAM and ROM:** done; the Z80 costs 2 % of the BW-80.
- **The clock tree** is half of the BW-80's Net changes. Anything that cuts it bends the design (see the oscillator lane).

### Rejected

- **Collapsing gate cones** (a chain of gates as one block, internal Nets not simulated): loses the glitches and timing on internal Nets, which are what Faults show, and any Net can be probed at any time. Last resort.
- **Coarser time** (delays quantised so changes share timestamps): does not reduce events, and parallel evaluation would need thousands of items per synchronisation against a BW-80's 1–20 per step. Loses timing detail.
- **GPU:** launch latency is microseconds and batches are one or two events; only an evaluate-everything model fits, thousands of times more work.
- **Fine-grained multi-core partitioning** (beyond the two halves above): the one-gate-delay lookahead makes threads synchronise every few nanoseconds.

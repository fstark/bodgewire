# Optimisation Ideas — Kernel Performance

Notes from P1d ([KERNEL_PERF.md](KERNEL_PERF.md), [KERNEL_PERF_PLAN.md](KERNEL_PERF_PLAN.md)) to come back to once a real Machine runs. Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Target

A Machine must run in real time to be playable: a BW-80 that boots 10× slower than the real one is not a game.

Rough load estimate for a ZX80-class board (BW-80), to be measured:

- 6.5 MHz pixel clock, 3.25 MHz CPU clock: ~20 clock edges per µs.
- Pin-level CPU bus: 16 address + 8 data + ~6 control lines toggling every machine cycle, plus decoding, video shift register and counters.
- Total ≈ 100–200 events per µs → **100–200 M events/s for real time**.

Current kernel: ≈ 37 M events/s on the test pattern. Gap: **≈ 3–6×**. Not hopeless, not there.

Decision: keep building towards a real Machine; measure its real load before more kernel work (see Next steps).

Measured (P5a, 2026-11-12, [BUS_LOAD.md](../p5a-bus-load/BUS_LOAD.md)): `netlists/bus_load.bwn`, a stub Z80 with ZX80 bus timing, decoding and video, needs **120 M applied events/s** for real time and runs at **0.22×** (≈ 26 M/s, 644 instructions an event). Gap to the 2× target set then: ≈ 10×. Profile on that load: event queue `pop` + `push` 37 %, `runUntil` 25 %, Parts ≈ 30 %. The simulation runs on its own thread, the UI on another: the single-threaded kernel needs a whole core.

After P5b (2026-11-13, [KERNEL_BUSES.md](../p5b-kernel-buses/KERNEL_BUSES.md), Results): **0.40×** on `bus_load`, 424 instructions and 90 cycles an applied event, traces unchanged. Gap to 2×: ≈ 5×; to real time: 2.5×.

After P5z (2026-11-16, [KERNEL_BW2001.md](../p5z-kernel-bw2001/KERNEL_BW2001.md), Results): the BW-2001 **0.21× → 0.36×**, 856 → 509 instructions and 156 → 95 cycles an applied event, half the evaluations, traces unchanged. The BW-80 0.25× (unchanged), `bus_load` 0.41×. Gap to real time: BW-2001 2.8×, BW-80 4×.

## How to measure

- Machine: Apple M5 Max. The kernel runs at **~6.4 instructions per cycle**: cache misses and branch mispredictions are not the bottleneck. Cost = instruction count.
- Stable metric: `/usr/bin/time -l ./build/release/src/cli/bw run netlists/test_pattern.bwn --time 5s` → "instructions retired". The ratio moves ±5% run to run; instructions are stable to 0.01%.
- Cost of one operation: a scratch build that does it twice (with `asm volatile("" ::: "memory")` to block merging); the instruction delta divided by the operation count.
- Where time goes: optimised build with symbols in `build/profile` (`-O3 -g -fno-omit-frame-pointer`), sampled with macOS `sample`; aggregate by function or source line.
- Call counts: temporary global counters printed at exit, then `git checkout`.
- Behaviour check after each change: same `applied_events` and `evaluations`; VCD of `--time 40ms --seed 1 --record SYNC_N,VIDEO,HSYNC_N,VSYNC_N` byte-identical.
- Compiler flags: `-O3` is the Release default; `-mcpu=native` changed nothing. LTO gave 2.5× → 4.0× but was rejected (build complexity); explicit inlining in headers gave more.

## Keeping an idea

An idea is kept when its gain is worth the complexity it adds (the user, 2026-11-16). This replaces the 5 % bar of P5b and P5z, which was set when the code was simpler, against complexity for gains that might not matter. A small gain from a few plain lines is kept; a large one that needs a second path or a Part API change is the user's choice. Traces stay exact and the kernel keeps one path unless the user decides otherwise.

## History (test pattern, 5 s simulated, release)

| Change | Result | Kept |
|---|---|---|
| P1c baseline | 2.48× | — |
| Hot `EvalContext` members, `DelayFor`, `GateFunction` inline in headers; `drive` fast path | 4.5× | yes |
| Rising-edge-only clock Pins (falling clock edges don't wake Components) | 5.0×, evaluations −33% | yes |
| Gate package dispatches on gate kind once | 5.3×, within noise | no |
| Packed inputs (two 64-bit words per Component, pushed on Net change) | ≈ 0 | no: second copy of Net values, 64-pin limit |
| Gate packages skip units with unchanged inputs | −9% instructions | no: 74LS gates only |
| Single-driver fast path in Net resolution | −8.4% instructions | yes |
| Timing wheel instead of the binary heap | +9–12% instructions | no |
| Now | 22.91 G instructions, ~5–5.7× | — |

P5b, on `netlists/bus_load.bwn` (ratio best of 3 over 2 s, instructions and cycles an applied event; the full log in [KERNEL_BUSES_PLAN.md](../p5b-kernel-buses/KERNEL_BUSES_PLAN.md)):

| Change | `bus_load` | Instr. | Cycles | Test pattern | Kept |
|---|---|---|---|---|---|
| P5a kernel | 0.225× | 643 | 165 | 5.46× | — |
| One 128-bit key for `(time, target, stamp)` in the heap | 0.244× | 640 | 149 | 5.50× | yes |
| Timing wheel, 4-ary heap, branchless heap, sorted vector | 0.20–0.24× | 626–710 | ≥ 149 | 4.3–5.6× | no |
| LTO in the release build | 0.256× | 584 | 141 | 5.73× | yes |
| ROM with specialised evaluation | 0.279× | 577 | 133 | 5.99× | yes |
| Event queue as runs of same-time events | 0.354× | 503 | 103 | 5.76× | yes |
| Sampled inputs (`D` of the 74LS74 and 74LS175) | 0.377× | 479 | 100 | 5.89× | yes |
| Gate packages with specialised evaluation | 0.407× | 434 | 91 | 7.27× | yes |
| The Net of each Pin kept by the Simulator | 0.407× | 424 | 89 | 7.41× | yes |
| 74LS157, 74LS244 logic rewritten | 0.40–0.41× | 422–424 | 88–90 | — | no |
| Now (2026-11-13) | 0.40× | 424 | 90 | 7.35× | — |

P5z, on `netlists/bw2001.bwn` (ratio best of 3 over 2 s; the full log in [KERNEL_BW2001_PLAN.md](../p5z-kernel-bw2001/KERNEL_BW2001_PLAN.md)):

| Change | `bw2001` | Instr. | Cycles | Eval./event | Kept |
|---|---|---|---|---|---|
| Before P5z (`f849080`) | 0.210× | 856 | 156 | 2.14 | — |
| Falling-edge-only clocks (74LS107, 74LS93, 74LS90), the 107's `J`/`K` sampled | 0.222× | 830 | 147 | 1.95 | yes |
| Quiet Pins: deselected 2114s and ROMs not woken by their address and data | 0.32× | 580 | 107 | 1.18 | yes |
| Quiet Pins on the 74LS244's disabled halves | 0.334× | 550 | 102 | 1.07 | yes |
| Plain buses resolved from a count of their driving Drivers | 0.365× | 509 | 95 | 1.07 | yes |
| 74LS244 skipping unchanged buffers | 0.362× | 511 | 97 | 1.07 | no |
| Fan-out masks: only loud entries visited | 0.344× | 495 | 100 | 1.07 | no |
| Now (2026-11-16) | 0.36× | 509 | 95 | 1.07 | — |

P5z2 ([KERNEL_PARTS.md](../p5z2-kernel-parts/KERNEL_PARTS.md)), kept when worth its complexity:

| Change | `bw2001` | `bw80` | Kept |
|---|---|---|---|
| Before (P5z's end, measured again) | 0.368×, 509 instr. | 0.248×, 499 instr. | — |
| Gate packages evaluate only the units whose inputs moved | 0.373×, 503 | 0.253×, 496 | yes |
| 74LS93 and 74LS107 known-value paths | equal | equal | no |
| Quiet D on the 74LS373 | — | equal (1.3 % fewer evaluations) | no |

## What we learned

Per simulated µs on the test pattern: 6 applied events, 12 evaluations, ~8 queue pops; ~5,000 instructions in total, ~760 per applied event.

| Item | Instructions |
|---|---|
| Heap push + pop | ≤ 102 (the heap holds only ~8–15 events) |
| Net resolution, full | 77; single-driver fast path ~10 |
| Evaluation of a gate package or 74LS74 | ~306 |
| Chip evaluations, all | > 50% of the total |

- **Guesses were wrong three times** (memory chains, branch mispredictions, heap cost). Measure instructions before changing anything.
- **Batches are tiny.** Nominal delays spread changes over distinct picosecond timestamps: a timestamp typically holds 1–2 changes. Nothing to vectorise or parallelise per timestamp.
- **Activity is low.** ~6 Net changes per µs over ~60 Nets. Event-driven is the right model; evaluating everything every step would cost thousands of times more.
- **Evaluation cost is per-Pin bookkeeping, not logic.** A NAND is one compare; each `read`, `DelayFor`, `drive` and the virtual call around it are several instructions, times every Pin of the chip, every time one input changes.

## Ideas, ranked

### 1. Specialised evaluation (most promising)

Each Part type gets evaluation code specialised at compile time: its Pins' Net indices are precomputed when the Circuit is built; no generic `read`/`drive` per Pin, no `GateFunction` switch per gate. Idea 2 done ahead of time, in plain C++, debuggable.

- Expected: 2–4× on evaluations (> 50% of instructions) → ~1.5–2.5× overall.
- First experiment: the 74LS161 alone, measured in instructions.
- C++26 reflection could generate the per-Part tables and unrolled code from a declarative pin description. Not needed: `constexpr` pin tables and templates in C++23 do the same with more boilerplate; reflection would only remove boilerplate, at the cost of an experimental toolchain.

### 2. Generated propagation code (JIT)

Per Circuit, generate code that, for "U4 output changed", runs exactly the resolution, fan-out and evaluations needed. Removes all generic dispatch.

- Must regenerate when wiring changes (Faults, Repairs): rare, so acceptable.
- Heavy to build and debug. Only if idea 1 is not enough.

### 3. Collapsing gate cones

Treat a chain of gates as one block with summed delays; internal Nets are not simulated.

- Fewer events.
- Loses glitches and timing on internal Nets, which are what Faults show. Any Net can be probed at any time, so cones must expand on demand. Last resort.

### 4. Coarser time

Quantise delays (e.g. to 1 ns or 10 ns) so more changes share a timestamp.

- Does not reduce the number of events; each still costs the same.
- Parallel evaluation needs thousands of items per synchronisation to beat thread overhead (~µs); even with coarse steps a BW-80 has ~1–20 events per step. SIMD within one thread would need dozens of uniform items per step.
- Loses timing detail. Low value.

### 5. GPU

Launch latency is microseconds; batches are 1–2 events. Only an evaluate-everything model fits, which is thousands of times more work. Rejected.

### 6. Multi-core partitioning

Split the board (e.g. CPU side, video side) over threads with conservative synchronisation. The lookahead between partitions is one gate delay (~10 ns), so threads would synchronise every few ns. Not viable at this granularity.

### 7. Fewer events

- Behavioural CPU, RAM, ROM (already planned): one evaluation per bus cycle instead of internal events.
- The bus remains: 24+ real Nets per bus cycle, unavoidable if the player can probe them. Most of a BW-80's load will be there; any per-event saving counts.

### 8. A Bus as one entity

The user's idea (2026-11-13), assessed, not built. A Bus is kept as one word (value, with a driven mask and an unknown mask) in place of one Net per line; Parts read and drive it as a word; a Bus touched by a Fault goes back to one Net per line.

Measured on `netlists/bus_load.bwn` with the kernel of `a0bc4cf` (0.41×) by `bench/bus_groups.py netlists/bus_load.bwn`: it runs 60 ms with every Net recorded (seed 1) and counts the Bus bits changing at one timestamp. A count of events, not a speed. To be run again when the Z80 Part replaces the stub (P5j).

- Per simulated µs: 119 applied events, 113 evaluations, 95 Net changes. Of the Net changes: Buses 45 (`A` 10.4, `D` 10.4, `MD` 13.4, `RA` 9.5, `CH` 1.6, `LC` under 0.1), clocks 26, single control and glue lines 24. 47 of the 86 Nets are Bus lines.
- The bits of a Bus changing at one timestamp taken as one event, timing unchanged: 45 → 12 (3.8 bits a group, a chip schedules only the bits that differ), all Net changes 95 → 62, 1.54× fewer.
- Only lines joining the same chips grouped (a chip's Bus pins fully connected to another's): 45 → 26, all Net changes 95 → 76, 1.26× fewer. Only `D0`–`D7` is whole. The address Bus is 8 pieces (lines go off to decoders and to 4-bit chips), `MD` 3, `RA` 3.
- One Bus transition is not always one timestamp: TTL drivers have different rise and fall delays (74LS157 select 15/18 ns, 74LS175 20/21 ns, 74LS244 enable 15/20 ns), so the rising bits and the falling bits land apart. With groups within 10 ns merged: 11.9 → 9.7 a µs.

Estimated from these counts and the P5b profile (run loop a third, queue a fifth, Parts the rest), not measured:

- 1.3–1.5× with lines grouped by the chip that drives them (0.41× → 0.55–0.6×); 1.1–1.2× under the fully-connected rule. Real time needs 2.5×.
- Evaluations do not drop: chips wake at the same timestamps, and Parts are almost half the time.

What it costs:

- A second kernel path, out by P5b's guard 2 and by the P5 decision of 2026-11-12 (Buses as individual Nets). It must match the first per Pin: inertial delay (each Pin has its own pending change), resolution per Net (strength, contention, floating), the random bit an X input draws, in Pin order.
- Every Part with Bus pins rewritten (CPU, ROM, RAM, 74LS244, 74LS157, 74LS175, 74LS165). One path in each if a Part reads and drives a port (a word and its masks) and the kernel chooses per port.
- A Puzzle with a Bus Fault (A4 stuck, D3 cracked) runs that Bus one Net per line: that path must reach real time anyway, or the faulty Machine runs slower than the healthy one.

What is easy:

- No switch while running. Repairs rebuild the Circuit at the next power-on, so each Bus is chosen at build; Instruments read one bit of the word.
- Divergence can be tested: the trace check, with Buses forced to one Net per line, against the same run with them whole.

Recommendation (2026-11-13): shelve. Look again only if what is left to real time is within its gain (about 1.4×). More than half the Net changes are clocks and single lines, which no Bus scheme touches.

## Next steps

P5z (2026-11-16): the BW-2001 at 0.36× with traces unchanged and the Part API grown by `fallingEdgeOnly`, `quiet` and `lastChange`. Nothing left reached P5z's 5 % bar alone: `runUntil` 31 % and the queue 21 % spread over many lines, no Part over 7.4 %. The options below stay open (on the roadmap as potential rows since 2026-11-16, with P5z2 for the cheap exact ones: gate packages per unit, 74LS93 and 74LS107 fast paths, quiet D on the 74LS373); for real time on the BW-2001 (2.8× away) or the BW-80 (4×) the remaining levers bend the design: 2 (10–20 %), 3, 4, 5.

Done: 1 (P5a), P5b (2026-11-13): the event queue, LTO, specialised evaluation of the spec-table Parts (ROM, gate packages), sampled inputs; 0.40× on `bus_load`. Within P5b's guards (Part API, traces and simulated Nets unchanged, no second kernel path) nothing left reaches 5 % alone. The options (spec, Results); **the user chose 1 (2026-11-13)**, the others stay open for when the measurements ask:

1. Go on to the 6502 with this kernel and measure the BW-2001 load (P5i), then the real Z80 in place of the stub (P5j), before more kernel work: `bus_load` is the heaviest computer.
2. Idea 1's real lever: an `EvalContext` that reads and drives several Pins at once (a Part API change). The cost left in Parts is per-Pin bookkeeping; 10–20 % estimated.
3. Idea 8, a Bus as one entity: 1.3–1.5× estimated, a second kernel path; `bench/bus_groups.py` counted again with the real Z80.
4. Idea 6, two cores (CPU side, video side): up to ≈ 2×.
5. Idea 2, generated code.

The P5b plan, kept for history: the event queue measured again on this load; the run loop's own code; LTO; idea 1 on the hottest Parts, shown to the user first. Ideas 2–4 were out of P5b: they bend the design (generated code, lost Nets, coarser timing).

The P1d plan, kept for history:


1. Measure a BW-80-like load: a crude netlist with a stub CPU driving the bus at real timing, plus the video chain. Gives the real gap.
2. If short: idea 1 on the 74LS161, measured in instructions; generalise if it delivers.
3. Ideas 2 and 3 only if 1 and 2 show we are still short.

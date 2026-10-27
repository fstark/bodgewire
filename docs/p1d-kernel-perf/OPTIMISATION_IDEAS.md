# Optimisation Ideas — Kernel Performance

Notes from P1d ([KERNEL_PERF.md](KERNEL_PERF.md), [KERNEL_PERF_PLAN.md](KERNEL_PERF_PLAN.md)) to come back to once a real Machine runs. Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Target

A Machine must run in real time to be playable: a BW-80 that boots 10× slower than the real one is not a game.

Rough load estimate for a ZX80-class board (BW-80), to be measured:

- 6.5 MHz pixel clock, 3.25 MHz CPU clock: ~20 clock edges per µs.
- Pin-level CPU bus: 16 address + 8 data + ~6 control lines toggling every machine cycle, plus decoding, video shift register and counters.
- Total ≈ 100–200 events per µs → **100–200 M events/s for real time**.

Current kernel: ≈ 37 M events/s on the test pattern. Gap: **≈ 3–6×**. Not hopeless, not there.

Decision: keep building towards a real Machine; measure its real load before more kernel work (see Next steps). The simulation runs on its own thread, the UI on another: the single-threaded kernel needs a whole core.

## How to measure

- Machine: Apple M5 Max. The kernel runs at **~6.4 instructions per cycle**: cache misses and branch mispredictions are not the bottleneck. Cost = instruction count.
- Stable metric: `/usr/bin/time -l ./build/release/src/cli/bw run netlists/test_pattern.bwn --time 5s` → "instructions retired". The ratio moves ±5% run to run; instructions are stable to 0.01%.
- Cost of one operation: a scratch build that does it twice (with `asm volatile("" ::: "memory")` to block merging); the instruction delta divided by the operation count.
- Where time goes: optimised build with symbols in `build/profile` (`-O3 -g -fno-omit-frame-pointer`), sampled with macOS `sample`; aggregate by function or source line.
- Call counts: temporary global counters printed at exit, then `git checkout`.
- Behaviour check after each change: same `applied_events` and `evaluations`; VCD of `--time 40ms --seed 1 --record SYNC_N,VIDEO,HSYNC_N,VSYNC_N` byte-identical.
- Compiler flags: `-O3` is the Release default; `-mcpu=native` changed nothing. LTO gave 2.5× → 4.0× but was rejected (build complexity); explicit inlining in headers gave more.

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

## Next steps

1. Measure a BW-80-like load: a crude netlist with a stub CPU driving the bus at real timing, plus the video chain. Gives the real gap.
2. If short: idea 1 on the 74LS161, measured in instructions; generalise if it delivers.
3. Ideas 2 and 3 only if 1 and 2 show we are still short.

# Kernel Performance — Implementation Plan (P1d)

Design: [KERNEL_PERF_DESIGN.md](KERNEL_PERF_DESIGN.md)
Spec: [KERNEL_PERF.md](KERNEL_PERF.md)

Phases 1–6 (packed inputs) completed 2026-10-27, commits 0cf99fd..b70a360. P1d stays open: S6 not met (see Results).

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug --output-on-failure`

Benchmark (phases 1, 3, 4, 5, 6): `cmake --build --preset release`, then 3 × `./build/release/src/cli/bw run netlists/test_pattern.bwn --time 5s`; record best `ratio`, plus `applied_events` and `evaluations`.

Behaviour check (phases 1, 3, 4, 5): `applied_events` and `evaluations` equal the pre-revert baseline (1.1); and

```
./build/release/src/cli/bw run netlists/test_pattern.bwn --time 40ms --seed 1 \
    --record SYNC_N,VIDEO,HSYNC_N,VSYNC_N --vcd /tmp/p1d.vcd && cmp /tmp/p1d.vcd /tmp/p1d_baseline.vcd
```

All code follows [STYLE.md](STYLE.md) and [NAMING.md](NAMING.md); run `clang-format -i` on changed C++ files.

## Results (2026-10-27)

Benchmark, best of 3, `bw run netlists/test_pattern.bwn --time 5s`, release. `applied_events` 30117545 and `evaluations` 60225260 at every step; VCD byte-identical to the baseline at every step.

| Step | Commit | ratio |
|---|---|---|
| 1.1 baseline (with 738d9e1) | cc00da1 | 5.41 |
| 1.2 revert 738d9e1 | 0cf99fd | 5.13 |
| 3 fan-out lists | 3326650 | 5.03 |
| 4 packed inputs, `read` | 92ff20f | 4.77 |
| 5 gate packages packed | 615c3e2 | 5.40 |

S6 **not met**: 5.40× vs 10× (shortfall 1.85×).

Profile (`build/profile`, `sample` 8 s, 6284 samples, top of stack):

| Function | Share |
|---|---|
| `Simulator::runUntil` | 29.8% |
| `Ls161::evaluate` | 16.9% |
| `PackedGateFunction` | 14.8% |
| `EventQueue::push` | 10.8% |
| `GatePackage::evaluate` | 7.0% |
| `EventQueue::pop` | 6.3% |
| `Ls74::evaluate` | 4.2% |
| `Simulator::markNetDirty` | 3.3% |
| `EvalContext::driveChanged` | 2.9% |
| `Resolver::finish` | 1.5% |
| other | 2.4% |

Event queue (push + pop) is 17.1%: removing it entirely would give at most ~6.5×, so a bucket queue alone cannot reach 10×. Next: design addendum in [KERNEL_PERF_DESIGN.md](KERNEL_PERF_DESIGN.md) covering the queue plus the run loop (29.8%) and `Ls161::evaluate` (16.9%), then append its phases here.

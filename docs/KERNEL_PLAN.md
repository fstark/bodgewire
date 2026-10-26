# Kernel — Implementation Plan (P1a)

Design: [KERNEL_DESIGN.md](KERNEL_DESIGN.md)
Spec: [KERNEL.md](KERNEL.md)

Completed 2026-10-26: all 10 phases, commits `ef03291..3ae8a01` (one per phase).

- Debug (sanitizers) and release: 65 tests pass.
- Benchmark (release, Apple silicon): 25.8 M applied events/s, 103 M evaluations/s, ratio 0.15 × real time — passes the ≥ 20 M target.

Remaining manual check (spec "Done when"): open the VCD files in GTKWave or Surfer:

```
./build/debug/tests/bw_tests "[.manual]"   # writes ring_oscillator.vcd, ripple_counter.vcd
```

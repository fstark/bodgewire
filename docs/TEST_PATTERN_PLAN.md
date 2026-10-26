# Test-pattern Generator — Implementation Plan (P1c)

Design: [TEST_PATTERN_DESIGN.md](TEST_PATTERN_DESIGN.md)
Spec: [TEST_PATTERN.md](TEST_PATTERN.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Netlist and golden check | Done (657e97d) |
| 2 | Benchmark and VCD | **Skipped: below target (ratio 2.48)** |

---

## Phase 2 — Benchmark and VCD

**Skipped: below target (ratio 2.48).** Opens P1d (kernel performance, profile first).

### 2.1 — Benchmark (2026-10-26, release, Apple clang 21)

```
$ ./build/release/src/cli/bw run netlists/test_pattern.bwn --time 1s
simulated_s 1
wall_s 0.403007
ratio 2.481346477852742
applied_events 6023512
evaluations 18045071
```

≈ 15 M applied events/s, 3 evaluations per applied event.

### 2.2 — VCD

Generated (gitignored):

```
./build/release/src/cli/bw run netlists/test_pattern.bwn --time 40ms --record SYNC_N,VIDEO,HSYNC_N,VSYNC_N --vcd pattern.vcd
```

### Fence

- [x] Benchmark output recorded here
- [x] Manual: `pattern.vcd` looks right in a viewer (spec "Done when"): 64 µs lines, grid every 8 px / 16 lines, 24 blank lines, 4-line VSYNC with inverted composite sync

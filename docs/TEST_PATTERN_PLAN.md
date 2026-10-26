# Test-pattern Generator — Implementation Plan (P1c)

Design: [TEST_PATTERN_DESIGN.md](TEST_PATTERN_DESIGN.md)
Spec: [TEST_PATTERN.md](TEST_PATTERN.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Netlist and golden check | |
| 2 | Benchmark and VCD | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug --output-on-failure`

All code follows [STYLE.md](STYLE.md) and [NAMING.md](NAMING.md).

---

## Phase 1 — Netlist and golden check

### 1.1 — `netlists/test_pattern.bwn`

Exactly the listing of design §4.

### 1.2 — `tests/machines/test_pattern_test.cpp`

Add to `tests/CMakeLists.txt`. Load with `LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn")`. File-local `Decode`, `Expected`, `ValueAt`, `Transitions` and the three tests of design §5.

`Expected(n)`: `m = (n - 1) mod kPixelsPerFrame` (non-negative), `Decode(m % 64, m / 64)`.

`Transitions(predicate, n0, count)`: number of `k` in `1 .. count - 1` where the value at `n0 + k` differs from `n0 + k - 1`; compared with the recorded changes strictly inside `(T, T + count × kPixel)`.

### 1.3 — If the golden check fails

The golden function is the spec; the netlist is what may be wrong. Diagnose with a VCD of the counters (`bw run … --record H0,H1,…,V8,LINE_END,FRAME_END_N --vcd debug.vcd`), fix the netlist, and update design §4 to match. Do not change `Decode` unless it contradicts spec S1/S2.

### Fence

- [ ] Build gate and test gate pass (debug, sanitizers)
- [ ] `ctest --preset release` passes too (configure and build `release` first)
- [ ] Commit: `"pattern: phase 1 — test-pattern netlist and golden check"`

---

## Phase 2 — Benchmark and VCD

No code.

### 2.1 — Benchmark

```
cmake --preset release && cmake --build --preset release
./build/release/src/cli/bw run netlists/test_pattern.bwn --time 1s
```

Record the full output in this plan under the phase. Pass: `ratio ≥ 10`. Below: mark **Skipped: below target (ratio N)** — not a failure of the plan; it opens P1d (kernel performance).

### 2.2 — VCD

```
./build/release/src/cli/bw run netlists/test_pattern.bwn --time 40ms --record SYNC_N,VIDEO,HSYNC_N,VSYNC_N --vcd pattern.vcd
```

Manual: user opens `pattern.vcd` in a viewer (spec "Done when").

### Fence

- [ ] Benchmark output recorded here
- [ ] Manual: `pattern.vcd` looks right in a viewer (spec "Done when")
- [ ] Commit: `"pattern: phase 2 — benchmark results"`

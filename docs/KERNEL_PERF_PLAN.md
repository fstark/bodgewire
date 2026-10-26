# Kernel Performance — Implementation Plan (P1d)

Design: [KERNEL_PERF_DESIGN.md](KERNEL_PERF_DESIGN.md)
Spec: [KERNEL_PERF.md](KERNEL_PERF.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Revert gate-kind dispatch, record baseline | |
| 2 | `PackedInputs`, pin limit, `PackedGateFunction` | |
| 3 | Circuit fan-out lists | |
| 4 | Simulator packed inputs, `read` on them | |
| 5 | Gate packages on packed inputs | |
| 6 | Results and KERNEL_DESIGN.md | |

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

---

## Phase 1 — Revert gate-kind dispatch, record baseline

Design §6. Gives the reference numbers and VCD every later phase is compared with. The baseline is taken before the revert, so the revert itself is checked too.

### 1.1 — Baseline (current `main`, before the revert)

Release build; run the benchmark; write the numbers under this phase. Produce the baseline VCD:

```
./build/release/src/cli/bw run netlists/test_pattern.bwn --time 40ms --seed 1 \
    --record SYNC_N,VIDEO,HSYNC_N,VSYNC_N --vcd /tmp/p1d_baseline.vcd
```

### 1.2 — Revert

`git revert --no-edit 738d9e1`. Build and test gates pass. Release build; behaviour check; benchmark recorded under this phase.

### Fence

- [ ] Build and test gates pass; `ctest --preset release` passes
- [ ] Pre-revert `ratio`, `applied_events`, `evaluations` recorded here; `/tmp/p1d_baseline.vcd` exists
- [ ] Behaviour check passes after the revert; post-revert benchmark recorded here
- [ ] The revert commit is the phase commit (no extra commit)

---

## Phase 2 — `PackedInputs`, pin limit, `PackedGateFunction`

Design §2, §5.4 (function only). Pure types and a pure function; nothing uses them yet.

### 2.1 — `src/sim/component.hpp`

Add before `class EvalContext`:

```cpp
inline constexpr std::size_t kMaxPins = 64;  // one bit per Pin in PackedInputs

// Input Pin values of one Component, bit = PinIndex. X: known = 0, value = 0.
struct PackedInputs {
    std::uint64_t value;  // 1 where the Net is 1
    std::uint64_t known;  // 1 where the Net is 0 or 1
};
```

Add `#include <cstddef>` and `#include <cstdint>`.

### 2.2 — `src/sim/component.cpp`

In `Component::Component`, body: `Check(pins_.size() <= kMaxPins, "component: more than 64 pins");` (include `sim/check.hpp` if missing).

### 2.3 — `src/sim/primitives.hpp`

After `GateFunction`, add `constexpr LogicValue PackedGateFunction(GateKind kind, PackedInputs inputs, std::uint64_t mask)` as design §5.4 pseudocode: `ones = inputs.value & mask`, `zeros = inputs.known & ~inputs.value & mask`; `std::popcount` for Xor (`#include <bit>`). Nand/Nor/Xnor = `Not(...)` of And/Or/Xor; Not = `Not(Buffer)`.

### 2.4 — Tests

- `tests/sim/primitives_test.cpp`: `"gate function: packed matches GateFunction"`. For each `GateKind`, input counts 1 (Not, Buffer only) or 2–4 (others), every combination of {0, 1, X}^n: build the `LogicValue` span and the `PackedInputs` with Pin i at bit `i + 3` (offset proves the mask is honoured; set an unrelated bit 0 to 1 in both planes), mask = bits 3..3+n-1. `CHECK(PackedGateFunction(...) == GateFunction(...))`.
- `tests/sim/circuit_test.cpp`: `"circuit: more than 64 pins rejected"`: a test Component with 65 Input Pins; `CHECK_THROWS_AS(circuit.add<...>(...), std::logic_error)`.

### Fence

- [ ] Build and test gates pass
- [ ] Commit: `"perf: phase 2 — PackedInputs, 64-pin limit, PackedGateFunction"`

---

## Phase 3 — Circuit fan-out lists

Design §4.1, §5.5. Replaces readers with fan-out entries; the run loop only changes how it iterates. No behaviour change.

### 3.1 — `src/sim/circuit.hpp`

- Add `struct FanOut { ComponentId component; bool risingEdgeOnly; std::uint64_t mask; };` with `static_assert(sizeof(FanOut) == 16);`, before `class Circuit`.
- Replace `readers()` and `readerRisingEdgeOnly()` with `std::span<const FanOut> fanOut(NetId net) const` (CSR on `fanOutStart_`, like `drivers()`).
- Members: replace `readerStart_`, `readers_`, `readerRisingEdgeOnly_` with `std::vector<std::uint32_t> fanOutStart_;` and `std::vector<FanOut> fanOut_;`. Update the CSR comment.
- Declare `void buildTables();` private.

### 3.2 — `src/sim/circuit.cpp`

Split `finalize()`: it keeps the `Check`, private-Net assignment and `finalized_ = true`; the rest moves to `buildTables()`, which clears and refills `drivers_`, `driverStart_`, `fanOut_`, `fanOutStart_`, `netHasTtlInput_`. Per Input Pin `p` on Net `n`, local index `i = p - firstPin(owner)`: if `fanOut[n]` is empty or its last entry's `component != owner`, push `{owner, info.risingEdgeOnly, 1ull << i}`; else `back().mask |= 1ull << i; back().risingEdgeOnly &= info.risingEdgeOnly;`.

### 3.3 — `src/sim/simulator.cpp`, `runUntil`

Replace the readers loop:

```cpp
for (const FanOut& f : circuit_.fanOut(net)) {
    if (rose || !f.risingEdgeOnly) {
        markComponentDirty(f.component);
    }
}
```

### 3.4 — Tests

`tests/sim/circuit_test.cpp`:
- The two tests using `readers(n)` now use `fanOut(n)`: one entry, `component == u2.id()`; in the two-input test `mask == 0b11` (A, B = Pins 0, 1 of `Dummy`; check pin indices in the file).
- `"circuit: fan-out merges pins of one component"`: a test Component with a rising-edge-only Input and a normal Input on one Net → one entry, `risingEdgeOnly == false`; both rising-edge-only → `true`.

### Fence

- [ ] Build and test gates pass; `grep -rn "readers(" src tests` returns nothing
- [ ] Behaviour check passes
- [ ] Benchmark recorded here
- [ ] Commit: `"perf: phase 3 — circuit fan-out lists"`

---

## Phase 4 — Simulator packed inputs, `read` on them

Design §3 points 2–5, §4.2, §5.1–5.3.

### 4.1 — `EvalContext` (`src/sim/component.hpp`)

- Constructor gains `const PackedInputs& inputs` (last parameter); member `const PackedInputs& inputs_;`.
- Public: `PackedInputs inputs() const { return inputs_; }  // Input Pins only; other bits 0`.

### 4.2 — `src/sim/simulator.hpp`

- Member `std::vector<PackedInputs> inputs_;  // index = ComponentId`.
- Private `void updateInputs(NetId net, LogicValue value);`.
- `EvalContext::read` as design §5.3 (`assert` on `PinKind::Input`; add `#include <cassert>`).

### 4.3 — `src/sim/simulator.cpp`

- Constructor: `inputs_.resize(circuit.components().size(), PackedInputs{0, 0});`.
- `updateInputs`: design §5.2 (two masked writes per `fanOut(net)` entry, no wake).
- `powerOn`: `std::fill(inputs_..., PackedInputs{0, 0})` with the other resets; in the Net loop, after `s.value = r.value`, call `updateInputs(NetId{n}, r.value)`. Initial evaluations: `EvalContext ctx(*this, component->firstPin(), component->id(), inputs_[ToIndex(component->id())]);`.
- `runUntil`: the fan-out loop of 3.3 also writes inputs, design §5.1 (compute `setValue`, `setKnown` once per Net change). Evaluate phase passes `inputs_[ToIndex(id)]`.

### 4.4 — Tests

`tests/sim/simulator_test.cpp`:
- `"simulator: read sees X, 0, 1 after net changes"`: a `Stimulus` stepping 0 → 1 → X → 0 on a Net read by a test Component that stores `ctx.read(0)` on each evaluation; check the stored value after each step.
- `"simulator: inputs() after powerOn matches net values"`: a test Component with Inputs on GND, VCC, a Net driven X by a `Stimulus` initial value, and an unconnected TTL Input (reads 1); after `powerOn`, its stored `ctx.inputs()` has the expected `value` and `known` bits.

### Fence

- [ ] Build and test gates pass; `ctest --preset release` passes
- [ ] Behaviour check passes
- [ ] Benchmark recorded here
- [ ] Commit: `"perf: phase 4 — packed inputs, read() on them"`

---

## Phase 5 — Gate packages on packed inputs

Design §5.4.

### 5.1 — `src/parts/gate_package.hpp/.cpp`

- `inline constexpr std::size_t kMaxUnits = 6;` (74LS04) in the header; `Check(spec.units.size() <= kMaxUnits, ...)` in the constructor.
- Member `std::array<std::uint64_t, kMaxUnits> inputMasks_{};`, filled in the constructor: for unit `u`, bit `unit.inputs[i] - 1` for `i < unit.inputCount`.
- `evaluate` as design §5.4 (`ctx.inputs()`, `PackedGateFunction(unit.kind, in, inputMasks_[u])`, `drive`).

### 5.2 — Tests

Existing gate-package truth-table and delay tests, P1c golden check and `sample_test.cpp` cover it. Add to `tests/parts/gate_package_test.cpp`: `"gate package: X input propagates"` — 74LS00 with one input driven X (Stimulus) and the other 1 → output X; other input 0 → output 1.

### Fence

- [ ] Build and test gates pass; `ctest --preset release` passes
- [ ] Behaviour check passes
- [ ] Benchmark recorded here
- [ ] Commit: `"perf: phase 5 — gate packages on packed inputs"`

---

## Phase 6 — Results and KERNEL_DESIGN.md

### 6.1 — Results

Benchmark (best of 3) and a profile (design §9: `build/profile`, `sample`, top functions with shares) recorded here.

- ≥ 10×: spec S6 passes.
- < 10×: record the shortfall and the profile. P1d stays open (spec "Done when"): the event-queue work (bucket queue) gets a design addendum in [KERNEL_PERF_DESIGN.md](KERNEL_PERF_DESIGN.md), then its phases are appended to this plan.

### 6.2 — `docs/KERNEL_DESIGN.md`

Per design §10: §4 `EvalContext` (new constructor parameter, `inputs()`) and `PackedInputs`; §7 `fanOut_` replaces readers, `inputs_`; §8.3 run loop as design §5.1; §13 new tests.

### Fence

- [ ] Results recorded here
- [ ] KERNEL_DESIGN.md updated
- [ ] Commit: `"perf: phase 6 — results and kernel design update"`

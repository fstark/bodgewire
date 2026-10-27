# Kernel Performance — Detailed Design (P1d)

Implements the specification in [KERNEL_PERF.md](KERNEL_PERF.md) (sections S1–S6).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md). Terminology: [GLOSSARY.md](../GLOSSARY.md). Changes the kernel of [KERNEL_DESIGN.md](../p1a-kernel/KERNEL_DESIGN.md); no new files.

---

## 1. Directory Layout

```
src/sim/component.hpp     + PackedInputs, kMaxPins, EvalContext::inputs()
src/sim/component.cpp     + Check pin count
src/sim/primitives.hpp    + PackedGateFunction()
src/sim/circuit.hpp/.cpp  readers → fan-out lists, built by buildTables()
src/sim/simulator.hpp/.cpp + inputs_, updated on Net change; read() from it
src/parts/gate_package.*  evaluate on PackedInputs
```

## 2. Public Interface — Packed Inputs (S1, S2)

In `component.hpp`:

```cpp
inline constexpr std::size_t kMaxPins = 64;  // one bit per Pin in PackedInputs

// Input Pin values of one Component, bit = PinIndex. X: known = 0, value = 0.
struct PackedInputs {
    std::uint64_t value;  // 1 where the Net is 1
    std::uint64_t known;  // 1 where the Net is 0 or 1
};

class EvalContext {
public:
    // ... unchanged members ...
    PackedInputs inputs() const { return inputs_; }  // Input Pins only; other bits 0

private:
    EvalContext(Simulator& sim, PinId firstPin, ComponentId component, const PackedInputs& inputs);
    const PackedInputs& inputs_;
    // ... unchanged members ...
};
```

Invariant: `value & ~known == 0`. Net values are never Z ([KERNEL_DESIGN.md](../p1a-kernel/KERNEL_DESIGN.md) §3, `Resolution`), so two planes cover 0, 1, X.

`Component::Component` adds `Check(pins.size() <= kMaxPins, "component: more than 64 pins")`.

In `primitives.hpp`, next to `GateFunction`:

```cpp
// GateFunction over the Pins in `mask`, three-valued. Not/Buffer: mask has one bit.
constexpr LogicValue PackedGateFunction(GateKind kind, PackedInputs inputs, std::uint64_t mask);
```

## 3. Integration Points

| # | File | Change | Hot-path cost |
|---|---|---|---|
| 1 | `circuit.hpp/.cpp` | `readers_`, `readerRisingEdgeOnly_` → `fanOut_` (§4.1); `finalize()` calls `buildTables()` | none |
| 2 | `simulator.cpp`, `runUntil` | Net-change loop writes `inputs_` and wakes, one pass over `fanOut(net)` (§5.1) | +2 masked writes per fan-out entry |
| 3 | `simulator.cpp`, `powerOn` | fill `inputs_` after Nets are resolved, before first evaluations (§5.2) | not hot |
| 4 | `simulator.hpp`, `EvalContext::read` | bit test on `inputs_` (§5.3) | 1 load instead of a dependent chain |
| 5 | `simulator.cpp`, evaluate phase and `powerOn` | pass `inputs_[id]` to `EvalContext` | none |
| 6 | `gate_package.cpp` | `evaluate` on `ctx.inputs()` and `PackedGateFunction` (§5.4) | replaces 2–8 reads per gate |

`readBefore`, `rose`, `readBit`, `drive`, `wakeAfter` are unchanged (S2). `readBit` calls `read`, so it uses the packed path.

## 4. Internal State

### 4.1 Circuit: fan-out lists (S1, S4)

```cpp
// One per (Net, reading Component): all Input Pins of that Component on that Net.
struct FanOut {
    ComponentId component;
    bool risingEdgeOnly;  // all those Pins are rising-edge-only
    std::uint64_t mask;   // bit = PinIndex
};
static_assert(sizeof(FanOut) == 16);

std::span<const FanOut> fanOut(NetId net) const;  // CSR, like drivers()

std::vector<std::uint32_t> fanOutStart_;  // size netCount + 1
std::vector<FanOut> fanOut_;
```

Replaces `readers()` and `readerRisingEdgeOnly()`; the two callers (`runUntil`, `circuit_test.cpp`) move to `fanOut()`.

### 4.2 Simulator

```cpp
std::vector<PackedInputs> inputs_;  // index = ComponentId; sized in the constructor
```

16 bytes per Component, one flat array owned by the `Simulator` (S5).

## 5. Key Algorithms

### 5.1 Net change (S1) — `runUntil`, replaces the readers loop

```
if r.value != s.value:
    ... update s, recorder (unchanged) ...
    rose     = s.previous == 0 and s.value == 1
    setValue = r.value == One ? ~0 : 0
    setKnown = r.value != X   ? ~0 : 0
    for f in circuit_.fanOut(net):
        in = inputs_[f.component]
        in.value = (in.value & ~f.mask) | (setValue & f.mask)
        in.known = (in.known & ~f.mask) | (setKnown & f.mask)
        if rose or not f.risingEdgeOnly: markComponentDirty(f.component)
```

Inputs are written during the update phase, before any evaluation at this timestamp, so `read` sees the same value as today's `nets_[net].value`.

### 5.2 Power-on fill — `Simulator::updateInputs(NetId net, LogicValue value)`

Same two masked writes as §5.1, no wake. `powerOn` zeroes `inputs_`, then calls it for every Net after resolution, before the initial evaluations.

### 5.3 `EvalContext::read`

```cpp
inline LogicValue EvalContext::read(PinIndex pin) const {
    assert(sim_.circuit_.pinInfo(pinId(pin)).kind == PinKind::Input);
    const std::uint64_t bit = std::uint64_t{1} << pin;
    if ((inputs_.known & bit) == 0) {
        return LogicValue::X;
    }
    return (inputs_.value & bit) != 0 ? LogicValue::One : LogicValue::Zero;
}
```

The `assert` catches Parts reading a non-Input Pin, whose bit is never written. No Part does today.

### 5.4 `PackedGateFunction` and gate packages (S3)

```
ones  = inputs.value & mask                  // inputs known 1
zeros = inputs.known & ~inputs.value & mask  // inputs known 0
And:  zeros ? 0 : ones == mask ? 1 : X
Or:   ones ? 1 : zeros == mask ? 0 : X
Xor:  (ones | zeros) != mask ? X : popcount(ones) odd ? 1 : 0
Nand, Nor, Xnor: Not(And, Or, Xor)
Not:  Not(Buffer)
Buffer: ones ? 1 : zeros ? 0 : X
```

Same results as `GateFunction` for every input combination (tested exhaustively, §8).

`GatePackage` precomputes one mask per unit in its constructor:

```cpp
std::array<std::uint64_t, kMaxUnits> inputMasks_;  // kMaxUnits = 6 (74LS04); bit = package pin - 1

void GatePackage::evaluate(EvalContext& ctx) {
    const PackedInputs in = ctx.inputs();
    for (std::size_t u = 0; u < spec_.units.size(); ++u) {
        const GateUnit& unit = spec_.units[u];
        const LogicValue value = PackedGateFunction(unit.kind, in, inputMasks_[u]);
        ctx.drive(unit.output - 1u, value, DelayFor(spec_.delays, value));
    }
}
```

Unchanged outputs cost only `drive`'s inline fast-path compare (543f850); no extra output state in the Part.

### 5.5 Building fan-out lists (S4) — `Circuit::buildTables()`

Private; clears and rebuilds `drivers_`, `fanOut_`, `netHasTtlInput_` from `pins_`. Same pass as today's `finalize()` body, with readers turned into `FanOut`: a new entry when the Component differs from the last entry on that Net (Pins of one Component are contiguous), else `mask |= bit` and `risingEdgeOnly &= pin.risingEdgeOnly`. `finalize()` assigns private Nets to unconnected Pins, then calls `buildTables()`. P2 runtime rewiring re-runs it and refreshes the affected `inputs_` with `updateInputs`.

## 6. Revert of 738d9e1

`git revert 738d9e1` before any of the above (spec, Starting point). §5.4 replaces its kind dispatch and drops its one-kind-per-package `Check`.

## 7. Reused Infrastructure

| Existing | Used for |
|---|---|
| CSR layout of `drivers_` | `fanOut_` |
| `risingEdgeOnly` on `PinSpec` / `PinInfo` (ccd9c47) | `FanOut::risingEdgeOnly` |
| `drive` inline fast path (543f850) | unchanged outputs in gate packages |
| `GatePackageSpec`, `GateUnit` tables | input masks, computed once |
| `Net` state `previous`, `changedAt` | `readBefore`, `rose`, unchanged |

## 8. Testing

| File | Test |
|---|---|
| `tests/sim/primitives_test.cpp` | `"gate function: packed matches GateFunction"`: every kind, 1–4 inputs, all 3ⁿ combinations of 0/1/X |
| `tests/sim/circuit_test.cpp` | existing readers tests rewritten on `fanOut()`; `"circuit: fan-out merges pins of one component"` (two inputs → one entry, mask with two bits; rising-edge-only only if both are); `"circuit: more than 64 pins rejected"` |
| `tests/sim/simulator_test.cpp` | `"simulator: read sees X, 0, 1 after net changes"`; `"simulator: inputs() after powerOn matches net values"` |
| existing | gate package truth tables and delays, P1c golden check, `sample_test.cpp` (C++ API vs netlist): unchanged and must pass |

Behaviour gate (S6): `applied_events` and `evaluations` of `bw run netlists/test_pattern.bwn --time 5s` equal before and after; VCD of `--time 40ms --seed 1 --record SYNC_N,VIDEO,HSYNC_N,VSYNC_N` byte-identical (`cmp`).

## 9. Benchmark

As spec S6: release preset, `bw run netlists/test_pattern.bwn --time 5s`, best of 3. Profiling build:

```
cmake -S . -B build/profile -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -g -fno-omit-frame-pointer"
./build/profile/src/cli/bw run netlists/test_pattern.bwn --time 60s & sample $! 8 -file /tmp/bw.txt
```

## 10. KERNEL_DESIGN.md Updates

§4 (`EvalContext`, `PackedInputs`), §7 (`fanOut_`, `inputs_`), §8.3 (run loop as §5.1), §13 (tests above).

# Kernel — Implementation Plan (P1a)

Design: [KERNEL_DESIGN.md](KERNEL_DESIGN.md)
Spec: [KERNEL.md](KERNEL.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Build skeleton, basic types (time, ids, check, rng) | |
| 2 | Logic values, drives, resolver | |
| 3 | Event queue | |
| 4 | Component and Circuit | |
| 5 | Recorder | |
| 6 | Simulator core and Stimulus | |
| 7 | Gates, tri-state buffer, inertial delay | |
| 8 | D flip-flop and clock source | |
| 9 | VCD export | |
| 10 | Benchmark | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug --output-on-failure`

All code follows [STYLE.md](STYLE.md) and [NAMING.md](NAMING.md); namespace `bw`; sources in `src/sim/`, tests in `tests/sim/`. Each new `.cpp` is added to `src/sim/CMakeLists.txt` or `tests/CMakeLists.txt` in the phase that creates it.

---

## Phase 1 — Build skeleton and basic types

Creates the CMake project, presets, Catch2 wiring and the dependency-free types every later file uses. First because nothing else compiles without it.

### 1.1 — Repository files

- `CMakeLists.txt`: exactly as design §10, but `add_subdirectory(bench)` is added in phase 10.
- `CMakePresets.json` (version 6): configure presets `debug` and `release` (generator Ninja, `binaryDir` `${sourceDir}/build/${presetName}`), `debug` sets `CMAKE_CXX_FLAGS` to `-fsanitize=address,undefined -fno-omit-frame-pointer`; matching build presets; test preset `debug` with `output.outputOnFailure: true`.
- `.clang-format`: `BasedOnStyle: LLVM`, `IndentWidth: 4`, `ColumnLimit: 100`, `PointerAlignment: Left`, `AccessModifierOffset: -4`.
- `.gitignore` already exists (`venv/`, `build/`).

### 1.2 — Library target and basic headers

- `src/sim/CMakeLists.txt`: `add_library(bw_sim STATIC check.cpp)`, public include dir `${PROJECT_SOURCE_DIR}/src`, `target_link_libraries(bw_sim PRIVATE bw_warnings)`.
- `src/sim/time.hpp`: `Time`, `kPicosPerSecond`, `namespace bw::literals` with `_ps`, `_ns`, `_us`, `_ms`, `_s` (design §2).
- `src/sim/ids.hpp`: `NetId`, `PinId`, `ComponentId`, `PinIndex`, `ToIndex` overloads (`constexpr`, `std::to_underlying`).
- `src/sim/check.hpp/.cpp`: `Check()`; message format `"<file>:<line>: <message>"`.
- `src/sim/rng.hpp`: `Rng` — splitmix64 expands the seed into the 4-word xoshiro256** state; `nextBit()` returns the top bit of `next()`.

### 1.3 — Test target

- `tests/CMakeLists.txt`: `include(FetchContent)`, `FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG v3.7.1)`, `FetchContent_MakeAvailable(Catch2)`, `add_executable(bw_tests …)`, link `bw_sim`, `bw_warnings`, `Catch2::Catch2WithMain`; `include(Catch)`, `catch_discover_tests(bw_tests)`.

### 1.4 — Tests

- `tests/sim/time_test.cpp`: `"time: literals"` — `1_ns == 1000`, `1_s == kPicosPerSecond`.
- `tests/sim/check_test.cpp`: `"check: throws logic_error with message"`; `"check: passes silently"`.
- `tests/sim/rng_test.cpp`: `"rng: fixed seed gives fixed sequence"` — record the first 4 values of `Rng(42)` once, hard-code them; `"rng: different seeds differ"`.

### Fence

- [ ] Files above exist; `build/debug/tests/bw_tests` runs
- [ ] Build gate and test gate pass
- [ ] Commit: `"kernel: phase 1 — build skeleton and basic types"`

---

## Phase 2 — Logic values, drives, resolver

Pure functions for S1/S2. No dependencies beyond phase 1.

### 2.1 — `src/sim/logic.hpp/.cpp`

`LogicValue`, `DriveStrength`, `Family`, `PinKind`, `Drive`, `DriveOf`, `Resolution`, `Resolver` exactly as design §3. Also:

```cpp
constexpr LogicValue Not(LogicValue value);  // 0↔1, X and Z → X
constexpr char ToChar(LogicValue value);     // '0', '1', 'z', 'x'
```

`DriveOf`: `Output` → `{value, value == Z ? None : Strong}`; `OpenCollector` → `Zero` is `{Zero, Strong}`, `X` is `{X, Strong}`, `One`/`Z` are `{Z, None}`; `Input` → `{Z, None}`. `Resolver` per design §8.1.

### 2.2 — Tests: `tests/sim/logic_test.cpp`

- `"logic: DriveOf"` — every `PinKind` × `LogicValue`.
- `"resolve: single strong driver"`, `"resolve: strong beats weak"`, `"resolve: supply beats strong"`, `"resolve: weak alone"`.
- `"resolve: no driver with TTL input reads 1, floating"`, `"resolve: no driver MOS-only keeps last driven, floating"`.
- `"resolve: TTL contention reads 0"`, `"resolve: mixed-family contention reads X"`, `"resolve: supply contention reads X"`, `"resolve: X driver gives X"`, `"resolve: weaker X is ignored"`.
- `"resolve: open-collector wired-AND with weak pull-up"` — all released → 1, one pulls 0 → 0.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"kernel: phase 2 — logic values and resolver"`

---

## Phase 3 — Event queue

### 3.1 — `src/sim/event_queue.hpp/.cpp`

`Event`, `EventQueue` per design §7 (`std::vector<Event>` with `std::push_heap` / `std::pop_heap`, comparator: greater `time`). Stamp helpers in the same header:

```cpp
constexpr std::uint32_t MakeStamp(std::uint32_t generation, bool isWake, LogicValue value);
constexpr std::uint32_t StampGeneration(std::uint32_t stamp);  // stamp >> 3
constexpr bool StampIsWake(std::uint32_t stamp);               // bit 2
constexpr LogicValue StampValue(std::uint32_t stamp);          // bits 0–1
```

`static_assert(sizeof(Event) == 16)`.

### 3.2 — Tests: `tests/sim/event_queue_test.cpp`

- `"event queue: pops in time order"` (pushed out of order).
- `"event queue: equal times all before later"`.
- `"event queue: clear empties"`.
- `"event queue: stamp round-trips"` — generation up to `2^29 - 1`, wake flag, each value.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"kernel: phase 3 — event queue"`

---

## Phase 4 — Component and Circuit

The fixed topology (design §4, §5 Circuit, §7 Circuit state). `EvalContext` / `PowerOnContext` are declared here; their members are defined in phase 6, so phase 4 tests must not call `evaluate()` or `powerOn()`.

### 4.1 — `src/sim/component.hpp/.cpp`

`PinSpec`, `Component` (ctor, `name()`, `pins()`, `id()`, `pinIndex()` with `Check`, default `powerOn()` empty), forward declaration `class Simulator;`, class declarations of `EvalContext` and `PowerOnContext` with private members `Simulator& sim_; PinId firstPin_; ComponentId component_;` and `friend class Simulator;`.

### 4.2 — `src/sim/circuit.hpp/.cpp`

API per design §5, state per design §7. Behaviour:

- `add<T>()`: `std::make_unique<T>`, sets `id_`, `firstPin_ = pins_.size()`, appends one `PinInfo` per `PinSpec` with `net` = sentinel `NetId{UINT32_MAX}`.
- `connect()`: `Check` not finalized, pin in range, pin not yet connected.
- `tie()`: combines with an existing tie per design §7 note.
- `finalize()`: `Check` not already finalized; give each unconnected pin a private net named `"<component>.<pin>"`; build `driverStart_/drivers_` (pins with kind ≠ `Input`), `readerStart_/readers_` (owners of `Input` pins, deduplicated per net, in id order), `netHasTtlInput_`.
- Accessors used by `Simulator` (public, read-only): `components()`, `pinInfo(PinId)`, `drivers(NetId)` / `readers(NetId)` returning `std::span`, `tieOf(NetId)`, `hasTtlInput(NetId)`, `isFinalized()`.

### 4.3 — Tests: `tests/sim/circuit_test.cpp`

A file-local `class Dummy : public Component` with pins `A` (Input, TTL), `B` (Input, MOS), `Y` (Output); `evaluate()` body empty (never called).

- `"circuit: pin ids are contiguous per component"`.
- `"circuit: connect twice throws"`, `"circuit: unknown pin name throws"`, `"circuit: edit after finalize throws"`.
- `"circuit: unconnected pins get private nets"` — name `"U1.A"`.
- `"circuit: drivers and readers"` — readers deduplicated when one component has two inputs on a net; output-only owner is not a reader.
- `"circuit: TTL input flag"` — only nets with `A` set.
- `"circuit: ties combine"` — supply beats weak; equal-strength disagreement → X.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"kernel: phase 4 — component and circuit"`

---

## Phase 5 — Recorder

### 5.1 — `src/sim/recorder.hpp/.cpp`

Per design §5. Storage: `std::vector<std::int32_t> slotOfNet_` (−1 = not recording, grown on demand), `std::vector<std::vector<Sample>> slots_`, `std::vector<NetId> nets_`. `add()` asserts the net is recording.

### 5.2 — Tests: `tests/sim/recorder_test.cpp`

- `"recorder: start stores initial sample"`, `"recorder: start twice is a no-op"`.
- `"recorder: add appends"`, `"recorder: nets in start order"`, `"recorder: clear forgets everything"`.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"kernel: phase 5 — recorder"`

---

## Phase 6 — Simulator core and Stimulus

The run loop. `Stimulus` is the first primitive because simulator tests need driven inputs; it uses `Z` and `Family` to test resolution without gates.

### 6.1 — `src/sim/simulator.hpp/.cpp`

API per design §5, state per design §7, algorithms §8.2 (`EvalContext::drive`), §8.3 (`runUntil`), §8.4 (`powerOn`). Also defines every `EvalContext` and `PowerOnContext` member. Private helpers:

```cpp
void markNetDirty(NetId net);
void markComponentDirty(ComponentId component);
Resolution resolveNet(NetId net) const;
void scheduleWake(ComponentId component, Time delay);
```

`runFor(d)` = `runUntil(now_ + d)`. `record(net)` = `recorder_.start(net, now_, value(net))`. `readBit()`: `X` → `rng_.nextBit()`.

### 6.2 — `src/sim/primitives.hpp/.cpp` — `Stimulus` only

Per design §6 and §8.6. Pin `OUT` (`Output`, given family). `powerOn`: `setOutput(OUT, initial)`. `evaluate`: per §8.6, with `next_` (index of the next step) as state, reset in `powerOn`.

### 6.3 — Tests: `tests/sim/simulator_test.cpp`

Helpers in the test file: a function building a circuit with Stimuli on named nets.

- `"simulator: stimulus steps at exact times"` — record the net; samples at 0 (initial), 10 ns, 25 ns.
- `"simulator: runUntil stops at end and keeps pending events"` — step at 20 ns, run to 15 ns, value unchanged, `now() == 15 ns`; run on, change applied.
- `"simulator: TTL contention reads 0"` — two TTL Stimuli, `One` vs `Zero`.
- `"simulator: mixed-family contention reads X"`.
- `"simulator: released MOS bus keeps last driven value, floating"` — MOS Stimulus `One` then `Z`; value `One`, `isFloating()`.
- `"simulator: weak pull-up wins over Z"` — `tie(net, One, Weak)`, Stimulus `Z` → `One`, not floating.
- `"simulator: same seed same recording"`, `"simulator: power-on has no X"` (no contention circuit), `"simulator: powerOn resets time, queue and recordings"`.
- `"simulator: stats count applied events and wakes"`.

### Fence

- [ ] Build gate and test gate pass (sanitizers clean)
- [ ] Commit: `"kernel: phase 6 — simulator core and stimulus"`

---

## Phase 7 — Gates, tri-state buffer, inertial delay

### 7.1 — `GateFunction`, `Gate`, `TriStateBuffer` in `primitives.hpp/.cpp`

`Delays`, `GateKind`, `GateFunction` (§8.5), `Gate` (pins `I0..I(n-1)` Input, `Y` Output, `Check` on input count), `TriStateBuffer` (pins `A`, `EN`, `Y`). `Gate::evaluate`: read inputs into a fixed `std::array<LogicValue, 16>` (`Check` `inputCount ≤ 16` in ctor), `GateFunction`, drive per §8.5. `TriStateBuffer::evaluate`: `EN == Zero` → drive `Z` after `toggle`; `EN == X` → drive `X`; else drive `A` (`data` delays; `toggle` if the current output is `Z`).

### 7.2 — Tests: `tests/sim/primitives_test.cpp` (gates) and additions to `simulator_test.cpp`

primitives_test:
- `"gate function: truth tables"` — every kind, all 0/1 combinations for 2 inputs, 3-input AND/OR.
- `"gate function: X handling"` — `And(0, X) == 0`, `And(1, X) == X`, `Or(1, X) == 1`, `Xor(1, X) == X`, `Not(X) == X`.
- `"tri-state: disable gives Z, enable restores"`.

simulator_test:
- `"inertial: pulse shorter than delay is swallowed"` — Buffer 10 ns, input pulse 5 ns → output never changes, one stale event.
- `"inertial: pulse longer than delay passes"` — shifted by the delay.
- `"inertial: pending change cancelled when input returns"` — design §8.2 case.
- `"inertial: rise and fall delays differ"`.
- `"simulator: simultaneous changes give no glitch"` — Stimuli A and B switch at the same time to opposite values; AND output never changes.
- `"simulator: floating TTL input reads 1"` — Gate input on a net driven only by a `Z` Stimulus.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"kernel: phase 7 — gates and inertial delay"`

---

## Phase 8 — D flip-flop and clock source

### 8.1 — `DFlipFlop`, `ClockSource` in `primitives.hpp/.cpp`

Per design §6 and §8.6. `ClockSource`: `highTime_ = period / 2`, `lowTime_ = period - highTime_`; `Check(period >= 2)`.

### 8.2 — Tests: additions to `primitives_test.cpp`

- `"dff: captures D on rising edge only"`.
- `"dff: samples D as before a same-timestamp clock edge"` — D and CLK Stimuli step at the same time.
- `"dff: power-on Q and Q_N complementary"` over several seeds.
- `"clock: period and duty"` — 1001 ps period: high 500, low 501.
- `"ring oscillator: period is 2 x 3 x delay"` — three `Not` gates, 5 ns each, measured after 1 µs.
- `"ripple counter: counts"` — 4 DFFs, `Q_N → D`, each stage's `Q_N` clocks the next (so `Q` counts up); after N clock cycles plus settling time, the `Q` bits advanced by N mod 16 from their value after the first settle.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"kernel: phase 8 — flip-flop and clock source"`

---

## Phase 9 — VCD export

### 9.1 — `src/sim/vcd.hpp/.cpp`

`WriteVcd()` per design §5:

```
$timescale 1ps $end
$scope module bodgewire $end
$var wire 1 <id> <net name> $end      (one per recorded net, ids '!' + index, base-94)
$upscope $end
$enddefinitions $end
#<time>                               (merged across nets, ascending; one block per time)
<value><id>                           (0, 1, x)
#<end>
```

Net names: characters other than `[A-Za-z0-9_.]` replaced by `_`.

### 9.2 — Tests: `tests/sim/vcd_test.cpp`

- `"vcd: two nets exact text"` — build, run, compare to an inline raw string.
- `"vcd: X written as x"`.
- `"vcd: manual export"` tagged `[.manual]` (hidden): writes `ring_oscillator.vcd` and `ripple_counter.vcd` to the working directory, for viewing in GTKWave or Surfer: `./build/debug/tests/bw_tests "[.manual]"`.

### Fence

- [ ] Build gate and test gate pass
- [ ] Manual: both VCD files open in a viewer and look right (spec "Done when")
- [ ] Commit: `"kernel: phase 9 — VCD export"`

---

## Phase 10 — Benchmark

### 10.1 — `bench/CMakeLists.txt`, `bench/kernel_bench.cpp`

Add `add_subdirectory(bench)` to the top-level `CMakeLists.txt`. Executable `bw_kernel_bench` linking `bw_sim`, `bw_warnings`. Circuit, output and exit code per design §12. Argument parsing: only `--seconds N` (default 1); anything else prints usage and exits 2. Wall time from `std::chrono::steady_clock`.

Output (one value per line, for easy comparison between runs):

```
simulated_s 1
wall_s 0.42
ratio 2.38
applied_events_per_s 68.1e6
evaluations_per_s 120.4e6
counter 0x3a1f
```

### Fence

- [ ] Build gate and test gate pass
- [ ] `cmake --preset release && cmake --build --preset release && ./build/release/bench/bw_kernel_bench` exits 0 (≥ 20 M applied events/s). If it fails: mark the phase **Skipped: below target**, record the numbers here; optimisation becomes its own plan
- [ ] Commit: `"kernel: phase 10 — benchmark"`

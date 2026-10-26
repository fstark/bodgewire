# Kernel — Detailed Design (P1a)

Implements the specification in [KERNEL.md](KERNEL.md) (sections S1–S7).

All code must follow [STYLE.md](STYLE.md) and [NAMING.md](NAMING.md). Terminology: [GLOSSARY.md](GLOSSARY.md).

There is no existing code: this design also creates the repository skeleton.

---

## 1. Directory Layout

```
CMakeLists.txt            top level: options, warnings, subdirectories
CMakePresets.json         debug (sanitizers) and release presets
.clang-format
.gitignore
src/sim/                  library bw_sim, namespace bw, standard library only
  time.hpp                Time, literals
  ids.hpp                 NetId, PinId, ComponentId, PinIndex
  check.hpp/.cpp          Check()
  rng.hpp                 Rng (seeded, platform-independent)
  logic.hpp/.cpp          LogicValue, DriveStrength, Family, PinKind, Drive, Resolver
  event_queue.hpp/.cpp    Event, EventQueue
  component.hpp/.cpp      PinSpec, Component, EvalContext, PowerOnContext
  circuit.hpp/.cpp        Circuit (topology, built then finalized)
  simulator.hpp/.cpp      Simulator (runtime state, run loop)
  recorder.hpp/.cpp       Recorder
  vcd.hpp/.cpp            WriteVcd()
  primitives.hpp/.cpp     Gate, TriStateBuffer, DFlipFlop, ClockSource, Stimulus
tests/
  CMakeLists.txt          bw_tests (Catch2)
  sim/*_test.cpp          one per source file
bench/
  CMakeLists.txt
  kernel_bench.cpp        bw_kernel_bench (S7)
```

Split rationale: `Circuit` is the fixed topology (later recomputed after Repairs, P2); `Simulator` is everything reset by `powerOn()`. Components keep their internal state themselves.

## 2. Public Interface — Basic Types

```cpp
// time.hpp
using Time = std::int64_t;  // picoseconds
inline constexpr Time kPicosPerSecond = 1'000'000'000'000;
namespace literals {
constexpr Time operator""_ps(unsigned long long v);  // also _ns, _us, _ms, _s
}

// ids.hpp
enum class NetId : std::uint32_t {};
enum class PinId : std::uint32_t {};        // global, across all Components
enum class ComponentId : std::uint32_t {};
using PinIndex = std::uint32_t;             // Pin number local to its Component
constexpr std::uint32_t ToIndex(NetId id);  // overloads for PinId, ComponentId

// check.hpp
void Check(bool condition, std::string_view message,
           std::source_location where = std::source_location::current());  // throws std::logic_error

// rng.hpp — xoshiro256**, seeded through splitmix64; identical on every platform
class Rng {
public:
    explicit Rng(std::uint64_t seed = 0);
    std::uint64_t next();
    bool nextBit();
};
```

## 3. Public Interface — Logic

```cpp
enum class LogicValue : std::uint8_t { Zero, One, Z, X };  // Z only as a drive (S1)
enum class DriveStrength : std::uint8_t { None, Weak, Strong, Supply };
enum class Family : std::uint8_t { Ttl, Mos };
enum class PinKind : std::uint8_t { Input, Output, OpenCollector };

struct Drive {
    LogicValue value;
    DriveStrength strength;
    Family family;
};

// Output Pin value → its Drive: totem-pole Z → None; open collector 1/Z → None.
constexpr Drive DriveOf(PinKind kind, Family family, LogicValue value);

struct Resolution {
    LogicValue value;  // never Z
    bool floating;
};

// Accumulates the Drives of one Net, then applies the S2 table.
class Resolver {
public:
    void add(Drive drive);
    Resolution finish(bool hasTtlInput, LogicValue lastDriven) const;
private:
    DriveStrength max_ = DriveStrength::None;
    LogicValue value_ = LogicValue::Z;
    bool conflict_ = false;
    bool sawX_ = false;
    bool allTtl_ = true;
};
```

`Resolver` is the single implementation used by the run loop and by unit tests (algorithm §8.1).

## 4. Public Interface — Components

```cpp
struct PinSpec {
    std::string name;  // datasheet name, NAMING.md "Pin names"
    PinKind kind;
    Family family;
};

class Component {
public:
    virtual ~Component() = default;
    std::string_view name() const;
    std::span<const PinSpec> pins() const;
    ComponentId id() const;              // valid once added to a Circuit
    PinIndex pinIndex(std::string_view pinName) const;  // Check()s the name
    // Outputs already hold random 0/1; override to seed state / make outputs consistent.
    virtual void powerOn(PowerOnContext& ctx);
    virtual void evaluate(EvalContext& ctx) = 0;
protected:
    Component(std::string name, std::vector<PinSpec> pins);
private:
    friend class Circuit;
    std::string name_;
    std::vector<PinSpec> pins_;
    ComponentId id_{};
    PinId firstPin_{};                    // global PinId of local Pin 0
};

class EvalContext {
public:
    Time now() const;
    LogicValue read(PinIndex pin) const;        // Net value now
    LogicValue readBefore(PinIndex pin) const;  // Net value before this timestamp (S3)
    bool rose(PinIndex pin) const;              // Net went 0 → 1 at this timestamp
    bool readBit(PinIndex pin);                 // X → seeded random bit (MOS inputs, S4)
    void drive(PinIndex pin, LogicValue value, Time delay);  // inertial, delay ≥ 1 ps
    void wakeAfter(Time delay);                 // evaluate this Component again at now + delay
};

class PowerOnContext {
public:
    bool randomBit();
    void setOutput(PinIndex pin, LogicValue value);  // initial output, no event
};
```

`EvalContext` and `PowerOnContext` hold a `Simulator&`, the Component's `firstPin_` and `id_`; they are created on the stack per call (two pointers and two integers).

## 5. Public Interface — Circuit and Simulator

```cpp
class Circuit {
public:
    NetId addNet(std::string name);
    template <std::derived_from<Component> T, class... Args>
    T& add(Args&&... args);                                  // assigns id and PinIds
    void connect(const Component& component, PinIndex pin, NetId net);
    void connect(const Component& component, std::string_view pin, NetId net);
    void tie(NetId net, LogicValue value, DriveStrength strength);  // GND/VCC or pull resistor
    void finalize();  // unconnected Pins get private Nets; builds lookup tables; no more edits

    std::size_t netCount() const;
    std::string_view netName(NetId net) const;
    NetId netOf(PinId pin) const;
};

struct SimStats {
    std::uint64_t appliedEvents = 0;  // output changes actually applied
    std::uint64_t staleEvents = 0;    // cancelled by inertial delay, skipped
    std::uint64_t wakes = 0;
    std::uint64_t evaluations = 0;
};

class Simulator {
public:
    explicit Simulator(Circuit& circuit);  // Check()s the Circuit is finalized
    void powerOn(std::uint64_t seed);      // S4
    void runUntil(Time end);
    void runFor(Time duration);
    Time now() const;
    LogicValue value(NetId net) const;
    bool isFloating(NetId net) const;
    void record(NetId net);                // from now on; kept until the next powerOn
    const Recorder& recorder() const;
    const SimStats& stats() const;
};

// recorder.hpp
struct Sample { Time time; LogicValue value; };
class Recorder {
public:
    void start(NetId net, Time now, LogicValue value);  // no-op if already recording
    void add(NetId net, Time time, LogicValue value);   // only called for recorded Nets
    bool isRecording(NetId net) const;
    std::span<const NetId> nets() const;                // in start() order
    std::span<const Sample> samples(NetId net) const;
    void clear();
};

// vcd.hpp — timescale 1 ps, one 1-bit wire per recorded Net, values 0/1/x
void WriteVcd(std::ostream& out, const Circuit& circuit, const Recorder& recorder, Time end);
```

## 6. Public Interface — Primitives (S5)

```cpp
struct Delays {
    Time rise;  // tPLH
    Time fall;  // tPHL
};

enum class GateKind : std::uint8_t { And, Or, Nand, Nor, Xor, Xnor, Not, Buffer };

// Pure three-valued gate function (§8.5); inputs are 0/1/X.
LogicValue GateFunction(GateKind kind, std::span<const LogicValue> inputs);

// Pins I0..I(n-1), then Y. Not/Buffer: n = 1. Others: n ≥ 2.
class Gate final : public Component {
public:
    Gate(std::string name, GateKind kind, std::uint32_t inputCount, Delays delays,
         Family family = Family::Ttl);
    PinIndex output() const;
};

// Pins A, EN, Y. EN = 0 → Y = Z after `toggle`; data changes use `data`.
class TriStateBuffer final : public Component {
public:
    TriStateBuffer(std::string name, Delays data, Time toggle, Family family = Family::Ttl);
    static constexpr PinIndex kA = 0, kEn = 1, kY = 2;
};

// Pins D, CLK, Q, Q_N. Rising-edge; samples D as before the edge.
class DFlipFlop final : public Component {
public:
    DFlipFlop(std::string name, Delays clockToQ, Family family = Family::Ttl);
    static constexpr PinIndex kD = 0, kClk = 1, kQ = 2, kQN = 3;
};

// Pin OUT. Square wave; high for period / 2, low for the remainder.
class ClockSource final : public Component {
public:
    ClockSource(std::string name, Time period);
    static constexpr PinIndex kOut = 0;
};

// Pin OUT. Test and benchmark stimulus: initial value, then steps at absolute times ≥ 1 ps.
// Values may be Z (tri-state driver); family selects contention rules.
class Stimulus final : public Component {
public:
    struct Step { Time time; LogicValue value; };
    Stimulus(std::string name, LogicValue initial, std::vector<Step> steps,
             Family family = Family::Ttl);  // steps sorted, distinct times
    static constexpr PinIndex kOut = 0;
};
```

## 7. Internal State

```cpp
// Circuit — fixed after finalize(); flat arrays indexed by ToIndex(id)
struct PinInfo { NetId net; ComponentId owner; PinKind kind; Family family; };
std::vector<std::unique_ptr<Component>> components_;
std::vector<PinInfo> pins_;
std::vector<std::string> netNames_;
std::vector<Drive> netTie_;              // strongest tie per Net, None if untied
std::vector<std::uint8_t> netHasTtlInput_;
// CSR: drivers (output PinIds) and readers (unique ComponentIds with an input on the Net)
std::vector<std::uint32_t> driverStart_, readerStart_;  // size netCount + 1
std::vector<PinId> drivers_;
std::vector<ComponentId> readers_;

// Simulator — reset by powerOn()
struct NetState {            // 16 bytes
    Time changedAt;
    LogicValue value, previous, lastDriven;
    bool floating, dirty;
};
struct OutputState {         // 8 bytes, one per Pin (inputs unused)
    std::uint32_t generation;  // bumped on cancellation; stale events carry an old one
    LogicValue current, pending;
    bool hasPending;
};
std::vector<NetState> nets_;
std::vector<OutputState> outputs_;
std::vector<std::uint8_t> componentDirty_;
std::vector<NetId> dirtyNets_;
std::vector<ComponentId> dirtyComponents_;
EventQueue queue_;
Rng rng_;
Recorder recorder_;
SimStats stats_;
Time now_ = 0;

// event_queue.hpp — 16 bytes, no allocation per event (vector-backed binary heap)
struct Event {
    Time time;
    std::uint32_t target;  // PinId, or ComponentId for a wake
    std::uint32_t stamp;   // generation << 3 | isWake << 2 | LogicValue
};
class EventQueue {
public:
    void push(Event event);
    const Event& top() const;
    void pop();
    bool empty() const;
    void clear();
};
```

Two `Tie`s on one Net combine: stronger wins; equal strength and different values → `X` drive at that strength.

## 8. Key Algorithms

### 8.1 Resolution (S2)

```
add(d):
  if d.strength == None: return
  if d.strength > max_: max_ = d.strength; value_ = d.value; conflict_ = false
                        sawX_ = (d.value == X); allTtl_ = (d.family == Ttl); return
  if d.strength == max_: conflict_ |= (d.value != value_); sawX_ |= (d.value == X)
                         allTtl_ &= (d.family == Ttl)

finish(hasTtlInput, lastDriven):
  if max_ == None: return { hasTtlInput ? One : lastDriven, floating = true }
  if sawX_: return { X, false }
  if conflict_: return { (max_ == Strong && allTtl_) ? Zero : X, false }
  return { value_, false }
```

### 8.2 Inertial scheduling (S3) — `EvalContext::drive`

```
o = outputs_[pin]; assert(delay >= 1)
target = o.hasPending ? o.pending : o.current
if value == target: return
if o.hasPending: ++o.generation; o.hasPending = false     // cancel
if value != o.current:
  queue_.push({ now_ + delay, pin, Stamp(o.generation, false, value) })
  o.pending = value; o.hasPending = true
```

### 8.3 Run loop — `runUntil(end)`

```
while !queue_.empty() and queue_.top().time <= end:
  now_ = queue_.top().time
  // update phase
  while !queue_.empty() and queue_.top().time == now_:
    e = pop()
    if e is wake: MarkDirty(component); ++stats.wakes; continue
    o = outputs_[e.target]
    if Generation(e) != o.generation: ++stats.staleEvents; continue
    o.current = Value(e); o.hasPending = false; ++stats.appliedEvents
    MarkDirty(netOf(e.target))
  for net in dirtyNets_:
    r = Resolver(); r.add(netTie_[net]); for pin in drivers(net): r.add(DriveOf(pin, outputs_[pin].current))
    res = r.finish(netHasTtlInput_[net], nets_[net].lastDriven)
    s = nets_[net]; s.dirty = false; s.floating = res.floating
    if !res.floating: s.lastDriven = res.value
    if res.value != s.value:
      s.previous = s.value; s.value = res.value; s.changedAt = now_
      if recorder_.isRecording(net): recorder_.add(net, now_, res.value)
      for c in readers(net): MarkDirty(c)
  dirtyNets_.clear()
  // evaluate phase
  for c in dirtyComponents_: componentDirty_[c] = 0; components[c].evaluate(ctx); ++stats.evaluations
  dirtyComponents_.clear()
now_ = end
```

`readBefore(pin)`: `s.changedAt == now_ ? s.previous : s.value`. `rose(pin)`: `s.changedAt == now_ && s.previous == Zero && s.value == One`.

Evaluation order is the dirty-list order, which is deterministic; results don't depend on it because evaluations only read Nets and schedule (S3).

### 8.4 Power-on (S4)

```
queue_.clear(); now_ = 0; rng_ = Rng(seed); recorder_.clear(); stats_ = {}
for each output pin: current = rng_.nextBit() ? One : Zero; hasPending = false
for each component (in id order): component.powerOn(PowerOnContext)
for each net: lastDriven = random bit; resolve (8.3), set value, previous = value,
              changedAt = -1, floating
for each component (in id order): evaluate
```

`changedAt = -1` ensures nothing "rose" at time 0.

### 8.5 Three-valued gates

```
And:  any input Zero → Zero; else any X → X; else One       (Nand = Not(And))
Or:   any input One  → One;  else any X → X; else Zero      (Nor  = Not(Or))
Xor:  any X → X; else parity                                (Xnor = Not(Xor))
Not(X) = X
drive(Y, v, v == One ? rise : v == Zero ? fall : max(rise, fall))
```

### 8.6 Sequential primitives

```
DFlipFlop.evaluate:  if rose(CLK): q = readBefore(D) (X stays X)
                     drive(Q, q, …); drive(Q_N, Not(q), …)     // rise/fall per value
DFlipFlop.powerOn:   q = randomBit; setOutput(Q, q); setOutput(Q_N, Not(q))

ClockSource.powerOn:  level_ = randomBit; setOutput(OUT, level_)
ClockSource.evaluate: level_ = Not(level_)
                      d = level_ == One ? lowTime : highTime    // time spent at the old level
                      drive(OUT, level_, d); wakeAfter(d)

Stimulus.powerOn:     setOutput(OUT, initial)
Stimulus.evaluate:    while next step exists and step.time - 1 == now: drive(OUT, step.value, 1)
                      if next step exists: wakeAfter(step.time - 1 - now)
```

`ClockSource` and `Stimulus` have no inputs: only power-on and wakes evaluate them.

## 9. Reused Infrastructure

None in the repository. External: Catch2 v3 (tests only), fetched by CMake `FetchContent` at a pinned tag.

## 10. Build Integration

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)
project(bodgewire LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(BW_BUILD_TESTS "Build unit tests" ON)
add_library(bw_warnings INTERFACE)
target_compile_options(bw_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Werror)
add_subdirectory(src/sim)
add_subdirectory(bench)
if(BW_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
```

- `src/sim/CMakeLists.txt`: `add_library(bw_sim STATIC …)`, `target_include_directories(bw_sim PUBLIC ${PROJECT_SOURCE_DIR}/src)`, links `bw_warnings` privately.
- `tests/CMakeLists.txt`: `FetchContent_Declare(Catch2 GIT_TAG v3.7.1)`, `bw_tests` linking `bw_sim` and `Catch2::Catch2WithMain`, `catch_discover_tests(bw_tests)`.
- `bench/CMakeLists.txt`: `bw_kernel_bench` linking `bw_sim`.
- `CMakePresets.json` (Ninja): `debug` → `build/debug`, `CMAKE_BUILD_TYPE=Debug`, flags `-fsanitize=address,undefined -fno-omit-frame-pointer`; `release` → `build/release`, `Release`. Matching build and test presets.

Gates used by the plan:

```
Build: cmake --preset debug && cmake --build --preset debug
Test:  ctest --preset debug
Bench: cmake --preset release && cmake --build --preset release && ./build/release/bench/bw_kernel_bench
```

## 11. Dependency Diagram

```
time, ids, check, rng
        ↓
      logic
        ↓
  event_queue    component ← primitives
        ↓            ↓
     circuit ← ← ← ←─┘
        ↓
    recorder
        ↓
    simulator   (defines EvalContext / PowerOnContext members)
        ↓
       vcd
tests, bench → bw_sim
```

`component.hpp` only forward-declares `Simulator`; context members are defined in `simulator.cpp`.

## 12. Benchmark (S7)

`bench/kernel_bench.cpp`, argument `--seconds N` (default 1):

- Nets `VCC` (tied supply `1`), `CLK`, `Q0..Q15`, `C1..C15` (carries), `N0..N15` (next values).
- `ClockSource` at 69'841 ps (14.318 MHz). 16 `DFlipFlop` (clock-to-Q 5 ns), 16 `Xor` (2 ns): `N_i = Q_i ⊕ C_i` with `C_0 = VCC`; 15 `And` (1 ns): `C_{i+1} = C_i · Q_i`. Worst path 5 + 15 + 2 = 22 ns < one period, so it counts correctly.
- Runs `powerOn(1)` then `runFor(N s)`; prints wall time, simulated/wall ratio, `appliedEvents/s`, `evaluations/s`, and the counter value as a sanity check.
- Exit code 1 if applied events per second < 20 M.

## 13. Testing

Catch2, `tests/sim/`, tag `[sim]`. Circuits are built inline in each test with `Stimulus` inputs and fixed seeds.

| File | Covers |
|---|---|
| `logic_test.cpp` | `DriveOf` for each `PinKind`; every row of the S2 table; weak loses to strong; supply beats strong; supply contention → X; open-collector wired-AND with weak pull-up |
| `event_queue_test.cpp` | pops in time order; all events of one timestamp popped before later ones; `clear()` |
| `rng_test.cpp` | fixed seed → fixed first values (platform independence) |
| `circuit_test.cpp` | `Check` on: pin connected twice, unknown pin name, edit after `finalize`; unconnected pins get private Nets; readers deduplicated; `netHasTtlInput` |
| `simulator_test.cpp` | inertial: pulse < delay swallowed, pulse > delay passes; pending cancel (0, pending 1, input back → nothing, one stale event); rise ≠ fall delay; simultaneous changes → AND of A and ¬A never pulses; `readBefore` at the same timestamp; floating TTL Net reads 1 with flag; MOS-only bus keeps last driven value after tri-state release; TTL contention reads 0; mixed-family contention reads X; power-on: no X anywhere; same seed → identical recordings, different seed → different initial state |
| `primitives_test.cpp` | gate truth tables including X; tri-state enable/disable; DFF captures on rising edge only; clock period and duty; Stimulus steps at exact times; 3-inverter ring oscillator period = 2 × 3 × delay; 4-bit ripple counter (Q_N → D) counts |
| `recorder_test.cpp` | samples start at `record()` with the current value; only changes recorded; cleared by `powerOn` |
| `vcd_test.cpp` | exact expected text for a two-Net circuit (header, `$var`, `#time` blocks, `x`) |

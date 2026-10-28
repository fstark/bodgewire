# Board Probe — Detailed Design

Implements the specification in [BOARD_PROBE.md](BOARD_PROBE.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/sim/                 (changed) Sample carries the floating flag; floating-only changes recorded
src/instruments/         (changed) ProbeState: the P2d lamp model fed with Samples; LogicProbe uses it
src/runner/
  probe_consumer.hpp/.cpp  ProbeConsumer: the Logic Probe on the devices thread, retargetable
src/machine/             (changed) island naming shared with ElectricalNetlist; Machine::probeNet, padPosition
src/ui/
  board_view.hpp/.cpp    (changed) left click reported, probe tip drawn
  main.cpp               (changed) ProbeConsumer, lamps row, --probe
tests/runner/probe_consumer_test.cpp, tests/machines/board_probe_test.cpp
```

Everything S5 checks headlessly lives in `bw_sim`, `bw_instruments`, `bw_runner` and `bw_machine`. The UI only turns a click into `Machine::probeNet` + `ProbeConsumer::touch`, and draws.

## 2. Public Interface

```cpp
// src/sim/recorder.hpp
struct Sample {
    Time time;
    LogicValue value;
    bool floating = false;  // no Driver: instruments only (logic is still `value`)
    friend bool operator==(const Sample&, const Sample&) = default;
};
class Recorder {
    void start(NetId net, Sample initial);  // no-op if already recording
    void add(NetId net, Sample sample);     // only called for recorded Nets
    // rest unchanged
};
```

```cpp
// src/instruments/logic_probe.hpp — LogicProbe keeps its interface; new pure core:
// The P2d lamp model over one Net's Samples: the first Sample is the initial state.
class ProbeState {
public:
    static constexpr Time kPulseStretch = 100'000'000'000;  // ps, 100 ms (from LogicProbe)

    void reset();                     // no Sample yet: all lamps dark
    void feed(Sample sample);         // in time order
    ProbeLamps lamps(Time now) const;

private:
    bool started_ = false;
    LogicValue value_ = LogicValue::X;
    bool floating_ = true;
    Time lastChange_ = -1;  // last value change after the first Sample; -1 = none
};
```

```cpp
// src/runner/probe_consumer.hpp
// The Logic Probe as a Consumer. touch() and lamps(): UI thread; Consumer overrides: devices thread.
class ProbeConsumer final : public Consumer {
public:
    // Watches `net` (nullopt: lifted), unwatches the previous one. Same Net: no-op.
    void touch(Runner& runner, std::optional<NetId> net);
    std::optional<NetId> touched() const { return touched_; }
    ProbeLamps lamps() const;  // latest completed slice, for the touched Net; dark otherwise

    std::span<const NetId> nets() const override;  // the touched Net, or none
    void powerOn() override;
    void sample(NetId net, Sample sample) override;
    void advanceTo(Time time) override;
    void powerOff() override;
};
```

```cpp
// src/machine/electrical.hpp — added
// Electrical Net name of each island of `board`, as ElectricalNetlist names it; "" for an island
// no Pin names (bare Copper, a left-out resistor's pad).
std::vector<std::string> IslandNetNames(const LoadedCircuit& designCircuit, const Board& board,
                                        const Islands& islands);

// src/machine/machine.hpp — Machine gains
// Electrical Net under `at` in `built` (this Machine's build()), in the current Copper: a pad or via
// on either Layer, else a Trace on `layer`. nullopt off Copper or on an unnamed island.
std::optional<NetId> probeNet(const LoadedCircuit& built, Layer layer, Point at) const;
// Position of a Pin's pad: "U1", "QA" or "14"; nullopt if unknown.
std::optional<Point> padPosition(std::string_view reference, std::string_view pin) const;
```

```cpp
// src/ui/board_view.hpp — BoardView gains
struct BoardClick {
    Point at;      // nm
    Layer layer;   // Active Layer at the click
    bool onCopper; // a pad, via or Trace of the Active Layer (S1)
};
std::optional<BoardClick> takeClick();          // left click since the last call
void setProbeTip(std::optional<Point> tip);     // drawn above everything
```

## 3. Integration Points

1. **`Simulator::record`** — [src/sim/simulator.cpp](../../src/sim/simulator.cpp): `recorder_.start(net, {now_, value(net), isFloating(net)})`.
2. **`Simulator::runUntil`**, dirty-Net loop — same file: remember `wasFloating = s.floating` before `s.floating = r.floating`; in the existing `r.value != s.value` branch record `{now_, r.value, r.floating}`; add `else if (r.floating != wasFloating && recorder_.isRecording(net))` recording the same. Cost: one compare per resolved Net whose value did not change; measured with instructions retired (P1d method), expected < 0.5 %.
3. **`Recorder`** — [src/sim/recorder.cpp](../../src/sim/recorder.cpp): `start`/`add` take a `Sample`. Other readers (`Tv`, `TvInput`, VCD) use `time`/`value` only; a floating-only change reaches them as a repeated value, which `Tv` and VCD accept (no Net in the committed Machines changes floating while running: all drivers are totem-pole).
4. **`LogicProbe`** — [src/instruments/logic_probe.cpp](../../src/instruments/logic_probe.cpp): `update()` feeds new Samples to a `ProbeState`; `lamps()` returns `state_.lamps(sim_.now())`. Output of `bw run --probe` unchanged.
5. **`ElectricalNetlist`** — [src/machine/electrical.cpp](../../src/machine/electrical.cpp): the naming pass (POWER preset, then non-resistor, non-power Pins in connection order) moves to a local `NameIslands(designCircuit, board, islands) → Namer`; `ElectricalNetlist` and `IslandNetNames` both call it, so names cannot drift.
6. **`Dispatcher`** — unchanged: it already calls `consumer->nets()` per Sample (5.2 relies on this).
7. **`bw_ui`** — [src/ui/main.cpp](../../src/ui/main.cpp), `RunWindow`: a `ProbeConsumer probe` joins the Runner's consumers (only with a Machine); each frame `boardView.takeClick()` → `probe.touch(runner, onCopper ? machine.probeNet(*circuit, layer, at) : nullopt)` and `setProbeTip(onCopper ? at : nullopt)`; the lamps row (5.5) after the TV. `--probe REF.PIN`: after loading, `padPosition` → touch before `powerOn`; unknown Pin or no Board → usage message, exit 2.
8. **CMake** — `bw_runner` links `bw_instruments`; `probe_consumer.cpp` added; tests added.

The sim thread and run loop are untouched apart from 1–2; S4 holds because the probe adds one ring like the TV's two.

## 4. Internal State

```cpp
class ProbeConsumer {
    // UI thread
    std::optional<NetId> touched_;
    // shared
    std::atomic<std::int64_t> target_{-1};       // touched Net id, -1 = lifted
    std::atomic<std::uint64_t> published_{0};    // net id << 32 | kValid | lamp bits
    // devices thread
    mutable std::array<NetId, 1> nets_{};        // nets() view of target_
    std::int64_t fedNet_ = -1;                   // Net the state_ belongs to
    ProbeState state_;
};
```

`published_` packs the Net with the lamps so the UI never shows the old Net's lamps after a move (5.2).

## 5. Key Algorithms

### 5.1 ProbeState (P2d S5 model)

```
reset:  started = false, lastChange = -1
feed(s): if started and s.value != value: lastChange = s.time
         value = s.value; floating = s.floating; started = true
lamps(now): high  = started and !floating and value == One
            low   = started and !floating and value == Zero
            pulse = lastChange >= 0 and now − lastChange < kPulseStretch (100 ms)
```

A floating-only Sample never lights PULSE. Crack on a driven input: the island has no Driver, so `floating` from the first Sample on → neither HIGH nor LOW.

### 5.2 Moving the probe (S1)

```
touch(runner, net):                                   UI thread
  if net == touched: return
  target = net or −1                                  (atomic, release)
  if net: runner.watch(net)                           counted: VIDEO stays watched for the TV
  if touched: runner.unwatch(touched)
  touched = net

nets():           t = target (acquire); t < 0 → {} ; else nets_[0] = t, {nets_[0]}
sample(net, s):   if net id != fedNet: state.reset(), fedNet = net id
                  state.feed(s)
advanceTo(time):  t = target; if t != fedNet: state.reset(), fedNet = t
                  published = t < 0 ? 0 : (t << 32 | kValid | bits(state.lamps(time)))
powerOn/powerOff: state.reset(); powerOff also publishes 0
lamps():          p = published; valid and p >> 32 == touched → bits, else dark
```

Samples of the old Net still staged after the move are filtered by the Dispatcher (`nets()` no longer lists it). A newly watched Net's first Sample is its state at the watch (`Simulator::record`), so lamps light within one slice. A Net already watched by another Consumer (VIDEO, SYNC_N) gets no initial Sample: its lamps light at its next change, < 64 µs for those two.

Known limit, accepted: a ring slot is freed only once the devices thread has handled the Unwatch marker, so touching more than `kMaxWatched` (16) different Nets before the devices thread catches up trips the Runner's "too many watched nets" Check. Out of reach at UI frame rates; revisit if instruments multiply (P3).

### 5.3 probeNet (S1)

```
islands = FindIslands(board_)                          current Copper, cracks included
item: first pad with Covers, else via, else Trace on `layer` with Covers   (BoardScene's order)
none → nullopt
name = IslandNetNames(*designCircuit_, board_, islands)[island of item]
name empty → nullopt;  else built.nets.find(name) → NetId or nullopt
```

One `FindIslands` per click (< 1 ms on the test-pattern Board). `padPosition`: component by reference; `FindPin` on its design Component, else the pin text as a pad number (resistors); pad by number → position.

### 5.4 Click and tip (BoardView)

```
input(): if hovered and IsMouseClicked(Left):
           hit = scene.hitTest(at, active)
           click = {at, active, hit and hit.kind != Component}
draw():  after courtyards, if tip: filled circle r = max(4 px, 0.4 mm) kProbeRed, 1 px white ring
```

Left click only probes (pan is right/middle drag). The tip is a board point, so it stays put across power cycles, zoom and pan.

### 5.5 Lamps row (S2)

```
right column, after the TV:  "Probe"  ● HIGH  ● LOW  ● PULSE
lit: HIGH red, LOW green, PULSE amber; dark: grey. All dark when status.state == Off.
```

No Net name is shown: an electrical name like `H0~2` would leak the crack (P2f Decisions).

## 6. Reused Infrastructure

- `Runner::watch`/`unwatch` (counted), `Dispatcher` fan-out by `nets()`, `Consumer`: the probe is one more Consumer, no Runner change.
- P2d `LogicProbe` model and `kPulseStretch`, `ProbeLamps`, `FormatLamps`: moved into `ProbeState`, shared by `bw run --probe` and the UI.
- `FindIslands`, `Covers` (P2f), the `ElectricalNetlist` naming (P2d): the probe reads exactly the Nets `build()` makes.
- `FindPin`: `--probe` Pin syntax, as `bw run --probe`.
- P2f `BoardScene::hitTest`: what counts as Copper under the click.

## 7. Build Integration

```cmake
# src/runner/CMakeLists.txt
add_library(bw_runner STATIC dispatcher.cpp probe_consumer.cpp runner.cpp tv_consumer.cpp)
target_link_libraries(bw_runner PUBLIC bw_devices bw_instruments PRIVATE bw_warnings)
# tests/CMakeLists.txt: runner/probe_consumer_test.cpp, machines/board_probe_test.cpp
```

## 8. Dependency Diagram

```
ui ──► view ──► board ──► netlist ──► parts ──► sim
 ├───► machine ──► board
 └───► runner ──► devices ──► sim
          └────► instruments ──► sim
```

## 9. Testing

- **recorder/simulator tests**: a Net whose only Driver goes to Z records a Sample with `floating = true` and the same value; recording an undriven Net starts with `floating = true`.
- **logic_probe_test**: `ProbeState` — first Sample lights HIGH/LOW, no PULSE; a value change lights PULSE for 100 ms; a floating-only change does not; floating → dark. Existing `LogicProbe` cases still pass.
- **probe_consumer_test** (`[runner]`, test pattern netlist, `Speed::Max`, `pauseAt` + `waitIdle`): touch CLK → HIGH or LOW plus PULSE; move to a steady Net → old Net unwatched (lamps follow the new Net, no stale PULSE after 100 ms); lift → dark; touch VIDEO then move away → the TV frame still equals the reference; power off → dark; touching more Nets in turn than `kMaxWatched` (16) works (a leaked watch would trip the Runner's "too many watched nets" Check).
- **board_probe_test** (`[machines]`, through the Runner): each Puzzle at 300 ms: `dead_counter` U3.QA pad → `LOW`; `stuck_gate` U12.4Y pad → `HIGH`; `broken_trace` past the crack (U13.2B pad) → `-`, U1.QA pad → `… PULSE`.
- **machine_test**: `broken_trace`: `probeNet` at a Trace point on each side of `kH0Crack` → two different NetIds; at U1 pin 14's pad and at a via → the NetId of `PinNet` of a Pin on that island; `padPosition("U1", "QA")` == `padPosition("U1", "14")`; unknown → nullopt.
- **electrical_test**: `IslandNetNames` agrees with the net names of `ElectricalNetlist` for every pad of `broken_trace`.
- **Agent, screenshot**: `bw_ui puzzles/<p>.bwp --probe <pin> --screenshot /tmp/bw-<p>.png` for the three Puzzles; the agent views tip and lamps.
- **Manual**: Done-when of BOARD_PROBE.md; ≈ 1.00× at `1×` probing U1.CLK (S4).

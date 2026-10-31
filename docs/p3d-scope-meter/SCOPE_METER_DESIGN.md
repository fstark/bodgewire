# Oscilloscope and Multimeter — Detailed Design (P3d)

Implements the specification in [SCOPE_METER.md](SCOPE_METER.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/parts/connector.hpp / .cpp     ConnectorVolts(): declared voltage of a connector pin
src/netlist/builder.hpp / .cpp     RailVolts(): voltage of a Net on a POWER connector pin
src/machine/machine.hpp / .cpp     islandAt() (factored out of probeNet), for continuity
src/view/board_scene.hpp / .cpp    LeadKind, AcceptsLead(), LeadLabel()
src/instruments/
  volts.hpp                        NEW: logic-state voltages (header only)
  multimeter.hpp / .cpp            NEW: MeterState (200 ms average) + headless Multimeter
  oscilloscope.hpp / .cpp          NEW: ScopeState (Sweeps) + headless Oscilloscope
src/runner/
  meter_consumer.hpp / .cpp        NEW: MeterState as a Consumer (retargetable, as ProbeConsumer)
  scope_consumer.hpp / .cpp        NEW: ScopeState as a Consumer (config handoff, as AnalyzerConsumer)
src/session/session.hpp / .cpp    PadLead (was Clip), Hooks, scope settings, Test Leads, meter mode
src/ui/
  time_text.hpp / .cpp             NEW: FormatTime() moved out of analyzer_window.cpp
  scope_window.hpp / .cpp          NEW: the `Oscilloscope` window
  meter_window.hpp / .cpp          NEW: the `Multimeter` window
  board_view.hpp / .cpp            Tool::Hook (O), Tool::Meter (V), takePicked(), Hook and Lead tags
  toolbar.hpp / .cpp               Hook/Meter buttons, refusal reasons, ToolButton() public,
                                   `Analyzer (A)` removed
  analyzer_window.hpp / .cpp       tool button in the header
  main.cpp                         clicks, tool↔window pairing, --hook/--timebase/--meter
tests/instruments/multimeter_test.cpp, oscilloscope_test.cpp   NEW
tests/runner/meter_consumer_test.cpp, scope_consumer_test.cpp  NEW
tests/machines/scope_meter_test.cpp                            NEW: on built Puzzles
```

Same split as the Logic Probe and Logic Analyzer: pure engine and headless wrapper in `bw_instruments`, Consumer in `bw_runner`, bench state in `Session`, drawing in `bw_ui`.

## 2. Public Interface

### 2.1 Voltages (`src/instruments/volts.hpp`, spec Decisions)

```cpp
inline constexpr double kLowVolts = 0.2, kHighVolts = 3.5, kFloatingVolts = 1.4,
                        kUnknownVolts = 1.0;

// Voltage of a logic Net (not a rail); power off is the caller's 0 V.
constexpr double LogicVolts(Sample sample) {
    if (sample.floating) return kFloatingVolts;
    switch (sample.value) {
    case LogicValue::Zero: return kLowVolts;
    case LogicValue::One: return kHighVolts;
    default: return kUnknownVolts;
    }
}
```

Rails (`src/parts/connector.hpp`, `src/netlist/builder.hpp`):

```cpp
// Declared voltage of a connector pin that supplies the Board: POWER VCC 5, GND 0; else nullopt.
std::optional<double> ConnectorVolts(std::string_view part, std::string_view pin);

// Voltage of `net` if a connector pin with a declared voltage is on it, else nullopt (logic Net).
std::optional<double> RailVolts(const LoadedCircuit& loaded, NetId net);
```

`ConnectorVolts` is a table next to `ConnectorPins`, so a later `+12 V` connector pin is one line there. `RailVolts` walks `loaded.parts` with `info->kind == PartKind::Connector`, each pin of `ConnectorPins(info->name)` with a voltage, and compares `PinNet(loaded, reference, pin)` with `net`.

### 2.2 Multimeter engine (`src/instruments/multimeter.hpp`)

```cpp
inline constexpr Time kMeterWindow = 200'000'000'000;   // ps averaged per reading
inline constexpr Time kMeterRefresh = 250'000'000'000;  // ps between readings

// Time-weighted average of LogicVolts over the last kMeterWindow, refreshed at multiples of
// kMeterRefresh. The first Sample starts the averaging (window clipped to it).
class MeterState {
public:
    void reset();              // no Sample yet: no reading
    void feed(Sample sample);  // in time order
    void advanceTo(Time end);
    std::optional<double> reading() const { return reading_; }  // last refresh

private:
    void integrateTo(Time time);
    void refresh();

    bool started_ = false;
    double volts_ = 0;        // current LogicVolts
    Time integrated_ = 0;     // area_ covers [windowStart_, integrated_)
    Time windowStart_ = 0;
    Time next_ = 0;           // next refresh
    double area_ = 0;         // V·ps
    std::optional<double> reading_;
};

// Headless Multimeter (V) on one Net (tests). Construct after powerOn().
class Multimeter {
public:
    Multimeter(Simulator& sim, NetId net);
    void update();  // as LogicProbe::update
    std::optional<double> reading() const { return state_.reading(); }

private:
    Simulator& sim_;
    NetId net_;
    Time fedUntil_ = -1;
    MeterState state_;
};
```

### 2.3 Oscilloscope engine (`src/instruments/oscilloscope.hpp`)

```cpp
inline constexpr std::size_t kScopeChannels = 2;
inline constexpr Time kDivisions = 10;
// ps per division: 5 ns … 10 ms in 1-2-5 steps.
inline constexpr std::array<Time, 20> kTimebases{
    5'000,         10'000,        20'000,        50'000,        100'000,
    200'000,       500'000,       1'000'000,     2'000'000,     5'000'000,
    10'000'000,    20'000'000,    50'000'000,    100'000'000,   200'000'000,
    500'000'000,   1'000'000'000, 2'000'000'000, 5'000'000'000, 10'000'000'000};
inline constexpr Time kAutoWait = 50'000'000'000;  // ps; auto waits max(this, one Sweep)

enum class ScopeEdge : std::uint8_t { Rise, Fall };
enum class ScopeMode : std::uint8_t { Auto, Normal };
enum class SweepKind : std::uint8_t { None, Triggered, Auto };  // None: no Sweep yet

struct ScopeSettings {
    Time timebase = 1'000'000;  // ps per division: 1 µs
    std::size_t source = 0;     // Trigger Channel
    ScopeEdge edge = ScopeEdge::Rise;
    ScopeMode mode = ScopeMode::Auto;
    bool operator==(const ScopeSettings&) const = default;
};

struct Sweep {
    std::uint64_t serial = 0;  // 0: none yet
    SweepKind kind = SweepKind::None;
    Time timebase = 0;
    Time start = 0;    // ps since power-on of the left edge; Trigger at start + timebase
    // Per Channel: the value at `start` (time = start), then every change in
    // (start, start + 10 divisions]. Empty: no Hook.
    std::array<std::vector<Sample>, kScopeChannels> channels;
};

// Sweep engine over Channel changes in time order. No Simulator, no threads.
class ScopeState {
public:
    void powerOn();  // Channels X, Waiting from 0; the last Sweep is kept
    void configure(const ScopeSettings& settings, Time now);  // drops a Sweep in progress
    void clear(std::size_t channel, Time now);  // new Net on the Channel: X until its Sample
    void change(std::size_t channel, Sample sample);  // may trigger
    void seed(std::size_t channel, Sample sample);    // current value, never triggers
    void advanceTo(Time end);  // completes Sweeps ending <= end, publishes the last one
    const Sweep& sweep() const { return sweep_; }
    SweepKind status() const { return status_; }  // kind of the last Sweep since configure/powerOn

private:
    struct Pending {          // last completed Sweep, materialized by advanceTo
        Time start;
        SweepKind kind;
    };

    void catchUp(Time time, bool inclusive);  // finish / auto-start Sweeps before `time`
    void startSweep(Time trigger, SweepKind kind);
    void materialize();       // history_ → sweep_
    void prune(Time keepFrom);

    ScopeSettings settings_;
    std::array<std::vector<Sample>, kScopeChannels> history_;  // see §4.2
    std::array<LogicValue, kScopeChannels> last_{LogicValue::X, LogicValue::X};
    Time now_ = 0;            // latest change or advanceTo
    bool sweeping_ = false;
    Time start_ = 0, end_ = 0;  // Sweep in progress: [start_, end_]
    SweepKind kind_ = SweepKind::None;
    Time waitFrom_ = 0;       // Waiting: Triggers only after this
    std::optional<Pending> pending_;
    SweepKind status_ = SweepKind::None;
    Sweep sweep_;
};

// Headless Oscilloscope (tests). Construct after powerOn().
class Oscilloscope {
public:
    Oscilloscope(Simulator& sim, std::array<std::optional<NetId>, kScopeChannels> channels,
                 const ScopeSettings& settings);
    void update();  // feeds changes since the last update in time order, then advanceTo(now)
    const ScopeState& state() const { return state_; }

private:
    Simulator& sim_;
    std::array<std::optional<NetId>, kScopeChannels> channels_;
    Time fedUntil_ = -1;
    ScopeState state_;
};

std::optional<std::size_t> TimebaseIndex(Time timebase);  // index in kTimebases
```

### 2.4 Consumers (`src/runner/meter_consumer.hpp`, `scope_consumer.hpp`)

```cpp
// MeterState as a Consumer. touch(), reading(): UI thread; overrides: devices thread.
class MeterConsumer final : public Consumer {
public:
    void touch(Runner& runner, std::optional<NetId> net);  // as ProbeConsumer::touch
    std::optional<NetId> touched() const { return touched_; }
    std::optional<double> reading() const;  // for the touched Net; nullopt before a refresh

    std::span<const NetId> nets() const override;
    void powerOn() override;
    void sample(NetId net, Sample sample) override;
    void advanceTo(Time time) override;
    void powerOff() override;

private:
    std::optional<NetId> touched_;                // UI
    std::atomic<std::int64_t> target_{-1};        // shared
    std::atomic<std::uint64_t> published_{0};     // (net + 1) << 32 | float bits; 0 = none
    mutable std::array<NetId, 1> nets_{};         // devices
    std::int64_t fedNet_ = -1;
    MeterState state_;
};

struct ScopeConfig {
    std::array<std::optional<NetId>, kScopeChannels> nets;  // electrical Net per Hook
    ScopeSettings settings;
};

// ScopeState as a Consumer. configure(), status(), sweeps(): UI thread; overrides: devices.
class ScopeConsumer final : public Consumer {
public:
    void configure(Runner& runner, ScopeConfig config);  // watches new Nets, unwatches old
    SweepKind status() const { return status_.load(std::memory_order_acquire); }
    TripleBuffer<Sweep>& sweeps() { return sweeps_; }

    std::span<const NetId> nets() const override;  // distinct hooked Nets
    void powerOn() override;
    void sample(NetId net, Sample sample) override;
    void seed(NetId net, Sample sample) override;
    void advanceTo(Time time) override;
    void powerOff() override;

private:
    void adoptNets() const;
    void adopt();

    std::array<std::optional<NetId>, kScopeChannels> watched_;  // UI
    mutable std::mutex mutex_;
    ScopeConfig pending_;                                       // guarded by mutex_
    std::atomic<std::uint64_t> generation_{0};
    std::atomic<SweepKind> status_{SweepKind::None};
    TripleBuffer<Sweep> sweeps_;
    mutable std::uint64_t netsGeneration_ = 0;                  // devices
    mutable std::vector<NetId> nets_;
    mutable std::vector<std::uint8_t> masks_;                   // Channels per nets_ entry
    std::uint64_t configGeneration_ = 0;
    ScopeConfig applied_;
    Time now_ = 0;
    std::uint64_t published_ = 0;
    ScopeState state_;
};
```

### 2.5 Session (`src/session/session.hpp`)

```cpp
struct PadLead {                 // a Clip or a Hook (renamed from Clip)
    std::string reference, pin;  // pin by number: "6"
    std::string label;           // "U6.6 1Y"
    Point at;                    // pad, nm
};

enum class MeterMode : std::uint8_t { Volts, Continuity };

struct TestLead {
    Point at;           // nm
    Layer layer;        // as ProbeTip
    std::string label;  // "U1.14 QA", "Trace", "via"
};

// Session, public:
    std::optional<PadLead> padLead(std::string_view reference, std::string_view pin) const;
    std::span<const std::optional<PadLead>, kChannels> clips() const;  // type renamed only

    std::optional<std::size_t> hook(std::string_view reference, std::string_view pin);
    void unhook(std::size_t channel);
    std::optional<std::size_t> hookAt(std::string_view reference, std::string_view pin) const;
    std::span<const std::optional<PadLead>, kScopeChannels> hooks() const { return hooks_; }
    std::optional<double> hookRail(std::size_t channel) const;  // RailVolts of the Hook's Net
    void setScope(const ScopeSettings& settings);
    const ScopeSettings& scopeSettings() const { return scope_.settings; }
    SweepKind scopeStatus() const { return live_->scope.status(); }
    TripleBuffer<Sweep>& sweeps() { return live_->scope.sweeps(); }  // replaced on rebuild

    void setMeterMode(MeterMode mode);
    MeterMode meterMode() const { return meterMode_; }
    bool placeLead(TestLead lead);  // false: Continuity with the power on (S1)
    void liftLeads();
    bool blackNext() const { return blackNext_; }  // Continuity: the next click places black
    const std::optional<TestLead>& red() const { return red_; }
    const std::optional<TestLead>& black() const { return black_; }
    std::optional<double> volts() const;      // Volts mode; nullopt: no red Lead or no reading yet
    std::optional<bool> continuity() const;   // nullopt: a Lead missing or powered
```

### 2.6 Board view (`src/view/board_scene.hpp`, spec Decisions "Where instruments go")

```cpp
enum class LeadKind : std::uint8_t { Held, Attached };  // probe, Test Leads / Clips, Hooks
bool AcceptsLead(LeadKind kind, HitKind hit);  // Held: Pad, Via, Trace; Attached: Pad
std::string LeadLabel(const BoardScene& scene, const Hit& hit);  // "U1.14 QA", "Trace", "via"
```

## 3. Integration Points

### 3.1 `Machine::islandAt` (`src/machine/machine.cpp`)

The island search at the top of `probeNet` (pads, vias, then Traces on `layer`) moves into:

```cpp
// Island of the current Copper under `at`; nullopt off Copper.
std::optional<std::uint32_t> islandAt(const Islands& islands, Layer layer, Point at) const;
// Same island under both points (continuity).
bool connected(Layer layerA, Point a, Layer layerB, Point b) const;
```

`probeNet` calls `islandAt`; `connected` runs `FindIslands(board_)` once. `board_` carries mends at once, so continuity needs no rebuild (the Live is only rebuilt at power-on).

### 3.2 Session (`src/session/session.hpp/.cpp`)

- `Live` gains `MeterConsumer meter;` and `ScopeConsumer scope;` before `runner`; `makeLive` pushes both after the analyzer when `machine_`. Watches: TV 2 + probe 1 + Clips 8 + Hooks 2 + meter 1 = 14 ≤ `kMaxWatched` (16), one more while a lead moves.
- Members: `std::array<std::optional<PadLead>, kScopeChannels> hooks_;`, `ScopeConfig scope_;`, `MeterMode meterMode_ = MeterMode::Volts;`, `std::optional<TestLead> red_, black_;`, `bool blackNext_ = false;`, `std::optional<bool> connected_;` (cache), `std::optional<double> meterRail_;`.
- `padLead()` is the body of today's `clip()` up to the label; `clip()` and `hook()` call it.
- `configureLive()` also fills `scope_.nets` with `PinNet` per Hook and calls `live_->scope.configure(*live_->runner, scope_)`.
- `touchLive()` also touches the meter:

```cpp
const auto meterNet = meterMode_ == MeterMode::Volts && red_
                          ? machine_->probeNet(*live_->circuit, red_->layer, red_->at)
                          : std::nullopt;
live_->meter.touch(*live_->runner, meterNet);
meterRail_ = meterNet ? RailVolts(*live_->circuit, *meterNet) : std::nullopt;
```

- `placeLead`: Volts → `red_ = lead`; Continuity → `false` if `powered_`, else `red_` or `black_` by `blackNext_`, then toggle. Both then `touchLive()` and recompute `connected_`. `liftLeads`, `setMeterMode`, and every `Done` Repair also recompute `connected_` (`machine_->connected(...)` when both Leads, else nullopt).
- Readings (spec Decisions, S3):

```cpp
std::optional<double> Session::volts() const {
    if (meterMode_ != MeterMode::Volts || !red_) return std::nullopt;
    if (!powered_ || !live_->meter.touched()) return 0.0;  // power off; bare Copper
    if (meterRail_) return meterRail_;
    return live_->meter.reading();
}
std::optional<bool> Session::continuity() const {
    return meterMode_ == MeterMode::Continuity && !powered_ ? connected_ : std::nullopt;
}
```

Cost: none per event outside the devices thread; one configure/touch per user action.

### 3.3 Board view (`src/ui/board_view.hpp/.cpp`)

- `enum class Tool : std::uint8_t { Probe, Swap, Mend, Clip, Hook, Meter };`. `keys()`: `O` → Hook, `V` → Meter; every tool key goes through `setTool`.
- `void setTool(Tool tool) { tool_ = tool; picked_ = tool; }`, `std::optional<Tool> takePicked();` — a pick of the same tool still counts (reopens a closed window).
- `kHookColours{IM_COL32(255, 230, 0, 255), IM_COL32(0, 230, 255, 255)}` (yellow, cyan).
- `struct HookTag { Point at; std::size_t channel; }; void setHooks(std::span<const HookTag>);` drawn as a filled circle up-left of the pad with the digit, so it differs from the square Clip tags (up-right).
- `struct LeadTag { Point at; bool red; }; void setLeads(std::span<const LeadTag>);` drawn last: a ring at the point, a short line up-left to a rounded tag, red with `+` or black (white outline) with `-`.

### 3.4 Toolbar (`src/ui/toolbar.hpp/.cpp`, spec S1)

- `bool ToolButton(const char* label, bool selected);` moves out of the anonymous namespace (declared in `toolbar.hpp`), for the window headers.
- `void refused(std::string reason = {});` — amber flash as today; a non-empty reason replaces the action text, in amber, for 2 s.
- Buttons `Hook (O)` and `Meter (V)` after `Clip (C)`. The `Analyzer (A)` button and key go. Members: `bool analyzerOpen, scopeOpen, meterOpen`.
- Action text: `Hook: click a pin` / amber `Hook: both channels used`; Meter, Volts: `Meter: click a point (red lead)`; Continuity: amber `Meter: power off for continuity` when powered, else `Meter: click the red lead` / `… black lead` (`Session::blackNext()` getter).

### 3.5 `main.cpp`

Options: `std::vector<std::string> hook;  // REF.PIN`, `std::optional<Time> timebase;`, `std::optional<std::string> meter;`. `--timebase` through `ParseDuration`, must satisfy `TimebaseIndex`. In `main`, after the Clips: each `--hook` → `session.hook` (else `unknown pin`, more than 2 → usage); `session.setScope({.timebase = ...})`; `--meter` → `session.padLead` → `session.placeLead({lead->at, Layer::Front, lead->label})`. `RunWindow`: `scopeOpen = !options.hook.empty()`, `meterOpen = options.meter.has_value()`.

Click switch, attached leads (spec S1):

```cpp
case Tool::Clip:
case Tool::Hook: {
    const bool isClip = boardView.tool() == Tool::Clip;
    const char* name = isClip ? "Clip" : "Hook";
    if (!pick || pick->hit.kind == HitKind::Component) break;
    if (!AcceptsLead(LeadKind::Attached, pick->hit.kind)) {
        toolbar.refused(std::format("{}: attaches to pins only", name));
        break;
    }
    // pad: as today's Clip case, with hookAt/unhook/hook for a Hook;
    // full → toolbar.refused(isClip ? "Clip: all 8 channels used" : "Hook: both channels used")
    break;
}
case Tool::Meter:
    if (pick && AcceptsLead(LeadKind::Held, pick->hit.kind) &&
        !session.placeLead({pick->contact, pick->layer, LeadLabel(*boardView.scene(), pick->hit)})) {
        toolbar.refused("Meter: power off for continuity");
    }
    break;
```

Escape: `Tool::Meter` → `session.liftLeads()`, any other tool → `session.touch(std::nullopt)` (today).

Pairing, after the windows are drawn each frame:

```cpp
if (const auto picked = boardView.takePicked()) {
    toolbar.analyzerOpen |= *picked == Tool::Clip;
    toolbar.scopeOpen |= *picked == Tool::Hook;
    toolbar.meterOpen |= *picked == Tool::Meter;
}
```

Tags each frame next to `setClips`: Hooks from `session.hooks()`; Leads: red if set, black if set and Continuity. Windows when `session.machine()`: `analyzerWindow.draw(toolbar.analyzerOpen, session, boardView)`, `scopeWindow.draw(toolbar.scopeOpen, session, boardView)`, `meterWindow.draw(toolbar.meterOpen, session, boardView)`.

### 3.6 Analyzer window

`draw(bool& open, Session&, BoardView&)`; the header starts with `ToolButton("Clip (C)", board.tool() == Tool::Clip)` → `board.setTool(Tool::Clip)`. `FormatTime` moves to `ui/time_text.hpp` (same body).

## 4. Internal State

### 4.1 Meter averaging

`next_` is the first multiple of `kMeterRefresh` after the first Sample's time `t0`; `windowStart_ = max(next_ - kMeterWindow, t0)`. A Lead moved mid-run reads at the next refresh (≤ 250 ms) over the part of the window since it touched. After each refresh `windowStart_ = next_ - kMeterWindow` with the new `next_`. O(1) memory.

### 4.2 Scope history

`history_[c]` holds Channel `c`'s Samples with time ≥ `keepFrom`, plus the last one before it (the value at `keepFrom`). `keepFrom` = `start_` while sweeping, else `now - timebase` (one division of pre-Trigger). Completing a Sweep only records `pending_ {start, kind}`; `advanceTo` (once per 1 ms slice) materializes the last pending Sweep from `history_`, then prunes. So at 5 ns/div on a 1 MHz clock (10⁶ Sweeps/s), each Sweep costs O(1) and only one per slice is copied. Pruning erases a prefix only when it is at least half the vector (amortized O(1) per Sample).

### 4.3 Scope consumer handoff

As `AnalyzerConsumer` (P3c design §4.2): `pending_` under the mutex plus `generation_`; `nets()` adopts Nets, `sample`/`seed`/`advanceTo` adopt the config. `adopt()`: for each Channel whose Net changed `state_.clear(c, now_)`; if the settings or any Net changed `state_.configure(settings, now_)`. `seed()` covers a second Hook (or Clip, probe) on an already watched Net.

## 5. Key Algorithms

### 5.1 Meter (`MeterState`)

```
feed(s):
    if !started_: started_ = true; integrated_ = s.time
                  next_ = (s.time / kMeterRefresh + 1) * kMeterRefresh
                  windowStart_ = max(next_ - kMeterWindow, s.time)
    else: advanceTo(s.time)         // a refresh exactly at s.time uses the old value
    volts_ = LogicVolts(s)
advanceTo(end):
    if !started_: return
    while next_ <= end: refresh()
    integrateTo(end)
refresh():
    integrateTo(next_); reading_ = area_ / (next_ - windowStart_)
    next_ += kMeterRefresh; windowStart_ = next_ - kMeterWindow; area_ = 0
integrateTo(t):
    from = max(integrated_, windowStart_)
    if t > from: area_ += volts_ * (t - from)
    integrated_ = max(integrated_, t)
```

A 25 % duty Net: 0.25 × 3.5 + 0.75 × 0.2 = 1.025 V; 50 %: 1.85 V (spec S4).

### 5.2 Scope (`ScopeState`)

```
wait() = max(kAutoWait, kDivisions * timebase)
change(c, s):
    catchUp(s.time, inclusive = false)
    if !sweeping_ and c == source and s.time > waitFrom_ and edge(last_[c] → s.value):
        startSweep(s.time, Triggered)          // edge: Rise Zero→One, Fall One→Zero
    append s to history_[c]; last_[c] = s.value; now_ = s.time
seed(c, s): append {max(s.time, now_), value, floating}; last_[c] = s.value
catchUp(time, inclusive):                      // "before": end < time, or <= when inclusive
    loop:
        if sweeping_ and end_ before time: pending_ = {start_, kind_}; status_ = kind_
                                           sweeping_ = false; waitFrom_ = end_
        elif !sweeping_ and mode == Auto and waitFrom_ + wait() before time:
            startSweep(waitFrom_ + wait(), Auto)
        else: break
startSweep(t, kind): sweeping_ = true; start_ = t - timebase; end_ = start_ + 10 * timebase
advanceTo(end):
    catchUp(end, inclusive = true); now_ = end
    if pending_: materialize(); pending_.reset()
    prune(sweeping_ ? start_ : end - timebase)
materialize():
    for c with a Hook: channels[c] = {value at start (last Sample <= start, else X), time = start}
                                    + Samples in (start, start + 10 div]
    sweep_.serial++, kind, timebase, start
configure(settings, now): settings_ = …; sweeping_ = false; pending_.reset()
                          waitFrom_ = now; status_ = None
```

A Trigger before one division after power-on gives a `start` < 0; its pre-Trigger part is X. Edges between Sweeps are ignored (a Trigger only after `waitFrom_`), as a real scope's holdoff.

### 5.3 Scope drawing (`ScopeWindow`)

```
take a newer Sweep: session.sweeps().update() → shown_ = front(); columns dirty
screen rect: width W, two bands of height H, 10 × 5 grid (1 V per minor line), Trigger mark
             at division 1; band label "1 U1.14 QA" in the Channel colour; "5 V" scale
power off: flat line at 0 V for each Hook; else for each Hook with Samples in shown_:
    columns (cached per serial, W, rail): for each Sample run [t0, t1) clipped to the screen,
        v = rail ? *rail : LogicVolts(sample); for px in [x(t0), x(t1)]: lo = min, hi = max
    draw per column: line (px, y(lo)) → (px, y(hi)), and a 1 px horizontal at y(lo) if lo == hi
hover: cursor line, "t = FormatTime(t - trigger, sign)"; click: marker; "dt = …" when set
```

`rail` is `session.hookRail(c)`, so a Hook on GND or VCC draws flat at 0 / 5 V. O(Samples + W) once per new Sweep.

### 5.4 Meter window (`MeterWindow`, spec S3)

```
header: ToolButton("Meter (V)") · radio "V" / "Continuity" → session.setMeterMode
readout (font × 3):
    V:          volts() → "%.2f V", nullopt → "- - -"
    Continuity: powered → amber "power off"; both Leads → lamp + "BEEP" (green) or "open";
                else "- - -"
lines: "red: <label>" or "red: -"; "black: GND (implied)" in V, else "black: <label>" / "-"
```

## 6. Reused Infrastructure

- `Consumer`, `Dispatcher`, counted `Runner::watch`/`unwatch`, `Rewatch` + `Consumer::seed` (P3c) — no runner change.
- `ProbeConsumer` pattern for `MeterConsumer` (atomic target, packed published value); `AnalyzerConsumer` pattern for `ScopeConsumer` (mutex + generation, `nets()`/masks adoption); `TripleBuffer` for Sweeps.
- `Machine::probeNet` for the red Lead's Net, `PinNet` for Hooks, `FindIslands` for continuity, `Machine::padPosition` + `FindPin` (via `padLead`) for Hook labels and `--meter`.
- `Session` rebuild path (`makeLive`, `touchLive`, `configureLive`): Hooks and Leads survive a Repair.
- `ConnectorPins` and `PartKind::Connector` for rails; `ParseDuration` for `--timebase`.
- `FormatTime`, the cursor/marker readout and the floating-window setup of the Analyzer window; `Toolbar::refused` flash.

## 7. Build Integration

```cmake
# src/instruments/CMakeLists.txt: + multimeter.cpp oscilloscope.cpp
# src/runner/CMakeLists.txt:      + meter_consumer.cpp scope_consumer.cpp
# src/ui/CMakeLists.txt:          + meter_window.cpp scope_window.cpp time_text.cpp in bw_ui
# tests/CMakeLists.txt:           + instruments/multimeter_test.cpp instruments/oscilloscope_test.cpp
#                                   runner/meter_consumer_test.cpp runner/scope_consumer_test.cpp
#                                   machines/scope_meter_test.cpp
```

No new library dependencies (`bw_netlist` already links `bw_parts`).

## 8. Dependency Diagram

```
bw_ui (scope_window, meter_window, analyzer_window, board_view, toolbar, main)
  ├─> bw_view (AcceptsLead, LeadLabel)
  └─> bw_session (Hooks, Test Leads) ─> bw_runner (Meter/ScopeConsumer) ─> bw_instruments
        └─> bw_machine (islandAt, connected, probeNet) ─> bw_netlist (RailVolts) ─> bw_parts
bw_instruments (volts, MeterState, ScopeState) ─> bw_sim
```

## 9. Testing

Catch2, tags `[instruments]`, `[runner]`, `[session]`, `[machines]`, `[view]`, `[netlist]`.

`tests/instruments/multimeter_test.cpp` (spec S4 voltages):

- `volts: logic states` — 0 → 0.2, 1 → 3.5, floating → 1.4, X → 1.0.
- `meter: 25 % duty reads 1.03 V` — 1 µs period, high 250 ns, fed to 1 s: `reading ≈ 1.025` (±0.001).
- `meter: no reading before the first refresh`, `meter: window starts at the first Sample` (constant 1 from 100 ms: 3.5 at 250 ms).
- `meter: test pattern CLK` — `Multimeter` on `test_pattern.bwn` `CLK` to 1 s: ≈ 1.85.

`tests/netlist/builder_test.cpp`: `rails: declared voltages` — `RailVolts` VCC 5, GND 0, `CLK` nullopt (`test_pattern.bwn`).

`tests/instruments/oscilloscope_test.cpp` (hand-fed, then `Oscilloscope`):

- `scope: sweep around the trigger` — 1 µs/div, ch0 rise at 5 µs: `start` 4 µs, ch1's Samples in (4, 14] µs, value at start.
- `scope: auto free-runs on a constant net` — no change, `advanceTo(60 ms)` at 1 µs/div: a Sweep of kind `Auto`.
- `scope: normal waits` — same, `Normal`: serial 0, status `None`.
- `scope: fall trigger, source ch2`; `scope: X before power-on is X in the sweep`.
- `scope: only the last sweep per advance is materialized` — 3 Triggers in one advance: serial +1, the third.
- `scope: configure drops a sweep in progress`.

`tests/view/board_scene_test.cpp`: `leads: attached on pads only` — `AcceptsLead` for every `LeadKind` × `HitKind`; `LeadLabel` for a pad, Trace and via.

`tests/runner/meter_consumer_test.cpp`, `scope_consumer_test.cpp` (Runner, `pauseAt`, `waitIdle`): reading through the runner equals the headless one; retarget resets; a Sweep through the runner equals the headless one; a second Hook on a watched Net gets its value (`seed`).

`tests/machines/scope_meter_test.cpp` (built `broken_trace`, spec S4):

- `scope: LS161 clock to QA` — ch1 `U1.2` (`CLK`) rise, ch2 `U1.14` (`QA`), 20 ns/div: over Sweeps, ch2's first change after the Trigger is +13 ns (rise) or +15 ns (fall); both seen.
- `continuity: crack` — `Machine::connected` between the `U1.14` and `U13.6` pads: false; after `mend` at the crack: true.

`tests/session/session_test.cpp`:

- `session: hooks` — two Hooks, third refused, labels, `hookAt`/`unhook`.
- `session: meter volts` — red on `U1.2`, run 1 s: ≈ 1.85; on the J1 `VCC` pad: 5.0; power off: 0.0; no Lead: nullopt.
- `session: continuity refused with the power on` — Continuity, powered: `placeLead` false, `continuity()` nullopt.
- `session: continuity follows a mend` — Leads on `U1.14` and `U13.6`, open; mend: beep (no power-on needed).
- `session: hooks and leads survive a repair` — after mend + power-on, Hook Net = `PinNet` in the rebuilt Circuit; meter Net = `probeNet` of the new build.

UI (S4, by the agent): `bw_ui puzzles/broken_trace.bwp --hook U1.2,U1.14 --timebase 20ns --meter U1.2 --screenshot /tmp/scope.png --after 1s`; read the PNG.

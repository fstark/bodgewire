# Logic Analyzer — Detailed Design (P3c)

Implements the specification in [LOGIC_ANALYZER.md](LOGIC_ANALYZER.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/instruments/
  logic_analyzer.hpp / .cpp     NEW: capture engine (AnalyzerState) + headless LogicAnalyzer
src/runner/
  analyzer_consumer.hpp / .cpp  NEW: AnalyzerState as a Consumer, UI-thread config handoff
src/session/
  session.hpp / .cpp            Clips, Analyzer settings, arm/disarm; analyzer in Live
src/ui/
  analyzer_window.hpp / .cpp    NEW: the floating `Logic Analyzer` window
  board_view.hpp / .cpp         Tool::Clip, key C, Clip tags, channel colours
  toolbar.hpp / .cpp            `Clip (C)` tool, `Analyzer (A)` toggle, action text
  main.cpp                      Clip clicks, --clip / --rate, window draw
tests/instruments/logic_analyzer_test.cpp   NEW
tests/runner/analyzer_consumer_test.cpp     NEW
tests/machines/logic_analyzer_test.cpp      NEW: no-leak checks on Puzzles
tests/session/session_test.cpp              Clip cases
```

The split follows the Logic Probe: a pure engine in `bw_instruments` (`ProbeState` ↔ `AnalyzerState`), a headless Simulator wrapper next to it (`LogicProbe` ↔ `LogicAnalyzer`), a Consumer in `bw_runner` (`ProbeConsumer` ↔ `AnalyzerConsumer`), the bench state in `Session`, drawing in `bw_ui`.

## 2. Public Interface

### 2.1 Engine (`src/instruments/logic_analyzer.hpp`)

```cpp
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "sim/simulator.hpp"

namespace bw {

inline constexpr std::size_t kChannels = 8;
inline constexpr std::size_t kDepth = 4096;  // sample points per Capture
// Sample periods, ps: 1 kHz … 100 MHz in 1-2-5 steps.
inline constexpr std::array<Time, 16> kAnalyzerPeriods{
    1'000'000'000, 500'000'000, 200'000'000, 100'000'000, 50'000'000, 20'000'000,
    10'000'000,    5'000'000,   2'000'000,   1'000'000,   500'000,    200'000,
    100'000,       50'000,      20'000,      10'000};

enum class TriggerCondition : std::uint8_t { Any, Low, High, Rise, Fall };
enum class TriggerPosition : std::uint8_t { Early, Middle, Late };  // 10, 50, 90 % of memory

struct AnalyzerSettings {
    Time period = 100'000;  // ps: 10 MHz
    TriggerPosition position = TriggerPosition::Early;
    std::array<TriggerCondition, kChannels> trigger{};  // all Any
    bool operator==(const AnalyzerSettings&) const = default;
};

// One sample point: Channel c is bit c; X where `unknown` has bit c set.
struct ChannelBits {
    std::uint8_t high = 0, unknown = 0xff;
    bool operator==(const ChannelBits&) const = default;
};

struct Capture {
    std::uint64_t serial = 0;         // 0: none yet; bumped per completed Capture
    Time period = 0;                  // ps between points
    Time triggerTime = 0;             // ps since power-on of points[trigger]
    std::size_t trigger = 0;          // index of the Trigger point
    std::size_t first = 0;            // points before this were not sampled (armed later)
    std::vector<ChannelBits> points;  // kDepth, oldest first
    std::array<std::string, kChannels> labels;  // "U6.6 1Y"; "" = no Clip
};

enum class AnalyzerPhase : std::uint8_t { Idle, Armed, Filling, Captured };

std::size_t PreTriggerPoints(TriggerPosition position);  // 409, 2048, 3686

// Capture engine over Channel changes in time order. No Simulator, no threads.
class AnalyzerState {
public:
    void powerOn();   // all Channels X, time 0; Armed: waits from point 0
    void powerOff();  // Filling → Armed (Capture in progress dropped)
    void arm(const AnalyzerSettings& settings, std::array<std::string, kChannels> labels,
             Time now);  // memory cleared; first point at the first multiple of period >= now
    void disarm();       // Armed/Filling → Idle; Captured stays
    void change(Time time, std::uint8_t channels, LogicValue value);  // `channels`: bit mask
    void advanceTo(Time end);  // samples every point <= end
    AnalyzerPhase phase() const { return phase_; }
    const Capture& capture() const { return capture_; }  // last completed

private:
    void emit(std::size_t count);      // `count` points of current_, trigger and memory
    bool triggers(ChannelBits now) const;  // vs previous_, per settings_.trigger
    void freeze();                     // ring → capture_, Captured

    AnalyzerPhase phase_ = AnalyzerPhase::Idle;
    AnalyzerSettings settings_;
    std::array<std::string, kChannels> labels_;
    ChannelBits current_;              // state after every change so far
    std::optional<ChannelBits> previous_;  // last emitted point; none right after arm
    Time next_ = 0;                    // ps of the next point to emit
    Time armedAt_ = 0;                 // ps of point 0 since arming
    std::vector<ChannelBits> ring_ = std::vector<ChannelBits>(kDepth);
    std::uint64_t emitted_ = 0;        // points since arming
    std::uint64_t triggerPoint_ = 0;   // emitted_ index of the Trigger point
    std::size_t remaining_ = 0;        // Filling: points still to sample
    Capture capture_;
};

// Headless Logic Analyzer on a Simulator (tests). Construct after powerOn().
class LogicAnalyzer {
public:
    LogicAnalyzer(Simulator& sim, std::array<std::optional<NetId>, kChannels> channels);
    void arm(const AnalyzerSettings& settings);  // from sim.now()
    void update();  // feeds changes recorded since the last update, in time order
    const AnalyzerState& state() const { return state_; }

private:
    Simulator& sim_;
    std::array<std::optional<NetId>, kChannels> channels_;
    Time fedUntil_ = -1;
    AnalyzerState state_;
};

std::optional<TriggerCondition> ParseTrigger(std::string_view text);  // "0" "1" "r" "f"

}  // namespace bw
```

### 2.2 Consumer (`src/runner/analyzer_consumer.hpp`)

```cpp
// Everything the UI thread decides; sent whole on every change.
struct AnalyzerConfig {
    std::array<std::optional<NetId>, kChannels> nets;  // electrical Net per Clip
    std::array<std::string, kChannels> labels;
    AnalyzerSettings settings;
    std::uint64_t arms = 0;  // bumped by each Arm
    bool armed = false;      // false after Stop
};

// AnalyzerState as a Consumer. configure(), phase(), captures(): UI thread; overrides: devices.
class AnalyzerConsumer final : public Consumer {
public:
    void configure(Runner& runner, AnalyzerConfig config);  // re-watches changed Channels only
    AnalyzerPhase phase() const { return phase_.load(std::memory_order_acquire); }
    TripleBuffer<Capture>& captures() { return captures_; }  // completed Captures

    std::span<const NetId> nets() const override;  // distinct clipped Nets
    void powerOn() override;
    void sample(NetId net, Sample sample) override;
    void seed(NetId net, Sample sample) override;  // = sample()
    void advanceTo(Time time) override;
    void powerOff() override;

private:
    void adoptNets() const;            // devices: pending Nets → nets_, masks_
    void adopt();                      // devices: pending config → state_, from now_

    // UI thread
    std::array<std::optional<NetId>, kChannels> watched_;
    AnalyzerConfig sent_;              // last configure(), for the UI-side phase
    // shared
    mutable std::mutex mutex_;
    AnalyzerConfig pending_;                   // guarded by mutex_
    std::atomic<std::uint64_t> generation_{0};  // bumped after pending_ changes
    std::atomic<AnalyzerPhase> phase_{AnalyzerPhase::Idle};
    TripleBuffer<Capture> captures_;
    // devices thread
    mutable std::uint64_t netsGeneration_ = 0;
    mutable std::vector<NetId> nets_;          // distinct
    mutable std::vector<std::uint8_t> masks_;  // Channels per nets_ entry
    std::uint64_t configGeneration_ = 0;
    Time now_ = 0;                 // last slice end; 0 after power-on
    AnalyzerConfig applied_;
    std::uint64_t published_ = 0;  // Capture serial last published
    AnalyzerState state_;
};
```

### 2.3 Session (`src/session/session.hpp`)

```cpp
struct Clip {
    std::string reference, pin;  // pin by number: "6"
    std::string label;           // "U6.6 1Y"
    Point at;                    // pad, nm
};

// Session, public:
    // Lowest free Channel; nullopt without a Machine, unknown pad, or all Channels used.
    std::optional<std::size_t> clip(std::string_view reference, std::string_view pin);
    void unclip(std::size_t channel);
    std::optional<std::size_t> clipAt(std::string_view reference, std::string_view pin) const;
    std::span<const std::optional<Clip>, kChannels> clips() const { return clips_; }
    void setAnalyzer(const AnalyzerSettings& settings);
    const AnalyzerSettings& analyzerSettings() const { return analyzer_.settings; }
    void arm();
    void disarm();
    AnalyzerPhase analyzerPhase() const { return live_->analyzer.phase(); }
    TripleBuffer<Capture>& captures() { return live_->analyzer.captures(); }  // replaced on rebuild
```

## 3. Integration Points

### 3.1 Session (`src/session/session.hpp/.cpp`)

- `Live` gains `AnalyzerConsumer analyzer;` before `runner` (destroyed after it). `makeLive` pushes `&live->analyzer` when `machine_`, after the probe.
- Members: `std::array<std::optional<Clip>, kChannels> clips_;`, `AnalyzerConfig analyzer_;` (nets filled per build).
- `configureLive()`, called by every Clip/settings/arm change and after a rebuild in `powerOn` (next to `touchLive()`):

```cpp
void Session::configureLive() {
    for (std::size_t c = 0; c < kChannels; ++c) {
        analyzer_.nets[c] = clips_[c] ? PinNet(*live_->circuit, clips_[c]->reference,
                                               clips_[c]->pin)
                                      : std::nullopt;
        analyzer_.labels[c] = clips_[c] ? clips_[c]->label : "";
    }
    live_->analyzer.configure(*live_->runner, analyzer_);
}
```

`PinNet` on the built Circuit gives the electrical Net at the pad (the same lookup `bw run --probe` uses), so no Fault leaks into labels and a crack shows. `clip()` resolves the pin through `FindPin` on `machine_->designCircuit()` (number or name → number), the label from the `PinSpec` name, `at` from `Machine::padPosition`. `arm()`: `++analyzer_.arms; analyzer_.armed = true;` then configure; `disarm()`: `armed = false`.

Rebuild: the new `AnalyzerConsumer` starts with `arms = 0`, so a still-`armed` config would re-arm it. Before replacing the Live, `powerOn` sets `analyzer_.armed = false` unless the old phase is `Armed`/`Filling` (a finished Capture is not re-armed by a Repair; the window keeps showing it).

Cost: none per event; one `configure` per user action.

### 3.2 Board view (`src/ui/board_view.hpp/.cpp`)

- `enum class Tool : std::uint8_t { Probe, Swap, Mend, Clip };`; `keys()`: `ImGuiKey_C` → `Tool::Clip`.
- `struct ClipTag { Point at; std::size_t channel; };`, `void setClips(std::span<const ClipTag> clips)`, drawn after the probe tip: a filled rounded rect in `kChannelColours[channel]` with the digit `channel + 1`, offset up-right of the pad so the pad stays visible.
- `inline constexpr std::array<ImU32, kChannels> kChannelColours` in `board_view.hpp` (shared with the window).

### 3.3 Toolbar (`src/ui/toolbar.hpp/.cpp`)

- `bool analyzerOpen = false;`, button `Analyzer (A)` after the manuals, key `A`.
- Tool button `Clip (C)` after `Mend (M)`; action text `Clip: click a pin` (`Clip: all 8 channels used` in amber when full).

### 3.4 `main.cpp`

- Options: `std::vector<std::string> clip;  // REF.PIN[:T]`, `std::optional<Time> rate;`. Parsed like `--repair` (comma list); `--rate` through `ParsePeriodFromFrequency`, must be in `kAnalyzerPeriods`.
- After `--probe` handling in `main`: each `--clip` → `session.clip(ref, pin)` (else usage error `unknown pin`), suffix through `ParseTrigger` into the settings; `session.setAnalyzer(settings)`, `session.arm()`; `toolbar.analyzerOpen = true` in `RunWindow` when `!options.clip.empty()`.
- In the `takeClick()` switch:

```cpp
case Tool::Clip:
    if (pick && pick->hit.kind == HitKind::Pad) {
        const BoardComponent& part = session.drawnBoard()->components[pick->hit.component];
        const std::string& pin = part.pads[pick->hit.index].number;
        if (const auto channel = session.clipAt(part.reference, pin)) {
            session.unclip(*channel);
        } else if (session.clip(part.reference, pin)) {
            toolbar.analyzerOpen = true;
        } else {
            result = RepairResult::Refused;  // all Channels used: amber flash
        }
    }
    break;
```

- Each frame before `boardView.draw`: `boardView.setClips(tags)` built from `session.clips()`. After `manualWindows.draw`, when `session.machine()`: `analyzerWindow.draw(toolbar.analyzerOpen, session)`.

### 3.5 Build

`src/instruments/CMakeLists.txt` + `logic_analyzer.cpp`; `src/runner/CMakeLists.txt` + `analyzer_consumer.cpp`; `src/ui/CMakeLists.txt` + `analyzer_window.cpp`; `tests/CMakeLists.txt` + the three new test files.

## 4. Internal State

### 4.1 Memory

`ring_` holds the last `kDepth` points since arming; point `emitted_` goes to `ring_[emitted_ % kDepth]`. On the Trigger, `triggerPoint_ = emitted_` and `remaining_ = kDepth - pre`; when `remaining_` reaches 0 the ring holds exactly `[triggerPoint_ - pre, triggerPoint_ + kDepth - pre)` and `freeze()` unrolls it oldest first into `capture_.points`, with `trigger = pre`, `first = pre > triggerPoint_ ? pre - triggerPoint_ : 0`, `triggerTime = armedAt_ + triggerPoint_ * period`. 8 KB per Capture.

### 4.2 Consumer handoff

`configure()` (UI): lock, `pending_ = config`, unlock, `generation_++` (release), then, for each Channel whose Net changed, `unwatch` the old Net and `watch` the new one (counted; unchanged Clips send no `Rewatch`; as `ProbeConsumer::touch`: the generation is visible before the Watch marker, so the initial Sample of a new Net is not dropped).

The devices thread adopts only while slices run, so `configure()` also stores the expected phase itself (Off and Paused show the right state): `Armed` when armed and (`arms` changed or the phase is `Armed`/`Filling`), `Idle` when disarmed from `Armed`/`Filling`. The next `advanceTo` overwrites it with the real one.

Already watched Nets: a counted `watch` of a Net that already has a slot sends no initial Sample, so a second Clip on a constant Net (or one on the probe's or TV's Net) would stay X. Runner change: a repeated watch pushes a `Rewatch` marker for the slot; the Dispatcher keeps the last dispatched Sample per slot (cleared on Watch and PowerOn) and on `Rewatch` calls the new `Consumer::seed(net, sample)` (default no-op) on each Consumer whose `nets()` include the Net. `AnalyzerConsumer::seed` is `sample()`: an older time is harmless (no point is emitted, the value is the current one).

Devices side, two adoption points:

- `nets()` (const, called by the Dispatcher before each fan-out): if `generation_ != netsGeneration_`, `adoptNets()` rebuilds `nets_`/`masks_` from `pending_` under the lock. One relaxed atomic load per call otherwise.
- `sample()` / `seed()` / `advanceTo()`: if `generation_ != configGeneration_`, `adopt()` copies `pending_` to `applied_` and updates `state_` (§5.3). Commands land between slices, so the change takes effect at the last slice end `now_`.

`advanceTo()` then stores `phase_` and, if `state_.capture().serial != published_`, copies the Capture into `captures_.back()` and publishes.

## 5. Key Algorithms

### 5.1 Sampling (`change`, `advanceTo`)

```
change(time, channels, value):
    emitUntil(time, inclusive = false)       // points strictly before the change
    for c in channels: set bit c of current_.high/unknown from value (X/Z → unknown)
advanceTo(end):
    emitUntil(end, inclusive = true)         // the Runner delivers every Sample <= slice end first
emitUntil(limit, inclusive):
    if phase_ not Armed/Filling: return     // next_ is re-based by arm()
    n = number of points next_, next_+period, … that are < limit (or <= limit)
    if n > 0: emit(n); next_ += n * period
```

### 5.2 Trigger and memory (`emit`)

```
emit(count):                                // count points, all equal to current_
    while count > 0:
        if phase_ == Armed and triggers(current_):   // only the run's first point can trigger:
            triggerPoint_ = emitted_; remaining_ = kDepth - pre; phase_ = Filling
        k = phase_ == Filling ? min(count, remaining_) : count
        write k points: only the last min(k, kDepth) touch ring_ (std::fill, at most 2 spans)
        emitted_ += k; count -= k; previous_ = current_
        if phase_ == Filling: remaining_ -= k; if remaining_ == 0: freeze(); return
        if phase_ == Armed: return                    // rest of the run cannot trigger
triggers(now):
    for c: Any → ok; Low/High → !unknown(c) && bit(c) == 0/1;
           Rise/Fall → previous_ && !unknown in both && previous bit 0→1 / 1→0
    all ok
```

Edges compare consecutive *points*, so a pulse that falls between two points neither shows nor triggers (S3, "can be missed"). Cost per change: O(1) plus at most `min(run, kDepth)` 2-byte writes while Armed/Filling; nothing while Idle/Captured. Worst case S4 (CLK at 1 MHz edges, 100 MHz rate): 2 M runs/s × 50 points ≈ 10⁸ writes/s on the devices thread, inside `std::fill`.

### 5.3 Adopting a config (devices)

```
adopt():
    previous = applied_; applied_ = pending_
    if !applied_.armed:                      state_.disarm()
    else if applied_.arms != previous.arms:  state_.arm(settings, labels, now_)   // Arm pressed
    else if phase Armed/Filling:             state_.arm(settings, labels, now_)   // S2 restart
```

`powerOn()`: `now_ = 0`, `state_.powerOn()` (Armed → arm at 0 with the applied settings); `powerOff()`: `state_.powerOff()`. `sample(net, s)`: `state_.change(s.time, masks_[index of net], s.value)`. `advanceTo(t)`: …, `now_ = t`.

### 5.4 Window drawing (`AnalyzerWindow::draw`)

```
take a newer Capture: session.captures().update() → copy front() to shown_, fit
    (not a serial compare: a rebuilt Live restarts serials at 0; shown_ survives Repairs)
header: phase text (Armed && !powered → "armed: waiting for power"), Arm/Stop, Rate combo,
        Trigger-at combo; any change → session.setAnalyzer / arm / disarm
table, one row per Channel:
    colour swatch, current label or "free" (greyed), trigger combo (any/0/1/rise/fall), "×"
    lane (stretch column): shown_ Channel c over the view range
lane drawing over [from, to) points:
    if points per pixel <= 1: one segment per run, vertical edges between runs
    else: per pixel column, all points equal → level, else → filled activity block
    unknown → grey band; index < first → nothing
axis under the lanes: ticks at 1-2-5 steps of time, labels relative to the Trigger (FormatTime)
input over lanes: wheel zooms around the cursor (16 points … kDepth), drag pans, double-click fits;
    hover: cursor line + "t = …"; click: marker; readout "dt = …" when a marker is set
no Capture yet (shown_ empty): "arm, then power on" greyed in the first lane
```

`FormatTime(Time ps)`: signed, three significant digits, `ps`/`ns`/`µs`/`ms`/`s` (`µ` is Latin-1, in the default font). ~4096 × 8 point reads per frame at most.

## 6. Reused Infrastructure

- `Consumer`, `Dispatcher` fan-out, `Runner::watch`/`unwatch` counted watches, `kMaxWatched` (16: TV 2 + probe 1 + 8 Clips, +1 while a Clip moves, fits). One runner change: `Rewatch` marker and `Consumer::seed` (§4.2).
- `TripleBuffer` (as `TvConsumer`'s pictures) for Captures to the UI.
- `Sample`, `Recorder`, `Simulator::record` — the `LogicAnalyzer` wrapper reads like `LogicProbe`.
- `PinNet` (`netlist/builder.hpp`) for electrical Nets at a pad; `FindPin`, `Machine::padPosition`, `Machine::designCircuit()` for Clip labels and tags.
- `Session` rebuild path (`makeLive` + `touchLive`) for Clips across Repairs.
- `ParsePeriodFromFrequency` (`netlist/units.hpp`) for `--rate`; `Toolbar::refused` for a full Analyzer.
- Floating window pattern of `ManualWindows` (`SetNextWindowPos/Size` `FirstUseEver`, `NoSavedSettings`).

## 7. Build Integration

```cmake
# src/instruments/CMakeLists.txt
add_library(bw_instruments STATIC
  logic_analyzer.cpp
  logic_probe.cpp
)
# src/runner/CMakeLists.txt: + analyzer_consumer.cpp
# src/ui/CMakeLists.txt: + analyzer_window.cpp in bw_ui
# tests/CMakeLists.txt: + instruments/logic_analyzer_test.cpp, machines/logic_analyzer_test.cpp,
#                         runner/analyzer_consumer_test.cpp
```

No new library dependencies.

## 8. Dependency Diagram

```
bw_ui (analyzer_window, board_view, toolbar, main)
  └─> bw_session (Clips, AnalyzerConfig) ─> bw_runner (AnalyzerConsumer) ─> bw_instruments
        └─> bw_machine (padPosition, designCircuit) ─> bw_netlist (PinNet, FindPin)
bw_instruments (AnalyzerState, LogicAnalyzer) ─> bw_sim
```

## 9. Testing

Catch2, tags `[instruments]`, `[runner]`, `[session]`, `[machines]`.

`tests/instruments/logic_analyzer_test.cpp` (`AnalyzerState` fed by hand, period 1000 ps):

- `analyzer: point reads last change at or before it` — change at 2500 → point 3000 sees it, point 2000 not; a change exactly at 3000 is in point 3000.
- `analyzer: short pulse is missed` — 0→1 at 3100, 1→0 at 3900: no point shows 1, `rise` does not trigger.
- `analyzer: X shows as X` — `unknown` bit set until the first 0/1.
- `analyzer: rise, fall, levels ANDed` — triggers on the first point where ch0 rises while ch1 is 1; not before.
- `analyzer: all any triggers at once` — trigger point = first point after arm.
- `analyzer: trigger positions` — `Early`/`Middle`/`Late`: `trigger` 409/2048/3686, `points.size() == kDepth`.
- `analyzer: early trigger leaves start empty` — trigger at point 100 with `Middle`: `first == 1948`.
- `analyzer: power-off drops filling` — Filling, `powerOff()` → Armed, `powerOn()` → captures afresh from 0.
- `analyzer: disarm keeps capture`.

Same file, `LogicAnalyzer` on `test_pattern.bwn`:

- `analyzer: LINE_END_N every 64 points` — ch0 `LINE_END_N` `Fall`, 1 MHz, `Early`: the next 1→0 on ch0 is at `trigger + 64`; ch1 on `U2.9` equals ch0 at every point.

`tests/runner/analyzer_consumer_test.cpp` (Runner + `pauseAt` + `waitIdle`, as the probe consumer tests):

- `analyzer consumer: capture through the runner` — same Capture as the headless `LogicAnalyzer` for the same Nets, settings and seed.
- `analyzer consumer: armed before power-on` — configure armed while Off, power on: Captured at 2 ms.
- `analyzer consumer: reconfigure while armed restarts` and `… new clip gets its initial value` (a constant Net clipped mid-run is 0/1, not X).
- `analyzer consumer: second clip on a watched net` — ch0 on a constant Net, run, then ch1 on the same Net: ch1 is 0/1, not X.
- `runner: rewatch seeds the last value` (`tests/runner/runner_test.cpp`): a second watch of a constant Net delivers `seed` with its value.

`tests/machines/logic_analyzer_test.cpp` (built Puzzle Circuits, `PinNet`):

- `analyzer: crack stops H0` — `broken_trace`: ch0 `U1.14` toggles in the Capture, ch1 `U13.6` constant.
- `analyzer: stuck gate` — `stuck_gate`: `U12.11` constant 1.

`tests/session/session_test.cpp`:

- `session: clips` — lowest free Channel, `clipAt`, unclip, ninth refused, label `U6.6 1Y`, pin by name `U6.1Y` → `6`.
- `session: clips survive a repair` — `broken_trace`, clip `U13.6`, mend the crack, power on: the Clip's Net is the one of `U1.14` in the rebuilt Circuit.
- `session: arm before power-on captures` — `stuck_gate`, armed while Off: `Captured` after the run.
- `session: a capture is not re-armed by a repair` — Captured, power off, mend, power on: phase `Idle`.

UI (S4, by the agent): `bw_ui puzzles/broken_trace.bwp --clip U1.14:r,U13.6,U6.6 --rate 1MHz --screenshot /tmp/la.png --after 1s`; read the PNG.

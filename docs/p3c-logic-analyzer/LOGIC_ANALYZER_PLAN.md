# Logic Analyzer — Implementation Plan (P3c)

Design: [LOGIC_ANALYZER_DESIGN.md](LOGIC_ANALYZER_DESIGN.md)
Spec: [LOGIC_ANALYZER.md](LOGIC_ANALYZER.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Capture engine `AnalyzerState` with hand-fed unit tests | |
| 2 | Headless `LogicAnalyzer` on the Simulator; test pattern and no-leak Puzzle tests | |
| 3 | `AnalyzerConsumer` through the Runner | |
| 4 | Session: Clips, settings, arm/disarm, rebuild | |
| 5 | Board: `Clip (C)` tool, Clip tags, `Analyzer (A)` toggle, `--clip`/`--rate` | |
| 6 | `Logic Analyzer` window + screenshots | |
| 7 | Release check, user check, docs | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate: `ctest --preset debug -j 10`

All C++ clang-format clean (never CMakeLists). Visual checks: `build/debug/src/ui/bw_ui FILE --screenshot /tmp/x.png --after 1s`, then view the PNG.

---

## Phase 1 — Capture engine

The whole sampling, Trigger and memory logic, with no Simulator and no threads, so it is tested exhaustively first (design §2.1, §4.1, §5.1, §5.2).

### 1.1 — Header

Create `src/instruments/logic_analyzer.hpp` with everything of design §2.1 **except** the `LogicAnalyzer` class (phase 2): `kChannels`, `kDepth`, `kAnalyzerPeriods`, `TriggerCondition`, `TriggerPosition`, `AnalyzerSettings`, `ChannelBits`, `Capture`, `AnalyzerPhase`, `PreTriggerPoints`, `AnalyzerState`, `ParseTrigger`. Includes: `<array>`, `<cstddef>`, `<cstdint>`, `<optional>`, `<string>`, `<string_view>`, `<vector>`, `"sim/logic.hpp"`, `"sim/time.hpp"`.

### 1.2 — Implementation

Create `src/instruments/logic_analyzer.cpp`:

- `PreTriggerPoints`: `kDepth * {10, 50, 90} / 100` → 409, 2048, 3686.
- `ParseTrigger`: `"0"` Low, `"1"` High, `"r"` Rise, `"f"` Fall, else nullopt.
- `AnalyzerState::powerOn()`: `current_ = {}` (all X), `previous_.reset()`; if `phase_ == Armed`: `arm(settings_, labels_, 0)`.
- `powerOff()`: `Filling` → `Armed`.
- `arm(settings, labels, now)`: store both; `next_ = armedAt_ = ceil(now / period) * period`; `emitted_ = 0`; `previous_.reset()`; ring filled with `ChannelBits{}`; `phase_ = Armed`.
- `disarm()`: `Armed`/`Filling` → `Idle`.
- `change`, `advanceTo`, `emit`, `triggers`: design §5.1 and §5.2 exactly. In `change`, `LogicValue::Zero` clears `high` and `unknown` bits, `One` sets `high` and clears `unknown`, `X`/`Z` set `unknown`. Ring writes: `k` points starting at `emitted_ % kDepth`, only the last `min(k, kDepth)`, as at most two `std::fill` spans.
- `freeze()`: `capture_.points` = ring unrolled from `(emitted_ % kDepth)` (oldest first), `period`, `trigger = pre`, `first = pre > triggerPoint_ ? pre - triggerPoint_ : 0`, `triggerTime = armedAt_ + triggerPoint_ * period`, `labels`, `++serial`; `phase_ = Captured`.

`src/instruments/CMakeLists.txt`: add `logic_analyzer.cpp` (before `logic_probe.cpp`).

### 1.3 — Tests

Create `tests/instruments/logic_analyzer_test.cpp` (tag `[instruments]`), engine cases of design §9 with `period = 1000`: `analyzer: point reads last change at or before it`, `analyzer: short pulse is missed`, `analyzer: X shows as X`, `analyzer: rise, fall, levels ANDed`, `analyzer: all any triggers at once`, `analyzer: trigger positions`, `analyzer: early trigger leaves start empty`, `analyzer: power-off drops filling`, `analyzer: disarm keeps capture`, plus `analyzer: ParseTrigger`. A local helper `Bit(const Capture&, std::size_t point, std::size_t channel)` returning `std::optional<bool>` (nullopt = X) keeps checks short. Drive time forward with `advanceTo` far enough to complete a Capture (`kDepth * period`).

`tests/CMakeLists.txt`: add `instruments/logic_analyzer_test.cpp`.

### Fence

- [ ] `AnalyzerState` complete; new `[instruments]` cases pass with the full suite
- [ ] clang-format clean
- [ ] Commit: `"logic analyzer: phase 1 — capture engine"`

---

## Phase 2 — Headless `LogicAnalyzer`

The engine on real circuits, including the no-leak checks (design §2.1 `LogicAnalyzer`, §9).

### 2.1 — `LogicAnalyzer`

Add the class to `logic_analyzer.hpp` (include `"sim/simulator.hpp"`). In the `.cpp`:

- Constructor: `sim_.record(net)` for each distinct clipped Net.
- `arm(settings)`: labels `""`; `state_.arm(settings, {}, sim_.now())`.
- `update()`: collect `(time, channel mask, value)` for Samples with `time > fedUntil_` from each distinct Net (mask = all Channels on that Net), `std::ranges::stable_sort` by time, feed `state_.change`; then `state_.advanceTo(sim_.now())`; `fedUntil_ = sim_.now()`. The first `update()` also feeds the initial Samples (time ≤ `fedUntil_ = -1` never excludes them).

### 2.2 — Tests

- `tests/instruments/logic_analyzer_test.cpp`: `analyzer: LINE_END_N every 64 points` on `test_pattern.bwn` (`LoadNetlistFile`, `Simulator sim(loaded->circuit)`, `sim.powerOn(1)`, construct, arm, `runUntil` in 1 ms steps with `update()` until `Captured`; Trigger `Fall` on ch0, 1 MHz, `Early`; the next 1→0 on ch0 after `trigger` is at `trigger + 64`; ch1 on `PinNet(*loaded, "U2", "9")` equals ch0 at every point from `first`).
- Create `tests/machines/logic_analyzer_test.cpp` (tag `[machines]`): `analyzer: crack stops H0` (`broken_trace`, `machine->build()`, nets via `PinNet(**built, "U1", "14")` and `("U13", "6")`, 1 MHz, all `Any`: ch0 has at least one change across the Capture, ch1 none) and `analyzer: stuck gate` (`stuck_gate`, `U12.11`: every point from `first` is 1).

`tests/CMakeLists.txt`: add `machines/logic_analyzer_test.cpp`.

### Fence

- [ ] Tests pass
- [ ] clang-format clean
- [ ] Commit: `"logic analyzer: phase 2 — headless analyzer on circuits"`

---

## Phase 3 — `AnalyzerConsumer`

The engine on the devices thread, configured from the UI thread (design §2.2, §4.2, §5.3).

### 3.0 — Runner: `Rewatch` and `Consumer::seed`

- `consumer.hpp`: `virtual void seed(NetId net, Sample sample) {}` ("current value of a Net already watched; may be older than Samples already fed").
- `dispatcher.hpp`: `MarkerKind::Rewatch`; `std::array<std::optional<Sample>, kMaxWatched> last_;`. `dispatcher.cpp`: `sliceEnd` stores `last_[best]` per dispatched Sample; `Watch` resets the slot's `last_`; `PowerOn` resets all; `Rewatch`: if `active_` and `last_`, `seed` on each Consumer whose `nets()` contain the slot's Net.
- `runner.cpp` `CommandKind::Watch` on an already-watched Net: `++count`, then `pushMarker({MarkerKind::Rewatch, slot, net})`.
- Test `runner: rewatch seeds the last value` in `tests/runner/runner_test.cpp`: a test Consumer on the `VCC`-tied `U1.CLR_N` Net recording `seed` calls; run 1 ms, `watch` the Net again, `waitIdle`: one `seed` with value 1.

### 3.1 — Consumer

Create `src/runner/analyzer_consumer.hpp/.cpp` as design §2.2:

- `configure(runner, config)`: lock, `pending_ = config`, unlock, `generation_.fetch_add(1, release)`; set the expected phase (design §4.2) from `sent_`, then `sent_ = config`; then `runner.watch` each Net in `config.nets` not in `watched_` (one watch per Clip, so counted per Channel), `runner.unwatch` each Net of `watched_` no longer present (per Channel); `watched_ = config.nets`. Simplest correct form: watch all new Channels' Nets first, then unwatch all old Channels' Nets.
- `nets()`: if `generation_.load(acquire) != netsGeneration_`: `adoptNets()` (lock, rebuild distinct `nets_` and `masks_` from `pending_.nets`, store generation). Return `nets_`.
- `sample(net, s)`: `adopt()` if `generation_ != configGeneration_`; find `net` in `nets_` (≤ 8, linear); `state_.change(s.time, masks_[i], s.value)`. `seed` = `sample`.
- `advanceTo(t)`: adopt if needed; `state_.advanceTo(t)`; `now_ = t`; `phase_.store(state_.phase(), release)`; publish the Capture if `serial != published_` (`captures_.back() = state_.capture(); captures_.publish();`).
- `powerOn()`/`powerOff()`: forward to `state_` (`powerOn` also `now_ = 0`), store `phase_`.
- `adopt()`: design §5.3, at `now_`.

`src/runner/CMakeLists.txt`: add `analyzer_consumer.cpp` (first).

### 3.2 — Tests

Create `tests/runner/analyzer_consumer_test.cpp` (tag `[runner]`), on `test_pattern.bwn`, `Runner runner(loaded->circuit, {&analyzer})`, `Speed::Max`, `pauseAt` before `powerOn`, `waitIdle`:

- `analyzer consumer: capture through the runner` — Nets `LINE_END_N` (`Fall`) and `CLK`, 1 MHz; armed before power-on; run to 2 ms; `captures().update()` true and `front()` equals the headless `LogicAnalyzer` Capture for the same seed (compare `points`, `trigger`, `triggerTime`).
- `analyzer consumer: armed before power-on` — `phase()` is `Armed` after `configure` while Off; `Captured` after the run.
- `analyzer consumer: reconfigure while armed restarts` — impossible Trigger (`LINE_END_N` `High` and `Low` on two Channels of the same Net), run 1 ms, reconfigure with a possible one: Captured, `triggerTime >= 1 ms`.
- `analyzer consumer: new clip gets its initial value` — run 1 ms with no Clips, then configure ch0 on `VCC`-tied `U1.CLR_N` Net (`PinNet(*loaded, "U1", "1")`) armed `Any`, run to 2 ms: every point from `first` is 1, none X.
- `analyzer consumer: second clip on a watched net` — as above with ch0 only (not armed), run 1 ms, then ch0 and ch1 on that Net armed `Any`, run to 2 ms: ch1 every point from `first` is 1.

`tests/CMakeLists.txt`: add `runner/analyzer_consumer_test.cpp`.

### Fence

- [ ] Tests pass (run the `[runner]` cases a few times: `ctest --preset debug -R "analyzer consumer" --repeat until-fail:5`)
- [ ] clang-format clean
- [ ] Commit: `"logic analyzer: phase 3 — analyzer consumer"`

---

## Phase 4 — Session

Clips and Analyzer state on the bench (design §2.3, §3.1).

### 4.1 — Session

`src/session/session.hpp`: include `"runner/analyzer_consumer.hpp"`; add `struct Clip` and the public API of design §2.3, plus `std::optional<NetId> clipNet(std::size_t channel) const { return analyzer_.nets[channel]; }` (tests). `Live` gains `AnalyzerConsumer analyzer;` between `probe` and `runner`. Private: `void configureLive();`, `std::array<std::optional<Clip>, kChannels> clips_;`, `AnalyzerConfig analyzer_;`.

`src/session/session.cpp`:

- `makeLive`: `consumers.push_back(&live->analyzer)` when `machine_`, after the probe.
- `powerOn`: in the rebuild branch, before `live_.reset()`, `analyzer_.armed = false` unless the old phase is `Armed`/`Filling` (design §3.1); after `touchLive()`, `configureLive()`.
- `clip(reference, pin)`: no Machine → nullopt; `component = machine_->designCircuit().components.find(reference)`; pin index via `FindPin` (number or name) else nullopt; `number = std::to_string(index + 1)`; `at = machine_->padPosition(reference, number)` else nullopt; already clipped there → that Channel; lowest free Channel else nullopt; `label = std::format("{}.{} {}", reference, number, spec.name)`; store, `configureLive()`.
- `unclip(c)`: reset, `configureLive()`. `clipAt`: match reference and number (pin given as number, as the Board passes it).
- `setAnalyzer(settings)`: `analyzer_.settings = settings`, `configureLive()`. `arm()`/`disarm()`: design §3.1.
- `configureLive()`: design §3.1.

Resistors have no `Component`: `clip` on `R1` returns nullopt (no Datasheet pins to label). Accept for P3c.

### 4.2 — Tests

`tests/session/session_test.cpp`:

- `session: clips` — `broken_trace`: `clip("U6", "6")` → 0, `clip("U2", "9")` → 1, `clipAt("U6", "6")` → 0, `clips()[0]->label == "U6.6 1Y"`, `clip("U6", "1Y")` → 0 (same pad), unclip 0 then `clip("U9", "5")` → 0; fill to 8 then a ninth → nullopt; `clip("U99", "1")` → nullopt.
- `session: clips survive a repair` — clip `U13.6` and `U1.14`: `clipNet(0) != clipNet(1)`; power on, `RunTo` 1 ms, power off, mend at `kH0Crack`, power on: `clipNet(0) == clipNet(1)`; power off.
- `session: arm before power-on captures` — `stuck_gate`, clip `U12.11`, `setAnalyzer({.period = 1'000'000})`, `arm()`: `analyzerPhase() == Armed` while Off; power on, `RunTo` 5 ms: `analyzerPhase() == Captured`, `captures().update()`, every point from `first` has bit 0 high.
- `session: a capture is not re-armed by a repair` — `broken_trace`, clip `U1.14`, arm, power on, `RunTo` 5 ms (Captured), power off, mend at `kH0Crack`, power on, `RunTo` 6 ms: `analyzerPhase() == Idle`.

### Fence

- [ ] Tests pass
- [ ] clang-format clean
- [ ] Commit: `"logic analyzer: phase 4 — clips and arming in the session"`

---

## Phase 5 — Board and command line

Clipping from the Board, visible tags, the window toggle (window body in phase 6) (design §3.2–3.4).

### 5.1 — `BoardView`

`src/ui/board_view.hpp`: `Tool::Clip`; `struct ClipTag { Point at; std::size_t channel; };`; `void setClips(std::span<const ClipTag> clips) { clips_.assign(clips.begin(), clips.end()); }`; private `std::vector<ClipTag> clips_;`; `inline constexpr std::array<ImU32, kChannels> kChannelColours` (8 distinct saturated colours readable on the green board: yellow, cyan, magenta, orange, light green, light blue, pink, white); include `"instruments/logic_analyzer.hpp"`.

`src/ui/board_view.cpp`: `keys()`: `ImGuiKey_C` → `Tool::Clip`. In `draw`, after the probe tip: for each tag, a rounded rect (`max(12, map.pixels(1'000'000))` px square) up-right of the pad centre, filled with its colour, black digit `channel + 1`, thin black outline.

### 5.2 — Toolbar

`src/ui/toolbar.hpp`: `bool analyzerOpen = false;`. `src/ui/toolbar.cpp`: action text `case Tool::Clip:` → `"Clip: click a pin"`, or `"Clip: all 8 channels used"` in amber when every `session.clips()` entry is set; `Clip (C)` button after `Mend (M)`; `Analyzer (A)` toggle after `Service Manual (H)`, key `A` in the same `WantTextInput` guard.

### 5.3 — `main.cpp`

- `Options`: `std::vector<std::string> clip;`, `std::optional<Time> rate;`; parse `--clip` like `--repair`, `--rate` via `ParsePeriodFromFrequency` and reject periods not in `kAnalyzerPeriods`. Usage string: `[--clip REF.PIN[:0|1|r|f],...] [--rate 10MHz]`.
- In `main`, after the `--probe` block: build `AnalyzerSettings` (`rate` if given); for each `--clip` item split at `:` (trigger via `ParseTrigger`, else usage error), at `.` for reference and pin; `session.clip(ref, pin)` else `bw_ui: unknown pin` + usage; set `settings.trigger[channel]`. If any: `session.setAnalyzer(settings)`, `session.arm()`.
- `RunWindow`: `toolbar.analyzerOpen = !options.clip.empty();`. `takeClick()` switch: `case Tool::Clip:` as design §3.4. Each frame before `boardView.draw`: build `std::vector<ClipTag>` from `session.clips()` and `setClips`.

### 5.4 — Visual check

`bw_ui puzzles/broken_trace.bwp --clip U1.14:r,U13.6,U6.6 --rate 1MHz --screenshot /tmp/p3c-5.png --after 1s`: tags 1, 2, 3 on those pads, toolbar shows `Clip (C)` and `Analyzer (A)` (pressed).

### Fence

- [ ] Build and tests pass
- [ ] Screenshot shows the tags
- [ ] clang-format clean
- [ ] Commit: `"logic analyzer: phase 5 — clip tool and command line"`

---

## Phase 6 — Window

The floating `Logic Analyzer` window (spec S2, S3; design §5.4).

### 6.1 — `AnalyzerWindow`

Create `src/ui/analyzer_window.hpp/.cpp`:

```cpp
// The floating Logic Analyzer: settings, Channel rows, the last Capture's waveforms.
class AnalyzerWindow {
public:
    void draw(bool& open, Session& session);

private:
    std::optional<Capture> shown_;  // kept across rebuilds and power-off
    double from_ = 0, to_ = kDepth;  // visible points
    std::optional<double> marker_;   // point
};
```

- Window: `SetNextWindowSize({760, 330}, FirstUseEver)`, position bottom-left of the work area, `ImGuiWindowFlags_NoSavedSettings`, title `Logic Analyzer`.
- Header: phase text (spec S2 names; `Armed` and `!session.powered()` → `armed: waiting for power`; `Idle` → `idle` even with an old Capture shown), `Arm` (calls `session.arm()`) or `Stop` (`session.disarm()`) when Armed/Filling, `Rate` combo over `kAnalyzerPeriods` labelled `FormatFrequency` (`1 kHz` … `100 MHz`), `Trigger at` combo `10 %`/`50 %`/`90 %`; changes → `session.setAnalyzer`.
- Table, 3 columns (`ImGuiTableFlags_SizingFixedFit`, last stretch): per Channel colour swatch + label or greyed `free`, trigger combo (`any`, `0`, `1`, `rise`, `fall`; `PushID(channel)`), `×` small button (`session.unclip`), lane (`Dummy` of lane height, drawn with the window draw list).
- Lanes, axis, input, readouts: design §5.4. `FormatTime` and `FormatFrequency` in the anonymous namespace. Lane label from `shown_->labels[c]` drawn faintly inside the lane when it differs from the current Clip label.
- No Capture: `arm, then power on` greyed in place of the lanes.

`src/ui/CMakeLists.txt`: add `analyzer_window.cpp`. `main.cpp`: `AnalyzerWindow analyzerWindow;`, `analyzerWindow.draw(toolbar.analyzerOpen, session)` after `manualWindows.draw`, only when `session.machine()`.

### 6.2 — Visual check

- `bw_ui puzzles/broken_trace.bwp --clip U1.14:r,U13.6,U6.6 --rate 1MHz --screenshot /tmp/p3c-6a.png --after 1s`: window with `captured`, three lanes: ch1 toggling, ch2 flat, ch3 low pulses every 64 µs; Trigger line at 10 %; axis labelled in µs.
- `bw_ui puzzles/stuck_gate.bwp --clip U12.11,U12.12,U12.13 --rate 100kHz --screenshot /tmp/p3c-6b.png --after 1s`: ch1 flat high, ch2/ch3 moving.
- `bw_ui netlists/counter.bwn --clip U1.14,U1.13,U1.12,U1.11 --rate 2MHz --screenshot /tmp/p3c-6c.png --after 1s` (the counter has a Board): four counter bits, each half the frequency of the previous.

### Fence

- [ ] Build and tests pass
- [ ] Screenshots read and match S1–S3
- [ ] clang-format clean
- [ ] Commit: `"logic analyzer: phase 6 — analyzer window"`

---

## Phase 7 — Release, user check, docs

### 7.1 — Release

`cmake --preset release && cmake --build --preset release && ctest --preset release -j 10`. `build/release/src/ui/bw_ui puzzles/broken_trace.bwp --clip U1.2,U1.14,U1.13,U1.12,U1.11,U2.14,U2.13,U6.6 --rate 100MHz --screenshot /tmp/p3c-rel.png --after 3s` (`U1.2` = `CLK`): ratio ≈ 1.00×, `ui` ms within noise of P3b (0.5 ms); Debug section ring wait share near 0. If below 1.00×, profile the devices thread (`sample` on the release `bw_ui`) before tuning `emit`.

### 7.2 — User check

User diagnoses the three Puzzles with the Analyzer (spec "Done when"). Record feedback under "Later" in this plan.

### 7.3 — Docs

- `docs/ARCHITECTURE.md` roadmap P3c row: done with the commit range and a link to this plan.
- This plan: status column, summary line, implementation notes (differences from the design).

### Fence

- [ ] Release tests pass; ratio ≈ 1.00× with 8 Clips at 100 MHz
- [ ] User confirmed
- [ ] Commit: `"docs: P3c logic analyzer done"`

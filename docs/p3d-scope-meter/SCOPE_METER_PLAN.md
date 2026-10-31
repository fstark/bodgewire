# Oscilloscope and Multimeter — Implementation Plan (P3d)

Design: [SCOPE_METER_DESIGN.md](SCOPE_METER_DESIGN.md)
Spec: [SCOPE_METER.md](SCOPE_METER.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Shared groundwork: voltages, rails, `islandAt`/`connected`, `AcceptsLead`/`LeadLabel` | |
| 2 | Multimeter engine `MeterState` + headless `Multimeter` | |
| 3 | Oscilloscope engine `ScopeState` + headless `Oscilloscope`; LS161 delay test | |
| 4 | `MeterConsumer` and `ScopeConsumer` through the Runner | |
| 5 | Session: `PadLead`, Hooks, scope settings, Test Leads, volts, continuity | |
| 6 | UI groundwork: tool↔window pairing, refusal reasons, placement rule, Hook/Lead tags | |
| 7 | `Multimeter` window, Meter clicks, `--meter` | |
| 8 | `Oscilloscope` window, `--hook`/`--timebase`, screenshot | |
| 9 | Release check, user check, docs | |

Build gate: `cmake --preset debug && cmake --build --preset debug` (and `release` in phase 9)
Test gate:  `ctest --preset debug`

Commit messages: `"scope meter: phase N — …"`. All C++ clang-format clean.

---

## Phase 1 — Shared groundwork

Small, independent pieces every later phase uses (design §2.1, §2.6, §3.1).

### 1.1 — Voltages

Create `src/instruments/volts.hpp` (header only): `kLowVolts`, `kHighVolts`, `kFloatingVolts`, `kUnknownVolts`, `constexpr double LogicVolts(Sample sample)` exactly as design §2.1.

### 1.2 — Rails

- `src/parts/connector.hpp/.cpp`: `std::optional<double> ConnectorVolts(std::string_view part, std::string_view pin);` — `POWER`: `VCC` 5.0, `GND` 0.0; anything else nullopt (the `VIDEO` `GND` pin is a load, not a supply: nullopt).
- `src/netlist/builder.hpp/.cpp`: `std::optional<double> RailVolts(const LoadedCircuit& loaded, NetId net);` — for each `LoadedPart` with `info->kind == PartKind::Connector`, for each `PinSpec` of `ConnectorPins(info->name)` with a `ConnectorVolts`, return it if `PinNet(loaded, part.reference, spec.name) == net`.

### 1.3 — Copper islands

`src/machine/machine.hpp/.cpp`: move the island search at the top of `probeNet` into the private `std::optional<std::uint32_t> islandAt(const Islands& islands, Layer layer, Point at) const;` (pads, vias, then Traces on `layer`). Add public `bool connected(Layer layerA, Point a, Layer layerB, Point b) const;` — one `FindIslands(board_)`, both `islandAt` set and equal. `probeNet` behaviour unchanged.

### 1.4 — Placement rule

`src/view/board_scene.hpp/.cpp`:

```cpp
enum class LeadKind : std::uint8_t { Held, Attached };
bool AcceptsLead(LeadKind kind, HitKind hit);  // Held: Pad, Via, Trace; Attached: Pad
std::string LeadLabel(const BoardScene& scene, const Hit& hit);  // "U1.14 QA", "Trace", "via"
```

`LeadLabel` for a pad: `reference + "." + pad.number`, then `" " + scene.pinName(component, index)` when not empty; Component hits: the reference.

### 1.5 — Tests

- `tests/instruments/multimeter_test.cpp` (created here, added to `tests/CMakeLists.txt`): `volts: logic states`.
- `tests/netlist/builder_test.cpp`: `rails: declared voltages` on `test_pattern.bwn` — VCC 5, GND 0, CLK nullopt.
- `tests/machines/scope_meter_test.cpp` (new, in CMake): `continuity: crack` — `broken_trace`, `connected(Front, pad U1.14, Front, pad U13.6)` false; after `mend(Front, kH0Crack)` true (pads via `padPosition`; `kH0Crack` from `tests/machine/test_points.hpp`).
- `tests/view/board_scene_test.cpp`: `leads: attached on pads only` (all 8 combinations, Component false for both) and `LeadLabel` on a pad, Trace and via of the counter board.

### Fence

- [ ] `volts.hpp`, `ConnectorVolts`, `RailVolts`, `Machine::connected`, `AcceptsLead`, `LeadLabel` exist
- [ ] New tests pass; full debug build and ctest clean
- [ ] Commit: `"scope meter: phase 1 — voltages, rails, continuity islands, placement rule"`

---

## Phase 2 — Multimeter engine

`MeterState` and the headless `Multimeter` (design §2.2, §4.1, §5.1).

### 2.1 — Engine

Create `src/instruments/multimeter.hpp/.cpp` with `kMeterWindow`, `kMeterRefresh`, `MeterState` (`reset`, `feed`, `advanceTo`, `reading`, private `integrateTo`, `refresh`) per the §5.1 pseudocode. `reset()` clears `started_`, `reading_`, `area_`.

### 2.2 — Headless wrapper

`Multimeter(Simulator& sim, NetId net)` records the Net; `update()` feeds the Samples with `time > fedUntil_` (as `LogicProbe::update`), then `state_.advanceTo(sim_.now())`, `fedUntil_ = sim_.now()`.

Add `multimeter.cpp` to `src/instruments/CMakeLists.txt`.

### 2.3 — Tests (`tests/instruments/multimeter_test.cpp`)

- `meter: 25 % duty reads 1.03 V` — feed 1 µs period, high 250 ns, from 0 to 1 s; `advanceTo(1 s)`: `reading` within 0.001 of 1.025.
- `meter: no reading before the first refresh` — feed at 0, `advanceTo(249 ms)`: nullopt; `advanceTo(250 ms)`: set.
- `meter: window starts at the first Sample` — first Sample One at 100 ms: reading 3.5 at 250 ms; then Zero at 300 ms: at 500 ms reading = (50·3.5 + 150·0.2) / 200.
- `meter: test pattern CLK` — `test_pattern.bwn`, `Multimeter` on `CLK`, run to 1 s in 1 ms steps with `update()`: within 0.01 of 1.85.

### Fence

- [ ] `MeterState`, `Multimeter` implemented; tests pass
- [ ] Commit: `"scope meter: phase 2 — multimeter engine"`

---

## Phase 3 — Oscilloscope engine

`ScopeState` and the headless `Oscilloscope` (design §2.3, §4.2, §5.2).

### 3.1 — Types

Create `src/instruments/oscilloscope.hpp` with `kScopeChannels`, `kDivisions`, `kTimebases` (20 entries), `kAutoWait`, `ScopeEdge`, `ScopeMode`, `SweepKind`, `ScopeSettings`, `Sweep`, `TimebaseIndex()`.

### 3.2 — Engine

`src/instruments/oscilloscope.cpp`: `ScopeState` per §5.2:

- `powerOn()`: histories = `{X at 0}`, `last_` X, `now_ = 0`, not sweeping, `waitFrom_ = 0`, `pending_` reset, `status_ = None`; `sweep_` kept.
- `configure`, `clear(c, now)` (history = `{X at now}`, `last_[c] = X`), `change`, `seed`, `advanceTo`, `catchUp`, `startSweep`, `materialize`, `prune` as the pseudocode. `materialize` uses `std::ranges::upper_bound` on time for the value at `start`.
- `prune(keepFrom)`: per Channel, `i` = index of the last Sample with time ≤ `keepFrom`; erase `[0, i)` only when `i >= size / 2`.

### 3.3 — Headless wrapper

`Oscilloscope(sim, channels, settings)`: records each Net, `state_.powerOn()`, `configure(settings, sim.now())`. `update()`: merge the new Samples of both Channels by time (Channel 0 first on ties; a Net on both Channels feeds both), `change` each, then `advanceTo(sim_.now())`.

Add `oscilloscope.cpp` to CMake.

### 3.4 — Tests

`tests/instruments/oscilloscope_test.cpp` (new, in CMake), hand-fed:

- `scope: sweep around the trigger` — 1 µs/div; ch0 0 at 0, 1 at 5 µs, 0 at 5.5 µs; ch1 1 at 3 µs, 0 at 6 µs, 1 at 20 µs; `advanceTo(30 µs)`: `start` 4 µs, kind Triggered, ch0 `{0@4, 1@5, 0@5.5}`, ch1 `{1@4, 0@6}`.
- `scope: auto free-runs on a constant net` — `advanceTo(60 ms)` at 1 µs/div: serial 1, kind Auto, `start = 50 ms - 1 µs`.
- `scope: normal waits` — same with `Normal`: serial 0, status None.
- `scope: fall trigger on ch2`.
- `scope: X before power-on` — Trigger at 500 ns at 1 µs/div: value at `start` (−500 ns) is X.
- `scope: only the last sweep per advance` — three Triggers 20 µs apart at 1 µs/div, one `advanceTo`: serial 1, `start` = third Trigger − 1 µs.
- `scope: configure drops a sweep in progress` — Trigger, configure before the end, advance past it: serial 0.

`tests/machines/scope_meter_test.cpp`: `scope: LS161 clock to QA` — built `broken_trace`, `Oscilloscope` ch0 `PinNet(U1, "2")`, ch1 `PinNet(U1, "14")`, 20 ns/div, Rise on ch0; run 1 ms steps to 20 ms, after each `update()` read the Sweep: ch1's first Sample after the Trigger time is at +13 ns or +15 ns; require both seen.

### Fence

- [ ] `ScopeState`, `Oscilloscope` implemented; tests pass
- [ ] Commit: `"scope meter: phase 3 — oscilloscope engine"`

---

## Phase 4 — Consumers

Both engines on the devices thread (design §2.4, §4.3).

### 4.1 — `MeterConsumer`

Create `src/runner/meter_consumer.hpp/.cpp`, a copy of `ProbeConsumer` with `MeterState`:

- `touch()` identical (watch new, unwatch old, `target_` store).
- `nets()`, `sample()`, `powerOn()` (`state_.reset()`), `powerOff()` (reset, `published_ = 0`) as the probe.
- `advanceTo(t)`: retarget check as the probe; `state_.advanceTo(t)`; `published_ = reading ? (target + 1) << 32 | std::bit_cast<std::uint32_t>(static_cast<float>(*reading)) : 0`.
- `reading()`: nullopt unless `touched_` and the high 32 bits are `ToIndex(*touched_) + 1`.

### 4.2 — `ScopeConsumer`

Create `src/runner/scope_consumer.hpp/.cpp`, following `AnalyzerConsumer`:

- `configure()`: store `pending_` under the lock, bump `generation_`, watch/unwatch only Channels whose Net changed, `watched_ = config.nets`. Sets `status_ = None` when the settings or Nets changed (UI-side, as the analyzer's phase).
- `adoptNets()`: distinct Nets + Channel masks (2 bits).
- `adopt()`: per §4.3.
- `sample`/`seed`: adopt if needed; for each Channel bit in the Net's mask `state_.change(c, s)` / `state_.seed(c, s)`.
- `advanceTo(t)`: adopt if needed, `state_.advanceTo(t)`, `now_ = t`, `status_ = state_.status()`; if `state_.sweep().serial != published_`: copy into `sweeps_.back()`, publish.
- `powerOn()`: `now_ = 0`, adopt if needed, `state_.powerOn()`, `state_.configure(applied_.settings, 0)`; `powerOff()`: `status_ = None`.

Add both to `src/runner/CMakeLists.txt`.

### 4.3 — Tests

`tests/runner/meter_consumer_test.cpp` and `scope_consumer_test.cpp` (new, in CMake), with the Runner on `test_pattern.bwn`, `Speed::Max`, `pauseAt` + `waitIdle` as `probe_consumer_test.cpp`:

- `meter consumer: reading through the runner` — touch `CLK`, run to 1 s: within 0.01 of the headless `Multimeter`.
- `meter consumer: retarget resets` — touch `CLK`, run 500 ms, touch `VCC`: reading after the next refresh is 3.5 (logic One; rails are the Session's job).
- `scope consumer: sweep through the runner` — `CLK`/`H0`, 1 µs/div, run 2 ms: the published Sweep's Samples equal the headless `Oscilloscope`'s last Sweep at the same time and seed.
- `scope consumer: second hook on a watched net` — ch0 on a constant Net, run, then ch1 on the same Net: ch1's value in the next Sweep is 0 or 1, not X.

### Fence

- [ ] Both Consumers implemented; tests pass (also under TSan if `/tmp/bw-tsan` exists)
- [ ] Commit: `"scope meter: phase 4 — meter and scope consumers"`

---

## Phase 5 — Session

Hooks, scope settings and the Multimeter on the bench (design §2.5, §3.2).

### 5.1 — `PadLead`

Rename `struct Clip` → `PadLead` (session, `main.cpp`, `analyzer_window.cpp`, tests). Extract `std::optional<PadLead> padLead(reference, pin) const` from `clip()` (component, `FindPin`, number, label, `padPosition`); `clip()` uses it.

### 5.2 — Hooks and scope

Members and methods per §2.5/§3.2: `hooks_`, `scope_`, `hook()` (lowest free of 2, existing → same Channel), `unhook`, `hookAt`, `hookRail` (`RailVolts(*live_->circuit, *scope_.nets[c])`), `setScope`, `scopeSettings`, `scopeStatus`, `sweeps`. `Live` gains `ScopeConsumer scope;` (before `runner`), pushed in `makeLive`; `configureLive()` fills `scope_.nets` and configures it.

### 5.3 — Test Leads

`MeterMode`, `TestLead`, members `meterMode_`, `red_`, `black_`, `blackNext_`, `connected_`, `meterRail_`; `Live` gains `MeterConsumer meter;`. `touchLive()` touches the meter (§3.2). `placeLead`, `liftLeads` (both reset, `blackNext_ = false`), `setMeterMode` (touchLive + recompute), `blackNext`, `red`, `black`, `volts`, `continuity`. `recomputeContinuity()` (private): `connected_ = red_ && black_ ? machine_->connected(...) : nullopt`; called from `placeLead`, `liftLeads`, `setMeterMode` and after a `Done` Repair.

### 5.4 — Tests (`tests/session/session_test.cpp`)

- `session: hooks` — `U1.2`, `U1.14` → 0, 1; third refused; label `U1.14 QA`; `hookAt`, `unhook`; `hookRail` of a Hook on the J1 `GND` pad is 0.0.
- `session: meter volts` — `broken_trace`, red on the `U1.2` pad, power on, run to 1 s: ≈ 1.85; red on J1 `VCC`: 5.0 (after the next run); power off: 0.0; `liftLeads`: nullopt.
- `session: continuity refused with the power on` — Continuity, powered: `placeLead` false; `continuity()` nullopt.
- `session: continuity follows a mend` — off, Leads on `U1.14` and `U13.6`: false; mend `kH0Crack`: true.
- `session: hooks and leads survive a repair` — Hook on `U13.6`, red on `U13.6`; mend, power on: Hook Net == `PinNet(rebuilt, "U13", "6")` == `PinNet(rebuilt, "U1", "14")`; meter Net likewise.

### Fence

- [ ] Session API as design §2.5; all tests pass
- [ ] Commit: `"scope meter: phase 5 — hooks and test leads in the session"`

---

## Phase 6 — UI groundwork

Tool↔window pairing, refusal reasons and the placement rule for the existing tools, plus the new tools and tags (design §3.3–§3.6). No new windows yet: picking Hook/Meter sets the flags only.

### 6.1 — Toolbar

`ToolButton` public in `toolbar.hpp`; `refused(std::string reason = {})` with `reason_`/`reasonUntil_` (2 s) replacing the action text in amber; `scopeOpen`, `meterOpen`; buttons `Hook (O)`, `Meter (V)` after `Clip (C)`; action texts per §3.4; `Analyzer (A)` button and key removed.

### 6.2 — Board view

`Tool::Hook`, `Tool::Meter`; keys `O`, `V`; all tool keys through `setTool`; `picked_` + `takePicked()`; `kHookColours`; `HookTag`/`setHooks`, `LeadTag`/`setLeads` drawn per §3.3.

### 6.3 — Analyzer window

New `src/ui/time_text.hpp/.cpp` with `FormatTime` (moved); `AnalyzerWindow::draw(bool& open, Session&, BoardView&)` with the `Clip (C)` `ToolButton` first in the header.

### 6.4 — `main.cpp`

Clip/Hook click case and Meter case per §3.5 (Meter calls `session.placeLead`); Escape by tool; `takePicked()` pairing; Hook and Lead tags each frame. Add `time_text.cpp` to `src/ui/CMakeLists.txt`.

### 6.5 — Check

Build, ctest, then `bw_ui puzzles/broken_trace.bwp --clip U1.14 --screenshot /tmp/p6.png --after 1s`: toolbar shows `Hook (O)`/`Meter (V)`, no `Analyzer (A)`, analyzer header starts with `Clip (C)`.

### Fence

- [ ] Clip on a Trace flashes `Clip: attaches to pins only`; C/O/V pick tools; analyzer opens from the tool
- [ ] Build, ctest clean; screenshot read
- [ ] Commit: `"scope meter: phase 6 — tool and window pairing, placement rule"`

---

## Phase 7 — Multimeter window

### 7.1 — Window

Create `src/ui/meter_window.hpp/.cpp`: `class MeterWindow { void draw(bool& open, Session&, BoardView&); };` per design §5.4 — `ImGui::Begin("Multimeter", &open, NoSavedSettings)`, `FirstUseEver` 320 × 200 at the top right of the board panel; `Meter (V)` `ToolButton`; `RadioButton` `V` / `Continuity`; readout with `PushFont(nullptr, FontSizeBase * 3)`; BEEP lamp a filled green circle; lead lines.

### 7.2 — Wiring

`main.cpp`: `MeterWindow meterWindow;` drawn with the others; `--meter REF.PIN` option (`session.padLead` → `placeLead({at, Layer::Front, label})`, unknown → usage error), `meterOpen` when given; usage string updated. CMake: `meter_window.cpp`.

### 7.3 — Check

`bw_ui puzzles/broken_trace.bwp --meter U1.2 --screenshot /tmp/meter.png --after 1s`: window, red Lead tag on `U1.2`, readout `1.85 V`.

### Fence

- [ ] Multimeter usable in both modes; screenshot read
- [ ] Commit: `"scope meter: phase 7 — multimeter window"`

---

## Phase 8 — Oscilloscope window

### 8.1 — Window

Create `src/ui/scope_window.hpp/.cpp`:

```cpp
// The floating Oscilloscope: settings, the screen with the last Sweep, cursor readout.
class ScopeWindow {
public:
    void draw(bool& open, Session& session, BoardView& board);

private:
    std::optional<Sweep> shown_;
    std::optional<Time> marker_;  // ps since the Trigger
    float pressX_ = 0;
    bool moved_ = false;
    // Column cache, per Channel: lo/hi volts per pixel; rebuilt when serial, width or rail change.
    std::array<std::vector<std::pair<float, float>>, kScopeChannels> columns_;
    std::uint64_t columnsSerial_ = 0;
    float columnsWidth_ = 0;
};
```

Header: `Hook (O)` `ToolButton`, `Timebase` combo (`FormatTime(tb, false) + "/div"`), `Trigger` combo (`ch1 rise`, `ch1 fall`, `ch2 rise`, `ch2 fall` → source/edge), `auto`/`normal` toggle, state text (`power off`, `no hooks`, `waiting for trigger`, `triggered`, `auto`). Screen per design §5.3 with an `InvisibleButton` for hover/click; Hook rows under it: colour, label, `×` (unhook). `FirstUseEver` 640 × 380 next to the analyzer.

### 8.2 — Wiring

`main.cpp`: `ScopeWindow scopeWindow;` drawn; `--hook REF.PIN,...` (max 2) and `--timebase` (`ParseDuration` + `TimebaseIndex`) per §3.5; `scopeOpen` when Hooks given; usage string. CMake: `scope_window.cpp`.

### 8.3 — Check (spec S4)

`bw_ui puzzles/broken_trace.bwp --hook U1.2,U1.14 --timebase 20ns --meter U1.2 --screenshot /tmp/scope.png --after 1s`: read the PNG — Hook tags, both traces, QA's edge 13 or 15 ns (≈ 0.65 / 0.75 div) after the Trigger mark.

### Fence

- [ ] Oscilloscope usable; screenshot shows the delay
- [ ] Commit: `"scope meter: phase 8 — oscilloscope window"`

---

## Phase 9 — Release check, user check, docs

### 9.1 — Release

`cmake --preset release && cmake --build --preset release && ctest --preset release`. Then `bw_ui puzzles/broken_trace.bwp --clip <8 pins incl. U1.2> --rate 100MHz --hook U1.2,U1.14 --timebase 5ns --meter U1.2 --screenshot /tmp/p3d.png --after 5s`: ≈ 1.00×, `ui` ms within noise of P3c (0.4 ms).

### 9.2 — User check

The user measures a gate delay with the Oscilloscope and finds the `broken_trace` crack by continuity with the power off. Record feedback under "Later".

### 9.3 — Docs

ARCHITECTURE roadmap P3d row → done; this plan: statuses, implementation notes, prune; repo memory.

### Fence

- [ ] Debug and release tests pass; release ≈ 1.00×
- [ ] User confirmed
- [ ] Commit: `"docs: P3d done"`

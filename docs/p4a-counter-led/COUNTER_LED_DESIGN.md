# P4a Counter LED — Detailed Design

Implements the Machine spec [COUNTER.md](../machines/010-counter/COUNTER.md) and the P4a row of the [roadmap](../ARCHITECTURE.md#roadmap): an on-board LED Part, drawn lit on the Board, and the counter reworked around it with its three Puzzles.

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/parts/led.hpp, led.cpp                 Led Component: two passive Pins (K, A)
src/devices/led_glow.hpp, led_glow.cpp     LedGlow: lit fraction per window (pure, like Tv)
src/runner/led_consumer.hpp, led_consumer.cpp   LedGlow per LED as a Consumer
datasheets/LED.md                          Datasheet (checked by src/manual)
netlists/counter.bwn                       rewritten: 5 × 74LS161, D1, R2
boards/counter/*                           re-laid out by the user in KiCad
manuals/counter/user.md, service.md        rewritten
puzzles/dark_led.bwp, frozen_led.bwp, too_fast.bwp
tests/parts/led_test.cpp, tests/devices/led_glow_test.cpp,
tests/runner/led_consumer_test.cpp, tests/machines/counter_test.cpp
```

The Part lives with the other Parts; the brightness model is an Output Device core next to `Tv` (no threads, no Simulator); the Consumer is the threaded wrapper next to `TvConsumer`. Same split as `Tv` / `TvConsumer`.

## 2. LED Part

```cpp
// src/parts/led.hpp
// Red 5 mm LED. Pin index = KiCad pad − 1: pad 1 is the cathode (LED_THT convention).
// Passive: both Pins are loads that never pull a floating Net (Family::Mos), no behaviour.
class Led final : public Component {
public:
    explicit Led(std::string reference);  // Pins {"K", Input, Mos}, {"A", Input, Mos}
    void evaluate(EvalContext& ctx) override;  // nothing
};
```

- `Family::Mos`: a TTL load would make a floating Net read 1 ([ARCHITECTURE.md](../ARCHITECTURE.md#logic-values)); a real LED pulls nothing. Whether the LED glows never depends on this: §5.1 uses the floating flag.
- Catalogue ([src/parts/catalogue.hpp](../../src/parts/catalogue.hpp#L15)): `enum class PartKind { Chip, Resistor, Connector, Led };`. A new kind, not `Chip`: `Chip` means a DIP package with `VCC`/`GND` (checked by `catalogue: chip pinouts are consistent`), and P3f's `FindPower` and Solder Bridges apply to `Chip` only. An LED has no power Pins, so it is always Powered; a broken anode or cathode leaves it dark by §5.1.
- Entry in `s_parts` ([catalogue.cpp](../../src/parts/catalogue.cpp#L71-L86)), sorted by name: `PartInfo{"LED", "red 5 mm LED", "LED-5mm", Led, &CreateLed}`. `CreateLed` (no parameters, as `CreateChip`) is defined above `using enum PartKind;`, where `Led` still names the class, the same way `CreateConnector` avoids the `Connector` clash.
- [NAMING.md](../NAMING.md) Abbreviations: add `led`, so `Led`, `LedGlow`, `LedConsumer` are allowed.
- Netlist: `part D1 LED`, Pins by name (`D1.A`, `D1.K`) or number (`D1.2`, `D1.1`). The builder needs no change: the LED has a `create`, so it is a Component like any chip.
- Net panel ([net_pins.cpp](../../src/netlist/net_pins.cpp#L54)): both Pins are `PinRole::Load`, unchanged code.

## 3. Integration Points

1. **KiCad footprint** — [src/board/kicad_netlist.cpp](../../src/board/kicad_netlist.cpp#L15-L19): `kFootprints` grows to 6 with `{"LED-5mm", "LED_THT:LED_D5.0mm"}`. Its `F.CrtYd` is an `fp_rect` (−1.94…4.49 × ±3.21 mm), already read by `readCourtyard`; pads `"1"` (rect, K) and `"2"` (circle, A) at 2.54 mm.
2. **Session** — [src/session/session.hpp](../../src/session/session.hpp#L121-L131): `Live` gets `std::optional<LedConsumer> leds;` before `runner`. In `makeLive` ([session.cpp](../../src/session/session.cpp#L33-L51)), when `machine_` is set and the built circuit has `PartKind::Led` parts:

   ```cpp
   std::vector<LedNets> nets;
   for (const LoadedPart& part : live->circuit->parts) {
       const auto a = PinNet(*live->circuit, part.reference, "A");
       const auto k = PinNet(*live->circuit, part.reference, "K");
       if (part.info->kind == PartKind::Led && a && k) {  // an unconnected Pin: never lit
           nets.push_back({part.reference, *a, *k});
       }
   }
   if (!nets.empty()) {
       live->leds.emplace(std::move(nets));
       consumers.push_back(&*live->leds);
   }
   ```

   The electrical (built) circuit is used, so a Broken Trace gives the cathode its own island. Rebuilt with everything else at the first power-on after a Repair.
   New accessor, UI thread:

   ```cpp
   struct LedLight {
       std::string reference;  // "D1"
       float brightness;       // 0 dark .. 1 fully lit
   };
   // Every LED Part of the Machine, in netlist order; 0 when not powered or not in live_->leds.
   std::vector<LedLight> leds() const;
   ```

3. **Frame loop** — [src/ui/main.cpp](../../src/ui/main.cpp#L495), after `boardView.setLeads(leadTags)`: map each `LedLight` to its Component with `scene()->componentIndex(reference)` and call `boardView.setLeds(tags)`.
4. **Board view** — [src/ui/board_view.hpp](../../src/ui/board_view.hpp#L101): `struct LedTag { std::size_t component; float brightness; };` and `void setLeds(std::span<const LedTag> leds)`. In `BoardView::draw` ([board_view.cpp](../../src/ui/board_view.cpp#L368-L376)), between `drawTraces(active_, …)` and the pad loop: §5.3. Under the pads, so probing and Clips on the LED's pads stay visible.

Cost on the simulation thread: none beyond 2 watched Nets per LED (Samples only on change; `C19` changes twice a second, `LED_A` never). Devices thread: one `LedGlow` per LED, O(Samples). UI: one disc per LED per frame.

Watched Nets: `kMaxWatched` = 16 ([dispatcher.hpp](../../src/runner/dispatcher.hpp#L16)). Counter worst case: 8 Clips + probe + meter + 2 Hooks + 2 LED Nets = 14, distinct Nets counted once. Machines with more LEDs (030 dice, 040 stopwatch) raise the limit in their own P4 step.

## 4. LedGlow

```cpp
// src/devices/led_glow.hpp
inline constexpr Time kLedWindow = 20'000'000'000;  // ps: about what the eye integrates

// One LED as the eye sees it: share of each kLedWindow it was lit. Windows start at multiples
// of kLedWindow from power-on (t = 0). Time only moves forward.
class LedGlow {
public:
    void setAnode(Sample sample);
    void setCathode(Sample sample);
    void advanceTo(Time time);           // integrates, closes every window ending <= time
    float brightness() const { return brightness_; }  // last closed window; 0 before the first
    void reset();                        // power-on: dark, both ends floating

private:
    bool lit() const;                    // §5.1
    void integrateTo(Time time);

    Sample anode_{0, LogicValue::Z, true}, cathode_{0, LogicValue::Z, true};
    Time integrated_ = 0;                // lit time counted up to here
    Time windowEnd_ = kLedWindow;
    Time litTime_ = 0;                   // ps lit in the current window
    float brightness_ = 0;
};
```

## 5. Key Algorithms

### 5.1 Lit

```
lit = anode.value == One  && !anode.floating
   && cathode.value == Zero && !cathode.floating
```

Current flows only from a driven high anode to a driven low cathode. Counter: `LED_A` is pulled up by `R2` (a Weak tie, not floating), so `D1` glows while `C19` is low (COUNTER.md, Circuit). Dark LED Puzzle: the cathode's island holds only `D1.K`, so it is floating, and `D1` stays dark whatever its value reads.

### 5.2 Windows

```
setAnode(s) / setCathode(s):  advanceTo(s.time); store s
advanceTo(t):
    while windowEnd_ <= t:
        integrateTo(windowEnd_)
        brightness_ = litTime_ / kLedWindow
        litTime_ = 0; windowEnd_ += kLedWindow
    integrateTo(t)
integrateTo(t):  if lit(): litTime_ += t − integrated_;  integrated_ = t
```

Counter: `C19` holds each level 0.52 s, so windows read 0 or 1 except the one holding an edge. Too fast Puzzle: `C19` cycles in 65.5 ms (about 33 ms each way), so the disc visibly flickers. A Net switching much faster than 20 ms reads its duty cycle: half-lit at 50 %.

### 5.3 Drawing

```
for each LedTag:
    box   = bounding box of scene_->courtyard(component)
    centre = box centre; radius = 0.39 × min(box width, box height)   // ≈ 2.5 mm on LED_D5.0mm
    body  = lerp(kLedDark, kLedLit, brightness)
    if brightness > 0: AddCircleFilled(centre, radius × 1.6, kLedLit with alpha 0.35 × brightness)
    AddCircleFilled(centre, radius, body)
```

`kLedDark = IM_COL32(90, 20, 20, 255)`, `kLedLit = IM_COL32(255, 40, 30, 255)`. The courtyard outline and reference (`DrawCourtyard`) are drawn later, on top, as for any Component.

## 6. LedConsumer

```cpp
// src/runner/led_consumer.hpp
struct LedNets {
    std::string reference;  // "D1"
    NetId anode, cathode;
};

// One LedGlow per LED. brightness(): UI thread; overrides: devices thread.
class LedConsumer final : public Consumer {
public:
    explicit LedConsumer(std::vector<LedNets> leds);
    std::span<const LedNets> leds() const { return leds_; }
    float brightness(std::size_t led) const;  // relaxed load of published_[led]

    std::span<const NetId> nets() const override { return nets_; }  // fixed: distinct A/K Nets
    void powerOn() override;                    // reset every LedGlow, publish 0
    void sample(NetId net, Sample sample) override;  // setAnode/setCathode of every LED on net
    void advanceTo(Time time) override;         // advance all, publish brightness()
    void powerOff() override;                   // publish 0

private:
    std::vector<LedNets> leds_;
    std::vector<NetId> nets_;
    std::vector<LedGlow> glows_;                     // devices thread
    std::vector<std::atomic<float>> published_;      // sized once in the constructor
};
```

Fixed Nets, like `TvConsumer`: the Runner constructor watches them ([runner.cpp](../../src/runner/runner.cpp#L27-L36)), and power-on pushes their time-0 Samples, so each `LedGlow` starts from real values. A Net that is both an anode and a cathode (an LED chain, later) feeds both roles.

## 7. Reused Infrastructure

- Catalogue, `FindPin` / `PinNet`: the LED is an ordinary Component.
- Pull-up `R` handling in the builder (`tieResistors`): `R2` from `VCC` to `LED_A` is a pull-up, already supported.
- Runner, Dispatcher, `Consumer`, power-on Samples: unchanged. `LedConsumer` follows `TvConsumer`; the window integration follows `MeterState` ([multimeter.cpp](../../src/instruments/multimeter.cpp)).
- `Sample::floating` (P2g) decides "no current" on a broken Trace.
- `BoardScene::courtyard`, `componentIndex`; KiCad reader `fp_rect` courtyards.
- Datasheet and manual checks (`src/manual`): `LED.md` is checked like any Part (title starts with `LED`, `Function`, `Pins` covering `K` and `A`).
- Puzzle file syntax: `fault crack` and `fault stuck` only. Repairs: Mend and Swap.

## 8. Build Integration

```cmake
# src/parts/CMakeLists.txt
add_library(bw_parts STATIC ... led.cpp ...)
# src/devices/CMakeLists.txt
add_library(bw_devices STATIC bmp.cpp led_glow.cpp tv.cpp tv_input.cpp)
# src/runner/CMakeLists.txt
add_library(bw_runner STATIC ... led_consumer.cpp ...)
# tests/CMakeLists.txt: devices/led_glow_test.cpp, machines/counter_test.cpp,
#                       parts/led_test.cpp, runner/led_consumer_test.cpp
```

No new dependencies: `bw_devices` → `bw_sim` already; `bw_runner` → `bw_devices`; `bw_session` → `bw_runner`, `bw_netlist`; `bw_ui` → `bw_session`, `bw_view`.

## 9. Dependency Diagram

```
bw_sim ← bw_parts (Led) ← bw_netlist ← bw_machine ← bw_session ← bw_ui
bw_sim ← bw_devices (LedGlow) ← bw_runner (LedConsumer) ← bw_session
bw_board (footprint) ← bw_cli (export-kicad)
```

One direction only; `LedGlow` knows nothing of Parts or Nets.

## 10. Counter Machine

[netlists/counter.bwn](../../netlists/counter.bwn), replacing the P1b sample (COUNTER.md, Circuit):

```
part X1 OSC freq=1MHz
part J1 POWER
part R1 R value=4k7
part R2 R value=330
part U1 74LS161
...
part U5 74LS161
part D1 LED

net CLK      X1.OUT U1.CLK U2.CLK U3.CLK U4.CLK U5.CLK
net RESET_N  R1.2 U1.CLR_N U2.CLR_N U3.CLR_N U4.CLR_N U5.CLR_N
net VCC      R1.1 R2.1 U1.ENT U1.ENP U1.LOAD_N ... U5.ENP U5.LOAD_N
net GND      U1.A U1.B U1.C U1.D ... U5.D
net C0       U1.QA          ...  net C19 U5.QD D1.K
net CARRY1   U1.RCO U2.ENT  ...  net CARRY4 U4.RCO U5.ENT
net LED_A    R2.2 D1.A
```

- User step: `bw export-kicad netlists/counter.bwn`, update `boards/counter` in KiCad (U1, U2, X1, J1, R1 keep their places; U3–U5, R2, D1 new; old U3 74LS74 and U4 74LS00 deleted), route, `bw check-board` and `drc_counter` pass. [KICAD_GUIDE.md](../p2a-board-layout/KICAD_GUIDE.md) covers the steps.
- Puzzles (COUNTER.md, Puzzles), `machine counter`:
  - `dark_led.bwp`: `fault crack F.Cu X Y` on the `C19` Trace between `U5` pin 11 and `D1` pad 1; X Y picked on the new layout (as `unpowered_chip.bwp`).
  - `frozen_led.bwp`: `fault stuck U3.RCO 0`.
  - `too_fast.bwp`: `fault stuck U1.RCO 1`.
- Manuals rewritten in the existing shape: User (What it does, Connections), Service (How it works, Components, Troubleshooting: walk `CLK` → `CARRY1`…`CARRY4` → `C19` → `D1`).

## 11. Testing

Catch2, tags as the module. Debug runs ≈ 0.07× real time, so headless tests stay well under one simulated second; full blink checks are `[slow]`.

| File | Checks |
|---|---|
| `tests/parts/led_test.cpp` | `FindPart("LED")`: kind `Led`, package `LED-5mm`, Pins `K`, `A` as `Input`/`Mos`; a floating Net with only LED Pins stays floating and keeps its last value |
| `tests/devices/led_glow_test.cpp` | steady lit → 1; floating cathode or anode → 0; anode 0 → 0; 50 % square wave at 1 kHz → 0.5; edge inside a window splits it; reset → 0 |
| `tests/runner/led_consumer_test.cpp` | small Circuit (OSC → LED cathode, pull-up anode) through a Runner: brightness ≈ 0.5, 0 after powerOff; one Net feeding two LEDs |
| `tests/machines/counter_test.cpp` `[machines]` | fault-free: `C0`–`C11` and `CARRY1`–`CARRY2` toggle at their rates within 10 ms; frozen_led: `CARRY3` never high in 10 ms, `C12`–`C19` constant; swap U3 fixes it; too_fast: `C19` toggles within 70 ms, LedGlow fed from the recorded Samples changes between windows; dark_led: `D1.K`'s Net floating, LedGlow 0, mend fixes it; `[slow]`: fault-free `D1` lit and dark within 1.1 s |
| existing | `boards_test` counter (new board), `drc_counter`, `electrical_test`, `board_scene_test` (R1/U5 lookups), `net_pins_test` (CLK loads U1–U5), `manual_test` (`kGoodService`), `manuals_test`, `sample_test` (the DIV/TOP_N cases go; counter Nets C0–C19) |

UI check (release build): `bw_ui netlists/counter.bwn --screenshot /tmp/counter.png --after 1s` shows `D1` lit or dark by `C19`; `bw_ui puzzles/too_fast.bwp --screenshot … --after 1s` and `dark_led.bwp` likewise.

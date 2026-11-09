# P4i Paddle Controller — Detailed Design

Implements [PADDLE.md](PADDLE.md): the 74LS175 Part, the `PADDLE` controller Part powered through its Connector, the knob through the Session, the command line and the side panel, and the tests of the knob meter. The Machine's netlist, Board, manuals and Puzzles are in [KNOB_METER_DESIGN.md](../machines/045-knob-meter/KNOB_METER_DESIGN.md).

Recipes followed: [ADD_PART.md](../recipes/ADD_PART.md) (74LS175), [ADD_BOARD_DEVICE.md](../recipes/ADD_BOARD_DEVICE.md) ("Input device": the Button chain is the model for the knob chain), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md), [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md). This design gives only what those recipes leave open.

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/parts/ls175.hpp, ls175.cpp           74LS175 Component
src/parts/paddle.hpp, paddle.cpp         Paddle Component, DurationFromText
src/machine/knob_setting.hpp, .cpp       KnobSetting, ParseKnob ("J2:40%")
datasheets/74LS175.md, PADDLE.md
tests/parts/ls175_test.cpp, paddle_test.cpp
tests/machine/knob_setting_test.cpp
tests/machines/knob_test.cpp
netlists/knob.bwn, manuals/knob/, boards/knob/, puzzles/*.bwp (Machine design)
docs/recipes/ADD_CONNECTOR_DEVICE.md     new recipe (phase 7)
```

Everything else is small changes to existing files (§8).

## 2. 74LS175

```cpp
// src/parts/ls175.hpp
// Quad D flip-flop, common clock (rising edge) and clear. Pins per datasheet: 1 CLR_N, 2 1Q,
// 3 1Q_N, 4 1D, 5 2D, 6 2Q_N, 7 2Q, 8 GND, 9 CLK, 10 3Q, 11 3Q_N, 12 3D, 13 4D, 14 4Q_N, 15 4Q,
// 16 VCC.
class Ls175 final : public Component {
public:
    explicit Ls175(std::string reference);
    static constexpr PinIndex kClrN = 0, kClk = 8;

    void powerOn(PowerOnContext& ctx) override;  // each state random
    void evaluate(EvalContext& ctx) override;

private:
    std::array<LogicValue, 4> state_{};  // Zero, One or X
};
```

The [Ls74](../../src/parts/ls74.cpp) shape: a `kUnits` table of `{d, q, qn}` per flip-flop (`{3, 1, 2}`, `{4, 6, 5}`, `{11, 9, 10}`, `{12, 14, 13}`), one shared `CLK` and `CLR_N`.

```
clr = read(CLR_N)
clocked = rose(CLK)
per unit:
    if clr == 0:        state = 0                    delays: kLs175Clear
    else if clr == X:   state = X
    else if clocked:    state = readBefore(D)        delays: kLs175ClockToQ
    drive Q = state, Q_N = Not(state)
```

- `CLK` is `PinSpec{"CLK", Input, Ttl, true}` (rising-edge-only, as the 74LS74's). An X edge on `CLK` is no edge (`rose` needs 0 → 1), as on the 74LS74.
- `src/parts/delays.hpp`, TI typicals (ps): `kLs175ClockToQ{20'000, 21'000}`, `kLs175Clear{16'000, 20'000}` (`Q` falls 20 ns, `Q_N` rises 16 ns after `CLR_N` falls). The PDF cannot be fetched: known typical values, said so in the commit.
- Catalogue: `PartInfo{"74LS175", "quad D flip-flop, clear", "DIP-16", Chip, &CreateChip<Ls175, "74LS175">}` after `74LS164` (sorted as strings).
- Power at the DIP corners (8, 16): not in `kOffCornerPower`.
- Datasheet `74LS175.md`: Function (four flip-flops on one clock; on each **rising** edge of `CLK` every `Q` takes its `D` as it was just before the edge, and holds it until the next; `Q_N` is always the opposite; `CLR_N` low clears all four at once, without the clock, and holds them; tie it high when unused), Pins (`1D`–`4D`, `1Q`–`4Q`, `1Q_N`–`4Q_N`, `CLK`, `CLR_N`, `VCC`, `GND`).

## 3. PADDLE Part

```cpp
// src/parts/paddle.hpp
inline constexpr Time kPaddleDelay = 100'000;  // ps: TRIG_N fall to OUT, a 555's typical

// "30us" → ps, rounded to nearest; error text if malformed or < 1 ps. Netlist ParseDuration
// wraps it (as ParsePeriodFromFrequency wraps PeriodFromFrequency).
std::expected<Time, std::string> DurationFromText(std::string_view text);

// Paddle controller behind a KK-4 Connector: a pot and a 555 one-shot, logic only. Pins
// 1 TRIG_N, 2 OUT, 3 VCC, 4 GND (pin index = KiCad pad − 1). OUT is low while idle; a fall on
// TRIG_N starts a high pulse of shortest + knob × (longest − shortest); falls during it are
// ignored.
class Paddle final : public Component {
public:
    Paddle(std::string reference, Time shortest, Time longest);  // Check(1 <= shortest < longest)
    static constexpr PinIndex kTrigN = 0, kOut = 1;

    // Any thread; clamped to 0..1. Read when a pulse starts, so a turn needs no wake.
    void setKnob(double turn);
    double knob() const { return knob_.load(std::memory_order_relaxed); }
    Time pulseLength() const;  // for the knob now

    void powerOn(PowerOnContext& ctx) override;  // idle, OUT low; knob_ untouched
    void evaluate(EvalContext& ctx) override;

private:
    Time shortest_, longest_;
    std::atomic<double> knob_{0.0};
    bool busy_ = false;
    Time endsAt_ = 0;  // ps; while busy_: when the pulse ends at TRIG_N's side
};
```

```
evaluate:
    ending  = busy_ && now() >= endsAt_          // the wake set by the trigger
    if ending: busy_ = false
    started = !busy_ && fell(TRIG_N)
    if started: busy_ = true; endsAt_ = now() + pulseLength(); wakeAfter(pulseLength())
    if ending || started: drive(OUT, busy_ ? 1 : 0, kPaddleDelay)
```

- One wake per pulse; other evaluations (a rise of `TRIG_N`, an X) drive nothing. A pulse that ends at the very time of a new fall goes straight into the next (`OUT` stays high).
- `pulseLength()` = `shortest_ + llround(knob × (longest_ − shortest_))`. `std::atomic<double>` is lock-free on the supported targets; the UI writes it, the simulation reads it.
- Pins come from `ConnectorPins("PADDLE")`: `{"TRIG_N", Input, Ttl}`, `{"OUT", Output, Ttl}`, `{"VCC", Power, Ttl}`, `{"GND", Power, Ttl}`. `ConnectorPins` stops `Check`ing for `VIDEO` only; `ConnectorVolts` is unchanged (the controller's `VCC` and `GND` take power, they do not supply it).
- `DurationFromText`: the body of today's `ParseDuration` moves here unchanged (same messages); `ParseDuration` in [units.cpp](../../src/netlist/units.cpp) becomes `return DurationFromText(text);`. `bw_netlist` already links `bw_parts`; `bw_parts` cannot reach `bw_netlist`.
- Factory in [catalogue.cpp](../../src/parts/catalogue.cpp), above `using enum PartKind;` next to `CreateOscillator`:

  ```
  CreatePaddle(circuit, reference, params):
      for each param: "min" or "max" (repeated: the last wins), else "PADDLE: unknown parameter 'x'"
      either missing: "PADDLE needs min=<duration> and max=<duration>"
      each through DurationFromText (its error as is)
      min < 1 µs: "PADDLE: min must be at least 1us"
      max <= min: "PADDLE: max must be longer than min"
      circuit.add<Paddle>(reference, min, max)
  ```

- Catalogue row, sorted between `OSC` and `POWER`: `PartInfo{"PADDLE", "paddle controller connector", "KK-4", Connector, &CreatePaddle}`.
- KiCad footprint: `kFootprints` in [kicad_netlist.cpp](../../src/board/kicad_netlist.cpp) grows to 9 with `{"KK-4", "Connector_Molex:Molex_KK-254_AE-6410-04A_1x04_P2.54mm_Vertical"}` (in KiCad 10's library).
- `PaddleOf` in [builder.hpp](../../src/netlist/builder.hpp), next to `ButtonOf`, the same lookup with `part->info->name == "PADDLE"`:

  ```cpp
  // The PADDLE Part `reference` of this build; nullptr if none (unknown or not a PADDLE).
  Paddle* PaddleOf(const LoadedCircuit& loaded, std::string_view reference);
  ```

  Unlike a Button, the controller is never left out of an electrical build: an unpowered one is still a Component, with its Pins off every Net.
- The Datasheet check builds each Part with no parameters (`PartPinNames` in [manual.cpp](../../src/manual/manual.cpp), OSC gets `freq=1MHz`): `PADDLE` gets `min=30us max=9.93ms` there, or its pin list is empty and a Datasheet missing a pin would pass. `tests/manual/manual_test.cpp` checks `PartPinNames(*FindPart("PADDLE")).size() == 4`.
- Datasheet `PADDLE.md`, in the shape of [VIDEO.md](../../datasheets/VIDEO.md): Function (the 4-pin connector of a paddle controller, a box with a knob on a cable; inside, a pot sets the time of a 555 one-shot; each fall of `TRIG_N` makes `OUT` high for a time set by the knob, shortest at the left stop, longest at the right, the range given in the Machine's Service Manual; a new fall while `OUT` is high is ignored; the controller runs on the Board's +5 V through `VCC` and `GND`, and without it `OUT` drives nothing), Pins (`TRIG_N`, `OUT`, `VCC`, `GND`).

## 4. Powered through its Connector

`FindPower` in [electrical.cpp](../../src/machine/electrical.cpp#L195) checks the power Pins of every chip; it now checks the controller's too:

```cpp
// connector.hpp
// True when the device behind connector `part` runs on the Board's power through the connector's
// own VCC and GND Pins ("PADDLE"); false for POWER (it supplies) and VIDEO (the monitor does not).
bool ConnectorPowersDevice(std::string_view part);
```

In the second loop of `FindPower`: `if (part.info->kind != PartKind::Chip && !(part.info->kind == PartKind::Connector && ConnectorPowersDevice(part.info->name))) continue;`. The rest is unchanged: each power Pin's island must carry exactly its `PowerPinVolts`, else the reference goes in `power.unpowered`, and `Machine::build` hands that list to `BuildCircuit`, which leaves the Component's Pins off every Net (P3f). `IslandNetNames` and `ElectricalNetlist` already skip power Pins of every Part (so the cut-off pad's island has no name; the meter reads nothing there, as on `dark_seconds`).

Comments and docs that say only chips are Powered change with it: `BoardPower::unpowered` and `FindPower` in [electrical.hpp](../../src/machine/electrical.hpp) ("chip and controller References"; "POWER and VIDEO connectors are always Powered"), `BuildCircuit`'s `unpowered` in [builder.hpp](../../src/netlist/builder.hpp), the Power paragraph of [ARCHITECTURE.md](../ARCHITECTURE.md) and **Powered** in [GLOSSARY.md](../GLOSSARY.md) (phase 2 for the code, phase 7 for the docs).

## 5. The Knob

### 5.1 Parsing

```cpp
// src/machine/knob_setting.hpp
struct KnobSetting {
    std::string reference;  // "J2"
    double turn;            // 0 (left stop) .. 1 (right stop)
};

// "J2:40%", "J2:12.5%" (0 to 100 %); error text otherwise.
std::expected<KnobSetting, std::string> ParseKnob(std::string_view text);
```

Errors: no `:`, empty reference, no number, no `%` at the end, more than 100. Next to `ParsePress` in `bw_machine` (`press.cpp` is the model).

### 5.2 Session

[session.hpp](../../src/session/session.hpp), after the Button members:

```cpp
// Every PADDLE Part of the circuit, in netlist order.
std::vector<std::string> knobs() const;
// Clamped to 0..1; any time, powered or not; kept across power-off, power-on and rebuilds.
// False (and nothing set) for a reference that is not a PADDLE.
bool setKnob(std::string_view reference, double turn);
double knob(std::string_view reference) const;  // 0 until set
```

- `std::map<std::string, double, std::less<>> knobs_;` holds the turns.
- `knobs()` walks `machine_ ? machine_->designCircuit() : *live_->circuit` parts for `info->name == "PADDLE"`, as `buttons()`.
- `setKnob`: refuses a reference not in `knobs()`; stores the clamped turn; `PaddleOf(*live_->circuit, reference)->setKnob(turn)` (no wake: P4i spec 4).
- `powerOn`, after a rebuild's `makeLive`: every stored turn goes to the new Paddle. Power-off changes nothing.

## 6. UI

### 6.1 Side panel

[side_panel.cpp](../../src/ui/side_panel.cpp), right after the `TV` section:

```
knobs = session.knobs()
if knobs not empty:
    keys: unless io.WantTextInput: dir = IsKeyDown(UpArrow) − IsKeyDown(DownArrow)
          dir ≠ 0 → setKnob(knobs[0], knob + dir × 0.5 × io.DeltaTime)   // 50 % a second
    if CollapsingHeader("Knob", open):
        per reference: percent = knob × 100
            SliderFloat(reference, &percent, 0, 100, "%.0f %%") → setKnob(reference, percent / 100)
            hovered and io.MouseWheel ≠ 0 → setKnob(reference, knob + wheel × 0.01)
```

- The keys act before the header, so they work with the section collapsed. Up and Down are free: the Board view's keys are letters, `+`, `-` and Escape, and keyboard navigation is off.
- The wheel over a slider turns it and does not scroll the side panel (`ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY)` on the slider while it is hovered).
- The slider's hover text: "Turn the knob: drag, mouse wheel, or the Up and Down keys" (Up and Down only on the first).

### 6.2 `bw_ui`

[main.cpp](../../src/ui/main.cpp): `Options::knob` (`std::vector<KnobSetting>`), `--knob J2:40%,…` parsed with `ParseKnob` (a bad one is a usage error), `[--knob REF:N%,...]` in `kUsage`. In `main()`, with the other option checks (`--probe`, `--clip`, …), before `SDL_Init`: `session.setKnob` for each (the power is still off, which a knob allows); `false` prints `bw_ui: 'J9' is not a paddle controller` and `kUsage`, and returns `kExitUsage`.

## 7. `bw run`

[cli/main.cpp](../../src/cli/main.cpp): `Options::knob`, `--knob` parsed as in `bw_ui` (also in the list of options that take a value), `[--knob REF:N%,...]` in `kUsage`. After the probes are resolved and before `sim.powerOn`: `PaddleOf(loaded, reference)` for each; `nullptr` → `UsageError("'J9' is not a paddle controller")`; else `setKnob(turn)`.

## 8. Integration Points

1. **Parts** — §2, §3: `ls175.cpp`, `paddle.cpp` in `src/parts/CMakeLists.txt` (sorted); catalogue rows; `delays.hpp`; `connector.{hpp,cpp}` (`ConnectorPins("PADDLE")`, `ConnectorPowersDevice`); `units.cpp` wraps `DurationFromText`; `PartPinNames` in `src/manual/manual.cpp` gives `PADDLE` its parameters.
2. **Board** — `kFootprints` KK-4 (§3).
3. **Netlist builder** — `PaddleOf` (§3). No builder rule: the controller's power Pins join the rails by name (`addImplicitPower`), its signal Pins are ordinary.
4. **Machine** — `FindPower` (§4); `knob_setting.cpp` in `src/machine/CMakeLists.txt`.
5. **Session** — §5.2.
6. **UI and CLI** — §6, §7.
7. **Machine name `knob`** — `LoadMachine` finds `netlists/knob.bwn` and `boards/knob/knob.kicad_pcb` by name. Per-Machine loops (ADD_MACHINE.md step 3) and the manuals list (step 2) gain `"knob"`.
8. **Recipes** — a new [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md) for an off-Board device behind a Connector (the paddle chain: Part with the device inside, connector pins, footprint, power through the connector, the Session setting, UI section, CLI flag, tests), listed in `AGENTS.md`; ADD_BOARD_DEVICE points to it; ADD_PART gains the 74LS175 as the shared-clock register reference.
9. **Glossary** — Input Device names the paddle controller as a standard device behind a Connector; **Knob**: what the player turns on a paddle controller, 0 at the left stop to 1 at the right; **Powered** covers the controller.

## 9. Reused Infrastructure

- `Ls74` structure, `CreateChip`, `DelayFor`, `EvalContext::wakeAfter`, `fell`, `readBefore`.
- The Button chain (P4d): an atomic setting on the Component written by the UI thread, `ButtonOf`, `Session::buttons`, `--press`. Unlike a press, a turn needs no wake.
- `FindPower`, `BoardPower::unpowered`, the `unpowered` build (P3f).
- The stopwatch's `DigitAt` (`tests/machines/stopwatch_test.cpp`) for reading the digits.

## 10. Build Integration

```cmake
# src/parts/CMakeLists.txt:   + ls175.cpp paddle.cpp
# src/machine/CMakeLists.txt: + knob_setting.cpp
# tests/CMakeLists.txt:       + machine/knob_setting_test.cpp machines/knob_test.cpp
#                               parts/ls175_test.cpp parts/paddle_test.cpp
```

No new dependencies, no new library edges.

## 11. Testing

Catch2, tags as the module.

### 11.1 Parts

| File | Checks |
|---|---|
| `tests/parts/ls175_test.cpp` `[parts]` | catalogue row, pins (power at 8 and 16, `CLK` rising-edge-only); for each flip-flop (`GENERATE`): `Q` takes `D` on the rising edge only (a falling edge changes nothing), `Q_N` opposite; `D` changed at the edge's timestamp is not seen (`readBefore`); the four independent on one clock; `CLR_N` low clears at once (`kLs175Clear` for `Q` fall and `Q_N` rise) and wins over a clock; delays `kLs175ClockToQ`; `D` at X clocked → X, then a known `D` recovers; `CLR_N` at X → X; an X → 1 on `CLK` takes nothing. Copy `ls74_test.cpp` |
| `tests/parts/paddle_test.cpp` `[parts]` | catalogue row (`Connector`, `KK-4`), pins and kinds; parameters: missing `min` or `max`, unknown name, bad duration, `min` under 1 µs, `max` ≤ `min` each refused with its message, `min=30us max=9.93ms` accepted; `OUT` low after power-on; a `TRIG_N` fall makes `OUT` rise `kPaddleDelay` later and fall `min` later for knob 0, `max` later for 1, half way for 0.5; a second fall during the pulse changes nothing; a rise or X does nothing; `setKnob` during a pulse leaves it, the next pulse uses it; `setKnob` clamps; `ConnectorPowersDevice("PADDLE")` true, `POWER`/`VIDEO` false |
| `tests/netlist/units_test.cpp` | unchanged, still passes (`ParseDuration` now wraps `DurationFromText`) |

### 11.2 Knob

| File | Checks |
|---|---|
| `tests/machine/knob_setting_test.cpp` `[machine]` | `J2:40%` → 0.4, `J2:0%`, `J2:100%`, `J2:12.5%`; refused: `J2`, `:40%`, `J2:40`, `J2:x%`, `J2:101%` |
| `tests/session/session_test.cpp` `[session]` | `session: knob kept across power and rebuild`: a netlist-only Session on a small netlist (`OSC`, `PADDLE`, its `TRIG_N` from a divided clock) lists `J2` in `knobs()`; `setKnob("J9", …)` false; `setKnob("J2", 1.5)` reads 1; a turn with the power off is on the Paddle at power-on; on the knob meter (phase 6) a Repair and power-on keep the turn |

### 11.3 The knob meter

`tests/machines/knob_test.cpp` (copy the shape of `stopwatch_test.cpp`: `DesignCircuit()`, `BoardCircuit()`, `DigitAt` and a `Meter` helper that sets the knob, powers on with a seed and reads `"dd"` from `DS1`, `DS2`). Until the Board exists (phase 5), the fault-free circuit is `LoadNetlistFile("knob.bwn")`.

| Case | Checks |
|---|---|
| `knob: netlist loads without warnings` | diagnostics empty |
| `knob: Puzzles have a title and a brief` | the five files (phase 6) |
| `knob: reads the knob` `[slow]`, one case per seed 1, 2, 3 | one power-on per seed, the knob stepped while running: 0 from power-on → `00` at 250 ms and 350 ms (steady); then each of 0.2 → `20`, 0.5 → `50`, 0.8 → `79`, 1 → `99`, read two cycles (200 ms) after the turn |
| `knob: follows a turn` | knob 0.3 from power-on, `30` at 250 ms; `setKnob(0.6)`; `59` (⌊59.9⌋) by 450 ms |
| `knob: one measuring cycle` | probes on `TRIG_N`, `PULSE`, `LATCH`, `TB1` with knob 0.5: `PULSE` high for 4.98 ms (30 µs + 0.5 × 9.9 ms) starting 0.1 µs after a `TRIG_N` fall, one `LATCH` rise at its end, `TB1`·`TB2` high 60–80 ms after the trigger, 100 ms between triggers |
| one case per Puzzle (KNOB_METER_DESIGN.md, Puzzles), Symptom then Repair | as tabled there |

The 1 MHz clock and its dividers are the stopwatch's load: about 0.12× real time in debug (measured, `stopwatch: the seconds count`). `reads the knob` simulates 1.15 s per seed (about 10 s of debug time each, one case per seed so ctest runs them in parallel); the other fault-free cases about 0.7 s; the Puzzles about 5 s. Every case over ~1 s simulated is `[slow]` (ADD_MACHINE.md step 2).

A Puzzle case that compares lit segments (Dead knob) compares the seven-segment masks of both digits, not `DigitAt` characters: power-on latch values 10–15 all read `?`.

### 11.4 Session, Board, release

| File | Checks |
|---|---|
| `tests/session/session_test.cpp` `[session][slow]` | `session: knob meter with every instrument`: 8 Clips (`U12` `A`–`D`, `J2.1`, `J2.2`, `U6.2Y`, `U6.1Y`), 2 Hooks (`U8.QA`, `U5.QB`), probe, meter lead; knob 0.42; `powerOn(1, 300_ms)`, `waitIdle`; `DS1`/`DS2` lit as 4 and 2 (`session.leds()`); nothing aborts (watch budget: 16 + 8 + 2 + 2) |
| existing loops | `boards_test`, `electrical_test`, `board_scene_test`, `manuals_test` gain `"knob"`; `drc_knob` |

UI check (release build): `bw_ui puzzles/odd_tens.bwp --knob J2:42% --screenshot /tmp/knob.png --after 1s` shows the digits reading 52 (odd tens) and the Knob section at 42 %; a fault-free check on `netlists/knob.bwn` reads 42. The status line stays 1.00× with every instrument attached (`--probe`, `--clip`, `--hook`, `--meter`).

# P4d Push Button — Detailed Design

Implements [PUSH_BUTTON.md](PUSH_BUTTON.md): an on-board push button Part, pressed from the Board view or the command line, and the tests of the dice. The Machine's netlist, Board, manuals and Puzzles are in [DICE_DESIGN.md](../machines/030-dice/DICE_DESIGN.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/parts/button.hpp, button.cpp     Button Component
src/machine/press.hpp, press.cpp     ButtonPress, ParsePress ("SW1:200ms:700ms")
datasheets/BUTTON.md                 Datasheet (checked by src/manual)
tests/parts/button_test.cpp
tests/machine/press_test.cpp
tests/machines/dice_test.cpp
```

The Part lives with the other Parts. `ParsePress` sits next to `ParseRepair` in `bw_machine`, which both `bw` and `bw_ui` link and which already reaches `ParseDuration` (`bw_netlist`). Everything else is small changes to existing files (§3).

## 2. Button Part

```cpp
// src/parts/button.hpp
inline constexpr Time kButtonDelay = 1;  // ps: a press shows right after the wake

// Push button between a rail and a signal (PUSH_BUTTON.md 2). Pin index = KiCad pad − 1.
// Pressed: the signal Pin drives the rail's value at Strong, TTL family, so it beats a pull-up
// and fights a TTL output like a second one would. Released: both Pins drive Z.
class Button final : public Component {
public:
    explicit Button(std::string reference);  // Pins {"1", Output, Ttl}, {"2", Output, Ttl}

    void attach(PinIndex signal, LogicValue rail);  // netlist builder, before finalize()
    PinIndex signal() const { return signal_; }

    // Any thread. Takes effect at the Button's next evaluation: follow with Simulator::wake
    // (headless) or Runner::wake (live).
    void setPressed(bool pressed) { pressed_.store(pressed, std::memory_order_relaxed); }
    bool pressed() const { return pressed_.load(std::memory_order_relaxed); }

    void powerOn(PowerOnContext& ctx) override;  // both Pins Z; pressed_ untouched
    void evaluate(EvalContext& ctx) override;    // signal_ ← pressed ? rail_ : Z

private:
    PinIndex signal_ = 0;
    LogicValue rail_ = LogicValue::Zero;
    std::atomic<bool> pressed_{false};
};
```

- Both Pins are `Output` so either can be the signal end; the rail end is never connected by the builder (§3.3), so it drives Z on a private Net. `DescribeNet` lists the signal Pin as a Driver (`PinRole::Output`): hovering `BTN_N` marks `SW1` pad 1 with `R9`, which is what a pressed button is.
- `pressed_` is the only state the UI thread writes. The command that follows (§3.5) goes through the Runner's mutex, so the simulation thread sees the store before the wake; relaxed is enough.
- Catalogue ([catalogue.hpp](../../src/parts/catalogue.hpp#L15)): `enum class PartKind { Chip, Resistor, Connector, Led, Button };`. Entry, sorted by name, between `74LS86` and `LED`: `PartInfo{"BUTTON", "push button", "TACT-6x3.5", Button, &CreateButton}`. `CreateButton` (no parameters) goes above `using enum PartKind;`, next to `CreateLed`, for the same name clash.
- Datasheet `BUTTON.md` in the shape of [R.md](../../datasheets/R.md): Function (two contacts joined while pressed; one end on 0 V or +5 V, the other on a signal with a pull-up or pull-down; reads the rail while pressed), Pins (`1`, `2`: the two contacts, either way round).

## 3. Integration Points

1. **Simulator** — [simulator.hpp](../../src/sim/simulator.hpp#L28), after `runUntil`:

   ```cpp
   void wake(ComponentId component);  // evaluate at now(), in the next runUntil
   ```

   `Check`s the index, then `queue_.push({now_, ToIndex(component), MakeStamp(0, true, Zero)})` as `scheduleWake` with a zero delay. `runUntil` already takes events at `now_` (`top().time <= end`). Not on the hot path.

2. **KiCad footprint** — [kicad_netlist.cpp](../../src/board/kicad_netlist.cpp#L15): `kFootprints` grows to 7 with `{"TACT-6x3.5", "Button_Switch_THT:SW_PUSH_1P1T_6x3.5mm_H5.0_APEM_MJTP1250"}`: pads `"1"` (rect) and `"2"` (circle) 6.5 mm apart, 2.25 mm, drill 1.3 mm.

3. **Netlist builder** — [builder.cpp](../../src/netlist/builder.cpp#L57): `wireButtons()` after `tieResistors()` (§5.1). It fills `std::set<std::pair<std::string, PinIndex>> notConnected_` with each Button's rail end; the connect loop ([L75](../../src/netlist/builder.cpp#L75)) skips those Pins (`else if (notConnected_.contains(pin)) {}` before `part.component != nullptr`), but still records the Connection, so `PinNet(…, "SW1", "2")` is `GND`. New in [builder.hpp](../../src/netlist/builder.hpp):

   ```cpp
   // The Button Part `reference` of this build; nullptr if none (unknown, not a Button, or left
   // out of an electrical build).
   Button* ButtonOf(const LoadedCircuit& loaded, std::string_view reference);
   ```

   Looks the reference up in `loaded.parts` (kind `Button`) and `loaded.components`, then `static_cast`s.

4. **Electrical netlist** — [electrical.cpp](../../src/machine/electrical.cpp#L122): Buttons are left out of a damaged build like resistors that no longer pull (§5.2).

5. **Runner** — [runner.hpp](../../src/runner/runner.hpp#L61): `CommandKind::Wake`; `Command` gains `ComponentId component{};`.

   ```cpp
   void wake(ComponentId component);  // command: Simulator::wake at the next slice boundary
   ```

   `apply`: `if (state_.load() != RunState::Off) { sim_.wake(command.component); }`. Off: nothing to wake; power-on evaluates every Component anyway.

6. **Board reader** — [kicad_board.cpp](../../src/board/kicad_board.cpp#L192): courtyards drawn as lines (§5.3).

7. **Board scene** — [board_scene.hpp](../../src/view/board_scene.hpp#L74), after `componentAt`:

   ```cpp
   // Component whose courtyard contains `at` while none of its pads covers it: its body.
   std::optional<std::size_t> bodyAt(Point at) const;
   ```

8. **Session** — §6.
9. **Board view and frame loop** — §7.
10. **Command lines** — §8.

## 4. Press Parsing

```cpp
// src/machine/press.hpp
struct ButtonPress {
    std::string reference;  // "SW1"
    Time down, up;          // simulated time, ps; down < up
};

// "SW1:200ms:700ms" (durations as ParseDuration); error text otherwise.
std::expected<ButtonPress, std::string> ParsePress(std::string_view text);
```

Errors: not three `:`-separated fields, empty reference, a bad duration, `up <= down`.

## 5. Key Algorithms

### 5.1 Wiring Buttons (builder)

```
for each placed part of kind Button with a component:
    a = pinNet_[(ref, 0)], b = pinNet_[(ref, 1)]
    if a or b missing, or IsRail(a.name) == IsRail(b.name):
        error(line, ref + ": a button needs one pin on GND or VCC and the other on a signal net")
        continue
    rail = IsRail(a.name) ? 0 : 1; signal = 1 − rail
    button.attach(signal, pinNet_[(ref, rail)].name == "VCC" ? One : Zero)
    notConnected_.insert((ref, rail))
```

Same rule and wording style as `tieResistors`. A Button only on rails, or only on signals, is refused.

### 5.2 Buttons on a damaged Board

`NameIslands` keeps naming Button Pins like chip Pins (only resistors and power Pins are skipped), so an island holding only `SW1` pad 1 is still named, from `SW1`'s own Connection. In `ElectricalNetlist`, Button Connections go into a `buttons` map next to `resistors` instead of straight into `pins`, and get the same test:

```
for each (reference, ends) in buttons:
    a = namer.nameOf(island of ends[0]); b = namer.nameOf(island of ends[1])
    if a && b && IsRail(*a) != IsRail(*b): add both Pins to pins[*a], pins[*b]
    else: leftOut.insert(reference)     // e.g. its GND pad cut off: named GND~2
```

A left-out Button has no Component in the build: `ButtonOf` returns nullptr and pressing it does nothing (PUSH_BUTTON.md 2). The Dead button Puzzle keeps `SW1`: its signal island is named `BTN_N`, not a rail (DICE_DESIGN.md, Electrical Nets).

### 5.3 Courtyard from lines (Board reader)

The new footprint draws its `F.CrtYd` with `fp_line`s, not `fp_rect`/`fp_poly`. While reading a footprint, collect the `start`/`end` points (local coordinates) of every `fp_line` on a `.CrtYd` layer; after the child loop, if `component.courtyard` is still empty and points were collected, it becomes the 4 corners of their bounding box, each through `place` (rotation as for `fp_rect`). Existing footprints (`fp_rect`, `fp_poly`) are unchanged: those win when present.

### 5.4 Pressing in the Board view

Each frame the frame loop gives the view its Buttons (`ButtonTag`, §7). In `BoardView::input`:

```
on left click over the panel:
    k = scene_->bodyAt(at)
    if k is a Button: held_ = k; return before pressed_ = true   // no pan, no tool click
on left release (anywhere, also off the panel): held_.reset()
on right release over the panel, with the right drag never past ImGui's drag threshold:
    k = scene_->bodyAt(at); if k is a Button: latchToggle_ = k
```

Right-drag still pans. The tool never sees a press: `click_` is not set for it.

### 5.5 What is pressed (frame loop)

```
if !session.powered(): latched.clear()
if toggled = boardView.takeLatchToggle() and session.powered(): flip latched[ref of toggled]
for each ref in session.buttons():
    want = session.powered() && (latched has ref || boardView.held() is ref's component
                                 || a --press of ref covers status.simTime)
    session.setPressed(ref, want)
    tag = {component of ref, session.pressed(ref)}
boardView.setButtons(tags)
```

`Session::setPressed` only acts on a change, so calling it every frame costs nothing.

### 5.6 Drawing

Like the LEDs ([board_view.cpp](../../src/ui/board_view.cpp#L389)), under the pads:

```
for each ButtonTag:
    box = bounding box of scene_->courtyard(component); centre = box centre
    radius = kButtonCapShare × min(box width, box height)      // 0.35: ≈ 1.4 mm on 4 mm
    AddRectFilled(box, kButtonBody)                            // dark grey housing
    pressed: AddCircleFilled(centre, radius × 0.85, kButtonCapDown)
    else:    AddCircleFilled(centre, radius, kButtonCap); AddCircle(…, kButtonRim)
```

`kButtonBody = IM_COL32(40, 40, 45, 255)`, `kButtonCap = IM_COL32(170, 170, 175, 255)`, `kButtonCapDown = IM_COL32(95, 95, 100, 255)`, `kButtonRim = IM_COL32(230, 230, 235, 255)`. The courtyard outline and reference are drawn later, on top, as for any Component.

## 6. Session

[session.hpp](../../src/session/session.hpp#L126), next to `leds()`:

```cpp
// Every Button Part of the circuit, in netlist order.
std::vector<std::string> buttons() const;
// Ignored while not powered. Applied at the next slice boundary; power-off releases all.
void setPressed(std::string_view reference, bool pressed);
bool pressed(std::string_view reference) const;
```

- Member `std::set<std::string, std::less<>> pressed_;` (references held down).
- `setPressed`: `if (!powered_ || pressed == pressed_.contains(reference)) return;` update `pressed_`; then if `Button* b = ButtonOf(*live_->circuit, reference)`: `b->setPressed(pressed); live_->runner->wake(b->id());`. A left-out Button is still drawn pressed, and does nothing.
- `powerOff`: every Button of the live circuit `setPressed(false)` (no wake: the Runner is going Off), `pressed_.clear()`. A rebuild at the next power-on makes new Buttons, released.
- `buttons()`: `PartKind::Button` parts of `machine_->designCircuit()`, else of `live_->circuit` (no Board: nothing to click, but `--press` still works).

## 7. Board View and Frame Loop

[board_view.hpp](../../src/ui/board_view.hpp#L105):

```cpp
struct ButtonTag {
    std::size_t component;
    bool pressed;
};

void setButtons(std::span<const ButtonTag> buttons);
std::optional<std::size_t> held() const { return held_; }       // left button down on a body
std::optional<std::size_t> takeLatchToggle();                   // right-clicked body
```

Members `std::vector<ButtonTag> buttons_`, `std::optional<std::size_t> held_, latchToggle_`. Input §5.4, drawing §5.6.

Frame loop ([main.cpp](../../src/ui/main.cpp#L482-L490)), after the LED tags: §5.5, with `std::set<std::string> latched` declared before the loop.

## 8. Command Lines

- **`bw run`** ([cli/main.cpp](../../src/cli/main.cpp#L38)): `--press SW1:200ms:700ms,…` → `Options::press` (`std::vector<ButtonPress>`), usage text updated. After loading, each reference must have `ButtonOf(loaded, ref)`, else usage error `"'SW1' is not a button"`. The stepped loop (today for `--tv`/`--probe`) also runs when presses are given; each step ends at the earliest of `end`, `now + 1 ms` and the next press edge; at an edge, `button->setPressed(down)` and `sim.wake(button->id())`. A press edge at `t` changes the signal at `t + kButtonDelay`.
- **`bw_ui`** ([ui/main.cpp](../../src/ui/main.cpp#L47)): same option, same parsing; `Options::press`. Unknown references are ignored (the frame loop just never finds them). Applied by §5.5 against the Runner's `simTime`.

## 9. Reused Infrastructure

- `Component`, `PowerOnContext::setOutput`, `EvalContext::drive`, `DriveOf` (Output Z → no drive), `Resolver` (TTL contention → 0): the Button is an ordinary Component.
- `Simulator`'s wake events (`scheduleWake`, `StampIsWake`): `wake` is the zero-delay public form.
- The Runner's command queue and slice boundaries; `Session` rebuild at power-on.
- `tieResistors`' rule and `ElectricalNetlist`'s left-out resistors: Buttons follow both.
- `BoardScene::courtyard`, `componentIndex`, `Covers`/`Contains`; LED drawing (P4a) as the model for §5.6.
- `ParseDuration`; `SplitCommas` in both command lines.
- Existing Parts (74LS161, 74LS04, 74LS00, 74LS02, OSC, R, LED, POWER), `LedConsumer`, faults `crack` and `stuck`, Repairs Mend and Swap.

## 10. Build Integration

```cmake
# src/parts/CMakeLists.txt
add_library(bw_parts STATIC button.cpp catalogue.cpp ...)
# src/machine/CMakeLists.txt
add_library(bw_machine STATIC crack.cpp electrical.cpp machine.cpp press.cpp puzzle.cpp repair.cpp)
# tests/CMakeLists.txt: machine/press_test.cpp, machines/dice_test.cpp, parts/button_test.cpp
```

No new dependencies.

## 11. Dependency Diagram

```
bw_sim (wake) ← bw_parts (Button) ← bw_netlist (wireButtons, ButtonOf) ← bw_board ← bw_machine (press, electrical)
bw_sim ← bw_runner (Wake command) ← bw_session (setPressed) ← bw_ui (BoardView, --press)
bw_machine ← bw_cli (--press)
```

One direction only; the Runner knows Components only by `ComponentId`.

## 12. Testing

Catch2, tags as the module. The dice counter cycles every 6 µs, so headless rolls are short; Puzzle tests need a few milliseconds of simulated time each.

| File | Checks |
|---|---|
| `tests/sim/simulator_test.cpp` | `wake` evaluates a Component at `now()` in the next `runUntil`; a wake while nothing else is queued still runs |
| `tests/parts/button_test.cpp` `[parts]` | `FindPart("BUTTON")`: kind `Button`, package `TACT-6x3.5`, Pins `1`, `2` Output/TTL; released: a pulled-up Net stays 1, not floating; pressed + `wake` → 0 at `now + kButtonDelay`; released again → 1; VCC rail drives 1; pressed against a TTL output driving 1 → 0 (TTL contention); after power-on, released, both Pins drive nothing (no random 0/1 left on them) |
| `tests/netlist/builder_test.cpp` | Button between `GND` and a pulled-up signal builds, `PinNet(SW1, 2)` is `GND`; pin order reversed works; both ends on signals → error naming `SW1`; both on rails → error; `ButtonOf` finds `SW1`, nullptr for `R1` and unknown |
| `tests/machine/electrical_test.cpp` | dice with its `SW1` GND pad cut off (crack on that Trace): `SW1` left out of the build; dead_button crack: `SW1` kept, its pad 1 Net is `BTN_N`, `U4` pin 3's Net is `BTN_N~2` |
| `tests/board/kicad_board_test.cpp` | a footprint with only `fp_line` `F.CrtYd` gets the 4-corner bounding box, rotated with the footprint; `fp_rect` still wins |
| `tests/view/board_scene_test.cpp` | `bodyAt` on the dice: `SW1` between its pads, nullopt on a pad, nullopt off every courtyard |
| `tests/machine/press_test.cpp` | `"SW1:200ms:700ms"` parses; missing field, bad duration, `up <= down`, empty reference → errors |
| `tests/runner/runner_test.cpp` | a Button behind a pull-up in a Runner: `setPressed(true)` + `wake` → the Net's Samples show 0 within the next slice; while Off, `wake` does nothing |
| `tests/session/session_test.cpp` `[session]` | dice: `buttons()` is `{"SW1"}`; `setPressed` while off is ignored; powered, held 50 ms → `D4` brightness ≈ 0.5, `D3` ≈ 1/6; released, after 50 ms every LED reads 0 or 1; `powerOff` → `pressed("SW1")` false |
| `tests/machines/dice_test.cpp` `[machines]` | netlist loads without warnings; Puzzles have a title and a brief, `machine dice`; fault-free: six fresh power-ons (seed 1), each a press of 100 + k µs (k = 0…5) → six different faces 1–6, LEDs (cathode Net low) matching the face; per Puzzle, Symptom then Repair (DICE.md, Puzzles): dead_button — `N0`–`N2` unchanged by a press, `SW1` pad 1 Net low while pressed, `U4` pin 3 Net high, mend → faces change; corners — `U2` pin 3 Net floating, `FOUR_N` 0 after every roll, mend → six faces; seven — `SIX_N` always 1, rolls reach `N` = 0 and 7, swap `U3` → six faces; odd_dice — `N0` always 1, faces only 1, 3, 5 or all seven lit, swap `U1` → six faces |
| existing | `boards_test`, `electrical_test`, `board_scene_test`, `manuals_test` loops gain `"dice"`; `drc_dice`; `catalogue_test`, `kicad_netlist_test` |

UI check (release build): `bw_ui netlists/dice.bwn --press SW1:200ms:700ms --screenshot /tmp/dice.png --after 1s` shows one face lit and `SW1` drawn released; with `--after 500ms`, every LED glows and `SW1` is drawn pressed. Status line 1.00× with every instrument attached.

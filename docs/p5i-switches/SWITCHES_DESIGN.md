# P5i Switches on the Board — Detailed Design

Implements [SWITCHES.md](SWITCHES.md): a switched Pin in the kernel, one `Switch` Part for `BUTTON` and `KEY` joined by a Board-wide `SwitchNetwork`, the 74145, `fault switch`, keycaps and PC keys in `bw_ui`, a bench netlist.

Recipes: [ADD_PART.md](../recipes/ADD_PART.md) (74145: "Chip with state or special logic", Files, the decoder X folding of `74LS138`), [ADD_BOARD_DEVICE.md](../recipes/ADD_BOARD_DEVICE.md) (input device chain, rewritten in §12), [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md) (Fault lines). All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Kernel — `src/sim/`

### 1.1 Pin kind — `logic.hpp`

```cpp
enum class PinKind : std::uint8_t { Input, Output, OpenCollector, Bidirectional, Power, NoConnect,
                                    Switched };
```

- `IsDriver(Switched)` true, `IsReader(Switched)` false. Not a reader, so it is in no `FanOut` and adds nothing to `hasTtlInput`. The component is woken by §1.4 instead.
- `DriveOf(Switched, family, v)`: `{v, v == Z ? None : Supply, family}`. At Supply the group's value is forced on each of its Nets. That value already accounts for every Driver of the group (§2.3), so only another Supply drive of the other value can conflict (two rails of opposite value joined, or a stuck bidirectional Pin's tie), and that gives X, as a short should.
- `Resolver` gains:
  - `void merge(const Resolver& other)`: the same rules as `add` applied to another accumulation (stronger replaces; equal: `conflict_ |= other.conflict_ || other.value_ != value_`, `sawX_ |=`, `allTtl_ &=`; `None` ignored);
  - `bool operator==(const Resolver&) const = default;`
  - `DriveStrength strength() const` (for tests).

### 1.2 Circuit — `circuit.{hpp,cpp}`

- `buildTables` fills `netSwitchPin_[net]`: the net's `Switched` Pin, else `kNoPin`. `Check`: at most one per Net ("circuit: two switched pins on one net").
- `PinId switchPin(NetId) const`.
- `singleDriver` is unchanged: it needs a totem-pole `Output`, so a switched Net never takes the fast path.

### 1.3 Simulator state — `simulator.{hpp,cpp}`

- `NetState` gains `bool switched` (it fits in the padding: the `static_assert(sizeof == 16)` holds), set in the constructor from `switchPin`.
- New `struct External { Resolver drives; Time changedAt; };` and `std::vector<External> external_` (one per Net, used only where `switched`).
- `Resolver externalDrives(NetId net) const`: the tie plus every Driver except `switchPin(net)`.
- A switched Net is resolved from `external_` (updated first, §1.4), the switch Pin's drive merged in, then `finish`: `externalDrives` runs once per dirty switched Net. A Net that is not switched is resolved as today.
- `powerOn`: a `Switched` output starts at Z with no random draw, like a bidirectional Pin. Before the Nets are resolved, each switched Net's `external_` is set with `changedAt = -1`. No new Net and no new random draw: a Circuit without switches powers on bit for bit as before.

### 1.4 Waking — `runUntil`

In the dirty-Net loop, before resolving the Net:

```cpp
if (s.switched) {
    const Resolver drives = externalDrives(net);
    External& e = external_[ToIndex(net)];
    if (drives != e.drives) {
        e = {drives, now_};
        markComponentDirty(circuit_.pinInfo(circuit_.switchPin(net)).owner);
    }
}
```

The component's own drive dirties the Net, but leaves `drives` unchanged, so it is never woken by its own echo. A Net that is not switched pays one byte test, in a cache line already loaded. Measured in the plan's phase 1 (`bus_load`, 070).

### 1.5 EvalContext — `component.hpp`, `simulator.{hpp,cpp}`

For `Switched` Pins only (`assert`):

```cpp
const Resolver& external(PinIndex pin) const;  // the Pin's Net's drives but its own
bool externalChanged(PinIndex pin) const;      // external(pin) changed at this timestamp
```

And for any Component: `void wake(ComponentId other, Time delay)` (delay ≥ 1 ps), so a switch can wake the network.

## 2. Switch Parts — `src/parts/switch.{hpp,cpp}` (replace `button.{hpp,cpp}`)

### 2.1 `Switch`

```cpp
inline constexpr Time kSwitchDelay = 1;  // ps: a press shows right after the wake

enum class SwitchFault : std::uint8_t { None, StuckClosed, NeverCloses };

// A push button or key: two legs ("1", "2", pad numbers) joined while closed; the SwitchNetwork
// joins their Nets. Its Pins are TTL Outputs that always drive Z, as the P4d Button's were, so a
// build draws the same random bits at power-on. Pin index = KiCad pad − 1.
class Switch final : public Component {
public:
    Switch(std::string reference, std::string legend, std::string pcKey);
    std::string_view legend() const;  // "" for a BUTTON or a blank key
    std::string_view pcKey() const;   // "" none; else as IsPcKey
    // Any thread. Takes effect at this switch's next evaluation: follow with Simulator or
    // Runner wake(id()), as for the P4d Button.
    void setPressed(bool pressed);
    bool pressed() const;
    void setFault(SwitchFault fault);  // builder, before power-on
    SwitchFault fault() const { return fault_; }
    bool closed() const { return closed_; }  // simulation thread: as of the last evaluation
    void powerOn(PowerOnContext& ctx) override;  // both Pins Z; closed_ = StuckClosed
    void evaluate(EvalContext& ctx) override;
private:
    friend class SwitchNetwork;
    SwitchNetwork* network_ = nullptr;  // nullptr: not joining two Nets (open leg)
    bool closed_ = false;
    ...
};
```

`evaluate`: `down = StuckClosed || (pressed() && fault != NeverCloses)`. If it differs from `closed_`, set it, and if attached, `network_->changed()` and `ctx.wake(network_->id(), kSwitchDelay)`. The press is read only at the switch's own wake (at `now + PressJitter()` from the Session), so it lands at a deterministic time whatever else wakes the network, and all the switch state is read and written by the simulation thread (`pressed_` stays the only atomic). A switch with an open leg is never attached: pressing it does nothing.

`bool IsPcKey(std::string_view)`: one printable ASCII character other than space and `#`, or one of `Space`, `Enter`, `Tab`, `Backspace`, `Escape`, `Up`, `Down`, `Left`, `Right`. Letters are stored in upper case. F2 is not a key name: it toggles Typing.

### 2.2 `SwitchNetwork`

```cpp
// The Board's switches as one Component (P5i): a Switched Pin on each Net a switch leg is on,
// named after the Net. Closed switches join Nets into groups; each group is driven with the
// combined drives of its Nets.
class SwitchNetwork final : public Component {
public:
    explicit SwitchNetwork(std::span<const std::string> netNames);
    void add(Switch& member, PinIndex a, PinIndex b);  // builder: legs on Pins a ≠ b
    void changed() { rebuild_ = true; }  // a member's closed() changed (simulation thread)
    void powerOn(PowerOnContext& ctx) override;  // rebuild_ = true
    void evaluate(EvalContext& ctx) override;
private:
    struct Member { Switch* member; PinIndex a, b; };
    std::vector<Member> members_;
    std::vector<PinIndex> parent_;        // union-find over Pins, rebuilt
    std::vector<PinIndex> order_;         // Pins of the groups, group by group
    std::vector<std::uint32_t> start_;    // group g: order_[start_[g], start_[g + 1])
    std::vector<std::uint8_t> grouped_;   // per Pin: in a group now
    bool rebuild_ = true;
};
```

The vectors are sized in the constructor and in `add` (at most every Pin in groups). Neither `evaluate` nor a rebuild allocates.

### 2.3 Evaluation

```text
evaluate:
  if rebuild_: rebuild_ = false
      union-find the legs of every closed() member; groups = sets of ≥ 2 Pins
      every Pin that was grouped and is no longer: drive Z
      resolve every group
  else:
      resolve each group with a Pin whose externalChanged() (none: nothing to do)

resolve group:
  Resolver r
  for pin in group: r.merge(ctx.external(pin))
  res = r.finish(false, Zero)   // a floating result is not used
  value = res.floating ? Z : res.value
  for pin in group: ctx.drive(pin, value, kSwitchDelay)
```

- A floating group drives Z: each Net then floats as it would alone (a TTL input reads 1). Nets that float together may read differently. That is a small inexactness, and no key matrix shows it: the columns are pulled up.
- One wake can follow the other at the same timestamp (a Driver changes, then the network's drive lands 1 ps later). `drive` skips an unchanged value, so a settled group costs only the merge.
- At power-on every switch is open except the stuck-closed ones (`Switch::powerOn`, which runs before any evaluation). The network's power-on evaluation rebuilds.

## 3. Catalogue — `src/parts/catalogue.{hpp,cpp}`

- `PartKind::Button` → `PartKind::Switch` (grep `PartKind::Button`: builder, electrical, session, net_pins, chip panel, manual).
- `CreateSwitch<"BUTTON">` / `<"KEY">`: `BUTTON` takes `pc=`, `KEY` takes `legend=` (1–4 printable characters, not `#`: the netlist parser cuts comments there) and `pc=`. An unknown name, a bad value or a repeated parameter is an error text. A `KEY` whose one-character legend is an `IsPcKey` gets that as its `pc` unless `pc=` is given.
- Rows: `PartInfo{"BUTTON", "push button", "TACT-6x3.5", Switch, …}`, `PartInfo{"KEY", "key switch", "MX-1U", Switch, …}`, sorted.
- No required parameter, so `PartPinNames` and the catalogue tests build both as they are (ADD_PART, gotchas).

## 4. Footprint

- `boards/footprints/SW_Cherry_MX_1.00u_PCB.kicad_mod` copied unchanged from KiCad 10.0.7 (`Button_Switch_Keyboard.pretty`). Pads `1` (0, 0) and `2` (−6.35, 2.54); three unnamed `np_thru_hole` holes, which `footprint.cpp` already skips. The courtyard is 13.2 mm square, centred on (−2.54, 5.08).
- `kFootprints` (15 → 16): `{"MX-1U", "Button_Switch_Keyboard:SW_Cherry_MX_1.00u_PCB"}`.
- Not a grid package (`IsGridPackage`). Keys are placed by `.place` rules when the BW-1 needs them (P5j).

## 5. Builder — `src/netlist/builder.{hpp,cpp}`

```cpp
// Switch Fault: the switch is stuck closed (`closed`) or never closes.
struct StuckSwitch {
    std::string reference;
    bool closed;
};
LoadResult BuildCircuit(const Netlist&, std::span<const StuckPin> = {},
                        std::span<const std::string> unpowered = {},
                        std::span<const StuckCell> cells = {},
                        std::span<const StuckSwitch> switches = {});
Switch* SwitchOf(const LoadedCircuit& loaded, std::string_view reference);  // was ButtonOf
```

`wireSwitches()` replaces `wireButtons()`:

- Each `Switch` Part: one leg is connected to its Net as `wireButtons` connected a button's signal leg (the first leg not on a rail, else leg 1), the other goes in `notConnected_` and gets a private Net at finalize, as the rail leg did. The Circuit then has the same Nets, Drivers and random draws as with the P4d Button. Both legs are recorded in `connections`. A leg on a listed Net gives that Net; a leg on no Net is open.
- Distinct Nets of all the legs, in `NetId` order. If there are any, `circuit.add<SwitchNetwork>(names)` and connect Pin i to Net i. Then `network.add(switch, a, b)` for every switch with two legs on two different Nets.
- PC keys: a `pc` already taken by an earlier switch → error `"SW3: PC key G is already SW2's"`.
- The rail rule and its error go.
- After the error return, each `StuckSwitch` sets `setFault` (`Check`: a Switch).

The network is not in `loaded_->components` or `parts`: it has no reference and no pads.

## 6. Electrical netlist — `src/machine/electrical.cpp`

Switches leave `railParts` (resistors only now, same rule). Their pins are named by their islands like a chip's, so a crack or a bridge on a key's copper moves its leg to another Net with no special code. The `"resistor or button"` texts become `"resistor"`.

## 7. 74145 — `src/parts/ttl145.{hpp,cpp}`

`class Ttl145 final : public Component`, `&CreateChip<Ttl145, "74145">`, `PartInfo{"74145", "BCD-to-decimal decoder/driver, open collector", "DIP-16", Chip, …}`.

- Pinout (TI): 1 `Y0_N`, 2 `Y1_N`, 3 `Y2_N`, 4 `Y3_N`, 5 `Y4_N`, 6 `Y5_N`, 7 `Y6_N`, 8 `GND`, 9 `Y7_N`, 10 `Y8_N`, 11 `Y9_N`, 12 `D`, 13 `C`, 14 `B`, 15 `A`, 16 `VCC`. Active-low outputs named as on the 74LS138 (NAMING.md): a bare `0` would read as a pad number.
- `Y0_N`–`Y9_N` `OpenCollector`, TTL. `A`–`D` inputs (`A` the low bit).
- `evaluate`: the 74LS138's folding with no enable. The fixed bits of `DCBA` select. Output `k` (0–9) drives 0 when every bit is known and the code is `k`, X when `k` matches the known bits and some bit is X, and 1 (released) otherwise. Codes 10–15 release all ten.
- `delays::kTtl145{25'000, 20'000}`: release 25 ns and pull-down 20 ns. These are assumed typicals: the TI sheet gives maximums of 50 ns, which the commit says.
- `datasheets/74145.md`. Its power pins are at the corners.

## 8. Puzzle and Machine — `src/machine/puzzle.{hpp,cpp}`, `machine.{hpp,cpp}`

```cpp
struct SwitchDecl {
    std::uint32_t line;
    std::string reference;
    bool closed;  // `closed`: stuck closed; `open`: never closes
};
// Puzzle:
    std::vector<SwitchDecl> switches;
```

- `fault switch REF closed|open`: exactly 4 tokens, else `"expected: fault switch REF closed|open"`. The catch-all becomes `"expected: fault stuck|crack|bridge|joint|cell|column|switch ..."`.
- `LoadMachine`: unknown reference → the existing text; a Part that is not `PartKind::Switch` → `"switch R1: buttons and keys only"`. Stored as `StuckSwitch` in `switches_`.
- `swap` also erases the reference's `switches_` (a switch is in `designCircuit_->components`, so `swap` already accepts it). `build()` passes `switches_`.

## 9. Session, CLI — `src/session/`, `src/cli/main.cpp`, `src/ui/main.cpp`

- `Session::buttons()` → `switches()`: every `PartKind::Switch`, netlist order.
- `struct SwitchKey { std::string reference, pc; }; std::vector<SwitchKey> switchKeys() const;`: switches with a PC key, read from the design circuit's `Switch` components.
- `setPressed`: `SwitchOf`, then `runner->wake(sw->id(), PressJitter())` as today. `powerOff` releases as today.
- `bw run` and `bw_ui` `--press`: `SwitchOf`, wake `id()`; `"is not a switch"`.

## 10. UI — `src/ui/`

### 10.1 Keycap — `keycap.{hpp,cpp}` (new)

`Darker`, `IsLetter`, `Centred`, `DrawLegends`, `DrawCap` and their colours move out of `keyboard_window.cpp` unchanged, behind:

```cpp
// One cap in [a, b] (screen points), raised or pressed down; u = one key unit in points.
void DrawKeyCap(ImDrawList& list, ImFont* font, const KeyCap& cap, ImVec2 a, ImVec2 b, float u,
                bool down);
```

The keyboard window calls it. Its drawing is unchanged, so its screenshot is identical.

### 10.2 Board — `board_view.{hpp,cpp}`

- `ButtonTag` → `SwitchTag { std::size_t component; bool pressed; bool key; std::string legend; }`; `setButtons` → `setSwitches`; `setLegendFont(ImFont*)`.
- A button is drawn as today. A key is a `KeyCap{0, 0, 1, legend, "", CapKind::Character, 0}` drawn by `DrawKeyCap` over its courtyard square, with `u` = the courtyard's side in pixels. `DrawLegends` sizes a Character cap's single label by its length: 0.38u for one character, 0.30u for two, 0.22u for three or four (the keyboard's caps are all one character, so they are unchanged). The cap is the courtyard, not an 18 mm cap, so the copper between keys stays visible. Pads are drawn on top as for every Part.
- Clicks: left held, right latched, `bodyAt`, except under the Swap tool: `buttonAt()` gives nothing then (board_view.cpp:724-742), so a left click on a switch body is an ordinary click and reaches `SwapRepair` (main.cpp:467).

### 10.3 PC keys — `board_keys.{hpp,cpp}` (new)

```cpp
// Typing on Board keys (P5i): while on, a PC key held down holds the switch with that `pc`.
struct BoardKeyCapture {
    bool on = false;
    std::vector<SwitchKey> keys;  // Session::switchKeys(); empty: no Board keys
    // Before ImGui_ImplSDL3_ProcessEvent; true when the event was taken. F2 turns it off.
    bool handle(const SDL_Event& event);
    bool holds(std::string_view reference) const;
    void sync();  // off: nothing held
private:
    std::map<SDL_Scancode, std::string> held_;  // scancode → reference
};
```

- Key down (not a repeat): `SDL_GetKeyFromScancode(scancode, mod & SHIFT, false)`. A printable keycode is upper-cased and compared with a one-character `pc`. Otherwise the named keys map to `SDLK_SPACE`, `SDLK_RETURN`/`SDLK_KP_ENTER`, `SDLK_TAB`, `SDLK_BACKSPACE`, `SDLK_ESCAPE` and the arrows. The match is held under the scancode. Every key down is taken while on.
- Key up: erase its scancode and pass the event on, as `KeyboardCapture` does. `SDL_EVENT_WINDOW_FOCUS_LOST`: forget every held key (its key-up goes to another application).
- A Machine with both a KEYBOARD and Board keys with `pc=` types only on the keyboard: no Machine has both yet, noted in ADD_BOARD_DEVICE.
- `main.cpp`: on a Machine with `keys` and no KEYBOARD, F2 (no text input wanted) turns it on. The press merge becomes `latched || scripted || held on the Board || boardKeys.holds(ref)`. `toolbar.typing` is `"Board keys"` while on, so the bar reads "Typing on Board keys · F2 to stop".

## 11. Bench — `tests/data/switch_bench.bwn`

```text
part X1  OSC freq=1MHz
part J1  POWER
part U1  74LS93      # row counter
part U2  74145       # rows
part U3  74LS04      # column readers
part SW1..SW16 KEY legend=0..F   # row r = (k / 4), column c = (k % 4)
part SW17 BUTTON pc=Space
part R1..R5 R value=4k7          # column pull-ups, the button's
```

`X1.OUT` → `U1.CKA`, `U1.QA` → `U1.CKB`, `U1.R01`/`R02` on `GND`. `U1.QA`, `U1.QB` → `U2.A`, `U2.B`; `U2.C`, `U2.D` on `GND`; `U2.Y4_N`–`Y9_N` unconnected (outputs). `ROW0`–`ROW3` = `U2.Y0_N`–`Y3_N`. `COLc` pulled up by `R(c+1)`, read by `U3.(c+1)A`, its output `KEYc` = `U3.(c+1)Y`. Key `k` from `ROW(k/4)` to `COL(k%4)`. `SW17` from `GND` to `BTN_N` (pull-up `R5`, `U3.5A`, `U3.5Y` = `BTN`). `U3.6A` on `GND`. Row r is scanned while the count mod 4 is r, 1 µs each.

## 12. Docs

- [ADD_BOARD_DEVICE.md](../recipes/ADD_BOARD_DEVICE.md): the input chain rewritten (Switch Part, network, switched Pin, `SwitchOf`, the switch woken then waking the network, same random draws, PC keys and their limit beside a KEYBOARD, keycaps), reference commits.
- [ADD_PART.md](../recipes/ADD_PART.md): the 74145 as the open-collector decoder reference commit.
- [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md): `fault switch REF closed|open` in the Faults list.
- [GLOSSARY.md](../GLOSSARY.md): **Switch** (a button or key: two legs joined while closed), **Switch Fault**, **Ghost key**. Typing: also Board keys.
- `datasheets/BUTTON.md` (two legs, any two Nets), `datasheets/KEY.md` (new), `datasheets/74145.md` (new).
- [ARCHITECTURE.md](../ARCHITECTURE.md): the P5i row done, with date and links.

## 13. Tests

`tests/sim/switched_test.cpp` (new): a test `SwitchNetwork` built by hand with two or three Nets, `Stimulus` drivers, ties and a `Switch` attached through `add`:

- pull-up through a closed switch; rail beats TTL output; two TTL outputs → 0; an X → X; floating group → each Net alone; open after closed → each Net its own;
- a chain of three Nets through two switches;
- a Driver change in a group moves every Net of the group 1 ps later;
- echo: the network's own evaluation count (a counter in the test's network subclass or test component; `SimStats` is global) grows by one per external change and per press, never from its own drive;
- a press lands at the switch's wake time, not at an earlier network wake.

`tests/parts/switch_test.cpp` (replaces `button_test.cpp`; `power_on_reset_test.cpp`'s hand-built button rewritten with a `SwitchNetwork`): catalogue rows, params (`legend=`, `pc=`, defaults, errors), `IsPcKey`, `closed()` with Faults.

`tests/parts/ttl145_test.cpp`: 0–15, X folding, release, delays.

`tests/netlist/switch_bench_test.cpp`: §11 cases (no key, one key, ghost, a crack by netlist text, a bridge by netlist text, both Faults, the button).

`tests/netlist/builder_test.cpp`: the three button cases become: a key between two signals; an unlisted leg; duplicate PC keys; `SwitchOf`.

`tests/machine/electrical_test.cpp`: "button cut from GND" now stays in, its leg on the dead island.

`tests/machine/puzzle_test.cpp`, `machine_test.cpp`: `fault switch` parsing, errors, swap.

`tests/machines/dice_test.cpp`: `InlinePuzzle` `fault switch SW1 closed` rolls with no press, `open` never rolls.

`tests/session/session_test.cpp`, `tests/runner/runner_test.cpp`: button cases renamed; `switchKeys()` on the bench.

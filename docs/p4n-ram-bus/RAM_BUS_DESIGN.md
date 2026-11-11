# P4n RAM and bus — Detailed Design

Implements [RAM_BUS.md](RAM_BUS.md): bidirectional pins, the 2114 and 74LS244 Parts, the DIP-18 and DIP-20 packages, the `KEYBOARD` device behind its Connector, typing through the Session, the command line and the UI, and a bench netlist. No Machine, no Board.

Recipes followed: [ADD_PART.md](../recipes/ADD_PART.md) (2114, 74LS244, the packages), [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md) (the keyboard: every row of "The chain"). This design gives only what those recipes leave open.

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/parts/ram2114.hpp, ram2114.cpp       Ram2114 Component
src/parts/ls244.hpp, ls244.cpp           Ls244 Component
src/parts/keyboard.hpp, keyboard.cpp     Keyboard Component, KeyboardCode
src/machine/typing.hpp, typing.cpp       Typing, ParseTyping ("J1:200ms:HELLO")
src/ui/keyboard_capture.hpp, .cpp        KeyboardCapture (SDL events while typing)
boards/footprints/DIP-18_W7.62mm.kicad_mod, DIP-20_W7.62mm.kicad_mod,
    Molex_KK-254_AE-6410-10A_1x10_P2.54mm_Vertical.kicad_mod
datasheets/2114.md, 74LS244.md, KEYBOARD.md
tests/data/ram_bench.bwn                 bench netlist (§9)
tests/parts/ram2114_test.cpp, ls244_test.cpp, keyboard_test.cpp
tests/machine/typing_test.cpp
tests/netlist/ram_bench_test.cpp
```

Everything else is small changes to existing files (§10).

## 2. Bidirectional pins

### 2.1 Kind and Drive

[logic.hpp](../../src/sim/logic.hpp):

```cpp
enum class PinKind : std::uint8_t { Input, Output, OpenCollector, Bidirectional, Power, NoConnect };

// Drives its Net: an output of any kind, or a bidirectional pin.
constexpr bool IsDriver(PinKind kind) {
    return kind == PinKind::Output || kind == PinKind::OpenCollector ||
           kind == PinKind::Bidirectional;
}
// Reads its Net: an input or a bidirectional pin.
constexpr bool IsReader(PinKind kind) {
    return kind == PinKind::Input || kind == PinKind::Bidirectional;
}
```

`DriveOf`: `case PinKind::Bidirectional:` falls through to `Output` (Z → no drive, else Strong).

### 2.2 Circuit tables

[circuit.cpp](../../src/sim/circuit.cpp) `buildTables`: a Driver no longer `continue`s before the reader test. Per Pin: `if (IsDriver) drivers[n].push_back`; then `if (!IsReader) continue;` and the existing TTL-input flag and fan-out merge. A bidirectional TTL pin sets `netHasTtlInput_` (it is a TTL input as well). `netSingleDriver_` stays for `PinKind::Output` only (a bidirectional pin is Z most of the time; the fast path would fall back anyway). The fan-out is unchanged in shape: pins of one Component are contiguous, so a chip's address inputs and data pins on different Nets each get their entry, and a bidirectional pin's own change wakes its chip.

### 2.3 Power-on and reading before an edge

[simulator.cpp](../../src/sim/simulator.cpp) `powerOn`: a Driver's output starts at a random 0/1 as today, **except a bidirectional pin, whose `o.current` is set to Z explicitly** (released; skipping it in the random branch would leave the default `Zero`, a Strong 0). Comment on `Component::powerOn` in [component.hpp](../../src/sim/component.hpp): "Outputs already hold random 0/1, bidirectional Pins Z".

New on `EvalContext` (component.hpp, defined in simulator.cpp beside `readBit`):

```cpp
bool readBitBefore(PinIndex pin);  // readBefore, X → seeded random bit (MOS inputs, S4)
```

### 2.4 Builder

[builder.cpp](../../src/netlist/builder.cpp):

- The `stuck_` `Check` already accepts any `IsDriver` pin; it now also covers a bidirectional one.
- The connection loop: a stuck Driver ties its Net (unchanged, `IsDriver`). The "own Net" loop below gives a stuck pin its private tied Net when `kind == Input || kind == Bidirectional` (`IsReader`): a stuck bidirectional pin both forces the bus and reads its value. For a bidirectional pin that private Net is tied at `DriveStrength::Supply`, so the chip's own Strong drive onto it never fights the tie into X.
- `warnUnconnectedInputs`: `IsReader(kind) && family == Ttl` (a 74LS245 later; the 2114 is MOS, so no warning for its unused `IO4` on the bench). No catalogue Part has a TTL bidirectional pin yet, so this branch is untested until one does.

### 2.5 Faults, Drivers, the rest

- [machine.cpp](../../src/machine/machine.cpp) Puzzle `stuck`: `IsDriver(spec.kind) || chipInput` already accepts it; the error text "is not an output or a chip input" stays true. `joint` uses `chipSignalPin` (not Power, not NoConnect): accepted. A dry Joint leaves the pin off its Net in `ElectricalNetlist`; `finalize` gives it a private Net, on which it drives nothing anyone reads and reads floating (MOS: its last driven value). No code change; tests in §11.1.
- [net_pins.cpp](../../src/netlist/net_pins.cpp) `DescribeNet`: `IsDriver` → `PinRole::Output`, so a 2114 `IO` pin is listed among Drivers and its pad gets the Driver mark on hover (`BoardScene::drivers`). No code change.
- `Simulator::driveChanged`, `setOutput` assert `IsDriver`: true for the new kind.
- Grep for `PinKind::Input` and `IsDriver` after the change (§10 lists every hit and its verdict) so no switch misses the new kind.

## 3. 2114 static RAM

```cpp
// src/parts/ram2114.hpp
// 1K x 4 static RAM (Intel 2114), MOS, shared data pins. Pins per datasheet: 1 A6, 2 A5, 3 A4,
// 4 A3, 5 A0, 6 A1, 7 A2, 8 CS_N, 9 GND, 10 WE_N, 11 IO4, 12 IO3, 13 IO2, 14 IO1, 15 A9, 16 A8,
// 17 A7, 18 VCC. CS_N low, WE_N high: IO1-IO4 drive the word at A0-A9. CS_N and WE_N low: the
// word is written from IO1-IO4 (floating); the address and data held just before the write ends
// are kept. CS_N high: IO1-IO4 float. Inputs read X as a random bit.
class Ram2114 final : public Component {
public:
    explicit Ram2114(std::string reference);
    static constexpr PinIndex kCsN = 7, kWeN = 9;
    static constexpr std::array<PinIndex, 10> kAddress{4, 5, 6, 3, 2, 1, 0, 16, 15, 14};  // A0..A9
    static constexpr std::array<PinIndex, 4> kData{13, 12, 11, 10};                      // IO1..IO4

    void powerOn(PowerOnContext& ctx) override;  // random contents
    void evaluate(EvalContext& ctx) override;
    std::uint8_t word(std::size_t address) const { return words_[address]; }  // tests, 4 bits

private:
    std::array<std::uint8_t, 1024> words_{};
};
```

Pins: `A*`, `CS_N`, `WE_N` `{Input, Mos}`, `IO1`–`IO4` `{Bidirectional, Mos}`, `VCC`/`GND` `{Power, Ttl}` (power family as the ROMs').

Delays in [delays.hpp](../../src/parts/delays.hpp), Intel 2114 (450 ns grade) maximums, the only figures published (as the 2716's):

```cpp
inline constexpr Time k2114Address = 450'000;  // tAA, address to output
inline constexpr Time k2114Select = 120'000;   // tCO, CS_N low (or WE_N back high) to output
inline constexpr Time k2114Off = 100'000;      // tOTD / tWZ, CS_N high or WE_N low to float
```

```
evaluate:
    // A write in progress just before now: keep the word it was writing (zero hold time).
    if !readBitBefore(CS_N) && !readBitBefore(WE_N):
        words_[address from readBitBefore(A0..A9)] = data from readBitBefore(IO1..IO4)
    selected = !readBit(CS_N); writing = selected && !readBit(WE_N)
    address = readBit(A0..A9)
    if writing:
        words_[address] = readBit(IO1..IO4)
    if !selected || writing:
        drive IO1..IO4 Z, k2114Off
        return
    delay = 0
    if any A pin: read != readBefore  → delay = max(delay, k2114Address)
    if CS_N or WE_N: read != readBefore → delay = max(delay, k2114Select)
    if delay == 0: delay = k2114Address              // power-on, or only the data pins moved
    drive IOk = bit k of words_[address], delay
```

- `powerOn`: `words_[a] = 4 × ctx.randomBit()`; the data pins start Z (§2.3); the power-on `evaluate` then drives them after `k2114Address` if selected.
- An evaluation caused only by the data pins (another Driver on the bus, or the chip's own drive) re-drives the same word: `EvalContext::drive` does nothing when the value equals the pending or current one, so the inertial delay is not restarted.
- While the chip reads, the data pins read back its own drive; nothing is written, so that is harmless. A stuck `IO` pin (§2.4) both forces the bus and is written into every word.
- `readBitBefore` of a pin that did not change at this timestamp is its current value: the "before" write is the same write as the current one and stores the same word.
- No allocation: fixed arrays.
- Catalogue: `PartInfo{"2114", "1K x 4 static RAM", "DIP-18", Chip, &CreateChip<Ram2114, "2114">}`, first in `s_parts` (sorted as strings: `"2114" < "2513"`).
- Power at 9 and 18: the corners of a DIP-18; not in `kOffCornerPower`.
- Datasheet `2114.md`: Function (1024 words of 4 bits; `CS_N` low selects the chip; with `WE_N` high it drives the word at `A0`–`A9` on `IO1`–`IO4` (450 ns after an address change, 120 ns after `CS_N` falls); with `WE_N` low it writes `IO1`–`IO4` into that word and its outputs float, so another chip can drive them; the address and data must stay put until `WE_N` goes back high; `CS_N` high floats the outputs; the contents are random at power-on and lost when it goes off; MOS inputs: a floating pin reads its last value), Pins (`A0`–`A9`, `IO1`–`IO4`, `CS_N`, `WE_N`, `VCC`, `GND`).

## 4. 74LS244

```cpp
// src/parts/ls244.hpp
// Octal buffer, tri-state, two groups of four. Pins per datasheet: 1 1OE_N, 2 1A1, 3 2Y4, 4 1A2,
// 5 2Y3, 6 1A3, 7 2Y2, 8 1A4, 9 2Y1, 10 GND, 11 2A1, 12 1Y4, 13 2A2, 14 1Y3, 15 2A3, 16 1Y2,
// 17 2A4, 18 1Y1, 19 2OE_N, 20 VCC. Each Y follows its A while its group's OE_N is low, else Z.
class Ls244 final : public Component {
public:
    explicit Ls244(std::string reference);
    void evaluate(EvalContext& ctx) override;

private:
    std::array<LogicValue, 8> driven_{};  // last value driven per buffer, for the disable delay
};
```

A `kBuffers` table `{oe, a, y}` per buffer (index = pin − 1): group 1 `{0, 1, 17}`, `{0, 3, 15}`, `{0, 5, 13}`, `{0, 7, 11}`; group 2 `{18, 10, 8}`, `{18, 12, 6}`, `{18, 14, 4}`, `{18, 16, 2}`.

```
per buffer:
    oe = read(OE_N), a = read(A)
    if oe == 1:        value = Z
    else if oe == X:   value = X
    else:              value = a (X stays X)
    delay: value Z → driven_ was 1 ? kLs244Disable.rise : kLs244Disable.fall  (1→Z 10 ns, 0→Z 15 ns)
           driven_ Z  → DelayFor(kLs244Enable, value)                            (Z→1 15 ns, Z→0 20 ns)
           else       → DelayFor(kLs244Data, value)                              (12 ns, 12 ns)
    if value != driven_: driven_ = value
    drive(Y, value, delay)
```

- `driven_` is the last value scheduled, not the Net: it decides between data, enable and disable delays. A pulse on `OE_N` shorter than the delay is swallowed by the inertial `drive`; `driven_` then names a value that never appeared, which only picks a delay a few ns off. Said in a comment.
- `powerOn`: the outputs hold random 0/1 (default); `driven_` = those values (a `powerOn` override reading nothing; the first evaluation corrects the outputs within 20 ns). Simpler: `powerOn` sets every `Y` to Z with `setOutput` and `driven_` to Z, so the power-on evaluation enables the driven ones with the enable delay. Use this.
- Delays in delays.hpp (TI typicals, the PDF cannot be fetched: known values, said in the commit): `kLs244Data{12'000, 12'000}`, `kLs244Enable{15'000, 20'000}`, `kLs244Disable{10'000, 15'000}` (rise field = from/to 1, fall = from/to 0).
- Catalogue: `PartInfo{"74LS244", "octal buffer, tri-state", "DIP-20", Chip, &CreateChip<Ls244, "74LS244">}` between `74LS20` and `74LS27`.
- Power at 10 and 20: corners.
- Datasheet `74LS244.md`: Function (eight one-way buffers in two groups of four; while a group's `OE_N` is low each `Y` copies its `A`; while it is high the group's outputs float, so other chips can drive the same Nets: a bus driver; tie `OE_N` low for a buffer always on), Pins (`1A1`–`1A4`, `1Y1`–`1Y4`, `2A1`–`2A4`, `2Y1`–`2Y4`, `1OE_N`, `2OE_N`, `VCC`, `GND`).

## 5. Packages

- Copy `DIP-18_W7.62mm.kicad_mod`, `DIP-20_W7.62mm.kicad_mod` (`Package_DIP.pretty`) and `Molex_KK-254_AE-6410-10A_1x10_P2.54mm_Vertical.kicad_mod` (`Connector_Molex.pretty`) from the installed KiCad 10 into `boards/footprints/` unchanged (ADD_PART.md, "A new package").
- `kFootprints` in [kicad_netlist.cpp](../../src/board/kicad_netlist.cpp) grows from 10 to 13: `{"DIP-18", "Package_DIP:DIP-18_W7.62mm"}`, `{"DIP-20", "Package_DIP:DIP-20_W7.62mm"}` after DIP-16, `{"KK-10", "Connector_Molex:Molex_KK-254_AE-6410-10A_1x10_P2.54mm_Vertical"}` after KK-4. The rows go in before any catalogue row uses them (the footprint test loads every catalogue package).
- [placer.cpp](../../src/board/placer.cpp) `IsGridPackage`: `DIP-14`, `DIP-16`, `DIP-18`, `DIP-20`, `axial`. The slot is the union of the grid parts' courtyards; a DIP-20's (9.73 × 25.9 mm in KiCad 10) fits the 16 × 27 mm pitch, 1.1 mm between rows, so a Board with one only grows its slot along y within the pitch. A Board without them is placed exactly as before (the slot comes from the parts present): the 070 and 080 placements do not change.

## 6. KEYBOARD

### 6.1 Part

```cpp
// src/parts/keyboard.hpp
inline constexpr Time kKeyboardDelay = 100'000;           // ps: encoder output delay
inline constexpr Time kKeyboardSetup = 1'000'000;         // data steady before STROBE rises
inline constexpr Time kKeyboardGap = 1'000'000;           // STROBE low before the next key
inline constexpr Time kKeyboardStrobe = 10'000'000;       // default strobe=
inline constexpr std::size_t kKeyboardWaiting = 32;       // keys waiting at most

// The 7-bit code the keyboard sends for a typed character: 0x20-0x5F as is, a-z as A-Z,
// '\r' or '\n' 0x0D (Return), '\b' 0x08 (Backspace); nullopt for anything else.
std::optional<std::uint8_t> KeyboardCode(char32_t key);

// ASCII keyboard behind a KK-10 Connector, logic only (an encoder chip). Pins 1-7 D0-D6,
// 8 STROBE, 9 VCC, 10 GND. A key's code goes out on D0-D6, then STROBE pulses high for
// `strobe`; the code stays until the next key. Keys typed meanwhile wait in turn.
class Keyboard final : public Component {
public:
    Keyboard(std::string reference, Time strobe);  // Check(strobe >= 1 µs)
    static constexpr PinIndex kD0 = 0, kStrobe = 7;

    // One producer thread (the UI; bw run's own). False when kKeyboardWaiting keys wait.
    // Follow with Simulator or Runner wake(): an idle keyboard takes it at its next evaluation.
    bool type(std::uint8_t code);

    // Producer side, any time: the keys waiting now are skipped by the consumer (power-off).
    void dropWaiting();

    void powerOn(PowerOnContext& ctx) override;  // Idle, STROBE low; waiting keys kept
    void evaluate(EvalContext& ctx) override;

private:
    enum class Phase : std::uint8_t { Idle, Setup, High, Gap };
    Time strobe_;
    Phase phase_ = Phase::Idle;
    Time nextAt_ = 0;  // ps; outside Idle: when the phase's own wake is due
    std::array<std::uint8_t, kKeyboardWaiting> waiting_{};
    std::atomic<std::uint32_t> head_{0}, tail_{0};  // SPSC: type() writes head_, evaluate tail_
    std::atomic<std::uint32_t> dropBefore_{0};      // dropWaiting(): keys before it are skipped
};
```

```
evaluate:
    if phase_ != Idle && now() < nextAt_: return          // a wake from type(), mid-key
    switch phase_:
      Setup: drive STROBE 1, kKeyboardDelay;  next(High, strobe_)
      High:  drive STROBE 0, kKeyboardDelay;  next(Gap, kKeyboardDelay + kKeyboardGap)   // 1 µs after the fall
      Gap:   phase_ = Idle, then as Idle
      Idle:  if a key waits: pop it; drive D0..D6 its bits, kKeyboardDelay; next(Setup, kKeyboardSetup)
    next(phase, after): phase_ = phase; nextAt_ = now() + after; wakeAfter(after)
```

Timing of one key taken at `t`: `D0`–`D6` at `t + 100 ns`; `STROBE` rises at `t + 1.1 µs`, falls at `t + 1.1 µs + strobe`; the next key is taken 1 µs after the fall, at `t + 2.1 µs + strobe` at the earliest, its code out 100 ns later. Keys typed together come `strobe + 2.1 µs` apart.

- The keyboard has no inputs: it is evaluated at power-on and on wakes only. A wake from `type` during a key is ignored by the `nextAt_` test; the key waits and is taken at the end of the Gap. Two wakes at one timestamp evaluate once (dirty flag).
- `type`: `if (head - tail.load(acquire) == kKeyboardWaiting) return false; waiting_[head % N] = code; head_.store(head + 1, release)`. The pop reads `head_` with acquire, then `tail = max(tail, dropBefore_.load(acquire))` (wrap-safe difference) before taking a key. No allocation, no lock.
- `dropWaiting`: `dropBefore_.store(head_.load(), release)` (producer side only: a producer reset of `head_` would race the consumer). The counters only grow, so a drop is never undone. The Session calls it at power-off; at power-on the Keyboard keeps its queue, so a key typed in the same frame as a power-on click, before the runner applies it, is not lost.
- `powerOn`: `phase_ = Idle`, `setOutput(kStrobe, Zero)`; `D0`–`D6` keep their random power-on values.
- Pins from `ConnectorPins("KEYBOARD")`: `D0`–`D6` `{Output, Ttl}`, `STROBE` `{Output, Ttl}`, `VCC`, `GND` `{Power, Ttl}`. `ConnectorPowersDevice("KEYBOARD")` is true (`PADDLE` or `KEYBOARD`): `FindPower` checks its power Pins, an unpowered keyboard's Pins go on private Nets (P4i §4). `ConnectorVolts` unchanged.
- Factory `CreateKeyboard` in [catalogue.cpp](../../src/parts/catalogue.cpp) next to `CreatePaddle`: `strobe` (the last wins), else `"KEYBOARD: unknown parameter 'x'"`; `DurationFromText` errors as is; under 1 µs `"KEYBOARD: strobe must be at least 1us"`; default `kKeyboardStrobe`.
- Catalogue row between `DIGIT` and `LED`: `PartInfo{"KEYBOARD", "ASCII keyboard connector", "KK-10", Connector, &CreateKeyboard}`.
- `KeyboardOf(const LoadedCircuit&, std::string_view)` in [builder.hpp](../../src/netlist/builder.hpp) beside `PaddleOf` (`part->info->name == "KEYBOARD"`).
- `PartPinNames` builds it without parameters: fine (optional parameter), 10 pins; `manual_test` checks the size.
- `KeyboardCode`: `'a'..'z'` → `- 0x20`; `0x20..0x5F` → itself; `'\r'`, `'\n'` → 0x0D; `'\b'` → 0x08; else nullopt.
- Datasheet `KEYBOARD.md` in the shape of `PADDLE.md`: Function (the 10-pin connector of an ASCII keyboard on a cable; a key puts its 7-bit ASCII code on `D0`–`D6`, then `STROBE` pulses high for the time given in the Machine's Service Manual; the code stays until the next key; upper case only, Return sends 13 and Backspace 8; keys typed faster than the strobe wait their turn; the keyboard runs on the Board's +5 V through `VCC` and `GND`, and without it its outputs drive nothing), Pins.

### 6.2 Session

[session.hpp](../../src/session/session.hpp), after the knob members:

```cpp
// Every KEYBOARD Part of the circuit, in netlist order.
std::vector<std::string> keyboards() const;
// Sends `key` (KeyboardCode) to KEYBOARD `reference`, which takes it at the next slice
// boundary. False, nothing sent: not powered, not a KEYBOARD, a key it does not send, or
// kKeyboardWaiting keys already waiting.
bool type(std::string_view reference, char32_t key);
// The codes sent to `reference` since power-on, the last 16.
std::string typed(std::string_view reference) const;
```

- `keyboards()` as `knobs()` with `"KEYBOARD"`.
- `type`: refuses when `!powered_` or the reference is not in `keyboards()` or `KeyboardCode` fails; `KeyboardOf(*live_->circuit, reference)` (non-null when listed); `keyboard->type(code)` false → false; `live_->runner->wake(keyboard->id(), PressJitter())`; appends the code to `typed_[reference]`, keeping the last 16.
- `typed_`: `std::map<std::string, std::string, std::less<>>`, cleared in `powerOn`.
- `powerOff`: `KeyboardOf(...)->dropWaiting()` for every keyboard, beside the Button releases.

### 6.3 Typing on the command line

```cpp
// src/machine/typing.hpp
inline constexpr Time kTypingInterval = 100'000'000'000;  // ps: 100 ms between keys

struct Typing {
    std::string reference;           // "J1"
    Time start;                      // the first key, simulated ps
    std::vector<std::uint8_t> keys;  // KeyboardCode values, one every kTypingInterval
};

// "J1:200ms:HELLO\n": reference, start (ParseDuration), text after the second ':' (may hold
// ':'); \n Return, \b Backspace, \\ backslash. Error text otherwise.
std::expected<Typing, std::string> ParseTyping(std::string_view text);
```

Errors: `"bad typing 'x' (expected REF:START:TEXT)"` for a missing colon, empty reference, bad duration or empty text; `"cannot type 'c' on a keyboard"` for a character `KeyboardCode` refuses; `"bad escape '\\x'"`.

**`bw run`** ([cli/main.cpp](../../src/cli/main.cpp)): `Options::type` (`std::vector<Typing>`), `--type` repeatable, its value not split at commas, in the list of options that take a value, `[--type REF:START:TEXT]` in `kUsage`. The press edges become input edges:

```cpp
struct InputEdge {
    Time at;
    ComponentId wake;
    std::function<void()> apply;
};
```

A press gives two (`setPressed(true/false)`); a typing gives one per key at `start + i × kTypingInterval` (`keyboard->type(code)`; the return is ignored: at 100 ms apart a key waits only with a `strobe=` near 100 ms, said in a comment). `KeyboardOf` null → `UsageError("'J9' is not a keyboard")`. The loop applies each due edge then `sim.wake(edge.wake)`; the step condition becomes `!edges.empty()`.

**`bw_ui`** ([ui/main.cpp](../../src/ui/main.cpp)): `--type` parsed the same, repeatable, `[--type REF:START:TEXT]` in `kUsage`; before `SDL_Init`, a reference not in `session.keyboards()` prints `bw_ui: 'J9' is not a keyboard` with the usage and returns `kExitUsage`. Each frame, after the Buttons block: per typing, an index of keys sent; while `index < keys.size()` and `start + index × kTypingInterval <= status.simTime`: `session.type(reference, keys[index++])`. The indices go back to 0 while not powered, and no key is sent in the first frame powered (its `status.simTime` is the previous run's: the power-on was only queued); a `wasPowered` flag kept by the loop.

## 7. Keyboard capture in `bw_ui`

```cpp
// src/ui/keyboard_capture.hpp
// While on, the keyboard and text events of SDL go to one KEYBOARD Part instead of ImGui.
struct KeyboardCapture {
    bool on = false;
    std::string reference;  // the first of Session::keyboards(); empty: none

    // Before ImGui_ImplSDL3_ProcessEvent; true when the event was taken.
    bool handle(const SDL_Event& event, Session& session);
    // Once a frame: text input on while capturing (SDL_StartTextInput), off when it stops.
    void sync(SDL_Window* window);

private:
    bool started_ = false;  // text input started by sync()
};
```

`handle`, only while `on`:

| Event | Action | Taken |
|---|---|---|
| `SDL_EVENT_TEXT_INPUT` | each byte from 0x20 to 0x7F of `event.text.text` → `session.type(reference, byte)` (refusals ignored; control and non-ASCII bytes skipped: Return and Backspace come from `KEY_DOWN`, so a platform that also sends them as text does not type twice) | yes |
| `SDL_EVENT_TEXT_EDITING` | none | yes |
| `SDL_EVENT_KEY_DOWN` | `SDLK_ESCAPE`, `SDLK_F2` → `on = false`; `SDLK_RETURN`, `SDLK_KP_ENTER` → type `'\r'`; `SDLK_BACKSPACE` → type `'\b'`; any other key nothing | yes |
| anything else (key up included, so no key stays held in ImGui; mouse; window) | none | no |

Key repeats (`event.key.repeat`) type again, as a held key repeats on a real keyboard.

`sync`: `on` and `!SDL_TextInputActive(window)` → `SDL_StartTextInput(window)`, `started_ = true`; `!on && started_` → `SDL_StopTextInput(window)`, `started_ = false`. The ImGui SDL3 backend only stops text input it started for a widget (`ImeWindow`, set only while an `InputText` wants the IME), so it leaves ours alone; if a widget's text input ends while capturing, `sync` restarts ours the next frame.

In `main.cpp`: `KeyboardCapture capture{.reference = keyboards.empty() ? "" : keyboards[0]}`; in the event loop `if (capture.handle(event, session)) continue;` before `ImGui_ImplSDL3_ProcessEvent`; `capture.sync(window)` once a frame after the side panel; `toolbar.typing = capture.on ? capture.reference : ""`.

**Side panel** ([side_panel.cpp](../../src/ui/side_panel.cpp)), `DrawKeyboard(Session&, KeyboardCapture&)` right after `DrawKnobs` (and `SidePanel::draw` takes the capture):

```
if capture.reference empty: return
unless io.WantTextInput or capture.on: IsKeyPressed(ImGuiKey_F2, false) → capture.on = true   (also with the section collapsed)
if CollapsingHeader("Keyboard", open):
    ToolButton(capture.on ? "Typing (Esc)" : "Type (F2)", capture.on) → capture.on = !capture.on
    hover: "Type on the Machine's keyboard; every key goes to it until Esc"
    Text("Sent: %s", typed with 0x0D as [CR], 0x08 as [BS])
```

**Toolbar** ([toolbar.cpp](../../src/ui/toolbar.cpp)): `std::string typing;` member; when not empty the banner reads `Typing on J1 · Esc to stop` instead of the tool's action, not blocked. The tool action is drawn only inside `if (hasBoard)`; the typing text is drawn outside that block, so it shows on a netlist-only Session (the bench) too. The `U` key test already skips while ImGui has no key (the key-down is taken).

## 8. Comments and docs that change

- `Component::powerOn` comment (§2.3); `IsDriver` comment; `PinSpec` unchanged.
- [ARCHITECTURE.md](../ARCHITECTURE.md) Pins, nets, drivers: one sentence on bidirectional pins (a Driver and a Load, released at power-on).
- [GLOSSARY.md](../GLOSSARY.md): **Pin** (a bidirectional Pin both reads and drives its Net, as a RAM's data pin); **Driver** and **Load** mention it; **Stuck-at Fault** (on a bidirectional Pin: forces its Net and is read by its chip); **Input Device** unchanged (it already names the ASCII keyboard); new **Typing**: the `bw_ui` mode in which every key goes to the Machine's keyboard, Escape ends it. _Avoid_: keyboard capture (in player text).
- [ADD_PART.md](../recipes/ADD_PART.md): reference commits for the 2114 (bidirectional MOS pins, random contents) and the 74LS244 (tri-state groups, the three delay kinds); a rule: a chip that shares data pins uses `PinKind::Bidirectional`; a new package of the DIP-14/16 width (DIP-18, DIP-20) is also a grid package.
- [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md): the keyboard as the second reference: a setting that is an event (a key) needs a wake, a lock-free queue in the Part; capture mode for a device that takes the whole keyboard; `--type` beside `--knob`.

## 9. Bench netlist

`tests/data/ram_bench.bwn`:

```
# Bench for P4n (docs/p4n-ram-bus/RAM_BUS.md), no Board. Each key typed on J1 is written into the
# two 2114s (U2 bits 0-3, U3 bits 4-6) at the address held by U4: while STROBE is high, WE_N is
# low and U1 drives the code onto the bus B0-B6; U4 steps when WE_N rises. The rest of the time
# the RAMs drive the word at U4's address onto the bus. U4 powers on at a random count: press
# SW1 (clears U4) before typing to start at address 0, and again to read address 0 back.
# The bus fights for up to 100 ns at the start of each write (the RAMs let go 100 ns after WE_N
# falls, U1 drives 15-20 ns after): a real bench's sloppiness, gone before the write ends.

part J1  KEYBOARD strobe=10us
part J2  POWER
part U1  74LS244   # keyboard onto the bus
part U2  2114      # bits 0-3
part U3  2114      # bits 4-6
part U4  74LS161   # address A0-A3
part U5  74LS04    # WE_N
part SW1 BUTTON
part R1  R value=4k7

net STROBE  J1.STROBE U5.1A
net WE_N    U5.1Y U1.1OE_N U1.2OE_N U2.WE_N U3.WE_N U4.CLK
net D0      J1.D0 U1.1A1
net D1      J1.D1 U1.1A2
net D2      J1.D2 U1.1A3
net D3      J1.D3 U1.1A4
net D4      J1.D4 U1.2A1
net D5      J1.D5 U1.2A2
net D6      J1.D6 U1.2A3
net B0      U1.1Y1 U2.IO1
net B1      U1.1Y2 U2.IO2
net B2      U1.1Y3 U2.IO3
net B3      U1.1Y4 U2.IO4
net B4      U1.2Y1 U3.IO1
net B5      U1.2Y2 U3.IO2
net B6      U1.2Y3 U3.IO3
net A0      U4.QA U2.A0 U3.A0
net A1      U4.QB U2.A1 U3.A1
net A2      U4.QC U2.A2 U3.A2
net A3      U4.QD U2.A3 U3.A3
net RESET_N R1.2 SW1.1 U4.CLR_N
net VCC     R1.1 U4.ENP U4.ENT U4.LOAD_N
net GND     SW1.2 U1.2A4 U2.CS_N U3.CS_N U4.A U4.B U4.C U4.D
net GND     U2.A4 U2.A5 U2.A6 U2.A7 U2.A8 U2.A9 U3.A4 U3.A5 U3.A6 U3.A7 U3.A8 U3.A9
net GND     U5.2A U5.3A U5.4A U5.5A U5.6A
```

`U3.IO4` and `U1.2Y4` are left unconnected (a MOS bidirectional pin and an output: no warning). Pin names checked against the Parts while writing (74LS04 `1A`/`1Y`, 74LS161 `QA`–`QD`, `ENP`, `ENT`, `LOAD_N`, `CLR_N`, `CLK`).

How to use it by hand: `bw run tests/data/ram_bench.bwn --type J1:1ms:HELLO --time 1s`; `bw_ui tests/data/ram_bench.bwn` (netlist only: the TV says no video; the Keyboard section and Typing work; the Probe needs a Board, so the bench is read by tests).

## 10. Integration Points

1. **sim** — §2: `logic.hpp` (`Bidirectional`, `IsDriver`, `IsReader`, `DriveOf`), `circuit.cpp` (`buildTables`), `simulator.cpp` (`powerOn`, `readBitBefore`), `component.hpp`.
2. **Every use of the pin kind**, checked: `builder.cpp` lines 68–69 (stuck check: covered), 78 (stuck Driver tie: covered), 91 (own Net: `IsReader`), 225/229/251 (NoConnect, Power: unchanged), 313 (`IsReader`); `electrical.cpp` (Power only: unchanged); `machine.cpp` 112–113 (accepted), 334 (unchanged); `net_pins.cpp` 64 (Driver: covered); `board_scene.cpp` 205 (Power: unchanged); `simulator.cpp` 31, 52, 128 (covered by `IsDriver`, 128 then excluded for Z start).
3. **Parts** — §3, §4, §6.1: `ram2114.cpp`, `ls244.cpp`, `keyboard.cpp` in `src/parts/CMakeLists.txt` (sorted); catalogue rows; `delays.hpp`; `connector.{hpp,cpp}` (`ConnectorPins("KEYBOARD")`, `ConnectorPowersDevice`, and their comments in connector.hpp naming the connectors).
   `tests/parts/catalogue_test.cpp` "chip pinouts are consistent" checks every Chip pin is TTL: a `kMosSignals` list (as `kOffCornerPower`) exempts the 2114's signal pins.
   `tests/CMakeLists.txt` gains every new test file (sorted).
4. **Board** — §5: footprints, `kFootprints`, `IsGridPackage`.
5. **Netlist builder** — `KeyboardOf`; §2.4.
6. **Machine** — `typing.cpp` in `src/machine/CMakeLists.txt`. `FindPower` already goes through `ConnectorPowersDevice`.
7. **Session** — §6.2. **CLI and UI** — §6.3, §7; `keyboard_capture.cpp` in `src/ui/CMakeLists.txt`.
8. **Docs** — §8.

## 11. Testing

Catch2, tags as the module. Parts tests copy `rom_test.cpp`'s `Bench` (a `Stimulus` per driven input). The 2114's and 74LS244's bus cases add a second Driver on the data Nets (a `Stimulus` behind a `BufferTri`-like enable, or a second chip).

### 11.1 Bidirectional pins

| File | Checks |
|---|---|
| `tests/sim/logic_test.cpp` | "logic: IsDriver" extended: `IsDriver`, `IsReader` per kind; `DriveOf(Bidirectional, …)` as `Output` (Z none, 0/1 Strong) |
| `tests/sim/circuit_test.cpp` | a test Component with one bidirectional pin: in its Net's `drivers()` and `fanOut()`; a TTL one sets `hasTtlInput`; `singleDriver` stays `kNoPin` |
| `tests/sim/simulator_test.cpp` | the pin is Z after `powerOn` (not random) for several seeds; the Component is evaluated when its own drive changes the Net; it reads another Driver's value while it drives Z; MOS bidirectional vs TTL output disagreeing → X; `readBitBefore` gives the value before the timestamp, X a random bit |
| `tests/netlist/builder_test.cpp` | a 2114 `IO1` stuck at 1: its Net reads 1 with the 74LS244 disabled, and 0 with it driving 0 (the tie is Strong TTL: TTL contention, 0 wins, as for a stuck output today); the RAM writes 1 into bit 0 of every word written; an unconnected 2114 `IO4` gives no warning |
| `tests/netlist/net_pins_test.cpp` | `DescribeNet` of a bus Net lists the 2114 `IO` pin and the 74LS244 `Y` as Drivers |
| `tests/machine/electrical_test.cpp` or the builder test | the netlist with `U2.IO1` left off `B0` (a dry Joint's electrical netlist): `U2` drives nothing on `B0`, and a write leaves bit 0 at the pin's floating value |

The Puzzle-file acceptance of `stuck`/`joint` on a bidirectional pin needs a Board with a 2114: it is left to P4o's Puzzles (P4p), said in the plan's notes.

### 11.2 Parts

| File | Checks |
|---|---|
| `tests/parts/ram2114_test.cpp` `[parts]` | catalogue row (DIP-18), pin names and kinds (`IO` Bidirectional MOS, inputs MOS, power at 9 and 18); with `CS_N`, `WE_N` from Stimuli: read after `k2114Address` for an address change, `k2114Select` after `CS_N` falls and after `WE_N` rises; `CS_N` high → Z after `k2114Off`; `WE_N` low → Z after `k2114Off` and the word written from Stimuli on the data Nets (behind a tri-state, released when not writing); data changed during the write: the last kept; address changed at the very timestamp `WE_N` rises: the old address written, the new one not; X on an address bit while writing writes somewhere (one of the two words), no X on reads; contents differ between two seeds and are the same for one seed |
| `tests/parts/ls244_test.cpp` `[parts]` | catalogue row (DIP-20), pins, power at 10 and 20; `GENERATE` over the 8 buffers: `Y` follows `A` (both delays), `OE_N` high → Z (10 ns from 1, 15 ns from 0), low → driven (15 ns to 1, 20 ns to 0); each group's enable alone; X on `A` → X, on `OE_N` → X; outputs Z after `powerOn` with `OE_N` high |
| `tests/parts/keyboard_test.cpp` `[parts]` | catalogue row (`Connector`, `KK-10`), pins and kinds; `strobe=` default 10 µs, `strobe=50us` accepted, `strobe=0.5us`, `strobe=x`, `speed=1` refused with their messages; `KeyboardCode`: `'A'`, `'a'` → 0x41, `' '`, `'_'`, `'\r'`, `'\n'`, `'\b'`, refused `` '`' ``, `'{'`, `'~'`, `'\t'`, `U'é'`; a key typed and woken at `t`: `D0`–`D6` its code at `t + 100 ns`, `STROBE` rises at `t + 1.1 µs`, falls `strobe` later; three keys typed together come `strobe + 2.1 µs` apart, in order; 32 waiting accepted, the 33rd refused; `dropWaiting` then a wake: nothing sent, a key typed after it is; `powerOn` leaves `STROBE` low and keeps a waiting key; a wake mid-key changes nothing; `ConnectorPowersDevice("KEYBOARD")` true |
| `tests/parts/catalogue_test.cpp` | the rules hold for the new rows (pin count from the package); the 2114 in `kMosSignals` |
| `tests/board/footprint_test.cpp`, `kicad_netlist_test.cpp` | the three footprints load (18, 20, 10 pads); `KicadFootprint` for each |
| `tests/board/placer_test.cpp` | a netlist with a 74LS244 and a 2114 among DIP-14s places them in the grid with no rule |
| `tests/manual/manual_test.cpp` | `PartPinNames(*FindPart("KEYBOARD")).size() == 10`; the three Datasheets cover their pins (`manuals: every catalogue Part has a Datasheet`) |

### 11.3 Typing and the bench

| File | Checks |
|---|---|
| `tests/machine/typing_test.cpp` `[machine]` | `J1:200ms:HI` → J1, 200 ms, `{0x48, 0x49}`; `J1:0ms:a:b` → `A`, `:`, `B`; `\n`, `\b`, `\\`; refused: `J1`, `J1:200ms`, `:1ms:A`, `J1:x:A`, `J1:1ms:`, `J1:1ms:{`, `J1:1ms:\q` |
| `tests/netlist/ram_bench_test.cpp` `[netlist]` | `ram_bench.bwn` loads without warnings; a helper presses and releases `SW1` first (U4 powers on at a random count), then types a string (`KeyboardOf(...)->type`, `sim.wake`, then runs 1 ms per key); "HELLO" → `U2.word(i) | U3.word(i) << 4` equals `H E L L O` for i = 0..4, for seeds 1–3; after the keys, `SW1` pressed (`setPressed`, wake) and released: `B0`–`B6` read 0x48 1 µs later and are never X from then on (recorded); the bus is X only in windows under 100 ns after each `WE_N` fall |
| `tests/session/session_test.cpp` `[session]` | `session: typing`: a netlist-only Session on the bench lists `J1` in `keyboards()`; `type` false with the power off, for `J9`, for `'{'`; powered: true, `typed` shows the keys, the last 16 only after 20; after `waitIdle`-style running the RAM holds them; power-off and on clears `typed`; a key typed then power-off: not sent at the next power-on |

### 11.4 Release

- `ctest --preset debug -j 12` green.
- `build/release/src/cli/bw run tests/data/ram_bench.bwn --press SW1:0ms:1ms --type J1:2ms:HELLO --press SW1:600ms:601ms --time 1s --record B0,B1,B2,B3,B4,B5,B6 --vcd /tmp/bench.vcd`: after 601 ms the bus reads 0x48 in the VCD (usage errors for `--type J9:…` and `--type J1:1ms:{`). Check `--press` and `--type` together are applied in time order.
- `build/release/src/ui/bw_ui tests/data/ram_bench.bwn --type J1:100ms:HELLO --screenshot /tmp/bench.png --after 1s`: the Keyboard section shows `Sent: HELLO`; view the PNG.
- Unchanged speed on 070 and 080: `bw run netlists/tennis.bwn --time 2s` and `netlists/message.bwn` within noise of their recorded ratios (the reader test moved in `buildTables`; `resolveNet` untouched).
- User check (in the plan's notes): Type in the Keyboard section, a few keys show under Sent and the banner says Typing, `U` does not open the User Manual while typing (the bench has no Board, so no tool keys), Escape stops it and `U` works again.

# P4b Shift Register — Detailed Design

Implements [SHIFT_REGISTER.md](SHIFT_REGISTER.md): the 74LS164 Part, a larger watch budget, and the tests of the LED chaser. The Machine's netlist, Board, manuals and Puzzles are in [LED_CHASER_DESIGN.md](../machines/020-led-chaser/LED_CHASER_DESIGN.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/parts/ls164.hpp, ls164.cpp       Ls164 Component
datasheets/74LS164.md                Datasheet (checked by src/manual)
tests/parts/ls164_test.cpp
tests/machines/chaser_test.cpp
```

Same shape as `Ls161`: one behavioural Component per chip, next to the others.

## 2. 74LS164 Part

```cpp
// src/parts/ls164.hpp
// 8-bit serial-in, parallel-out shift register, asynchronous clear. Pins per datasheet: 1 A,
// 2 B, 3 QA, 4 QB, 5 QC, 6 QD, 7 GND, 8 CLK, 9 CLR_N, 10 QE, 11 QF, 12 QG, 13 QH, 14 VCC.
class Ls164 final : public Component {
public:
    explicit Ls164(std::string reference);
    static constexpr PinIndex kA = 0, kB = 1, kQA = 2, kQB = 3, kQC = 4, kQD = 5, kClk = 7,
                              kClrN = 8, kQE = 9, kQF = 10, kQG = 11, kQH = 12;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    std::array<LogicValue, 8> stages_{};  // QA..QH; X after an unknown clear
};
```

- Pins as `Ls161Pins()` ([ls161.cpp](../../src/parts/ls161.cpp#L20-L28)): TTL inputs `A`, `B`, `CLR_N`; `CLK` with the rising-edge-only flag; outputs `QA`–`QH`; power `VCC`, `GND`.
- Delays ([delays.hpp](../../src/parts/delays.hpp#L20)), TI typical, unverified like the rest:

  ```cpp
  inline constexpr Delays kLs164ClockToQ{17'000, 21'000};
  inline constexpr Delays kLs164Clear{24'000, 24'000};  // only fall is used
  ```

- Catalogue ([catalogue.cpp](../../src/parts/catalogue.cpp#L88)), after `74LS161` (sorted by name): `PartInfo{"74LS164", "8-bit serial-in shift register", "DIP-14", Chip, &CreateChip<Ls164, "74LS164">}`. `DIP-14` already has a KiCad footprint; `catalogue: chip pinouts are consistent` checks `VCC` 14 and `GND` 7.
- Datasheet `74LS164.md` in the shape of [74LS161.md](../../datasheets/74LS161.md): Function (shift on the rising edge, `A` AND `B` into `QA`, clear), Pins (`A`, `B`, `CLK`, `CLR_N`, `QA`–`QH`, `VCC`, `GND`).

## 3. Key Algorithms

### 3.1 Power-on

```
for i in 0..7: stages_[i] = randomBit() ? One : Zero; setOutput(output i, stages_[i])
```

### 3.2 Evaluate

```
clr = read(CLR_N); delays = kLs164ClockToQ
if clr == Zero:   stages_.fill(Zero); delays = kLs164Clear
elif clr == X:    stages_.fill(X)
elif rose(CLK):
    shift stages_ one place towards QH (QH's old value is lost)
    stages_[0] = GateFunction(And, {readBefore(A), readBefore(B)})
for i in 0..7: drive(output i, stages_[i], DelayFor(delays, stages_[i]))
```

- `readBefore`: data launched by the same clock edge is not seen, as in `Ls161`.
- `AND` with `GateFunction` gives 0 if either input is 0 even when the other is X, as the real gate.
- Evaluations without an edge re-drive the same values; the kernel schedules nothing for an unchanged output.

## 4. Watch Budget

[src/runner/dispatcher.hpp](../../src/runner/dispatcher.hpp#L16):

```cpp
inline constexpr std::size_t kMaxWatched = 32;  // ring slots
```

- Worst case on the chaser: 16 LED Nets + 8 Clips + 2 Hooks + probe + meter = 28, one more while a lead moves; distinct Nets are counted once, so this is an upper bound.
- Cost: one ring of `Runner::kRingCapacity` (65 536) 16-byte Samples per slot, allocated by the Runner constructor: 32 MB instead of 16 MB. Per slice, the Runner and Dispatcher loops over slots skip empty slots; the Dispatcher merge in `sliceEnd` scans all slots per dispatched Sample, so its cost doubles, on the devices thread only. LED Nets change at most 15 times a second.
- `Marker::slot` is `std::uint8_t`: 32 fits.
- `probe consumer: touching many Nets in turn` still touches more Nets than the limit (42 Nets on the test pattern).

## 5. Integration Points

1. **Catalogue** — §2, one line plus the include of `parts/ls164.hpp`.
2. **Delays** — §2, two constants.
3. **Dispatcher** — §4, one constant.
4. **Per-Machine test loops** — add `"chaser"` to the lists of Machines:
   - [boards_test.cpp](../../tests/board/boards_test.cpp#L76) (Board against netlist),
   - [electrical_test.cpp](../../tests/machine/electrical_test.cpp#L73),
   - [board_scene_test.cpp](../../tests/view/board_scene_test.cpp#L146) and [L425](../../tests/view/board_scene_test.cpp#L425),
   - [manuals_test.cpp](../../tests/machines/manuals_test.cpp#L41).

   `drc_chaser` comes from the `boards/*/*.kicad_pcb` glob in [tests/CMakeLists.txt](../../tests/CMakeLists.txt#L90-L97).

No change to the Session, the Board view or the UI: `makeLive` already builds one `LedConsumer` entry per `PartKind::Led` Part, `Session::leds()` lists them all, and `BoardView::setLeds` draws any number.

## 6. Reused Infrastructure

- `Component`, `PowerOnContext::randomBit`, `EvalContext::rose` / `readBefore` / `drive`, `GateFunction`, `DelayFor`: as `Ls161`.
- `CreateChip<T, Name>` in the catalogue; DIP-14 footprint in `kFootprints`.
- `74LS161`, `74LS30`, `OSC`, `R`, `LED`, `POWER`: existing Parts and Datasheets.
- `LedConsumer`, `LedGlow`, LED drawing (P4a); Puzzle faults `crack`, `stuck`, `bridge` and Repairs Mend, Swap, Cut (P2d, P3e, P3f).
- Test helpers: the same shape as [counter_test.cpp](../../tests/machines/counter_test.cpp#L26-L76) (`LoadPuzzle`, `FaultFree`, `Record`, `Rises`).

## 7. Build Integration

```cmake
# src/parts/CMakeLists.txt
add_library(bw_parts STATIC ... ls161.cpp ls164.cpp ls74.cpp ...)
# tests/CMakeLists.txt: machines/chaser_test.cpp, parts/ls164_test.cpp
```

No new dependencies.

## 8. Dependency Diagram

```
bw_sim ← bw_parts (Ls164) ← bw_netlist ← bw_machine ← bw_session ← bw_ui
bw_runner (kMaxWatched) ← bw_session
```

## 9. Testing

Catch2, tags as the module. `STEP` rises every 65.5 ms and debug runs at about 0.1× real time on this Machine, so Machine tests that need a settled chaser (8 steps) take several seconds and are tagged `[slow]`.

| File | Checks |
|---|---|
| `tests/parts/ls164_test.cpp` `[parts]` | `FindPart("74LS164")`: kind `Chip`, `DIP-14`, Pin names by index; shift: a 1 clocked in reaches `QH` after 8 rises, at `kLs164ClockToQ`; `A` 0 or `B` 0 shifts in 0; `A` X with `B` 0 shifts in 0; `CLR_N` low clears at once, at `kLs164Clear`, and holds against the clock; `A` changing with the edge is not seen; power-on outputs are 0/1, never X |
| `tests/machines/chaser_test.cpp` `[machines]` | Puzzles have a title and a brief, `machine chaser`; `[slow]` fault-free: after 9 `STEP` rises, at each of the next 8 exactly one of `S1`–`S8` is low and it moves one stage per rise; `[slow]` per Puzzle, Symptom then Repair (LED_CHASER.md, Puzzles): gap — `D5.A`'s Net floating, `S5` low in turn, mend → not floating; stuck_light — `S4` always 0, the others 1 after 9 rises, swap `U5` → one moving low bit; dark_chaser — `U5.A`'s Net floating, `S1`–`S8` all 1 after 9 rises, mend → one moving low bit; twins — `S1` equals `S2` throughout, low for two consecutive steps, unbridge → one moving low bit |
| `tests/session/session_test.cpp` `[session]` | chaser with 8 Clips, 2 Hooks, the probe and the meter leads attached, powered: runs without a Check, `leds()` lists `D1`–`D8`, their brightnesses sum to about 1 after 1 s (one LED lit at a time) |
| existing | `boards_test`, `electrical_test`, `board_scene_test`, `manuals_test` (§5.4), `drc_chaser`, `catalogue_test`, `probe_consumer_test` |

UI check (release build): `bw_ui netlists/chaser.bwn --screenshot /tmp/chaser.png --after 1s` shows one LED lit; the status line reads 1.00× with every instrument attached.

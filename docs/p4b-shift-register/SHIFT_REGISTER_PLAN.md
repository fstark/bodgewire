# P4b Shift Register — Implementation Plan

Design: [SHIFT_REGISTER_DESIGN.md](SHIFT_REGISTER_DESIGN.md)
Spec: [SHIFT_REGISTER.md](SHIFT_REGISTER.md); Machine: [LED_CHASER.md](../machines/020-led-chaser/LED_CHASER.md), [LED_CHASER_DESIGN.md](../machines/020-led-chaser/LED_CHASER_DESIGN.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | 74LS164 Part: `Ls164`, delays, catalogue, Datasheet, unit tests | |
| 2 | Chaser netlist, manuals, headless fault-free test | |
| 3 | Chaser Board (**user layout**), per-Machine test loops | |
| 4 | Chaser Puzzles + headless Symptom/Repair tests | |
| 5 | `kMaxWatched` 32, Session test with every instrument, screenshot | |
| 6 | Release check, user check, docs | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate: `ctest --preset debug -j 12`

`clang-format -i` on changed C++ files only. Commit only the paths of the phase (the user edits docs in parallel).

---

## Phase 1 — 74LS164 Part

The Part on its own, tested on a bench Circuit. Nothing uses it yet, so every existing test still passes. Design §2, §3.

### 1.1 — Delays

[src/parts/delays.hpp](../../src/parts/delays.hpp), after `kLs161EntToRco`:

```cpp
inline constexpr Delays kLs164ClockToQ{17'000, 21'000};
inline constexpr Delays kLs164Clear{24'000, 24'000};  // only fall is used
```

### 1.2 — `Ls164` Component

`src/parts/ls164.hpp`:

```cpp
#pragma once

#include <array>
#include <string>

#include "sim/component.hpp"

namespace bw {

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

}  // namespace bw
```

`src/parts/ls164.cpp`, same shape as [ls161.cpp](../../src/parts/ls161.cpp):

- Anonymous namespace: `constexpr std::array<PinIndex, 8> kOutputs{kQA, kQB, kQC, kQD, kQE, kQF, kQG, kQH}` (qualified `Ls164::`); `std::vector<PinSpec> Ls164Pins()` with the `in` / `out` / `power` lambdas of `Ls161Pins()` and `PinSpec{"CLK", PinKind::Input, Family::Ttl, true}`. Order: `A`, `B`, `QA`, `QB`, `QC`, `QD`, `GND`, `CLK`, `CLR_N`, `QE`, `QF`, `QG`, `QH`, `VCC`.
- `powerOn`: for each stage, `ctx.randomBit() ? One : Zero`, then `ctx.setOutput(kOutputs[i], stages_[i])`.
- `evaluate` (design §3.2):

  ```cpp
  const LogicValue clr = ctx.read(kClrN);
  const bool edge = ctx.rose(kClk);
  Delays qDelays = delays::kLs164ClockToQ;
  if (clr == Zero) {
      stages_.fill(Zero);
      qDelays = delays::kLs164Clear;
  } else if (clr == X) {
      stages_.fill(X);
  } else if (edge) {
      const std::array<LogicValue, 2> serial{ctx.readBefore(kA), ctx.readBefore(kB)};
      std::shift_right(stages_.begin(), stages_.end(), 1);
      stages_[0] = GateFunction(GateKind::And, serial);
  }
  for (std::size_t i = 0; i < kOutputs.size(); ++i) {
      ctx.drive(kOutputs[i], stages_[i], DelayFor(qDelays, stages_[i]));
  }
  ```

  Read `A` and `B` before shifting. Call `ctx.rose(kClk)` once at the top, before the `clr` branches, as `Ls161::evaluate` does, so edge tracking sees every evaluation.

[src/parts/CMakeLists.txt](../../src/parts/CMakeLists.txt): add `ls164.cpp` after `ls161.cpp`.

### 1.3 — Catalogue and Datasheet

[src/parts/catalogue.cpp](../../src/parts/catalogue.cpp): `#include "parts/ls164.hpp"` after `ls161.hpp`; after the `74LS161` entry:

```cpp
PartInfo{"74LS164", "8-bit serial-in shift register", "DIP-14", Chip,
         &CreateChip<Ls164, "74LS164">},
```

`datasheets/74LS164.md`, shape of [74LS161.md](../../datasheets/74LS161.md):

- `# 74LS164 — 8-bit serial-in shift register`
- `## Function`: on each rising edge of `CLK` every stage takes the value of the one before (`QA` → `QB` … `QG` → `QH`; the old `QH` is lost) and `QA` takes `A` AND `B` (tie one high to use the other as the serial input). `CLR_N` low sets all eight outputs to 0 at once, without waiting for the clock, and holds them there.
- `## Pins`: `A`, `B`; `CLK`; `CLR_N`; `QA`–`QH` (`QA` first stage); `VCC`; `GND`.

`manuals: every catalogue Part has a Datasheet` and `catalogue: chip pinouts are consistent` cover both.

### 1.4 — Tests

`tests/parts/ls164_test.cpp` `[parts]`, bench as [ls161_test.cpp](../../tests/parts/ls161_test.cpp) (`Input`, `Clock(rises)`, `ClearN(later)`, `Bench` with one `Ls164` and Stimulus on the listed inputs, outputs `QA`–`QH` recorded). Add to [tests/CMakeLists.txt](../../tests/CMakeLists.txt) after `parts/ls161_test.cpp`.

| Test case | Checks |
|---|---|
| `74LS164: catalogue entry` | `FindPart("74LS164")`: kind `Chip`, package `DIP-14`; Pin names by index match the header constants |
| `74LS164: a one walks to QH` | `CLR_N` per `ClearN()`, `A` 1 for the first rise only (then 0), `B` 1, 8 rises: `QA` rises at 100 ns + 17 ns; `QH` rises at the 8th rise + 17 ns and is the only 1 after it |
| `74LS164: A and B are ANDed` | `A` 1 `B` 0 → `QA` 0 after a rise; `A` 0 `B` 1 → 0; `A` X `B` 0 → 0 (never X); `A` 1 `B` 1 → 1 |
| `74LS164: clear` | stages all 1, then `CLR_N` low between rises: every output falls 24 ns later, before the next rise; clock rises while `CLR_N` is low leave them 0 |
| `74LS164: data changing with the edge is not seen` | `A` steps 0 → 1 at the same time as the rise: `QA` stays 0 |
| `74LS164: power-on outputs are known` | for seeds 1..8: no output is X after `powerOn`; at least one seed gives a different pattern from seed 1 |

### Fence

- [ ] `src/parts/ls164.{hpp,cpp}`, two delays, catalogue entry, `datasheets/74LS164.md`
- [ ] `ctest --preset debug -R "74LS164|catalogue|manuals"` passes; full test gate passes
- [ ] Commit: `"p4b: phase 1 — 74LS164 Part"`

---

## Phase 2 — Chaser netlist, manuals, fault-free test

The Machine's design netlist and manuals, tested without a Board (`LoadNetlistMachine` needs one, so the test builds the netlist directly). LED_CHASER_DESIGN.md, Netlist and Manuals.

### 2.1 — Netlist

`netlists/chaser.bwn`, written out in full from the sketch in LED_CHASER_DESIGN.md (style of [counter.bwn](../../netlists/counter.bwn): header comment pointing at `docs/machines/020-led-chaser/LED_CHASER.md`, one `net` line per Net, comments on the VCC/GND ties and on the feedback):

- Parts: `X1 OSC freq=1MHz`, `J1 POWER`, `R1 R value=4k7`, `R2`–`R9 R value=330`, `U1`–`U4 74LS161`, `U5 74LS164`, `U6 74LS30`, `D1`–`D8 LED`.
- `CLK`: `X1.OUT`, `U1`–`U4.CLK`. `RESET_N`: `R1.2`, `U1`–`U4.CLR_N`, `U5.CLR_N`.
- `VCC`: `R1.1`, `R2.1`–`R9.1`; `U1`–`U4` `ENP`, `LOAD_N`; `U1.ENT`; `U5.B`; `U6.H`.
- `GND`: `A`–`D` of `U1`–`U4`.
- `C0`–`C14` on `QA`–`QD` (no `C15`), `CARRY1`–`CARRY3`, `STEP U4.QD U5.CLK`; `U4.RCO` unconnected.
- `SER U6.Y U5.A`; `S1`–`S7` on `U5.QA`–`U5.QG`, `U6.A`–`U6.G`, `D1.K`–`D7.K`; `S8 U5.QH D8.K`.
- `LED_A1`–`LED_A8`: `R(n+1).2 Dn.A`.

`build/debug/src/cli/bw run netlists/chaser.bwn --time 1ms` loads without diagnostics.

### 2.2 — Manuals

`manuals/chaser/user.md` and `manuals/chaser/service.md`, shape of [manuals/counter](../../manuals/counter) (LED_CHASER_DESIGN.md, Manuals):

- User: What it does (one light runs along eight LEDs, about 15 steps a second; a few may light together for the first half second), Connections (`J1`).
- Service: How it works (clock, 16-bit divider to `STEP`, shift register, `U6` feedback feeding one low bit, LEDs lit while their stage is low); Components (a role for every reference `X1`, `J1`, `R1`–`R9`, `U1`–`U6`, `D1`–`D8`); Troubleshooting (`STEP` at `U5` pin 8; `SER` at `U5` pin 1 and `U6` pin 8; each stage against the next on the analyzer; each LED's anode).

[tests/machines/manuals_test.cpp](../../tests/machines/manuals_test.cpp#L41): loop over `{"counter", "test_pattern", "chaser"}`; rename the test case to `"manuals: every Machine's manuals pass their checks"`.

### 2.3 — Fault-free test

`tests/machines/chaser_test.cpp` `[machines]`; add after `machines/bodge_wire_test.cpp` in [tests/CMakeLists.txt](../../tests/CMakeLists.txt). Helpers in the anonymous namespace, as [counter_test.cpp](../../tests/machines/counter_test.cpp#L26-L76):

```cpp
constexpr Time kStep = 65'536_us;  // STEP period: 2^16 CLK cycles

// "S3" → "U5.QC", "STEP" → "U4.QD", "SER" → "U6.Y", else `name` is already "REF.PIN".
std::string PinOf(const std::string& name);

// Samples of each named Net from a seeded power-on of `loaded` to `end`.
Recorded Record(const LoadedCircuit& loaded, Time end, const std::vector<std::string>& names);

std::vector<Time> Rises(const std::vector<Sample>& samples);

// Value of `samples` at `t` (last Sample at or before `t`).
LogicValue At(const std::vector<Sample>& samples, Time t);

// The stages S1..S8 read just before each of the STEP rises 10..17: one string of '0'/'1'/'X'
// per rise.
std::vector<std::string> Stages(const Recorded& recorded);
```

`Stages` reads 1 µs before each rise, after the previous shift has settled.

| Test case | Checks |
|---|---|
| `chaser: fault-free light runs` `[slow]` | `LoadNetlistFile(BW_NETLIST_DIR "/chaser.bwn")`, record `STEP`, `S1`–`S8` to 18 × `kStep`: at least 17 `STEP` rises, spaced `kStep`; each `Stages` entry has exactly one `'0'`, and its index moves +1 (mod 8) from one entry to the next |

### Fence

- [ ] `netlists/chaser.bwn`, `manuals/chaser/{user,service}.md`
- [ ] `ctest --preset debug -R "chaser|manuals"` passes; full test gate passes
- [ ] Commit: `"p4b: phase 2 — chaser netlist and manuals"`

---

## Phase 3 — Chaser Board (user layout)

The user places and routes the Board in KiCad; the agent stops and waits. LED_CHASER_DESIGN.md, Board.

### 3.1 — KiCad netlist (agent)

`build/debug/src/cli/bw export-kicad netlists/chaser.bwn -o boards/chaser/chaser.net`. Tell the user:

- `D1`–`D8` in one row left to right, `R2`–`R9` beside them; `U5` and `U6` next to the row; `U1`–`U4`, `X1`, `J1`, `R1` as on the counter.
- `LED_A5` (`R6` → `D5`) and `SER` (`U6` pin 8 → `U5` pin 1) each need one straight segment several mm long: the crack is at its midpoint, and both pieces must clear the pads (a 1.2 mm stub at a 1.6 mm pad can't be cracked).
- KiCad DRC clean; save as `boards/chaser/chaser.kicad_pcb`.

**Stop here until the user says the Board is done.**

### 3.2 — Board checks

- `bw check-board netlists/chaser.bwn boards/chaser/chaser.kicad_pcb` clean.
- Re-run `cmake --preset debug` so the `boards/*/*.kicad_pcb` glob picks up `drc_chaser`.

### 3.3 — Per-Machine test loops

Add `"chaser"` to each list of Machines:

- [boards_test.cpp](../../tests/board/boards_test.cpp#L76) (courtyards), plus a test case `"boards: chaser matches its netlist"` as the counter one at L36.
- [electrical_test.cpp](../../tests/machine/electrical_test.cpp#L73).
- [board_scene_test.cpp](../../tests/view/board_scene_test.cpp#L146) (outline; rename to `"view: outline of every Board is a rectangle"`) and [L425](../../tests/view/board_scene_test.cpp#L425).

### 3.4 — Fault-free test on the Machine

In `chaser_test.cpp`, `FaultFree()` as in counter_test (`LoadNetlistMachine`, `REQUIRE`), and `Record` takes a `const Machine&` and builds it (as counter_test); the fault-free test uses `FaultFree()`.

### Fence

- [ ] `boards/chaser/chaser.kicad_pcb` (+ `chaser.kicad_pro`, `chaser.net`) committed
- [ ] `drc_chaser`, `boards`, `electrical`, `view`, `chaser` tests pass; full test gate passes
- [ ] Commit: `"p4b: phase 3 — chaser Board"`

---

## Phase 4 — Chaser Puzzles

Four Puzzle files and a Symptom/Repair test for each. LED_CHASER.md, Puzzles; LED_CHASER_DESIGN.md, Puzzles.

### 4.1 — Crack points

Pick on the layout the midpoint of the longest straight `LED_A5` segment and of the longest straight `SER` segment (layer, x, y; `FindCrackTrace` must find each). [tests/machine/test_points.hpp](../../tests/machine/test_points.hpp):

```cpp
// Midpoint of the straight LED_A5 Trace (R6.2 → D5.A) of boards/chaser; also in puzzles/gap.bwp.
inline constexpr Point kLedA5Crack{..., ...};

// Midpoint of the straight SER Trace (U6.Y → U5.A) of boards/chaser; also in
// puzzles/dark_chaser.bwp.
inline constexpr Point kSerCrack{..., ...};
```

### 4.2 — Puzzle files

`puzzles/gap.bwp`, `stuck_light.bwp`, `dark_chaser.bwp`, `twins.bwp`, format of [dark_led.bwp](../../puzzles/dark_led.bwp): `machine chaser`, a title, a brief in the owner's words (the Symptom, never the cause), one fault line:

| File | Title | Fault line |
|---|---|---|
| `gap.bwp` | Gap | `fault crack <layer> <x> <y>` at `kLedA5Crack` |
| `stuck_light.bwp` | Stuck light | `fault stuck U5.QD 0` |
| `dark_chaser.bwp` | Dark chaser | `fault crack <layer> <x> <y>` at `kSerCrack` |
| `twins.bwp` | Twins | `fault bridge U5.3 U5.4` |

### 4.3 — Tests

In `chaser_test.cpp`, `LoadPuzzle(name)` as counter_test. Each `[slow]` test records to 18 × `kStep`, then applies the Repair to the Machine (as the counter Puzzle tests do) and records again:

| Test case | Symptom | After the Repair |
|---|---|---|
| `chaser: Puzzles have a title and a brief` (not slow) | each of the four: `machine == "chaser"`, title and brief not empty | — |
| `chaser: gap` | every `D5.A` Sample has `floating` set (LED Pins never pull a Net, so its value means nothing); `S5` is `'0'` in exactly one `Stages` entry | mend at `kLedA5Crack`: no `D5.A` Sample floating after power-on |
| `chaser: stuck_light` | every `Stages` entry is `"11101111"` | swap `U5`: one moving `'0'` |
| `chaser: dark_chaser` | `U5.A` floating and reading 1 (TTL input); every `Stages` entry is `"11111111"` | mend at `kSerCrack`: one moving `'0'` |
| `chaser: twins` | in every entry `S1 == S2`; exactly two entries have `S1` `'0'` and they are consecutive (mod 8) | `UnbridgeRepair` on `U5.3`–`U5.4` (as in shorts_power_test): one moving `'0'` |

"One moving `'0'`" is the fault-free check of phase 2, factored into a helper `void CheckRunning(const std::vector<std::string>& stages)`.

For `twins`: two TTL totem-pole outputs in contention resolve to 0 ([logic.cpp](../../src/sim/logic.cpp#L12-L14)), so the bridged Net is low while either stage is low: the low bit shows on `S1` and `S2` while in `QA` and while in `QB`.

### Fence

- [ ] Four `.bwp` files, two crack constants
- [ ] `ctest --preset debug -R chaser` passes; full test gate passes
- [ ] Commit: `"p4b: phase 4 — chaser Puzzles"`

---

## Phase 5 — Watch budget, Session, screenshot

Design §4 and §9.

### 5.1 — `kMaxWatched`

[src/runner/dispatcher.hpp](../../src/runner/dispatcher.hpp#L16): `inline constexpr std::size_t kMaxWatched = 32;  // ring slots`. Check that `probe consumer: touching many Nets in turn` and the dispatcher/runner tests do not hard-code 16; fix any that do to use `kMaxWatched`.

### 5.2 — Session test

[tests/session/session_test.cpp](../../tests/session/session_test.cpp), after `session: counter LED`:

```cpp
TEST_CASE("session: chaser with every instrument", "[session][slow]")
```

- Machine from `LoadNetlistMachine(BW_NETLIST_DIR "/chaser.bwn")`, `Speed::Max`.
- `leds()` references are `D1`–`D8`.
- 8 Clips: `U5` pins 3, 4, 5, 6, 10, 11, 12, 13. Hooks: `U5` pin 8 and `U6` pin 8. Probe: `touch` at `padLead("U5", "1")`. Meter in Volts: `placeLead` on the `D1.A` pad (Volts uses only the red Lead; a second `placeLead` would replace it).
- `powerOn(1, 1_s)`, `waitIdle()`: no Check fired (the test reaching here), sum of the eight brightnesses within 0.05 of 1. `powerOff()`.

### 5.3 — Screenshot

Release build; `build/release/src/ui/bw_ui netlists/chaser.bwn --screenshot /tmp/chaser.png --after 1s`: one LED lit. Same for `puzzles/stuck_light.bwp` (`D4` lit) and `puzzles/twins.bwp`.

### Fence

- [ ] `kMaxWatched == 32`
- [ ] `ctest --preset debug -R "session|runner|probe"` passes; full test gate passes
- [ ] Screenshots checked
- [ ] Commit: `"p4b: phase 5 — watch budget 32, chaser Session test"`

---

## Phase 6 — Release check, user check, docs

### 6.1 — Release

`cmake --preset release && cmake --build --preset release && ctest --preset release`. `bw_ui netlists/chaser.bwn` with 8 Clips, 2 Hooks, probe and meter attached: status line 1.00×, ui time as P4a (≈ 0.2 ms).

### 6.2 — User check

Ask the user to play the four Puzzles. **Wait for the answer.**

### 6.3 — Docs

- This plan: Status column with commit hashes, a short "Implemented" paragraph and implementation notes (crack points, test times); prune phase detail as execute-plan does.
- [ARCHITECTURE.md](../ARCHITECTURE.md): P4b row marked done (only that row; the user may have edits elsewhere).
- LED_CHASER_DESIGN.md: crack coordinates in the Puzzles table instead of `<layer> <x> <y>`.

### Fence

- [ ] Release tests pass; 1.00× with every instrument
- [ ] User says the Puzzles work
- [ ] Commit: `"docs: P4b done"`

# P4a Counter LED — Implementation Plan

Design: [COUNTER_LED_DESIGN.md](COUNTER_LED_DESIGN.md)
Spec: [COUNTER.md](../machines/010-counter/COUNTER.md), roadmap row P4a in [ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | LED Part: `PartKind::Led`, `Led` Component, catalogue, KiCad footprint, Datasheet | |
| 2 | `LedGlow` (bw_devices) with unit tests | |
| 3 | `LedConsumer` (bw_runner) with Runner tests | |
| 4 | Counter rework: netlist, KiCad board (**user layout**), manuals, existing tests | |
| 5 | Counter Puzzles + headless Symptom/Repair tests | |
| 6 | Session `leds()`, Board view drawing, screenshots | |
| 7 | Release check, user check, docs | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug -j 12`

C++ is `clang-format -i` clean (never CMakeLists.txt). Release checks: `cmake --preset release && cmake --build --preset release`; UI screenshots need the release build.

---

## Phase 1 — LED Part

The Part first: everything else (netlist, board, consumer) names it. Design §2, §3.1.

### 1.1 — Naming and kind

- [docs/NAMING.md](../NAMING.md) Abbreviations: add `led` to the allowed list.
- [src/parts/catalogue.hpp](../../src/parts/catalogue.hpp#L15): `enum class PartKind : std::uint8_t { Chip, Resistor, Connector, Led };`

### 1.2 — `Led` Component

Create `src/parts/led.hpp`, `src/parts/led.cpp` (add `led.cpp` to `src/parts/CMakeLists.txt`):

```cpp
// Red 5 mm LED. Pin index = KiCad pad − 1: pad 1 is the cathode (LED_THT convention).
// Passive: both Pins are loads that never pull a floating Net (Family::Mos), no behaviour.
class Led final : public Component {
public:
    explicit Led(std::string reference);  // {"K", Input, Mos}, {"A", Input, Mos}
    void evaluate(EvalContext& ctx) override;  // nothing
};
```

### 1.3 — Catalogue and footprint

- [src/parts/catalogue.cpp](../../src/parts/catalogue.cpp): `#include "parts/led.hpp"`; above `using enum PartKind;` add
  `std::expected<Component*, std::string> CreateLed(Circuit&, std::string reference, std::span<const PartParam> params)` → `"LED takes no parameters"` if any, else `&circuit.add<Led>(std::move(reference))`.
  In `s_parts`, sorted by name (between `74LS86` and `OSC`): `PartInfo{"LED", "red 5 mm LED", "LED-5mm", Led, &CreateLed}`.
- [src/board/kicad_netlist.cpp](../../src/board/kicad_netlist.cpp#L15-L19): `kFootprints` size 6, add `{"LED-5mm", "LED_THT:LED_D5.0mm"}`.

### 1.4 — Datasheet

`datasheets/LED.md`: title `# LED — red 5 mm LED`; `## Function`: lights while current flows from `A` to `K`: `A` high and `K` low; TTL usually sinks it (+5 V → resistor → `A`, `K` on an output), so it lights while that output is low; dark if either end is not connected. `## Pins`: `` `K` ``: cathode, pad 1 (square pad, flat side); `` `A` ``: anode, pad 2.

### 1.5 — Tests

- `tests/parts/led_test.cpp` (add to `tests/CMakeLists.txt`): `"led: catalogue entry"` (kind `Led`, package `LED-5mm`, `create` with params fails); `"led: pins are passive loads"` (2 Pins `K`, `A`, `Input`, `Mos`); `"led: a Net with only LED pins stays floating"` (Circuit with one Net on `D1.K`, Simulator: `Resolution.floating` / Sample floating true).
- `tests/board/kicad_netlist_test.cpp`: `KicadFootprint("LED-5mm") == "LED_THT:LED_D5.0mm"`.
- Existing `catalogue: chip pinouts are consistent` skips non-`Chip` kinds (unchanged); `manuals: every catalogue Part has a Datasheet` covers `LED.md`.

### Fence

- [ ] `src/parts/led.{hpp,cpp}`, `datasheets/LED.md` exist; `FindPart("LED")` works
- [ ] Tests pass: `ctest --preset debug -R "led|catalogue|kicad netlist|manuals"`, then full gate
- [ ] Commit: `"P4a: phase 1 — LED Part"`

---

## Phase 2 — LedGlow

Pure brightness model, no threads. Design §4, §5.1, §5.2.

### 2.1 — Class

Create `src/devices/led_glow.hpp`, `led_glow.cpp`; add `led_glow.cpp` to `src/devices/CMakeLists.txt`. Interface exactly as design §4 (`kLedWindow = 20'000'000'000` ps, `setAnode`, `setCathode`, `advanceTo`, `brightness`, `reset`). Private `lit()`:

```cpp
return anode_.value == LogicValue::One && !anode_.floating &&
       cathode_.value == LogicValue::Zero && !cathode_.floating;
```

`setAnode(s)` / `setCathode(s)`: `advanceTo(s.time)`, then store `s`. `advanceTo(t)`: while `windowEnd_ <= t` { `integrateTo(windowEnd_)`; `brightness_ = litTime_ / kLedWindow` (as float); `litTime_ = 0`; `windowEnd_ += kLedWindow` }, then `integrateTo(t)`. `integrateTo(t)`: if `t > integrated_` and `lit()`, `litTime_ += t − integrated_`; `integrated_ = max(integrated_, t)`. `reset()`: back to the member defaults.

### 2.2 — Tests

`tests/devices/led_glow_test.cpp` (tag `[devices]`):
- `"led glow: dark before the first window"`; `"led glow: steadily lit reads 1"` (A 1, K 0 at t 0, advance 40 ms);
- `"led glow: floating cathode stays dark"`, `"led glow: floating anode stays dark"`, `"led glow: anode low stays dark"`;
- `"led glow: 1 kHz square wave reads half"` (K toggles every 500 µs; 0.5 ± 0.001);
- `"led glow: an edge inside a window splits it"` (K goes 0 at 5 ms → first window 0.75);
- `"led glow: reset goes dark"`.

### Fence

- [ ] `ctest --preset debug -R "led glow"`, then full gate
- [ ] Commit: `"P4a: phase 2 — LedGlow"`

---

## Phase 3 — LedConsumer

Threaded wrapper, fixed Nets like `TvConsumer`. Design §6.

### 3.1 — Class

Create `src/runner/led_consumer.hpp`, `led_consumer.cpp`; add to `src/runner/CMakeLists.txt`. Interface as design §6.
- Constructor: store `leds_`; `nets_` = distinct anode/cathode Nets in first-seen order; `glows_.resize(n)`; `published_` = `std::vector<std::atomic<float>>(n)`.
- `sample(net, s)`: for each LED `i`: if `leds_[i].anode == net` → `glows_[i].setAnode(s)`; if `cathode == net` → `setCathode(s)` (both when equal).
- `advanceTo(t)`: each glow `advanceTo(t)`; `published_[i].store(glows_[i].brightness(), relaxed)`.
- `powerOn()`: reset all glows, publish 0. `powerOff()`: publish 0. `brightness(i)`: relaxed load.

### 3.2 — Tests

`tests/runner/led_consumer_test.cpp` (tag `[runner]`), on a hand-built Circuit (Parts via `FindPart(...)->create`, `VCC`/`GND` Supply ties and power Pins connected, as `ApiCounter` in [sample_test.cpp](../../tests/netlist/sample_test.cpp); Runner use as [meter_consumer_test.cpp](../../tests/runner/meter_consumer_test.cpp)): `OSC freq=1kHz` on Net `K`, Net `A` tied `One` Weak, `Led` `D1` on both.
- `"led consumer: watches its Nets"` (`nets()` = {A, K}, distinct when two LEDs share K);
- `"led consumer: square wave glows half"` (Runner `pauseAt(100 ms)`, `powerOn`, `waitIdle`: brightness 0.5 ± 0.01);
- `"led consumer: dark after power off"`;
- `"led consumer: one Net feeds two LEDs"` (second LED with A on K's Net, K on a Strong 0: both get Samples).

### Fence

- [ ] `ctest --preset debug -R "led consumer"`, then full gate
- [ ] Commit: `"P4a: phase 3 — LedConsumer"`

---

## Phase 4 — Counter rework

Netlist, board, manuals and every existing counter test together, so the build stays green. **Stops for the user's KiCad layout.** Design §10.

### 4.1 — Netlist

Rewrite [netlists/counter.bwn](../../netlists/counter.bwn) as design §10: header comment (010 counter: 1 MHz → 20-bit chain → LED on `C19`, lit while low); parts `X1 OSC freq=1MHz`, `J1 POWER`, `R1 R value=4k7`, `R2 R value=330`, `U1`–`U5 74LS161`, `D1 LED`; Nets `CLK`, `RESET_N`, `VCC` (R1.1, R2.1, all `ENP`, `LOAD_N`, `U1.ENT`), `GND` (all data inputs), `C0`–`C19` (`U5.QD` and `D1.K` on `C19`), `CARRY1`–`CARRY4`, `LED_A` (`R2.2 D1.A`). `bw run netlists/counter.bwn --time 1ms` loads without warnings.

### 4.2 — Board (user)

Agent: `build/debug/src/cli/bw export-kicad netlists/counter.bwn > boards/counter/counter.net`. **User**, in KiCad ([KICAD_GUIDE.md](../p2a-board-layout/KICAD_GUIDE.md)): Update PCB from netlist, delete old U3/U4 footprints, place U3–U5, R2, D1 (U1, U2, X1, J1, R1 keep their places), route, save. Agent: `bw check-board netlists/counter.bwn boards/counter/counter.kicad_pcb` clean, `ctest --preset debug -R drc_counter` passes.

### 4.3 — Manuals

Rewrite `manuals/counter/user.md` (What it does: `D1` blinks about once a second, on 0.5 s, off 0.5 s; bits `C0`–`C19`, carries; Connections: `J1` only) and `service.md` (How it works: clock, reset, 20-bit chain via `CARRY1`–`CARRY4`, `D1` sinks into `C19` through `R2`; Components: `X1`, `J1`, `R1`, `R2`, `U1`–`U5`, `D1`; Troubleshooting: walk `X1` pin 8 → `U1`–`U4` pin 15 (`RCO`) → `U5` pin 11 → `D1`, with expected rates: `CARRY1` pulses every 16 µs, `CARRY2` 256 µs, `CARRY3` 4.1 ms, `CARRY4` 65.5 ms).

### 4.4 — Existing tests

- [tests/netlist/sample_test.cpp](../../tests/netlist/sample_test.cpp): `Count()` over `C0`–`C19` (20 bits, wraps at 2^20); `ApiCounter` rebuilt for the new circuit (same creation order as the netlist; `LED_A` Weak One tie for R2; `Led` D1); component count 8 (X1, U1–U5, J1, D1); `counter counts` unchanged otherwise; drop `DIV toggles…` and `TOP_N`; add `"sample: carries pulse at their rates"` (600 µs from a seeded power-on: `CARRY1` rises every 16 µs, 37–38 times; `CARRY2` rises 2–3 times, 256 µs apart); keep `same as API-built circuit`.
- [tests/netlist/net_pins_test.cpp](../../tests/netlist/net_pins_test.cpp#L37-L54): `CLK` and `RESET_N` load `U1`–`U5` (pin 2 `CLK`, pin 1 `CLR_N`).
- [tests/machine/test_points.hpp](../../tests/machine/test_points.hpp#L11-L13): re-pick `kR1VccCracks` on the new layout (midpoint of every `VCC` Trace at `R1` pad 1) for `electrical: pull-up cut from VCC is left out`.
- [tests/manual/manual_test.cpp](../../tests/manual/manual_test.cpp#L38-L50): `kGoodService` with the new Components (`R2`, `U1`–`U5`, `D1`; no `DIV`, `TOP_N`).
- [tests/view/board_scene_test.cpp](../../tests/view/board_scene_test.cpp), [tests/board/boards_test.cpp](../../tests/board/boards_test.cpp), [tests/machine/electrical_test.cpp](../../tests/machine/electrical_test.cpp): fix counter-specific references and counts to the new board (the removed-track test still expects one open).

### Fence

- [ ] `boards/counter/counter.kicad_pcb` laid out by the user; `bw check-board` clean; `drc_counter` passes
- [ ] Full test gate passes
- [ ] Commit: `"P4a: phase 4 — counter rework (20-bit chain, LED)"`

---

## Phase 5 — Counter Puzzles

Three Puzzles with headless Symptom and Repair checks. COUNTER.md Puzzles, design §10, §11.

### 5.1 — Puzzle files

`puzzles/dark_led.bwp`, `frozen_led.bwp`, `too_fast.bwp`, shape of [unpowered_chip.bwp](../../puzzles/unpowered_chip.bwp): comment, `machine counter`, `title`, `brief`, `fault`:
- Dark LED: brief "The LED never lights." `fault crack F.Cu X Y` on the `C19` Trace between `U5` pin 11 and `D1` pad 1; pick X Y on a straight segment of the new layout (if `C19` runs on B.Cu there, use `B.Cu`).
- Frozen LED: brief "The LED never changes: it stays on, or stays off." `fault stuck U3.RCO 0`.
- Too fast: brief "The LED flickers instead of blinking." `fault stuck U1.RCO 1`.

### 5.2 — Tests

`tests/machines/counter_test.cpp` (tag `[machines]`, add to `tests/CMakeLists.txt`), loading via `LoadPuzzleFile(BW_PUZZLE_DIR / …)` / `LoadNetlistMachine`, building with `Machine::build()`, running a `Simulator` (`powerOn(1)`) with recorded Nets; LED brightness by feeding the recorded `D1.A` / `D1.K` Net Samples to a `LedGlow` merged in time order (`LedGlow` time only moves forward), then `advanceTo(end)`. Power-on state is seeded random, so no check may rely on where the counter starts:
- `"counter: fault-free bits and carries run"`: within 10 ms `C0`–`C11` toggle, `CARRY1`, `CARRY2` pulse;
- `"counter: frozen_led"`: 10 ms, `CARRY3` never 1, `C12`–`C19` constant; after `swap U3` and rebuild, `CARRY3` pulses within 10 ms;
- `"counter: too_fast"`: `C19` changes within 70 ms; LedGlow windows differ over 140 ms; after `swap U1`, `CARRY1` rises every 16 µs over 100 µs and `C19` changes at most once over 70 ms;
- `"counter: dark_led"`: `D1.K`'s Net floating, LedGlow 0 over 40 ms; after `mend` at the crack, `D1.K`'s Net is `C19`'s;
- `"counter: LED blinks once a second" [machines][slow]`: fault-free, 1.1 s, LedGlow has windows at 0 and at 1. `[slow]` is a plain tag (first in the repo): it runs in both test gates (≈ 16 s in debug); `-R` filters skip it by name only.

### Fence

- [ ] `ctest --preset debug -R counter`, then full gate
- [ ] Commit: `"P4a: phase 5 — counter Puzzles"`

---

## Phase 6 — LED in the session and on the Board

Design §3.2–§3.4, §5.3.

### 6.1 — Session

[src/session/session.hpp](../../src/session/session.hpp): `struct LedLight { std::string reference; float brightness; };`; `Live` gets `std::optional<LedConsumer> leds;` before `runner`; `std::vector<LedLight> leds() const;`. [session.cpp](../../src/session/session.cpp) `makeLive`: design §3.2 snippet (inside `if (machine_)`). `leds()`: for each `PartKind::Led` part of `machine_->designCircuit()` in netlist order, brightness from `live_->leds` (index by reference) when `powered_`, else 0; empty without a Machine.

### 6.2 — Board view

[src/ui/board_view.hpp](../../src/ui/board_view.hpp): `struct LedTag { std::size_t component; float brightness; };`, `void setLeds(std::span<const LedTag> leds)`, member `std::vector<LedTag> leds_`. [board_view.cpp](../../src/ui/board_view.cpp): constants `kLedDark = IM_COL32(90, 20, 20, 255)`, `kLedLit = IM_COL32(255, 40, 30, 255)`; in `draw`, after `drawTraces(active_, 1.0f, 1.0f)` and before the pad loop, design §5.3 (courtyard bounding box, radius 0.39 × min side, halo at 1.6× with alpha 0.35 × brightness, body lerped).

### 6.3 — Frame loop

[src/ui/main.cpp](../../src/ui/main.cpp#L495), after `boardView.setLeads(leadTags)`: build `std::vector<LedTag>` from `session.leds()` with `boardView.scene()->componentIndex(reference)` (skip if none), `boardView.setLeds(tags)`.

### 6.4 — Tests and screenshots

- `tests/session/session_test.cpp`: `"session: counter LED"` — counter Session (`LoadNetlistMachine`, `Speed::Max`): `leds()` = {`D1`, 0} before power-on; `powerOn(1, 100 ms)`, `waitIdle`: brightness is 0 or 1 (a C19 level); power off → 0. `"session: no LEDs on the test pattern"`: `leds()` empty.
- Release screenshots (agent views the PNGs): `bw_ui netlists/counter.bwn --screenshot /tmp/p4a/counter.png --after 1s`, and `puzzles/too_fast.bwp`, `puzzles/dark_led.bwp`. D1 drawn red-lit or dark, pads visible on top; `bw_ui` prints ratio 1.00× and ui ms.

### Fence

- [ ] Full test gate passes
- [ ] Screenshots show `D1` as expected
- [ ] Commit: `"P4a: phase 6 — LED drawn on the Board"`

---

## Phase 7 — Release check, user check, docs

### 7.1 — Release

`ctest --preset release -j 12`; `bw_ui` on the counter and each new Puzzle: ratio 1.00×, ui ms noted.

### 7.2 — User check

User plays the three counter Puzzles in `bw_ui` (release). Record the verdict and any "Later" items here.

### 7.3 — Docs

[ARCHITECTURE.md](../ARCHITECTURE.md) P4a row: "done <date>: …" with this plan's link; prune this plan to the status table and implementation notes (execute-plan convention).

### Fence

- [ ] Release tests pass; user check recorded
- [ ] Commit: `"docs: P4a done"`

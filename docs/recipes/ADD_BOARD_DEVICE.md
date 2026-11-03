# Recipe — Add an on-board device Part

A Part the player watches or presses on the Board itself: LED (P4a), push button (P4d), next a 7-segment digit. Off-board devices (TV, keyboard) go behind a Connector instead.

Reference commits: LED output `ab92f30` → `22f5a90` → `b8eb2c2` → `8049a68`; button input `28ecaea` → `e5983f3` → `c2958a2` → `469ce1d` → `e86fa8c` → `175929e`.

## Output device (lit/shown on the Board): the LED chain

| Layer | File | What |
|---|---|---|
| Part | `src/parts/led.{hpp,cpp}` | passive `Component`, pins are `Input` loads with `Family::Mos` (never pull a floating Net), empty `evaluate`. Pin index = KiCad pad − 1 |
| Catalogue | `src/parts/catalogue.{hpp,cpp}` | new `PartKind` value; `CreateXxx` factory defined **above** `using enum PartKind`; `PartInfo` row sorted by name |
| Footprint | `src/board/kicad_netlist.cpp` | package → KiCad footprint in `kFootprints` |
| Look | `src/devices/led_glow.{hpp,cpp}` (bw_devices) | pure, time-driven model of what the eye sees (`setAnode/setCathode(Sample)`, `advanceTo(Time)`, `brightness()`, `reset()`); 20 ms windows; floating = off |
| Feed | `src/runner/led_consumer.{hpp,cpp}` (bw_runner) | `Consumer` with fixed Nets; one model per device; publishes `std::atomic<float>` for the UI thread; dark on `powerOff` |
| Session | `src/session/session.{hpp,cpp}` | `makeLive()` collects Parts of the kind via `PinNet(...)` and adds the consumer; accessor (`leds()`) returns per-Part state, dark when not powered |
| UI | `src/ui/board_view.{hpp,cpp}`, `src/ui/main.cpp` | `setLeds(std::span<const LedTag>)` each frame from `session.leds()` + `scene->componentIndex(ref)`; drawn under the pads |
| Datasheet | `datasheets/<PART>.md` | required for every catalogue Part |

Tests: `tests/parts/<part>_test.cpp`, `tests/devices/<x>_test.cpp`, `tests/runner/<x>_consumer_test.cpp`, a `[session]` case in `tests/session/session_test.cpp` (cf. `session: counter LED`).

## Input device (pressed on the Board): the button chain

- Part `src/parts/button.{hpp,cpp}`: `Output` TTL pins; `attach(signal, rail)` by the builder (`wireButtons` in `src/netlist/builder.cpp`); the rail pin is not connected as a driver; `std::atomic` pressed state, change wakes the sim via `Simulator::wake` / `Runner::wake`.
- `ButtonOf(loaded, ref)` in `src/netlist/builder.hpp` finds it in a build.
- Electrical netlist leaves buttons out like resistors (`src/machine/electrical.cpp`), but their pins are still named.
- Session holds presses (`Session::setPressed`/`pressed`); UI picks the body with `BoardScene::bodyAt`; CLI `--press SW1:200ms:700ms` (`bw run`, `bw_ui`).
- A UI press must land at any time inside the slice (`836dd59`: dice rolled only even faces otherwise).

## New PartKind checklist

Grep `PartKind::` and decide each branch: `src/netlist/builder.cpp`, `src/netlist/net_pins.cpp`, `src/machine/electrical.cpp`, `src/machine/machine.cpp`, `src/session/session.cpp`.

## Budgets and gotchas

- `kMaxWatched` (`src/runner/dispatcher.hpp`) = 32 watched Nets for devices + instruments together; each LED costs 2. Check the worst Machine with every instrument attached.
- Release gate: `bw_ui --screenshot` on the Machine with every instrument attached must stay at 1.00× and the UI under ~1 ms.
- Courtyards: some footprints draw `F.CrtYd` as lines that stop at pad centres; the reader unions pads in (`23e790e`, `360a787`).

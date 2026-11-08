# Recipe — Add an on-board device Part

A Part the player watches or presses on the Board itself: LED (P4a), push button (P4d), 7-segment digit (P4f). Off-board devices (TV, keyboard) go behind a Connector instead.

Reference commits: LED output `ab92f30` → `22f5a90` → `b8eb2c2` → `8049a68`; button input `28ecaea` → `e5983f3` → `c2958a2` → `469ce1d` → `e86fa8c` → `175929e`; digit (lamps, drawing) `1419047` → `a2deab7`.

## Output device (lit/shown on the Board): the LED chain

A Part that lights is a list of lamps in its catalogue row (`Lamp`: shape, up to two anode Pins, cathode Pin). The Session watches every lamp of every such Part, so a new lamp Part with a known shape (round LED, 7-segment digit) is a catalogue row, a Part class and a Datasheet; only a new shape needs the Look and UI rows below.

| Layer | File | What |
|---|---|---|
| Part | `src/parts/led.{hpp,cpp}` | passive `Component`, pins are `Input` loads with `Family::Mos` (never pull a floating Net), empty `evaluate`. Pin index = KiCad pad − 1 |
| Catalogue | `src/parts/catalogue.{hpp,cpp}` | `PartInfo` row sorted by name, with its `lamps` (`kLedLamps`, `kDigitLamps`); `CreateXxx` factory defined **above** `using enum PartKind`; a new `PartKind` only for a new kind of behaviour |
| Footprint | `src/board/kicad_netlist.cpp` | package → KiCad footprint in `kFootprints` |
| Look | `src/devices/led_glow.{hpp,cpp}` (bw_devices) | pure, time-driven model of what the eye sees (`setAnode(Sample, which)` for up to two anodes, `setCathode(Sample)`, `advanceTo(Time)`, `brightness()`, `reset()`); 20 ms windows; floating = off |
| Feed | `src/runner/led_consumer.{hpp,cpp}` (bw_runner) | `Consumer` with fixed Nets (`LedNets`: reference, lamp, anodes, cathode); one model per lamp; watches each distinct Net once; publishes `std::atomic<float>` for the UI thread; dark on `powerOff` |
| Session | `src/session/session.{hpp,cpp}` | `makeLive()` walks `part.info->lamps`, skipping unconnected Pins (`ConnectedNet`); `leds()` returns one `LedLight` (reference, shape, brightness) per lamp, dark when not powered |
| UI | `src/ui/board_view.{hpp,cpp}`, `src/ui/main.cpp` | `setLeds(std::span<const LedTag>)` each frame from `session.leds()` + `scene->componentIndex(ref)`; drawn under the pads. Round lamps on the body; digit segments from `src/view/digit_shape.{hpp,cpp}` (`DigitFrameOf(courtyard, pad 1)`: upright along the long side, standing on the pad 1 row; `LampOutline` per shape) over a dark body |
| Datasheet | `datasheets/<PART>.md` | required for every catalogue Part |
| Chip panel | `src/ui/chip_panel.cpp` | the pinout as the part is built: DIP pins left and right; packages with pins along the top and bottom (`TopBottomRows`: `DIGIT-…`) drawn by `DigitPinout`, the hovered pin's segment lit; a new package shape needs its layout here |

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

- `kMaxWatched` (`src/runner/dispatcher.hpp`) = 48 watched Nets for devices + instruments together. Distinct Nets count: an LED costs 2, a common-anode digit its 7–8 segment Nets plus one anode Net, and a shared `GND` once. Lamps on unconnected Pins are not watched. Each ring is allocated up front (~1 MB). Check the worst Machine with every instrument attached (the stopwatch: 37 + 1).
- Resistors only go from a rail to a signal (the builder refuses series resistors): one anode resistor per digit, not one per segment.
- Lamp pins are `Family::Mos` inputs: left unconnected (an unused decimal point) they give no "floats high" warning, which is for TTL inputs only.
- Release gate: `bw_ui --screenshot` on the Machine with every instrument attached must stay at 1.00× and the UI under ~1 ms.
- Courtyards: some footprints draw `F.CrtYd` as lines that stop at pad centres; the reader unions pads in (`23e790e`, `360a787`).

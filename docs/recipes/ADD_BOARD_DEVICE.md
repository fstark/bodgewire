# Recipe — Add an on-board device Part

A Part the player watches or presses on the Board itself: LED (P4a), push button (P4d, a switch since P5i), 7-segment digit (P4f), key (P5i). Off-board devices (TV, keyboard, paddle controller) go behind a Connector instead: [ADD_CONNECTOR_DEVICE.md](ADD_CONNECTOR_DEVICE.md).

Reference commits: LED output `ab92f30` → `22f5a90` → `b8eb2c2` → `8049a68`; button input `28ecaea` → `e5983f3` → `c2958a2` → `469ce1d` → `e86fa8c` → `175929e`; digit (lamps, drawing) `1419047` → `a2deab7`; common-cathode digit and slide switch (P5l) `e88bc09`.

## Output device (lit/shown on the Board): the LED chain

A Part that lights is a list of lamps in its catalogue row (`Lamp`: shape, up to two anode Pins, up to two cathode Pins; lit while one anode is a driven 1 and one cathode a driven 0). The Session watches every lamp of every such Part, so a new lamp Part with a known shape (round LED, 7-segment digit) is a catalogue row, a Part class and a Datasheet; only a new shape needs the Look and UI rows below.

| Layer | File | What |
|---|---|---|
| Part | `src/parts/led.{hpp,cpp}` | passive `Component`, pins are `Input` loads with `Family::Mos` (never pull a floating Net), empty `evaluate`. Pin index = KiCad pad − 1 |
| Catalogue | `src/parts/catalogue.{hpp,cpp}` | `PartInfo` row sorted by name, with its `lamps` (`kLedLamps`, `kDigitLamps`, `kDigitCcLamps`: `DIGITCC`, common cathode, the same footprint as `DIGIT`; part names are letters and digits only); `CreateXxx` factory defined **above** `using enum PartKind`; a new `PartKind` only for a new kind of behaviour |
| Footprint | `src/board/kicad_netlist.cpp` | package → KiCad footprint in `kFootprints` |
| Look | `src/devices/led_glow.{hpp,cpp}` (bw_devices) | pure, time-driven model of what the eye sees (`setAnode(Sample, which)` for up to two anodes, `setCathode(Sample)`, `advanceTo(Time)`, `brightness()`, `reset()`); 20 ms windows; floating = off |
| Feed | `src/runner/led_consumer.{hpp,cpp}` (bw_runner) | `Consumer` with fixed Nets (`LedNets`: reference, lamp, anodes, cathodes); one model per lamp; watches each distinct Net once; publishes `std::atomic<float>` for the UI thread; dark on `powerOff` |
| Session | `src/session/session.{hpp,cpp}` | `makeLive()` walks `part.info->lamps`, skipping unconnected Pins (`ConnectedNet`); `leds()` returns one `LedLight` (reference, shape, brightness) per lamp, dark when not powered |
| UI | `src/ui/board_view.{hpp,cpp}`, `src/ui/main.cpp` | `setLeds(std::span<const LedTag>)` each frame from `session.leds()` + `scene->componentIndex(ref)`; drawn under the pads. Round lamps on the body; digit segments from `src/view/digit_shape.{hpp,cpp}` (`DigitFrameOf(courtyard, pad 1)`: upright along the long side, standing on the pad 1 row; `LampOutline` per shape) over a dark body. Segments go through `Perceived` (a cube root): a multiplexed digit lit a sixth of the time reads clearly; round LEDs stay linear so a half-lit LED looks half lit. The digit footprint reads upright only turned 90° (pad 1 row at the bottom): `bw place` places at 0°, so the user turns digits in KiCad |
| Datasheet | `datasheets/<PART>.md` | required for every catalogue Part |
| Chip panel (a chip's page in the manuals binder) | `src/ui/chip_panel.cpp` | the pinout as the part is built: DIP pins left and right; packages with pins along the top and bottom (`TopBottomRows`: `DIGIT-…`) drawn by `DigitPinout`, the hovered pin's segment lit; a new package shape needs its layout here |

Tests: `tests/parts/<part>_test.cpp`, `tests/devices/<x>_test.cpp`, `tests/runner/<x>_consumer_test.cpp`, a `[session]` case in `tests/session/session_test.cpp` (cf. `session: counter LED`).

## Input device (pressed on the Board): switches

Buttons and keys are one Part since P5i ([SWITCHES.md](../p5i-switches/SWITCHES.md)): a new kind of switch is a catalogue row of `Switch`, not a new chain. Reference commits: P4d button `28ecaea` → `175929e`; P5i switches (phases 1, 2, 5 of [SWITCHES_PLAN.md](../p5i-switches/SWITCHES_PLAN.md)).

- Part `src/parts/switch.{hpp,cpp}`: `Switch`, two legs `1`, `2` (TTL Outputs that never drive: the same random draws as the P4d button); `BUTTON` takes `pc=`, `KEY` (`MX-1U`, Cherry MX 1u) takes `legend=` (1–4 characters, not `#`) and `pc=` (defaults to a one-character legend; `IsPcKey` lists the names); Faults `SwitchFault::StuckClosed`, `NeverCloses`.
- Joining: the builder (`wireSwitches`) adds one `SwitchNetwork` with a `PinKind::Switched` Pin on every Net a leg is on. Closed switches group Nets; each group is driven at Supply strength with the merged drives of its Nets read without the network's own (`EvalContext::external`), so a rail beats a TTL output, a pull-up reaches the other Net, and ghosts, cracks and bridges follow from the copper. The kernel wakes the network when a Net's other drives change, never on its own drive.
- A press: `setPressed` (any thread), then wake the switch (`Simulator`/`Runner::wake(sw->id())`); the switch reads it at that instant and wakes the network: the Nets move `kPressLatency` (2 ps) after the wake. `SwitchOf(loaded, ref)`.
- A leg on no Net is open; both legs on one Net do nothing. PC keys are unique in a netlist.
- `SLIDE` (P5l, the BW-1's SST): a `Switch` on a one-position DIP slide footprint, `legend=` only and no PC key; drawn with its knob (`SwitchLook::Slide`); any click moves it (`latchToggle_`, no `BoardClick`) and `main.cpp` keeps it in `slid`, not cleared at power-off (`Session::setPressed` is ignored while unpowered; the per-frame call carries it to power-on).
- Session: `switches()`, `switchKeys()`, `setPressed`/`pressed`; CLI `--press SW1:200ms:700ms` (`bw run`, `bw_ui`).
- UI: `SwitchTag` per switch; a button drawn round, a key as the ASCII keyboard's cap (`src/ui/keycap.{hpp,cpp}`, `DrawKeyCap`) over its courtyard so the copper between keys stays visible; left-click holds, right-click latches, except under the Swap tool (it swaps). PC keys through Typing (`src/ui/board_keys.{hpp,cpp}`, F2) on a Machine without a KEYBOARD; with one, only the keyboard types (no Machine has both yet).
- A UI press must land at any time inside the slice (`836dd59`: dice rolled only even faces otherwise).

## New PartKind checklist

Grep `PartKind::` and decide each branch: `src/netlist/builder.cpp`, `src/netlist/net_pins.cpp`, `src/machine/electrical.cpp`, `src/machine/machine.cpp`, `src/session/session.cpp`.

## Budgets and gotchas

- `kMaxWatched` (`src/runner/dispatcher.hpp`) = 96 watched Nets for devices + instruments together (instruments alone: 32 Clips, 4 Hooks, the probe, the meter = up to 38). Distinct Nets count: an LED costs 2, a common-anode digit its 7–8 segment Nets plus one anode Net (multiplexed digits share their segment Nets: the BW-1's six cost 7 + 6, plus 6 decimal-point pads, each its own Net on a Board-built Circuit), and a shared `GND` once. Lamps on unconnected Pins are not watched. Rings (~1 MB each) are allocated but only touched when used. Check the worst Machine with every instrument attached.
- Resistors only go from a rail to a signal (the builder refuses series resistors): one anode resistor per digit, not one per segment.
- Lamp pins are `Family::Mos` inputs: left unconnected (an unused decimal point) they give no "floats high" warning, which is for TTL inputs only.
- Release gate: `bw_ui --screenshot` on the Machine with every instrument attached must stay at 1.00× and the UI under ~1 ms.
- Courtyards: some footprints draw `F.CrtYd` as lines that stop at pad centres; the reader unions pads in (`23e790e`, `360a787`).

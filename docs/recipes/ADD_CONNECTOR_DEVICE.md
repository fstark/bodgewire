# Recipe — Add a device behind a Connector

A standard device off the Board, plugged into a Connector Part: the paddle controller (P4i), the ASCII keyboard (P4n). The device's logic lives inside the Connector Part, so it is simulated like a chip; the Board holds only the header. Devices whose arrangement belongs to the Board (LEDs, digits, buttons) are on-board Parts instead: [ADD_BOARD_DEVICE.md](ADD_BOARD_DEVICE.md).

Reference commits (paddle controller): `8bfbcc6` Part, connector pins, footprint, power; `da76381` knob through the Session, `bw run`, `bw_ui`; `d24f490` → `e1468a0` the knob meter Machine (045). Keyboard: `e0feb33` Part (a setting that is an event), `53e109f` typing through the Session, `--type`, a bench netlist, `605db6c` the capture mode in `bw_ui`; `23a4d87` control codes through the Session, `5ef5183` the drawn layout, `785e7a7` the keyboard window (P4o).

## The chain

| Layer | File | What |
|---|---|---|
| Part | `src/parts/<device>.{hpp,cpp}` | a `Component` with the connector's pins in pad order (pin index = pad − 1); signal pins TTL `Input`/`Output`, power pins `Power`. The device's behaviour in `evaluate` (`fell`, `wakeAfter`, `drive` with a delay of at least 1 ps). The player's setting is a `std::atomic` written by the UI thread (`Paddle::setKnob`, clamped); a setting read only at the next event needs no wake, unlike a button press. A setting that is an event (a key) goes into a lock-free single-producer queue in the Part (`Keyboard::type`) followed by a wake; a device with no inputs is evaluated only on wakes, so it tells its own wakes from the Session's by time (`nextAt_`). Dropping queued events from the producer side needs a marker the consumer honours (`dropWaiting`), never a reset of the consumer's index |
| Parameters | `src/parts/catalogue.cpp` | `CreateXxx(reference, parameters)` refuses unknown or missing names with a message; an optional one has a default (`reversed=yes\|no`, default `no`); durations by `DurationFromText` (`src/parts/paddle.hpp`: `bw_parts` cannot reach `bw_netlist`'s `ParseDuration`) |
| Catalogue | `src/parts/catalogue.cpp` | `PartInfo` row sorted by name, `PartKind::Connector`, package e.g. `KK-4` |
| Pins | `src/parts/connector.{hpp,cpp}` | `ConnectorPins("<NAME>")`: names and kinds for the electrical netlist |
| Power | `connector.cpp` `ConnectorPowersDevice`, `src/machine/electrical.cpp` `FindPower` | a device fed by the Board is Powered like a chip: unpowered, its pins go on private Nets (`BoardPower::unpowered`). `POWER` and `VIDEO` stay unpowered Connectors |
| Footprint | `src/board/kicad_netlist.cpp` | package → KiCad footprint in `kFootprints` |
| Finder | `src/netlist/builder.{hpp,cpp}` | `XxxOf(loaded, ref)` (`PaddleOf`), for the Session, CLI and tests |
| Manual check | `src/manual/manual.cpp` `PartPinNames` | builds each Part without parameters: give a Part that needs them its own |
| Setting | `src/machine/<x>_setting.{hpp,cpp}` | the CLI text form (`ParseKnob("J2:40%")`) |
| Session | `src/session/session.{hpp,cpp}` | list (`knobs()`), set, get; kept in a map by reference and reapplied after every rebuild (power-on after a Repair) |
| CLI | `src/cli/main.cpp`, `src/ui/main.cpp` | `--knob REF:N%`, applied before power-on; a reference that is not the device is a usage error |
| UI | `src/ui/side_panel.cpp` | a section per kind of device (`DrawKnobs`: slider, mouse wheel, Up/Down keys unless text input has the keyboard); a device with many controls gets its own floating window drawn like the real thing (`keyboard_window.cpp`: caps from a layout in `src/view`, tested headless), drawn also without a Board |
| Datasheet | `datasheets/<NAME>.md` | the connector's pins and the device inside |

Tests: `tests/parts/<device>_test.cpp` (row, pins, parameters refused with their messages, behaviour, setting clamped, `ConnectorPowersDevice`), `tests/machine/<x>_setting_test.cpp`, `[session]` cases (setting kept across power, and across a Repair once a Machine has a Puzzle), `manual_test.cpp` (pin names).

## Gotchas

- A device that takes the whole PC keyboard (the ASCII keyboard) needs a capture mode in `bw_ui` (`src/ui/keyboard_capture.cpp`): on while its window is focused (`NoFocusOnAppearing`, else it captures at launch), F2 toggles; SDL key and text events taken before ImGui sees them, key-ups always seen and passed on so no key stays held; PC keys drawn by what they type, not where they sit (any PC layout); Ctrl codes from the shifted key (`SDL_GetKeyFromScancode` with Shift); `SDL_StartTextInput` kept on while capturing; a banner drawn with or without a Board.
- In `bw_ui`, a scripted input (`--press`, `--type`) must not act in the first frame after a power-on: `status.simTime` is still the previous run's.

- A Puzzle that unpowers the device cracks its supply Trace: ask for the device's `VCC` pad as a leaf of `VCC` in the layout (045 Dead knob).
- On a damaged Board the Nets are named from the Copper: compare pin Nets (`PinNet`) rather than design Net names in Puzzle tests.

# Net Panel — Implementation Plan (P3b)

Design: [NET_PANEL_DESIGN.md](NET_PANEL_DESIGN.md)
Spec: [NET_PANEL.md](NET_PANEL.md)

P3b DONE 2026-10-29 (9054d0b..87fbd16, 351 tests); release 1.00×, `ui` 0.5 ms. User: "ok as it is, but there is future work" (see Later).

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Headless Drivers/Loads listing (`net_pins`) with tests, incl. no-leak Puzzle tests | done 9054d0b |
| 2 | Selected Net in `BoardView`: Board clicks, `route`, `--probe`, highlight | done 5db1e07 |
| 3 | `Net` section in the right column (`NetPanel`) + screenshots | done 87fbd16 |
| 4 | Release check, user check, docs | done |

Implementation notes (differences from the design):

- `IsRail` was also duplicated in `src/machine/electrical.cpp`; that copy was removed (ambiguous call once public).
- Extra test `net pins: video GND is a Load` (`J2 pin 3 GND`).
- `Driven by` entries are indented to line up with the `Feeds` tree entries.
- At 1280 × 800 with the TV open, the Net section pushes the Chip pinout below the fold (reachable by scrolling).

## Later (user check, 2026-10-29)

- **Consistent selection.** Chip and Net selection behave differently; align them, probably by *removing* some Chip behaviour rather than adding to Net.
- **Placement.** Net and Chip info should not be tucked in the right column; e.g. floating windows, so several can stay open at once.
- **Less hover.** Hover is overused: moving anywhere makes something flash on the Board, so the UI feels busy.
- **Presentation.** Make all this info discoverable but not overwhelming.

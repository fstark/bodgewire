# Connectors — Specification (P2b)

Second milestone of P2 (see [ARCHITECTURE.md](../ARCHITECTURE.md), Board authoring and Roadmap). Power and video enter and leave a Board through connectors the player can see and probe, not through Net names. Builds on [BOARD_LAYOUT.md](../p2a-board-layout/BOARD_LAYOUT.md) and [PARTS.md](../p1b-parts/PARTS.md).

Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Decisions

- **Connectors are Parts**, in the catalogue and the netlist like chips (`part J1 POWER`), with datasheet-style named Pins, a footprint, pads and Traces. Reference prefix `J`.
- **Function-specific, not generic.** Each connector type has a fixed job the game knows: the power supply plugs into `POWER`, the TV into `VIDEO`. No generic N-pin connector: keyboard, cassette, expansion edge connector etc. arrive as their own Parts with the Machines that need them. The player recognises "the power connector", and Puzzles can rely on it.
- **Period-style wire-to-board headers** (Molex KK, 2.54 mm): internal harnesses to the power supply and the monitor, as on the PET 2001 (built-in monitor fed with TTL video and sync). Placed on the Board edge in KiCad; where exactly is a layout choice.
- **One supply: 5 V.** Other rails (−5 V, 12 V) come with a Part that needs them (e.g. 4116 DRAM).
- **Power is visible, not yet simulated.** Chip power Pins stay implicit in the simulation: a chip on a broken VCC Trace keeps working. Power Faults (unpowered chip, low supply) are a later milestone; the Copper is already correct for them.

## Scope

In: `POWER` and `VIDEO` Parts; board rules in `export-kicad` / `check-board`; both netlists and Boards updated; the P1c golden check reads its outputs through the `VIDEO` connector; KiCad's DRC run headless in the tests.

Out: TV model and picture (rest of P2), power Faults, regulators, other connectors.

## Behaviour

### S1 Parts

| Part | Pins (pin number = pad) | Footprint |
|---|---|---|
| `POWER` (5 V in) | 1 VCC, 2 GND | `Connector_Molex:Molex_KK-254_AE-6410-02A_1x02_P2.54mm_Vertical` |
| `VIDEO` (TTL monitor out) | 1 VIDEO, 2 SYNC_N, 3 GND | `Connector_Molex:Molex_KK-254_AE-6410-03A_1x03_P2.54mm_Vertical` |

- `POWER` Pins are power Pins: implicitly on VCC and GND, like chip power Pins.
- `VIDEO` Pins VIDEO and SYNC_N are TTL inputs (the monitor's inputs): a load, no behaviour. GND is a power Pin.
- `bw run --record` and VCDs are unchanged.

### S2 Board rules

`bw export-kicad` and `bw check-board` reject a netlist without exactly one `POWER` Part: "a board needs one POWER connector". `bw run` and tests do not require it (small test circuits stay connector-free).

### S3 Netlists and Boards

- `netlists/counter.bwn`: adds `part J1 POWER`.
- `netlists/test_pattern.bwn`: adds `part J1 POWER` and `part J2 VIDEO`, with `SYNC_N` and `VIDEO` routed to J2.
- The user re-imports both netlists in KiCad, places J1 (and J2) on the Board edge and routes them; both pass `bw check-board`.

### S4 Outputs through the connector

The P1c golden check (`tests/machines/test_pattern_test.cpp`) finds the Part of type `VIDEO` and samples the Nets on its VIDEO and SYNC_N Pins, not Nets looked up by name. This is how the TV model will connect.

### S5 Headless DRC

A CTest test runs `kicad-cli pcb drc` on every `boards/*/*.kicad_pcb` and fails on any error or unconnected item. Skipped (not failed) when `kicad-cli` is not installed.

## Done when

- S1–S5 implemented; tests pass in debug and release.
- Both Boards updated by the user, pass `bw check-board` and the headless DRC, committed.

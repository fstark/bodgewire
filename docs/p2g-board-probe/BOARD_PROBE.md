# Board Probe — Specification (P2g)

Second of three milestones that complete P2's **first playable** (see [ARCHITECTURE.md](../ARCHITECTURE.md), Instruments, Roadmap): the player probes the running Board by clicking it and reads the lamps, enough to locate each Puzzle's Fault. Builds on [BOARD_VIEW.md](../p2f-board-view/BOARD_VIEW.md) (P2f), the runner's counted watches of [UI_SHELL.md](../p2e-ui-shell/UI_SHELL.md) and the logic probe of [FAULTS.md](../p2d-faults/FAULTS.md). Repairs follow in [BOARD_REPAIR.md](../p2h-board-repair/BOARD_REPAIR.md) (P2h).

Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Decisions

- **One Logic Probe**, the P2d model (HIGH, LOW, PULSE lamps), fed on the devices thread by the runner's counted watches (P2e).
- **The probe reads electrical Nets**: the island of the current Copper at the touched point, cracks included. It is the only thing in the UI that does (P2f: the UI does not leak Faults).
- **Probing is the only click action** in P2g; P2h adds tool selection.

## Scope

In: touching Copper by left click; probe tip and lamps; watching only the touched Net; `--probe` for screenshots; headless tests.

Out: Repairs (P2h); scope, analyzer, multimeter (P3); more than one probe.

## Behaviour

### S1 Touching

- A left click on Copper touches it: a pad or via touches its island on both Layers, a Trace point touches the island of the Active Layer's Copper at that point (so probing past a crack reads the far side). A click off Copper lifts the probe.
- The probe tip is drawn where it touches and stays there across power cycles; it reads the island at that point in the current Copper.
- Moving the probe costs nothing on other Nets: only the touched Net is watched, from the moment it is touched (ARCHITECTURE, Instruments).

### S2 Lamps

- HIGH, LOW, PULSE as in FAULTS.md S5 (pulse stretched 100 ms simulated), in the right column under the TV.
- All dark when the power is off or the probe is lifted. Lamps follow the latest completed slice, refreshed every UI frame.

### S3 Screenshot

- `--probe U1.QA` (optional, same Pin syntax as `bw run --probe`) touches that Pin's pad at start, so the lamps and tip show in a `--screenshot`. Unknown Pin → usage error, exit 2.

### S4 Performance

- The test pattern still runs at ≈ 1.00× at `1×` with the probe on a busy Net (e.g. CLK).

### S5 Checks

- Headless (no SDL/ImGui), through the runner:
  - touching a Trace point on each side of the `broken_trace` crack gives two different electrical Nets; a pad and a via give their island;
  - for each Puzzle: probe at the faulty Pin or past the crack → the P2d lamps (stuck: steady HIGH or LOW, no PULSE; crack on a driven input: neither lamp);
  - moving the probe from one Net to another unwatches the first; the TV is unaffected when the probe leaves VIDEO.
- `--screenshot --probe` on each Puzzle: the agent reads the PNG to check tip and lamps.

## Done when

- S1–S5 implemented; tests pass in debug and release.
- The user locates the Fault of each Puzzle in `bw_ui` with the probe, without reading the `.bwp` files.

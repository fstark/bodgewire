# Connectors — Implementation Plan (P2b)

Design: [CONNECTORS_DESIGN.md](CONNECTORS_DESIGN.md)
Spec: [CONNECTORS.md](CONNECTORS.md)

**P2b done 2026-10-27**, commits 2d0a456..(phase 4 commit). All 4 phases completed:

1. `Connector` Component, `PartKind::Connector`, `POWER` (KK-2) and `VIDEO` (KK-3) in the catalogue and footprint table.
2. `BoardNetlistProblems`: `export-kicad` and `check-board` need exactly one POWER connector.
3. `drc_<board>` CTest tests run `kicad-cli pcb drc` on every committed Board (skipped without KiCad).
4. J1 on both netlists, J2 on the test pattern; golden check reads SYNC_N/VIDEO at J2's Pins; both Boards re-laid by the user and pass `check-board` and DRC.

Follow-up (small, when convenient): `export-kicad` lists every unconnected Pin in a single-pin net (`unconnected-(U3-Pad8)`, as KiCad's own exporter does), so pcbnew's import shows no "no net found" warnings and real problems stand out.

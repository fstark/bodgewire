# Connectors — Implementation Plan (P2b)

Design: [CONNECTORS_DESIGN.md](CONNECTORS_DESIGN.md)
Spec: [CONNECTORS.md](CONNECTORS.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Connector Component, catalogue entries, footprints | |
| 2 | Board netlist rule, CLI | |
| 3 | Headless DRC tests | |
| 4 | Netlists with connectors, outputs through J2, Boards re-laid (needs the user) | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug --output-on-failure`

All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md); `clang-format -i` on changed C++ files.

---

## Phase 1 — Connector Component, catalogue, footprints

Design §2, §3. `src/parts/connector.hpp/.cpp` (add to `src/parts/CMakeLists.txt`); `PartKind::Connector`; `CreateConnector<Name>`; POWER and VIDEO entries; KK-2/KK-3 in `kFootprints`. Tests: `tests/parts/connector_test.cpp`, builder implicit power for J1, footprint table.

Commit `"connectors: phase 1 — POWER and VIDEO Parts"`.

## Phase 2 — Board netlist rule, CLI

Design §4. `BoardNetlistProblems`; `export-kicad` and `check-board` use it. Tests in `board_check_test.cpp`.

Commit `"connectors: phase 2 — board needs one POWER connector"`.

## Phase 3 — Headless DRC tests

Design §6, in `tests/CMakeLists.txt`. Check that a Board with a deleted track fails (manual, temporary copy).

Commit `"connectors: phase 3 — KiCad DRC in ctest"`.

## Phase 4 — Netlists, outputs through J2, Boards

Design §5, spec S3. Add J1 to `counter.bwn`, J1 and J2 to `test_pattern.bwn`; update `test_pattern_test.cpp`; export both `.net`. The user re-imports, places and routes J1/J2; `bw check-board` and DRC pass. One commit with netlists, test and Boards.

Commit `"connectors: phase 4 — connectors on the Boards"`.

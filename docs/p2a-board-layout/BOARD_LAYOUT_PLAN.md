# Board Layout via KiCad — Implementation Plan (P2a)

Design: [BOARD_LAYOUT_DESIGN.md](BOARD_LAYOUT_DESIGN.md)
Spec: [BOARD_LAYOUT.md](BOARD_LAYOUT.md)

Phases 1–7 completed 2026-10-27, commits dd33ff2..ff9b0ac plus the guide: netlist parts/connections, `bw_board` module, `bw export-kicad`, `.kicad_pcb` reader, copper islands, `bw check-board`, [KICAD_GUIDE.md](KICAD_GUIDE.md). The reader and the netlist format are untested against real KiCad files until phase 8.

| Phase | Description | Status |
|-------|-------------|--------|
| 8 | User layouts, integration tests, decision (needs the user) | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug --output-on-failure`

All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md); run `clang-format -i` on changed C++ files (never on CMakeLists.txt).

---

## Phase 8 — User layouts, integration tests, decision

Needs the user.

1. The user lays out `boards/counter`, then `boards/test_pattern`, following the guide; fix any reader or export problem found on the way (with a unit test each).
2. `tests/board/boards_test.cpp` (tag `[board]`; `BW_BOARD_DIR="${PROJECT_SOURCE_DIR}/boards"` in `tests/CMakeLists.txt`): both Boards pass `CheckBoard` against their netlists.
3. Fixture `tests/board/fixtures/counter_open.kicad_pcb`: the counter Board with one track removed; reports exactly one open.
4. Decision in [ARCHITECTURE.md](../ARCHITECTURE.md), Board authoring: keep KiCad or switch to our own generator, with the pain points.

Fence: tests pass; commit `"board: phase 8 — real boards and decision"`.

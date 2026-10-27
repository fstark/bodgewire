# Board Layout via KiCad — Implementation Plan (P2a)

Design: [BOARD_LAYOUT_DESIGN.md](BOARD_LAYOUT_DESIGN.md)
Spec: [BOARD_LAYOUT.md](BOARD_LAYOUT.md)

**P2a done 2026-10-27**, commits dd33ff2..8d89c52 plus this docs commit. All 8 phases completed:

- Phases 1–6: `LoadedCircuit` parts/connections, `bw_board` module (S-expression reader, Board model), `bw export-kicad`, `.kicad_pcb` reader, copper islands, `bw check-board`.
- Phase 7: [KICAD_GUIDE.md](KICAD_GUIDE.md).
- Phase 8: `boards/counter` and `boards/test_pattern` laid out by the user in KiCad 10.0.7 (file version 20260206); both pass `CheckBoard` in `tests/board/boards_test.cpp`, which also removes a counter track and expects one open.

Decision: keep KiCad as the layout tool; see [ARCHITECTURE.md](../ARCHITECTURE.md), Board authoring, for the pain points and next steps (connectors for power and video, headless KiCad DRC in tests).

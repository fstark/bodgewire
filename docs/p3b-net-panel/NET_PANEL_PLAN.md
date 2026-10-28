# Net Panel — Implementation Plan (P3b)

Design: [NET_PANEL_DESIGN.md](NET_PANEL_DESIGN.md)
Spec: [NET_PANEL.md](NET_PANEL.md)

Phases 1–3 implemented 2026-10-29 (9054d0b..87fbd16, 351 tests); release 1.00×, `ui` 0.5 ms. Remaining: user check, roadmap row.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Headless Drivers/Loads listing (`net_pins`) with tests, incl. no-leak Puzzle tests | done 9054d0b |
| 2 | Selected Net in `BoardView`: Board clicks, `route`, `--probe`, highlight | done 5db1e07 |
| 3 | `Net` section in the right column (`NetPanel`) + screenshots | done 87fbd16 |
| 4 | Release check, user check, docs | release done; user check pending |

Implementation notes (differences from the design):

- `IsRail` was also duplicated in `src/machine/electrical.cpp`; that copy was removed (ambiguous call once public).
- Extra test `net pins: video GND is a Load` (`J2 pin 3 GND`).
- `Driven by` entries are indented to line up with the `Feeds` tree entries.
- At 1280 × 800 with the TV open, the Net section pushes the Chip pinout below the fold (reachable by scrolling). Judge in the user check.

---

## Phase 4 — Release, user check, docs

### 4.1 — Release

Done: release tests pass, `broken_trace --probe U1.CLK` at 3 s: 1.00×, `ui` 0.5 ms.

### 4.2 — User check

User plays the three Puzzles following Nets with the panel (spec "Done when"). Record feedback under "Later" in this plan.

### 4.3 — Docs

- `docs/ARCHITECTURE.md` roadmap P3b row: mark done with the commit range.
- This plan: status column, a short summary line at the top, implementation notes (differences from the design).

### Fence

- [x] Release tests pass; ratio ≈ 1.00×
- [ ] User confirmed
- [ ] Commit: `"docs: P3b net panel done"`

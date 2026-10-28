# Board Repair — Implementation Plan (P2h)

Design: [BOARD_REPAIR_DESIGN.md](BOARD_REPAIR_DESIGN.md)
Spec: [BOARD_REPAIR.md](BOARD_REPAIR.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Machine: `ParseRepair`, `FormatRepair`; CLI uses them | done 8357493 |
| 2 | View: `BoardScene::componentAt` | done b64d269 |
| 3 | Session: Machine + rebuildable Runner, Repairs, log, blobs, probe | done 59af4d2 |
| 4 | Puzzles repaired through a Session; rebuild time | done d34a4b9 |
| 5 | `bw_ui` on a Session (no visible change) | done c1c4e3e |
| 6 | `bw_ui`: tools, Swap, Mend, blobs, repair log, `--repair` | done acf499a |
| 7 | Release, Done-when | waiting for the user's play-through |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug`
Last phase also: `cmake --preset release && cmake --build --preset release && ctest --preset release`

All C++ is `clang-format` clean (never run it on CMakeLists.txt).

---

Phases 1–6 done (8357493..acf499a); their sections are removed. Deviations: `power off to repair` sits on the tools row (spec S1) rather than a second line; the repair log is under the status line.

---

## Phase 7 — Release, Done-when

- Release build, `ctest --preset release`; `build/release/tests/bw_tests "[.perf]"` → rebuild mean < 100 ms (S3).
- `bw_ui puzzles/broken_trace.bwp --probe U1.CLK --screenshot /tmp/bw-clk.png --after 3s`: still ≈ 1.00× (P2g S4).
- User check (spec Done-when): play the three Puzzles end to end without reading the `.bwp` files. Record tuning wishes in this plan.
- [ARCHITECTURE.md](../ARCHITECTURE.md) roadmap: P2h row done, P2 first playable done.

### Fence

- [x] Rebuild < 100 ms (release mean 0.7 ms); 1.00× kept (locked, probing U1.CLK)
- [x] Debug and release tests pass (317)
- [ ] User confirms the Done-when
- [ ] Commit: `"board repair: phase 7 — release check"`

# Board Probe — Implementation Plan (P2g)

Design: [BOARD_PROBE_DESIGN.md](BOARD_PROBE_DESIGN.md)
Spec: [BOARD_PROBE.md](BOARD_PROBE.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Sim: `Sample::floating`, floating-only changes recorded | done 7def269 (+0.40 % instructions) |
| 2 | Instruments: `ProbeState`, `LogicProbe` on top of it | done 330b5a8 |
| 3 | Machine: `IslandNetNames`, `probeNet`, `padPosition` | done 1886f40 |
| 4 | Runner: `ProbeConsumer`; Puzzles probed through the Runner | done ab34826 (TSan clean) |
| 5 | `bw_ui`: click to probe, tip, lamps, `--probe` | done 42497c7 (screenshots checked) |
| 6 | Performance, release, Done-when | done: user located all three Faults |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug`
Last phase also: `cmake --preset release && cmake --build --preset release && ctest --preset release`

All C++ is `clang-format` clean (never run it on CMakeLists.txt).

---

## Phase 6 — Performance, release, Done-when

- Release build and `ctest --preset release`.
- `bw_ui puzzles/broken_trace.bwp --probe U1.CLK --screenshot /tmp/bw-clk.png --after 3s`: status ≈ 1.00× (S4); agent views it.
- User check (spec Done-when): locate the Fault of each Puzzle with the probe, without reading the `.bwp` files. Record tuning wishes in this plan.

### Fence

- [x] S4 met: `--probe U1.CLK`, release, 1.00×, locked, ui 0.6 ms
- [x] Debug and release tests pass (306)
- [x] User confirms the Done-when (minor tuning wishes deferred until after first playable)
- [x] Commit: `"board probe: phase 6 — release check"`

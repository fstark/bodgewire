# Faults, Repairs, Logic Probe — Implementation Plan (P2d)

Design: [FAULTS_DESIGN.md](FAULTS_DESIGN.md)
Spec: [FAULTS.md](FAULTS.md)

**Done 2026-10-28**, commits 529ed11..890500f, 243 tests green in debug and release.

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Builder: Stuck-at Pins, `PinNet()`, `FindPin()` | 529ed11 |
| 2 | Shared recordings (`trimRecordings`, TvInput by time) and `LogicProbe` | e0452ca |
| 3 | `bw_machine`: Puzzle parser and crack geometry | be5cb16 |
| 4 | Electrical netlist and `Machine` (load, swap, mend, build) | dc2198d |
| 5 | Puzzles, `bw run` on `.bwp` with `--repair`/`--probe`, end-to-end checks, docs | 890500f |

Notes:

- Symptoms as in design §5.6: dead_counter rolls (TV never locks), stuck_gate shows a white box, broken_trace loses the vertical grid lines. Right Repairs give a frame identical to the fault-free one; the fault-free Machine equals the netlist-only run.
- Crack points shared by tests: [tests/machine/test_points.hpp](../../tests/machine/test_points.hpp) (`kH0Crack`, `kR1VccCracks`).
- Machine names in `.bwp` are `[a-z0-9_]+`, so they cannot escape `netlists/` or `boards/`.

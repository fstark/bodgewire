# UI Shell — Implementation Plan (P2e)

Design: [UI_SHELL_DESIGN.md](UI_SHELL_DESIGN.md)
Spec: [UI_SHELL.md](UI_SHELL.md)

**Done 2026-10-28**, commits dc815d0..b714e66 (one per phase, plus doc fixes).

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | `SpscRing`, `TripleBuffer` with tests | 1cfcad8 |
| 2 | `Recorder::stop`, `Consumer`, `Dispatcher`, `TvConsumer`, `bw_runner` | 07af19b |
| 3 | `Runner`: simulation and devices threads, commands, status | 4667bf9 |
| 4 | `Machine::name()`, SDL3/ImGui fetch, `bw_ui` skeleton window | 97c1eb2 |
| 5 | `bw_ui`: TV panel, controls, status line | b714e66 |

Results: 266 tests pass in debug and release; runner tests clean under ThreadSanitizer. Smoke test confirmed by the user: grid locks, ≈ 1.00× at `1×`, `Max` ≈ 5.5× (Apple M5 Max; headless `bw run --time 5s --tv` ≈ 3.2× including load).

Changes from the design found while implementing (design updated): the devices thread also wakes on a full ring (`devicesWake_`), else it could sleep waiting for a marker while the simulation waits for ring space; PowerOn does not clear staged Samples (the new run's may already be staged); `pauseAt` is one-shot and survives `powerOn`, so tests queue it first.

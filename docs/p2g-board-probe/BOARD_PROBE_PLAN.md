# Board Probe — Implementation Plan (P2g)

Design: [BOARD_PROBE_DESIGN.md](BOARD_PROBE_DESIGN.md)
Spec: [BOARD_PROBE.md](BOARD_PROBE.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Sim: `Sample::floating`, floating-only changes recorded | |
| 2 | Instruments: `ProbeState`, `LogicProbe` on top of it | |
| 3 | Machine: `IslandNetNames`, `probeNet`, `padPosition` | |
| 4 | Runner: `ProbeConsumer`; Puzzles probed through the Runner | |
| 5 | `bw_ui`: click to probe, tip, lamps, `--probe` | |
| 6 | Performance, release, Done-when | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug`
Last phase also: `cmake --preset release && cmake --build --preset release && ctest --preset release`

All C++ is `clang-format` clean (never run it on CMakeLists.txt).

---

## Phase 1 — Sim: floating in Samples

(design 2 `Sample`/`Recorder`, 3.1–3.3)

### 1.1 — Before

Release build; record instructions retired of `/usr/bin/time -l build/release/src/cli/bw run netlists/test_pattern.bwn --time 5s` (twice, keep the lower).

### 1.2 — Change

- [src/sim/recorder.hpp](../../src/sim/recorder.hpp): `Sample::floating = false`; `start(NetId, Sample)`, `add(NetId, Sample)`; [recorder.cpp](../../src/sim/recorder.cpp) accordingly.
- [src/sim/simulator.cpp](../../src/sim/simulator.cpp): `record` and the dirty-Net loop of `runUntil` as design 3.1–3.2.
- Fix every `start`/`add` call site and any test building `Sample`s (grep `recorder`, `Sample{`).

### 1.3 — Tests

[tests/sim/recorder_test.cpp](../../tests/sim/recorder_test.cpp) / [simulator_test.cpp](../../tests/sim/simulator_test.cpp): `"simulator: recording an undriven Net starts floating"`; `"simulator: floating-only change is recorded"` (a Net whose only Driver goes to Z: the value holds, a Sample with `floating = true` is recorded).

### 1.4 — After

Same measurement as 1.1: instructions retired within +0.5 %. Note both numbers in the commit message.

### Fence

- [ ] `Sample::floating`; floating-only changes recorded
- [ ] Instruction count within +0.5 %
- [ ] All tests pass; full build clean
- [ ] Commit: `"board probe: phase 1 — floating in Samples"`

---

## Phase 2 — Instruments: `ProbeState`

(design 2, 3.4, 5.1)

- [src/instruments/logic_probe.hpp](../../src/instruments/logic_probe.hpp): `ProbeState` as design 2 (`kPulseStretch` moves to it); `LogicProbe` keeps its interface, holds a `ProbeState`, `update()` feeds Samples after `fedUntil_`, `lamps()` = `state_.lamps(sim_.now())`.
- [tests/instruments/logic_probe_test.cpp](../../tests/instruments/logic_probe_test.cpp): `"probe state: first Sample is the level, no PULSE"`, `"probe state: change lights PULSE for 100 ms"`, `"probe state: floating-only change no PULSE"`, `"probe state: floating is dark"`, `"probe state: reset"`. Existing `LogicProbe` tests unchanged and passing.

### Fence

- [ ] `ProbeState` exists; `LogicProbe` uses it; `bw run --probe` output unchanged (faults tests pass)
- [ ] All tests pass; full build clean
- [ ] Commit: `"board probe: phase 2 — ProbeState"`

---

## Phase 3 — Machine: what the probe touches

(design 2, 3.5, 5.3)

- [src/machine/electrical.cpp](../../src/machine/electrical.cpp): extract `NameIslands(designCircuit, board, islands) → Namer` (POWER preset + the non-resistor, non-power Pin loop); `ElectricalNetlist` uses it and keeps its output; new `IslandNetNames` in [electrical.hpp](../../src/machine/electrical.hpp) returns `[island] → name or ""`.
- [src/machine/machine.hpp](../../src/machine/machine.hpp)/.cpp: `probeNet(built, layer, at)` and `padPosition(reference, pin)` as design 5.3.
- Tests, [tests/machine/machine_test.cpp](../../tests/machine/machine_test.cpp) and [electrical_test.cpp](../../tests/machine/electrical_test.cpp):
  - `"electrical: island names match the electrical netlist"` (`broken_trace` Board: for every pad of a listed Pin, `IslandNetNames[padIsland]` is the Net that Pin is on in `ElectricalNetlist`);
  - `"machine: probeNet across a crack"`: Trace points 1 mm each side of `kH0Crack` along the Trace → two different NetIds; U1 pin 14 pad → `PinNet(built, "U1", "QA")`; a via → its island's Net; a point off Copper → nullopt; a B.Cu-only point with `Layer::Front` → nullopt;
  - `"machine: padPosition"`: `U1.QA` == `U1.14` == the pad's position; `R1.1` on counter; `U99.1`, `U1.XX` → nullopt.

### Fence

- [ ] `IslandNetNames`, `probeNet`, `padPosition` exist; electrical netlist output unchanged
- [ ] All tests pass; full build clean
- [ ] Commit: `"board probe: phase 3 — probeNet and padPosition"`

---

## Phase 4 — Runner: `ProbeConsumer`

(design 2, 4, 5.2, 7)

### 4.1 — Consumer

`src/runner/probe_consumer.hpp/.cpp` as design 2, 4, 5.2. CMake: add the file, `bw_runner` links `bw_instruments` publicly.

### 4.2 — Tests

- `tests/runner/probe_consumer_test.cpp` (`[runner]`): the cases of design 9 `probe_consumer_test`. Helper: `RunTo(runner, t)` (existing pattern: `pauseAt`, `resume`, `waitIdle`).
- `tests/machines/board_probe_test.cpp` (`[machines]`): the Puzzle cases of design 9 `board_probe_test`, each with `LoadPuzzleFile`, `build()`, a Runner with only the `ProbeConsumer`, `padPosition` + `probeNet` + `touch`, run to 300 ms, `FormatLamps(probe.lamps())`.
- Add both to [tests/CMakeLists.txt](../../tests/CMakeLists.txt).

### Fence

- [ ] `ProbeConsumer` exists; Puzzle lamps through the Runner match P2d
- [ ] All tests pass; full build clean
- [ ] Commit: `"board probe: phase 4 — ProbeConsumer"`

---

## Phase 5 — `bw_ui`: probing

(design 3.7, 5.4, 5.5)

- [src/ui/board_view.hpp](../../src/ui/board_view.hpp)/.cpp: `BoardClick`, `takeClick()`, `setProbeTip()`; click in `input()`, tip drawn last (design 5.4); `kProbeRed`.
- [src/ui/main.cpp](../../src/ui/main.cpp):
  - `Options::probe` (`--probe REF.PIN`), parsed with the others;
  - `RunWindow`: `ProbeConsumer probe` in the consumer list when there is a Machine; after creating the Runner and before `powerOn`, `--probe` → `padPosition` → `probeNet` → `touch` and tip; unknown Pin or no Machine → `bw_ui: unknown pin 'X'` + usage, exit 2 (check before `SDL_Init`: resolve the pad position in `main`, pass it to `RunWindow`);
  - each frame: `takeClick()` → touch / lift (design 3.7); lamps row after the TV (design 5.5).
- Check: build and test gates; screenshots `--probe` on the three Puzzles (design 9), agent views them: tip on the pad, lamps as P2d; `--probe U99.1` → exit 2.

### Fence

- [ ] Click touches / lifts; tip and lamps drawn; `--probe` works
- [ ] All tests pass; full build clean
- [ ] Commit: `"board probe: phase 5 — probe in bw_ui"`

---

## Phase 6 — Performance, release, Done-when

- Release build and `ctest --preset release`.
- `bw_ui puzzles/broken_trace.bwp --probe U1.CLK --screenshot /tmp/bw-clk.png --after 3s`: status ≈ 1.00× (S4); agent views it.
- User check (spec Done-when): locate the Fault of each Puzzle with the probe, without reading the `.bwp` files. Record tuning wishes in this plan.

### Fence

- [ ] S4 met
- [ ] Debug and release tests pass
- [ ] User confirms the Done-when
- [ ] Commit: `"board probe: phase 6 — release check"`

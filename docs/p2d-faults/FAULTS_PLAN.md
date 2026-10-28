# Faults, Repairs, Logic Probe — Implementation Plan (P2d)

Design: [FAULTS_DESIGN.md](FAULTS_DESIGN.md)
Spec: [FAULTS.md](FAULTS.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Builder: Stuck-at Pins, `PinNet()` | |
| 2 | Shared recordings (`trimRecordings`, TvInput by time) and `LogicProbe` | |
| 3 | `bw_machine`: Puzzle parser and crack geometry | |
| 4 | Electrical netlist and `Machine` (load, swap, mend, build) | |
| 5 | Puzzles, `bw run` on `.bwp` with `--repair`/`--probe`, end-to-end checks, docs | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate: `ctest --preset debug --output-on-failure`

`clang-format -i` every changed C++ file (never CMakeLists.txt). One commit per phase.

---

## Phase 1 — Stuck-at Pins in the builder

### 1.1 — `StuckPin`, `BuildCircuit(netlist, stuck)`

[src/netlist/builder.hpp](../../src/netlist/builder.hpp) / `.cpp` per design §2 and §3.1:

```cpp
struct StuckPin { std::string reference; PinIndex pin; LogicValue value; };  // Zero or One
LoadResult BuildCircuit(const Netlist& netlist, std::span<const StuckPin> stuck = {});
```

`Builder` takes the span; in `build()`'s `pinNet_` loop a stuck `(reference, pin)` gets `circuit.tie(net.net, value, DriveStrength::Strong)` instead of `connect`; the `Connection` is still pushed. Before the loop, `Check()` every StuckPin names a placed Component's Output/OpenCollector Pin.

### 1.2 — `PinNet()`

```cpp
std::optional<NetId> PinNet(const LoadedCircuit& loaded, std::string_view reference,
                            std::string_view pin);
```

Component from `loaded.components`; pin by number (all digits, 1-based) or by name over `pins()` (no `Check()`); nullopt if either is unknown. Then the matching `loaded.connections` entry → `loaded.nets.at(c.net)`; none → `circuit.netOf(PinId{firstPin + index})`. Rewrite `ConnectorNet()` as: find the `part` reference in `loaded.parts`, return `PinNet(loaded, reference, pin)`.

### 1.3 — Tests: [tests/netlist/builder_test.cpp](../../tests/netlist/builder_test.cpp)

- `"builder: stuck pin ties its net"`: `74LS04` with `net A U1.2 U1.3` (1Y drives 2A), stuck `{U1, 1, Zero}` → `circuit.tieOf(net A)` is Strong Zero; `circuit.netOf(U1 pin 2)` ≠ net A; `PinNet(loaded, "U1", "1Y")` == net A; simulate: net A reads 0 whatever 1A is.
- `"builder: PinNet by name and number"`, `"builder: PinNet unknown"` (reference, pin).
- Existing ConnectorNet tests still pass.

### Fence

- [ ] Build and test gates green
- [ ] Commit: `"faults: phase 1 — stuck pins in the builder, PinNet"`

---

## Phase 2 — Shared recordings and the logic probe

### 2.1 — Trimming by the caller

- [src/sim/simulator.hpp](../../src/sim/simulator.hpp): `void trimRecordings();` → `recorder_.trim(net)` for each `recorder_.nets()`. Remove `trimRecording(NetId)`; update its test in [tests/sim/simulator_test.cpp](../../tests/sim/simulator_test.cpp) to the new name.
- [src/devices/tv_input.hpp](../../src/devices/tv_input.hpp) / `.cpp`: drop `History` and the index cursors; add `Time fedUntil_ = -1`. `update()`: take the Samples of each Net with `time > fedUntil_` (scan from the back: after a trim only a few remain), merge as before, `advanceTo(sim.now())`, `fedUntil_ = sim.now()`. Constructor `TvInput(Simulator&, NetId video, NetId syncN)`.
- [src/cli/main.cpp](../../src/cli/main.cpp): slice loop calls `tv->update()` then `sim.trimRecordings()` unless `--vcd`.
- Adapt [tests/devices/tv_input_test.cpp](../../tests/devices/tv_input_test.cpp) (trimmed run calls `sim.trimRecordings()` after `update()`) and [tests/machines/test_pattern_test.cpp](../../tests/machines/test_pattern_test.cpp).

### 2.2 — `src/instruments/logic_probe.hpp` / `.cpp`

Interface per design §2, state §4, algorithm §5.5. `FormatLamps`: lit lamps in order `HIGH`, `LOW`, `PULSE`, space-separated; none → `-`. New `src/instruments/CMakeLists.txt` (design §7); root `add_subdirectory(src/instruments)` after `src/devices`; tests link `bw_instruments`, add `instruments/logic_probe_test.cpp`.

### 2.3 — Tests

- [tests/instruments/logic_probe_test.cpp](../../tests/instruments/logic_probe_test.cpp), circuits with `Stimulus` (see simulator_test.cpp `AddDrivers`):
  - `"logic probe: steady high"` → `HIGH`, no PULSE.
  - `"logic probe: pulse stretches 100 ms"`: one change at 1 ms → PULSE at 100 ms, not at 101.1 ms.
  - `"logic probe: floating net lights nothing"`: TTL input only, no Driver.
  - `"logic probe: survives trimRecordings"`: update + trim each 1 ms slice, same lamps as without trim.
  - `FormatLamps` cases.
- tv_input_test: `"tv input: probe and TV share a net"` (VIDEO also probed, trimming each slice) → same frame as TV alone.

### Fence

- [ ] TV tests still green; probe tests pass
- [ ] Build and test gates green
- [ ] Commit: `"faults: phase 2 — shared recordings, logic probe"`

---

## Phase 3 — Puzzle parser and cracks

### 3.1 — Module

`src/machine/CMakeLists.txt` (design §7; start with `crack.cpp puzzle.cpp`); root `add_subdirectory(src/machine)` after `src/board`; tests link `bw_machine`, define `BW_PUZZLE_DIR`.

### 3.2 — `src/machine/puzzle.hpp` / `.cpp`

Types and `ParsePuzzle` per design §2, §5.1. Error texts: `unknown directive 'x'`, `expected: fault stuck REF.PIN 0|1`, `expected: fault crack F.Cu|B.Cu X Y`, `bad coordinate 'x'`, `machine already given at line N`, `no machine line` (line 0). Collect all errors, by line.

### 3.3 — `src/machine/crack.hpp` / `.cpp`

```cpp
// Straight Trace on `layer` whose centre line is nearest to `at`, within half its width.
std::expected<std::size_t, std::string> FindCrackTrace(const Board& board, Layer layer, Point at);
Board CrackBoard(const Board& board, std::span<const CrackDecl> cracks);  // design §5.3
std::string FormatMillimetres(std::int64_t nm);  // "52.30"; also used by the CLI
```

Error texts: `no Trace at F.Cu 52.30 41.00`, `cracks on arc Traces are not supported`. Coordinates formatted with `FormatMillimetres` (2 decimals).

### 3.4 — Tests

- [tests/machine/puzzle_test.cpp](../../tests/machine/puzzle_test.cpp): a full valid file; each error with its line; comments and blank lines.
- [tests/machine/crack_test.cpp](../../tests/machine/crack_test.cpp): Board with two circular pads joined by one straight Trace (build `Board` in code): `FindIslands` 1 island → after `CrackBoard` 2; point off the Trace → error; arc → error; crack within a pad's radius → still 1 island.

### Fence

- [ ] Build and test gates green
- [ ] Commit: `"faults: phase 3 — puzzle parser, cracks"`

---

## Phase 4 — Electrical netlist and Machine

### 4.1 — `src/machine/electrical.hpp` / `.cpp`

```cpp
Netlist ElectricalNetlist(const Netlist& design, const LoadedCircuit& designCircuit,
                          const Board& board);
```

Per design §5.4. `padIsland` keyed by `(reference, pad number string)` from `FindIslands(board).padIsland` and `board.components`. J1 = the `loaded.parts` entry with `info->name == "POWER"`. Resistors (`info->kind == PartKind::Resistor`) are tested first: they are not in `components`. Power Pins: `designCircuit.components.at(ref)->pins()[pin].kind == PinKind::Power`. Nets emitted in the design's declaration order, `~N` splits right after their parent.

### 4.2 — `src/machine/machine.hpp` / `.cpp`

`Machine`, `LoadMachine`, `LoadPuzzleFile` per design §2, §5.2. Error texts: `unknown machine 'x'`, `board does not match netlist: <first problem>`, `unknown reference 'U9'`, `U3 (74LS161) has no pin XX`, `U3.A (A) is not an output`, crack errors from 3.3, `crack at F.Cu x y does not cut the copper`. `swap` erases `stuck_` entries of that reference (true if the reference exists). `mend` erases cracks on that Layer within 1 mm (Euclidean, nm), rebuilds `board_`. `build()` per §5.4.

### 4.3 — Tests

- [tests/machine/electrical_test.cpp](../../tests/machine/electrical_test.cpp): test_pattern without cracks → Net names in the same order and the same Pin sets as the design; with a crack on an `H0` segment → `H0` and `H0~2`, U1.QA on one, U13.2B on the other; counter.bwn with a crack on the Trace from R1 to VCC → R1 absent; a synthetic case where R1's signal side is cut off alone → R1 absent.
- [tests/machine/machine_test.cpp](../../tests/machine/machine_test.cpp): `LoadMachine` errors above (in-memory `Puzzle`, root = `BW_NETLIST_DIR/..`); `swap` of the stuck chip removes its fault, of another keeps it; `mend` near → true, far → false; `build()` with no faults == `LoadNetlistFile` (same Net names).

Pick the `H0` crack point now: a straight `(segment … (net "H0"))` in boards/test_pattern/test_pattern.kicad_pcb, midpoint of its longest segment; record it in a constant shared by the tests and in `puzzles/broken_trace.bwp` (phase 5).

### Fence

- [ ] Build and test gates green
- [ ] Commit: `"faults: phase 4 — electrical netlist, Machine"`

---

## Phase 5 — Puzzles, CLI, end to end

### 5.1 — Puzzle files

`puzzles/dead_counter.bwp`, `stuck_gate.bwp`, `broken_trace.bwp` per design §5.6, each with a one-line comment naming the Fault (the file is the answer key).

### 5.2 — CLI

[src/cli/main.cpp](../../src/cli/main.cpp) per design §3.4: `.bwp` loading, `--repair`, `--probe`, probe output lines; `kUsage` gains `[--repair swap:REF,mend:LAYER:X:Y] [--probe REF.PIN,...]`; `--repair` on a `.bwn` → usage error. [src/cli/CMakeLists.txt](../../src/cli/CMakeLists.txt) links `bw_machine bw_instruments`.

### 5.3 — End-to-end test: [tests/machines/test_pattern_faults_test.cpp](../../tests/machines/test_pattern_faults_test.cpp)

Helper `Frame(LoadedCircuit&, seed, probes)` → completed frame at 60 ms plus probe lamps (TvInput + LogicProbes, 1 ms slices, `trimRecordings`). Cases per design §9, seeds 1–2. If a Symptom differs from design §5.6's description, record what is seen in the design (the test only checks "differs").

### 5.4 — Smoke and release

- `bw run puzzles/<each>.bwp --time 500ms --tv /tmp/<each>.bmp --probe …` and the same with the right `--repair`; open the BMPs, ask the user to confirm the Symptoms.
- Release: `cmake --preset release && cmake --build --preset release && ctest --preset release --output-on-failure`.

### 5.5 — Docs

- ARCHITECTURE.md roadmap row P2d: `done: …`, link to this plan.
- GLOSSARY.md: nothing new (Stuck-at Fault, Broken Trace, Repair, Puzzle exist). "crack" stays a code/Puzzle-file word for the point of a Broken Trace; no Glossary entry.
- Prune this plan to a summary.

### Fence

- [ ] Three Puzzles show their Symptoms; right Repairs restore the grid
- [ ] Debug and release gates green
- [ ] Commit: `"faults: phase 5 — puzzles, bw run --repair/--probe"`

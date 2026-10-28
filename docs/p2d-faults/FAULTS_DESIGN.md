# Faults, Repairs, Logic Probe — Detailed Design

Implements the specification in [FAULTS.md](FAULTS.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/machine/                 new module bw_machine: Machine model (Board + Faults → Circuit)
  puzzle.hpp, puzzle.cpp     ParsePuzzle(): .bwp syntax (S4)
  crack.hpp, crack.cpp       FindCrackTrace(), CrackBoard(): Broken Trace geometry (S2)
  electrical.hpp/.cpp        ElectricalNetlist(): Board islands → Netlist (S1)
  machine.hpp, machine.cpp   Machine: load, swap, mend, build (S2, S3)
src/instruments/             new module bw_instruments
  logic_probe.hpp/.cpp       LogicProbe (S5)
puzzles/
  dead_counter.bwp, stuck_gate.bwp, broken_trace.bwp     (S7)
tests/machine/               puzzle_test, crack_test, electrical_test, machine_test
tests/instruments/           logic_probe_test
tests/machines/              test_pattern_faults_test.cpp (S7 end to end)
```

`machine` is the "Machine model" layer of ARCHITECTURE.md: it owns the design netlist, the pristine Board and the Faults, and produces a `LoadedCircuit` for the Simulator. `instruments` sits next to `devices`: both only read recorded Net changes.

## 2. Public Interface

```cpp
// src/machine/puzzle.hpp — syntax only, like netlist/parser.hpp
struct StuckAtDecl { std::uint32_t line; std::string reference, pin; LogicValue value; };
struct CrackDecl   { std::uint32_t line; Layer layer; Point at; };  // nm, KiCad coordinates
struct Puzzle {
    std::string file;
    std::uint32_t machineLine = 0;
    std::string machine;  // "test_pattern"
    std::vector<StuckAtDecl> stuck;
    std::vector<CrackDecl> cracks;
};
std::expected<Puzzle, std::vector<Diagnostic>> ParsePuzzle(std::string_view text, std::string file);
```

```cpp
// src/machine/machine.hpp
// Design netlist + Board + Faults. Repairs edit it; build() makes the Circuit for one power-on.
class Machine {
public:
    bool swap(std::string_view reference);  // chip swap; false if no such Component
    bool mend(Layer layer, Point at);       // removes cracks within 1 mm; false if none
    LoadResult build() const;               // electrical Circuit with the current Faults
    const Board& board() const { return board_; }  // with cracks

private:
    friend std::expected<Machine, std::vector<Diagnostic>> LoadMachine(const Puzzle&,
                                                                       const std::filesystem::path&);
    Netlist design_;
    std::unique_ptr<LoadedCircuit> designCircuit_;  // design Nets: pin lookup, check-board
    Board pristine_, board_;                        // board_ = pristine_ + cracks_
    std::vector<StuckPin> stuck_;
    std::vector<CrackDecl> cracks_;
};

// root/netlists/<machine>.bwn + root/boards/<machine>/<machine>.kicad_pcb; all semantic errors.
std::expected<Machine, std::vector<Diagnostic>> LoadMachine(const Puzzle& puzzle,
                                                            const std::filesystem::path& root);
// Parse + LoadMachine with root = the puzzle file's directory's parent (puzzles/../).
std::expected<Machine, std::vector<Diagnostic>> LoadPuzzleFile(const std::filesystem::path& path);
```

```cpp
// src/instruments/logic_probe.hpp
struct ProbeLamps { bool high = false, low = false, pulse = false; };

// TTL logic probe on one Net (S5). Construct after powerOn().
class LogicProbe {
public:
    static constexpr Time kPulseStretch = 100'000'000'000;  // ps, 100 ms
    LogicProbe(Simulator& sim, NetId net);  // starts recording the Net
    void update();                          // reads changes recorded since the last update
    ProbeLamps lamps() const;               // at sim.now()
};
std::string FormatLamps(ProbeLamps lamps);  // "HIGH", "LOW PULSE", "-"
```

```cpp
// src/netlist/builder.hpp — additions
struct StuckPin { std::string reference; PinIndex pin; LogicValue value; };  // Zero or One
LoadResult BuildCircuit(const Netlist& netlist, std::span<const StuckPin> stuck = {});
// Net at a Pin's pad: the Net it is listed on (also for a stuck Pin), else its own Net.
std::optional<NetId> PinNet(const LoadedCircuit& loaded, std::string_view reference,
                            std::string_view pin);  // pin: name or number; nullopt if unknown
```

## 3. Integration Points

1. **Stuck-at in the builder** — [src/netlist/builder.cpp](../../src/netlist/builder.cpp) `Builder::build()`, loop `for (const auto& [pin, net] : pinNet_)`: if `(reference, pin)` is in `stuck`, call `circuit.tie(net.net, value, DriveStrength::Strong)` instead of `circuit.connect(...)`; the `Connection` is still recorded. The chip's own output Pin then has a private Net (finalize), so its Driver is gone and the Net is held at full strength from power-on. Zero cost in the run loop. `Builder` stores the span; `Check()` each entry names an Output or OpenCollector Pin of a placed Component (the Machine validated it already).
2. **`PinNet()`** — builder.cpp: resolve the pin like `resolvePin` (number or name, no `Check()`), look it up in `loaded.connections` → `loaded.nets.at(net)`; not listed → `circuit.netOf()` (unconnected Pin, private Net). `ConnectorNet()` becomes a call to `PinNet()` with the connector's reference.
3. **Recorder trimming by the caller** — [src/sim/simulator.hpp](../../src/sim/simulator.hpp): `void trimRecordings();` trims every recorded Net. [src/devices/tv_input.hpp](../../src/devices/tv_input.hpp): `History` and per-Net index cursors are replaced by `Time fedUntil_ = -1`: `update()` feeds Samples with `time > fedUntil_` (all on the first call) and never trims. Several consumers (TV, probes) can now share a Net; whoever runs the loop calls `sim.trimRecordings()` after updating all of them (unless a VCD needs the history). Tests of phase P2c are adapted.
4. **CLI** — [src/cli/main.cpp](../../src/cli/main.cpp) `Run()`:
   - The file ending in `.bwp` → `LoadPuzzleFile()`, then each `--repair` (`swap:U3`, `mend:F.Cu:52.30:41.00`, comma-separated) on the Machine, then `machine.build()`; otherwise `Load()` as today. Unknown reference or bad syntax → usage error; a mend that finds no crack prints `bw: no crack near F.Cu 52.30 41.00` and continues (a wrong Repair is legal).
   - `--probe U1.QC,U13.2B`: `PinNet()` each (unknown → usage error), a `LogicProbe` each after `powerOn`.
   - Slice loop when a TV or probe is attached: `runUntil` 1 ms, `tv.update()`, each `probe.update()`, then `sim.trimRecordings()` unless `--vcd`.
   - After the run: `probe U1.QC LOW` per probe, after the existing lines.
   - `bw` links `bw_machine bw_instruments`.

## 4. Internal State

```cpp
class LogicProbe {
    Simulator& sim_;
    NetId net_;
    Time fedUntil_ = -1;     // Samples up to here already read (sim.now() at the last update)
    Time lastChange_ = -1;   // last change after the recording started; -1 = none
    bool started_ = false;   // the first Sample is the initial value, not a change
};
```

The Machine keeps the pristine Board and the crack list; `board_` is rebuilt from them after every `mend()`, so cracks never accumulate geometry.

## 5. Key Algorithms

### 5.1 Puzzle syntax (S4)

```
line := "machine" NAME | "fault" "stuck" REF "." PIN ("0"|"1") | "fault" "crack" LAYER X Y
LAYER := "F.Cu" | "B.Cu";  X, Y via ParseMillimetres() (board/kicad_board.hpp) → nm, exact
```

Errors (file, line): unknown directive, wrong arity, bad value/layer/number, second `machine`, no `machine`. Tokenising: split on whitespace after stripping `#` comments, as in the netlist parser.

### 5.2 Loading a Machine (S2, S4)

```
LoadMachine(puzzle, root):
  read + ParseNetlist(root/netlists/NAME.bwn) → design_;  BuildCircuit(design_) → designCircuit_
  ReadKicadBoardFile(root/boards/NAME/NAME.kicad_pcb) → pristine_
  missing files → error at puzzle.machineLine ("unknown machine 'NAME'")
  BoardNetlistProblems + CheckBoard not empty → error at machineLine ("board does not match …: first problem")
  each StuckAtDecl: component in designCircuit_->components, pin by name or number,
      kind Output or OpenCollector, else error at its line → stuck_ (PinIndex resolved)
  each CrackDecl: FindCrackTrace(pristine_, layer, at) (5.3) else error;
      FindIslands(CrackBoard(pristine_, {crack})).islandSize.size() must grow, else
      error "crack at F.Cu 52.30 41.00 does not cut the copper" → cracks_
  board_ = CrackBoard(pristine_, cracks_)
```

### 5.3 Cracks (S2)

```
FindCrackTrace(board, layer, p): index of the straight Trace on `layer` with the smallest
  distance from p to its centre line, if that distance ≤ width / 2; arcs → error
  "cracks on arc Traces are not supported"
CrackBoard(board, cracks): result = board; for each crack in order, t = FindCrackTrace(result, ...)
  (so a second crack on the same Trace finds the half it lies on);
  u = unit(t.end − t.start); g = t.width / 2 + 50 µm   (the two halves end > width apart)
  replace t by [t.start, p − u·g] and [p + u·g, t.end]; a half is dropped if p is within g of its end
```

The gap is wider than the Trace, so `FindIslands` (unchanged) sees two islands; a pad or via within `g` of the crack can still bridge it, which the "does not cut" check rejects.

### 5.4 Electrical netlist (S1)

```
ElectricalNetlist(design, designCircuit, board) → Netlist:
  islands = FindIslands(board); padIsland[(reference, pad number)] → island
  name islands: J1 (the POWER part) pad 1 → "VCC", pad 2 → "GND"
  for c in designCircuit.connections (order: reference, then pin):
      skip resistors (below; test first: they are not in designCircuit.components)
      skip Power Pins (implicit, always powered)
      island = padIsland[(c.reference, c.pin + 1)]
      name = islandName[island] ?: first free of c.net, c.net + "~2", "~3"…
      nets[name] += PinRef{c.reference, to_string(c.pin + 1)}
  resistor: kept (PartDecl and both pins) only if one pin's island is named VCC or GND and the
      other's island already has a name (a non-resistor Pin on it) that is not VCC or GND;
      otherwise left out (rail side cut, or nothing to pull: an open pull-up)
  parts = design.parts minus left-out resistors; lines 0
  net order: the design's net declaration order (first NetDecl of each name), each "~N" split
      right after its parent — so NetIds, hence same-seed runs, match the netlist-only Circuit
```

Without cracks every island is one design Net (check-board guarantees it), so names and Pins equal the design netlist's.

`Machine::build()` = `BuildCircuit(ElectricalNetlist(design_, *designCircuit_, board_), stuck_)`.

### 5.5 Logic probe (S5)

```
update(): for Samples s of net_ with s.time > fedUntil_:
              if started_: lastChange_ = s.time      else started_ = true
          fedUntil_ = sim.now()   (runUntil(end) handles every event ≤ end, then now = end)
lamps():  floating = sim.isFloating(net_); v = sim.value(net_)
          high = !floating && v == One;  low = !floating && v == Zero
          pulse = lastChange_ >= 0 && sim.now() - lastChange_ < kPulseStretch
```

### 5.6 The three Puzzles (S7)

| File | Fault | Symptom | Probe |
|---|---|---|---|
| dead_counter.bwp | `stuck U3.QA 0` (V0) | frame end never decoded (line 311 needs V0 = 1): 512-line frames, the TV cannot lock, picture rolls | U3.QA steady LOW, no PULSE |
| stuck_gate.bwp | `stuck U12.4Y 1` (GRID) | white box over the whole active area | U12.4Y steady HIGH |
| broken_trace.bwp | crack on a `H0` Trace (U1.QA → U13.2B) | U13.2B floats high: vertical grid lines gone | U13.2B no lamp; U1.QA HIGH/LOW + PULSE |

The crack point is taken from a straight `H0` segment of [boards/test_pattern/test_pattern.kicad_pcb](../../boards/test_pattern/test_pattern.kicad_pcb) (segments carry `(net "H0")`), away from its ends.

## 6. Reused Infrastructure

- `ParseNetlist`, `BuildCircuit`, `LoadedCircuit::connections` / `nets`: the electrical Circuit is an ordinary netlist built by the existing builder; no second Net-building path.
- `ReadKicadBoardFile`, `FindIslands` (unchanged), `CheckBoard`, `BoardNetlistProblems`: Board loading, connectivity, Machine sanity.
- `ParseMillimetres`: puzzle and CLI coordinates.
- `Circuit::tie` (Strong) for Stuck-at; `Simulator::isFloating` for the probe.
- `Recorder` Samples (P2c) for probe and TV; `Diagnostic` / `Format` for puzzle errors.

## 7. Build Integration

```cmake
# src/machine/CMakeLists.txt
add_library(bw_machine STATIC crack.cpp electrical.cpp machine.cpp puzzle.cpp)
target_link_libraries(bw_machine PUBLIC bw_board PRIVATE bw_warnings)

# src/instruments/CMakeLists.txt
add_library(bw_instruments STATIC logic_probe.cpp)
target_link_libraries(bw_instruments PUBLIC bw_sim PRIVATE bw_warnings)
```

Root CMakeLists: `add_subdirectory(src/instruments)` after `src/devices`, `add_subdirectory(src/machine)` after `src/board`. `bw` and `bw_tests` link both. Tests get `BW_PUZZLE_DIR="${PROJECT_SOURCE_DIR}/puzzles"`.

## 8. Dependency Diagram

```
cli ──► machine ──► board ──► netlist ──► parts ──► sim
 ├────► devices ────────────────────────────────────┤
 └────► instruments ────────────────────────────────┘
```

## 9. Testing

- **puzzle_test**: valid file → fields (coordinates in nm); each error message with its line.
- **crack_test**: synthetic Board (two pads joined by one Trace): crack splits it into 2 islands; crack off the Trace / on an arc → nullopt or error; crack next to a pad does not cut.
- **electrical_test**: test_pattern with its Board and no cracks → same Net names, order and Pins as the design; crack on `H0` → `H0` and `H0~2`; counter.bwn with a crack between R1 and VCC → R1 left out.
- **builder_test**: Stuck-at Pin → Net tied Strong, chip Pin on a private Net, `PinNet()` returns the tied Net; `PinNet` by name and number.
- **machine_test**: `LoadMachine` errors (unknown machine, pin, non-output, crack off copper, crack not cutting); `swap` removes only that Component's Stuck-at; `mend` near / far.
- **logic_probe_test**: Stimulus circuit: steady 1 → HIGH; toggle → PULSE until 100 ms after the last change; floating Net → no lamp.
- **tv_input_test**: adapted to `fedUntil_`; a probe and the TV on the same Net with `trimRecordings()` give the same frame as without trimming.
- **test_pattern_faults_test** (`[machines]`), seeds 1–2, completed frame at 60 ms (no lock needed: same seed, same Circuit → same frame):
  - fault-free Machine on its Board == netlist-only run;
  - each Puzzle: frame differs from fault-free; probe lamps as in 5.6;
  - right Repair (`swap:U3`, `swap:U12`, `mend` at the crack) → frame identical to fault-free; wrong Repair (`swap:U4`, `swap:U13`, mend 5 mm away) → still differs.

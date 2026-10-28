# Board Repair — Detailed Design

Implements the specification in [BOARD_REPAIR.md](BOARD_REPAIR.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/machine/
  repair.hpp/.cpp        (new) RepairAction, ParseRepair, FormatRepair: shared by bw and bw_ui
src/session/             (new module bw_session, headless)
  session.hpp/.cpp       Session: Machine + current Circuit + Runner + TV + probe + Repairs
src/view/                (changed) BoardScene::componentAt (Swap target)
src/cli/main.cpp         (changed) Repair() uses ParseRepair
src/ui/
  board_view.hpp/.cpp    (changed) tools, keys P/S/M, BoardClick::component, solder blobs
  main.cpp               (changed) everything through a Session; Swap/Mend clicks, repair log, --repair
tests/machine/repair_test.cpp, tests/session/session_test.cpp, tests/machines/board_repair_test.cpp
```

Everything S5 checks headlessly lives in `bw_session` and below. The UI turns clicks into `Session` calls and draws what the `Session` holds.

## 2. Public Interface

```cpp
// src/machine/repair.hpp
struct SwapRepair {
    std::string reference;
};
struct MendRepair {
    Layer layer;
    Point at;
};
using RepairAction = std::variant<SwapRepair, MendRepair>;

// "swap:U3", "mend:F.Cu:52.30:41.00"; error "bad repair '...'".
std::expected<RepairAction, std::string> ParseRepair(std::string_view text);
// Repair log line: "swap U3", "mend F.Cu 52.30 41.00" (FormatPoint).
std::string FormatRepair(const RepairAction& repair);
```

```cpp
// src/session/session.hpp
struct SolderBlob {
    Layer layer;
    Point at;  // nm
};

struct ProbeTip {
    Point at;     // nm
    Layer layer;  // Active Layer when touched: which Trace a point on two Traces reads
};

enum class RepairResult : std::uint8_t { Done, Refused, Unknown };  // Refused: powered

// One Machine on the bench, UI thread only. Repairs edit the Machine; the next power-on rebuilds
// the Circuit, Runner, TV and probe from it. Without a Machine (netlist without a Board): no
// Repairs, no probe, the Circuit never changes.
class Session {
public:
    Session(std::optional<Machine> machine, std::unique_ptr<LoadedCircuit> circuit);
    Session(Session&&) noexcept;  // safe: the Runner only points into *live_
    ~Session();

    const Machine* machine() const;           // nullptr without a Board
    const Board* drawnBoard() const;          // Board at load, never changed by Repairs (S1)
    const LoadedCircuit& circuit() const;     // the current build
    Runner& runner();                         // pause, resume, status
    TvConsumer* tv();                         // nullptr without VIDEO; replaced on rebuild

    void powerOn(std::uint64_t seed, std::optional<Time> pauseAt = std::nullopt);
    void powerOff();
    bool powered() const { return powered_; }
    void setSpeed(Speed speed);               // kept across rebuilds

    RepairResult repair(const RepairAction& repair);  // logs and blobs every Done
    std::span<const std::string> log() const { return log_; }
    std::span<const SolderBlob> blobs() const { return blobs_; }

    void touch(std::optional<ProbeTip> tip);  // nullopt: lifted
    std::optional<ProbeTip> probeTip() const { return tip_; }
    std::optional<NetId> probedNet() const;   // in circuit(); nullopt lifted or unnamed island
    ProbeLamps lamps() const;                 // dark when not powered
};
```

```cpp
// src/view/board_scene.hpp — BoardScene gains
// Component whose pad covers `at`, else whose courtyard contains it (S1 Swap).
std::optional<std::size_t> componentAt(Point at) const;
```

```cpp
// src/ui/board_view.hpp
enum class Tool : std::uint8_t { Probe, Swap, Mend };

struct BoardClick {
    Point at;
    Layer layer;
    bool onCopper;
    std::optional<std::size_t> component;  // componentAt(at)
};

// BoardView gains
Tool tool() const;
void controls(bool powered);                     // tools, Layer, Fit; "power off to repair"
void setBlobs(std::span<const SolderBlob> blobs);
```

## 3. Integration Points

1. **CLI `Repair()`** — [src/cli/main.cpp](../../src/cli/main.cpp) line 152: parse with `ParseRepair`, apply with `std::visit` to `machine.swap` / `machine.mend`. Messages unchanged (`unknown reference 'U9'`, `bw: no crack near F.Cu …`).
2. **`BoardScene::componentAt`** — [src/view/board_scene.cpp](../../src/view/board_scene.cpp): pads as in `hitTest`, then `Contains(courtyards_[k], at)`.
3. **`BoardView`** — [src/ui/board_view.cpp](../../src/ui/board_view.cpp): `input()` adds `P`, `S`, `M` next to `L`, `F` and fills `BoardClick::component`; `draw()` draws blobs after vias (5.4); `controls()` gains the tool radio buttons and the power line.
4. **`bw_ui`** — [src/ui/main.cpp](../../src/ui/main.cpp): `Loaded` becomes a `Session` built in `main` (before `SDL_Init`); `--probe` and `--repair` act on it there; `RunWindow(window, renderer, session, options)` takes it. The Runner, `TvConsumer` and `ProbeConsumer` it creates today move into `Session`. Per frame: clicks by tool (5.3), `setProbeTip(session.probeTip())`, `setBlobs(session.blobs())`, repair log under the power rows.
5. **CMake** — new `bw_session`; `bw_ui` and `bw_tests` link it.

No change to the sim thread, Runner, consumers or Machine.

## 4. Internal State

```cpp
class Session {
    // What one build of the Machine runs on. Members in this order: the Runner is destroyed first.
    struct Live {
        std::unique_ptr<LoadedCircuit> circuit;
        std::optional<TvConsumer> tv;
        ProbeConsumer probe;
        std::unique_ptr<Runner> runner;  // consumers: tv, probe (with a Machine)
    };

    std::optional<Machine> machine_;
    std::optional<Board> drawn_;  // machine_->board() at construction
    std::unique_ptr<Live> live_;
    bool dirty_ = false;          // a Repair since live_ was built
    bool powered_ = false;
    Speed speed_ = Speed::RealTime;
    std::optional<ProbeTip> tip_;
    std::vector<std::string> log_;
    std::vector<SolderBlob> blobs_;
};
```

## 5. Key Algorithms

### 5.1 Power-on and rebuild (S3)

```
powerOn(seed, pauseAt):
  if dirty_:
    live_.reset()                                   Runner joins its threads, then the Circuit goes
    live_ = MakeLive(machine_->build())             Check()ed: Repairs only remove Faults
    touchLive(tip_)
    dirty_ = false
  if pauseAt: live_->runner->pauseAt(*pauseAt)     queued before powerOn (P2e)
  live_->runner->powerOn(seed); powered_ = true

MakeLive(circuit):
  tv = VIDEO and SYNC_N via ConnectorNet(circuit) → TvConsumer
  runner = Runner(circuit.circuit, {tv?, &probe if machine_}); runner.setSpeed(speed_)

touchLive(tip):
  live_->probe.touch(*runner, tip ? machine_->probeNet(*circuit, tip.layer, tip.at) : nullopt)
```

Electrical NetIds may shift after a mend (an `H0~2` disappears), so the TV and the probe resolve their Nets again from the new Circuit; nothing keeps an old NetId. Cost: `build()` plus `FindIslands`, ≈ 9 ms for load + build of a Puzzle (release), well under S3's 100 ms.

### 5.2 Repairs (S1, S2)

```
repair(r):
  if !machine_: return Unknown
  if powered_: return Refused
  Swap: if !machine_->swap(ref): return Unknown
  Mend: machine_->mend(layer, at); blobs_.push_back({layer, at})    mended or not
  log_.push_back(FormatRepair(r)); dirty_ = true; return Done
powerOff(): live_->runner->powerOff(); powered_ = false
```

`powered_` is the Session's own flag, set synchronously, so a Repair right after `powerOff()` is accepted without waiting for the sim thread. The Machine is UI-thread data; the sim thread only reads the Circuit.

### 5.3 Clicks by tool (S1, bw_ui)

```
Probe: session.touch(click.onCopper ? ProbeTip{click.at, click.layer} : nullopt)
Swap:  if click.component: session.repair(SwapRepair{drawnBoard.components[*component].reference})
Mend:  session.repair(MendRepair{click.layer, click.at})
```

A Refused Repair does nothing; the tools row already reads `power off to repair` while powered. Changing tool does not touch the probe.

### 5.4 Solder blob (S1)

```
after vias, before courtyards and the tip:
  radius = max(3 px, 0.6 mm)    covers the 0.3 mm gap on a 0.2 mm Trace; seen fitted
  colour kSolder (light grey), alpha kInactiveAlpha when blob.layer != Active Layer
```

### 5.5 `--repair` (S4)

In `main`, after loading and `--probe`, before `SDL_Init`: for each comma-separated item, `ParseRepair` then `session.repair`; a parse error or `Unknown` → `bw_ui: <message>` + usage, exit 2. Messages: the `ParseRepair` error; `unknown reference 'U9'`; `no Board to repair` without a Machine. The first power-on then rebuilds, as in the UI.

## 6. Reused Infrastructure

- `Machine::swap`, `mend`, `build`, `probeNet`, `padPosition` (P2d, P2g): Repairs and rebuilds, nothing new in `bw_machine` but parsing.
- `FormatPoint` (P2d): the log line and the CLI message share it.
- `Runner`, `TvConsumer`, `ProbeConsumer` (P2e, P2g): unchanged, rebuilt as a unit.
- `BoardScene` drawn from `drawnBoard()`; `Contains` and the pad loop of `hitTest` for `componentAt`.
- The CLI's `--repair` syntax and `ParseMillimetres`.

## 7. Build Integration

```cmake
# src/session/CMakeLists.txt
add_library(bw_session STATIC session.cpp)
target_link_libraries(bw_session PUBLIC bw_machine bw_runner PRIVATE bw_warnings)
# top-level CMakeLists.txt: add_subdirectory(src/session) after src/machine; src/machine: repair.cpp
# src/ui: bw_ui links bw_session; tests: bw_tests links bw_session, new test files
```

## 8. Dependency Diagram

```
ui ──► session ──► machine ──► board ──► netlist ──► parts ──► sim
 │        └──────► runner ──► devices, instruments ──► sim
 └───► view ──► board
```

## 9. Testing

- **repair_test** (`[machine]`): `ParseRepair` of `swap:U3`, `mend:F.Cu:52.30:41.00`, `mend:B.Cu:1:2`; errors for `cut:U1`, `mend:X.Cu:1:2`, `mend:F.Cu:1`, `mend:F.Cu:a:2`; `FormatRepair` lines.
- **board_scene_test**: `componentAt` on a pad → its Component; inside a courtyard off any pad → that Component; off every courtyard → nullopt.
- **session_test** (`[session]`, `broken_trace`):
  - powered: `repair` → Refused, nothing logged; off: swap and a far mend → Done, `log()` == `{"swap U3", "mend F.Cu …"}`, one blob; `swap U99` → Unknown;
  - `drawnBoard()` Traces equal before and after a mend at `kH0Crack`;
  - probe 1 mm each side of `kH0Crack`: two different `probedNet()`; power off, mend, power on: the same Net; the tip is unchanged;
  - netlist without a Board: `repair` → Unknown, `touch` ignored, power cycle runs.
- **board_repair_test** (`[machines]`): for each Puzzle, a Session at seed 1, `Speed::Max`: right Repair, power on to 60 ms → TV frame equals the fault-free Session's; a wrong Repair (wrong swap, or a mend away from the crack) does not (as P2d's faults test).
- **Perf** (`[.perf]`, hidden): 20 rebuilds of `broken_trace` (mend, timed `powerOn`, power off); mean < 100 ms in release. The rebuild is synchronous inside `powerOn`.
- **Agent, screenshot**: `bw_ui puzzles/broken_trace.bwp --repair mend:F.Cu:137.59:99.17 --probe U13.2B --screenshot /tmp/bw-repair.png --after 2s`: blob at the crack, log line, grid, U13.2B pulsing.
- **Manual**: Done-when play-through.

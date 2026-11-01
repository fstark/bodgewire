# Solder Bridges and Real Power — Detailed Design (P3f)

Implements the specification in [SHORTS_POWER.md](SHORTS_POWER.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

No new modules or source files. Everything extends existing ones:

```
src/parts/catalogue.hpp/.cpp     PowerPinVolts()
src/netlist/builder.hpp/.cpp     BuildCircuit(..., unpowered)
src/machine/puzzle.hpp/.cpp      BridgeDecl, `fault bridge`
src/machine/repair.hpp/.cpp      UnbridgeRepair, `unbridge:`
src/machine/electrical.hpp/.cpp  BoardPower, FindPower()
src/machine/machine.hpp/.cpp     bridges_, removeBridge(), power_
src/board/board.hpp              comment only: bodgeWires also holds Solder Bridges
src/session/session.hpp/.cpp     UnbridgeRepair dispatch, currentLimited()
src/ui/board_view.hpp/.cpp       setBridges(), bridgeAt(), bridge drawing
src/ui/toolbar.cpp               current-limit LED
src/ui/main.cpp                  Cut on a bridge, feed bridges to the view, --repair usage
puzzles/                         four new .bwp files
tests/                           see §9
```

Power lives in `electrical.cpp` because it is worked out from the same islands as the electrical Nets and reuses its pad → island lookup (`Namer::island`).

## 2. Public Interface

### 2.1 Parts: expected voltage of a power Pin (S3)

```cpp
// src/parts/catalogue.hpp
// Voltage a chip's power Pin expects: "VCC" 5, "GND" 0; nullopt for other names.
std::optional<double> PowerPinVolts(std::string_view pin);
```

A table, so more Rails later (12 V, −5 V) are new rows. Values match `ConnectorVolts("POWER", ...)` exactly, so `==` on doubles is safe.

### 2.2 Puzzle: `fault bridge` (S1)

```cpp
// src/machine/puzzle.hpp
struct BridgeDecl {
    std::uint32_t line;
    PinRef a, b;  // pin by number or name: U1.7, U1.ENP
};

struct Puzzle {
    // ... existing ...
    std::vector<BridgeDecl> bridges;
};
```

### 2.3 Repair: `unbridge` (S2)

```cpp
// src/machine/repair.hpp
struct UnbridgeRepair {
    PinRef a, b;
};

using RepairAction =
    std::variant<SwapRepair, MendRepair, CutRepair, WireRepair, UnwireRepair, UnbridgeRepair>;
// ParseRepair: "unbridge:U1.7:U1.8"; FormatRepair: "unbridge U1.7 U1.8".
```

### 2.4 Power (S3, S4)

```cpp
// src/machine/electrical.hpp
struct BoardPower {
    std::vector<std::string> unpowered;  // chip References, design order
    bool deadRail = false;               // an island holds supply pins of different voltages
};

// Powered check of every chip on `board`'s islands. Connectors are always Powered.
BoardPower FindPower(const LoadedCircuit& designCircuit, const Board& board,
                     const Islands& islands);
```

### 2.5 Builder

```cpp
// src/netlist/builder.hpp
// `unpowered`: References of chips left out of the Circuit's Nets (outputs Z, inputs ignored).
LoadResult BuildCircuit(const Netlist& netlist, std::span<const StuckPin> stuck = {},
                        std::span<const std::string> unpowered = {});
```

### 2.6 Machine

```cpp
// src/machine/machine.hpp
class Machine {
public:
    // ... existing ...
    bool removeBridge(const PinRef& a, const PinRef& b);  // either order; false: no such bridge
    std::span<const WireEnds> bridges() const { return bridges_; }
    const BoardPower& power() const { return power_; }  // of the current Copper
private:
    std::vector<WireEnds> bridges_;  // Solder Bridges: same shape as a Bodge Wire
    BoardPower power_;               // see rebuildBoard()
};
```

### 2.7 Session

```cpp
// src/session/session.hpp
// Powered with a Dead Rail: the bench supply is in current limit.
bool currentLimited() const { return powered_ && machine_ && machine_->power().deadRail; }
```

While powered, the live Circuit is the current Machine (Repairs are refused), so reading `machine_->power()` is exact.

## 3. Integration Points

1. **`PowerPinVolts`** — [catalogue.cpp](../../src/parts/catalogue.cpp): two-row table. Cost: none at run time.

2. **Puzzle parser** — [puzzle.cpp](../../src/machine/puzzle.cpp) `Parser::line()`: a `bridge` branch next to `stuck`/`crack`; the catch-all message becomes `expected: fault stuck|crack|bridge ...`.
   ```cpp
   } else if (tokens[0] == "fault" && tokens.size() >= 2 && tokens[1] == "bridge") {
       bridge(number, tokens);
   ```
   `bridge()` accepts exactly 4 tokens, each `REF.PIN` split at the first dot as `stuck()` does; else `expected: fault bridge REF.PIN REF.PIN`.

3. **Repair parse/format** — [repair.cpp](../../src/machine/repair.cpp): `unbridge:` parsed by the same two-`PinRef` path as `unwire:`; formatted `unbridge A B`.

4. **Builder** — [builder.cpp](../../src/netlist/builder.cpp) `Builder::build()`, the connect loop:
   ```cpp
   if (unpowered(pin.first)) {
       // no Driver, no Load: the chip's Pins get private Nets at finalize()
   } else if (const StuckPin* s = stuckAt(pin.first, pin.second)) {
       circuit.tie(net.net, s->value, DriveStrength::Strong);
   } else if (part.component != nullptr) {
       circuit.connect(*part.component, pin.second, net.net);
   }
   loaded_->connections.push_back(...);  // unchanged: PinNet() still names the Net at the pad
   ```
   A Stuck-at Pin of an Unpowered chip drives nothing either (the damaged driver has no supply). `Circuit::finalize()` already gives unconnected Pins private Nets ([circuit.hpp](../../src/sim/circuit.hpp#L57)), so the Component still exists, evaluates nothing (its inputs never change) and is invisible. Cost: one span scan per Pin at build, ≤ 16 chips.

5. **`FindPower`** — [electrical.cpp](../../src/machine/electrical.cpp), next to `IslandNetNames`; uses `Namer::island(reference, pin)` (§5.1).

6. **Machine** — [machine.cpp](../../src/machine/machine.cpp):
   - `LoadMachine`: after cracks, validate `puzzle.bridges` (§5.2) into `bridges_`.
   - `rebuildBoard()`: after the wires, `board_.bodgeWires.push_back(b.wire)` for each bridge, then `power_ = FindPower(*designCircuit_, board_, FindIslands(board_))`.
   - `build()`: `BuildCircuit(ElectricalNetlist(design_, *designCircuit_, board_), stuck_, power_.unpowered)`.
   - `removeBridge()`: as `removeWire()` on `bridges_`; both share a file-local `FindEnds(std::vector<WireEnds>&, const PinRef&, const PinRef&)`.
   - `padPosition()` reads `pristine_` instead of `board_` (same pads; cracks never move them), so `LoadMachine` can validate bridges before the first `rebuildBoard()`.

   Cost: one extra `FindIslands` per Repair (P2h rebuild is 0.8 ms in release, budget 100 ms).

7. **Board** — [board.hpp](../../src/board/board.hpp#L72): comment becomes `// pad-to-pad joins: Bodge Wires and Solder Bridges; empty on a loaded Board`. `FindIslands` ([copper.cpp](../../src/board/copper.cpp#L278)) already joins them, and continuity (`Machine::connected`) follows.

8. **Session** — [session.cpp](../../src/session/session.cpp) `repair()`: one more visitor arm `[&](const UnbridgeRepair& r) { return machine_->removeBridge(r.a, r.b); }`; logged like every Done Repair. `currentLimited()` inline (§2.7).

9. **Board view** — [board_view.hpp](../../src/ui/board_view.hpp) / [board_view.cpp](../../src/ui/board_view.cpp):
   ```cpp
   void setBridges(std::span<const BodgeWire> bridges);
   // Index of the nearest bridge within reach().copper of `at`; for the Cut tool.
   std::optional<std::size_t> bridgeAt(Point at) const;
   ```
   Drawn right after the pads (below cracks, Cut marks and courtyards): a line from pad to pad in `kPadTin`, width `kBridgeWidth = 600'000` nm, alpha `kInactiveAlpha` unless B.Cu is the Active Layer. At fit zoom it reads as two pads touching; zoomed in it is a visible blob. Net highlight and the Net panel use design Nets, so they never show the bridge.

10. **UI main** — [main.cpp](../../src/ui/main.cpp#L386) `Tool::Cut`: after `wireAt`, before the Trace checks:
    ```cpp
    } else if (const auto bridge = boardView.bridgeAt(click->at)) {
        const WireEnds& b = machine->bridges()[*bridge];
        result = session.repair(UnbridgeRepair{b.a, b.b});
    ```
    Next to `setBodgeWires` (line ~499): `boardView.setBridges(...)` from `machine->bridges()`. Usage text adds `unbridge:REF.PIN:REF.PIN`. Toolbar and refusal texts unchanged (they must not hint at a bridge).

11. **Toolbar LED** — [toolbar.cpp](../../src/ui/toolbar.cpp#L126): after the Power button, `SameLine`, a filled circle (radius 0.4 × text height, as `DrawLamps`) red `IM_COL32(230, 40, 40, 255)` when `session.currentLimited()`, else `IM_COL32(70, 70, 70, 255)`, followed by the text `LIMIT`; tooltip `Bench supply in current limit`.

12. **`bw run` CLI** — [src/cli/main.cpp](../../src/cli/main.cpp#L156) `Repair()`: an `UnbridgeRepair` branch calling `machine.removeBridge()` (its last branch is currently `std::get<UnwireRepair>`); usage string adds `unbridge:`.

## 4. Internal State

| Where | Field | Meaning |
|---|---|---|
| `Puzzle` | `std::vector<BridgeDecl> bridges` | as written, unvalidated |
| `Machine` | `std::vector<WireEnds> bridges_` | validated, pins as numbers, pad centres in `wire` |
| `Machine` | `BoardPower power_` | of `board_`, refreshed by every `rebuildBoard()` |
| `Board` | `bodgeWires` | wires then bridges; only `FindIslands` reads it |

No new state in the Session or the Runner.

## 5. Key Algorithms

### 5.1 FindPower (S3, S4)

```
namer = Namer(board, islands)
rails: map<island, set<double>>
for part in designCircuit.parts with kind == Connector:
    for i, spec in ConnectorPins(part.info->name):
        if v = ConnectorVolts(part.info->name, spec.name):
            rails[namer.island(part.reference, i)].insert(v)
power.deadRail = any set in rails has size > 1
for part in designCircuit.parts with kind == Chip:
    pins = designCircuit.components.at(part.reference)->pins()
    for i where pins[i].kind == Power:
        want = PowerPinVolts(pins[i].name)         // Check(): every chip power Pin has one
        rail = rails.find(namer.island(part.reference, i))
        if rail == end or rail->second != {want}:
            power.unpowered.push_back(part.reference); break
```

Cases, all from the same rule:
- fault-free Board: every chip Powered (`CheckBoard` already ties every power pad to J1);
- cracked VCC Trace: the chip's VCC island holds no supply pin → Unpowered (the meter on that stub shows `- - -`: unnamed island, as any isolated copper since P3d);
- GND pin wired to VCC: its island holds 5 V → Unpowered (and usually a Dead Rail, through its own GND Trace);
- Dead Rail: that island's set is {0, 5} → every chip with a power Pin on it is Unpowered.

### 5.2 Bridge validation in LoadMachine (S1)

For each `BridgeDecl`, errors at its line, in this order:
1. `padOf(a)`, `padOf(b)`: unknown reference → `unknown reference 'U99'`; unknown pin → `U1 (74LS161) has no pin X` (as for `fault stuck`).
2. different References or the same pad → `bridge U1.7 U2.8: pins of one chip only`.
3. the Part is not `PartKind::Chip` → `bridge J1.1 J1.2: pins of one chip only`.
4. pad centres more than `kBridgeReach = 2'600'000` nm apart → `bridge U1.7 U1.9: pins are not neighbours`.

Geometric neighbours, not pin arithmetic: works for any footprint, and rejects pins 8 and 9 of a DIP-16 (7.62 mm apart, across the package).

### 5.3 What a bridge does electrically

Nothing new. The two pads' islands merge in `FindIslands`; `NameIslands` names the merged island after the first name it meets (POWER presets first), so:
- signal + signal → one Net, two strong Drivers → TTL Contention, 0 wins;
- signal + GND-tied pin → the island is GND (preset), the output loses to the Supply tie;
- VCC-tied + GND → presets "VCC" then "GND" on the same island: it is named GND, tied Supply 0, and `RailVolts` reads 0 V through J1.GND. The empty "VCC" Net keeps its tie but no island maps to it. Tested, not changed.

## 6. Reused Infrastructure

- `FindIslands` and `Board::bodgeWires` (P3e): bridges join islands exactly as wires do.
- `WireEnds`: the bridge record; `padOf()` / `padPosition()` for pin → pad.
- `Namer::island()`: pad → island for `FindPower`.
- `ConnectorPins` / `ConnectorVolts`: supply pins and their voltages.
- `Circuit::finalize()` private Nets: an Unpowered chip needs no new kernel concept.
- `RailVolts`, `Session::volts()`: Dead Rail reads 0 V with no change.
- `Machine::connected()`: continuity across a bridge with no change.
- Cut tool flow and `wireAt()` pattern for `bridgeAt()`.
- Repair log, `ParseRepair`'s two-PinRef parsing.

## 7. Build Integration

No CMake changes: no new source files. New test file `tests/machines/shorts_power_test.cpp` is added to the `bw_tests` list in [tests/CMakeLists.txt](../../tests/CMakeLists.txt).

## 8. Dependency Diagram

```
bw_ui ──▶ bw_session ──▶ bw_machine ──▶ bw_netlist ──▶ bw_parts ──▶ bw_sim
                              │                ▲
                              └──▶ bw_board ───┘
```

Unchanged. `FindPower` (bw_machine) calls `PowerPinVolts`/`ConnectorVolts` (bw_parts); `BuildCircuit` (bw_netlist) takes plain References.

## 9. Testing

Catch2, existing files unless noted.

| File | Checks |
|---|---|
| `tests/machine/puzzle_test.cpp` | `fault bridge U1.7 U1.ENP` parses; wrong token count, missing dot → error with line |
| `tests/machine/repair_test.cpp` | `unbridge:U1.7:U1.8` round-trips; bad forms rejected |
| `tests/machine/machine_test.cpp` | bridge errors (§5.2: unknown pin, two chips, connector, U1.7–U1.9); `removeBridge` either order, false when absent; bridged pads `connected()`, unbridged not |
| `tests/machine/electrical_test.cpp` | `FindPower` on the test-pattern Machine: fault-free none Unpowered, no Dead Rail; crack on U6's VCC Trace → `{"U6"}`; bridge U1.7–U1.8 → Dead Rail, every chip (X1, U1–U14) Unpowered; wire U9.7 to U9.14 → Dead Rail |
| `tests/netlist/builder_test.cpp` | `BuildCircuit` with `unpowered = {"U1"}`: U1's output Nets have no Driver (floating), `PinNet` still names them |
| `tests/machines/shorts_power_test.cpp` (new, `[machines]`) | each new Puzzle's TV frame differs from the fault-free one and equals it after its Repair (§10); Dead Rail: `currentLimited()` true when powered, false off and after `unbridge`; meter red lead on U1.16 reads 0 V |
| existing `[machines]` tests | unchanged results: the three P2 Puzzles and the fault-free frame |

Screenshots (agent reads the PNG): `puzzles/dead_rail.bwp --screenshot` (LED lit), and with `--repair unbridge:U1.7:U1.8` (LED dark). `bw_ui` has no zoom or layer flags, so the zoomed-in bridge on B.Cu is checked by the user. Release check: 1.00× on each new Puzzle, rebuild < 100 ms.

## 10. Puzzles

All `machine test_pattern`. Coordinates from the `.kicad_pcb` (VCC Trace found by removing each VCC segment and checking which pads lose J1).

| File | Fault | Expected Symptom | Intended Repair |
|---|---|---|---|
| `puzzles/sync_bridge.bwp` | `fault bridge U10.1 U10.2` (HSYNC_N, VSYNC_N) | Contention, 0 wins: both XOR inputs equal, SYNC stays 0, no sync pulses; TV does not lock | `unbridge:U10.1:U10.2` |
| `puzzles/line_bridge.bwp` | `fault bridge U9.6 U9.7` (3Y LINE_END, GND) | LINE_END held at 0: no vertical count, rolling picture; swapping U9 changes nothing | `unbridge:U9.6:U9.7` |
| `puzzles/unpowered_chip.bwp` | `fault crack F.Cu 164.85 51.05` (U6's VCC stub) | U6 outputs Z → LINE_END_N and HSYNC_N float high: no line sync | `mend:F.Cu:164.85:51.05` or a wire U6.14 to VCC |
| `puzzles/dead_rail.bwp` | `fault bridge U1.7 U1.8` (ENP on VCC, GND) | all chips dead, black screen, meter 0 V, LIMIT lit | `unbridge:U1.7:U1.8` |

Titles and Briefs describe the Symptom only, as a customer would. If a test shows a Symptom differs from the table, the test states the actual Symptom and the Brief follows it.

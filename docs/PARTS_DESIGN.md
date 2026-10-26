# Parts and Netlist — Detailed Design (P1b)

Implements the specification in [PARTS.md](PARTS.md) (sections S1–S4).

All code must follow [STYLE.md](STYLE.md) and [NAMING.md](NAMING.md). Terminology: [GLOSSARY.md](GLOSSARY.md). Builds on the kernel ([KERNEL_DESIGN.md](KERNEL_DESIGN.md)).

---

## 1. Directory Layout

```
src/sim/                 (existing) small change: power and no-connect pin kinds (§3)
src/parts/               library bw_parts → bw_sim
  catalogue.hpp/.cpp     PartInfo, PartParam, FindPart(), AllParts()
  delays.hpp             datasheet delays, one constexpr table
  gate_package.hpp/.cpp  GatePackageSpec, GatePackage (00/02/04/08/20/30/32/86)
  ls74.hpp/.cpp          Ls74 (dual D flip-flop)
  ls161.hpp/.cpp         Ls161 (counter)
  oscillator.hpp/.cpp    Oscillator (OSC)
src/netlist/             library bw_netlist → bw_parts, bw_sim
  units.hpp/.cpp         ParsePeriodFromFrequency(), ParseDuration()
  diagnostic.hpp/.cpp    Diagnostic, Format()
  parser.hpp/.cpp        Netlist (syntax tree), ParseNetlist()
  builder.hpp/.cpp       LoadedCircuit, BuildCircuit(), LoadNetlist(), LoadNetlistFile()
src/cli/main.cpp         executable bw
netlists/counter.bwn     sample (PARTS.md "Done when")
tests/parts/*_test.cpp
tests/netlist/*_test.cpp
```

Split rationale: `parts` knows chips but not text; `netlist` knows text but no chip internals (it only uses `FindPart()` and `Component::pins()`). P2's Board and KiCad export reuse `parts` (package, pin names) without the parser.

## 2. Public Interface — Catalogue

```cpp
// catalogue.hpp
enum class PartKind : std::uint8_t { Chip, Resistor };

struct PartParam {
    std::string name;
    std::string value;
};

// Creates the Component in `circuit`, named `reference`; error text on a bad parameter.
using PartFactory = std::expected<Component*, std::string> (*)(
    Circuit& circuit, std::string reference, std::span<const PartParam> params);

struct PartInfo {
    std::string_view name;         // "74LS161"
    std::string_view description;  // "synchronous 4-bit binary counter"
    std::string_view package;      // "DIP-16"
    PartKind kind;
    PartFactory create;            // nullptr for resistors (no Component, §8.4)
};

const PartInfo* FindPart(std::string_view name);  // nullptr if unknown
std::span<const PartInfo> AllParts();
```

Every chip Component: pin index `i` = package pin `i + 1`; GND and VCC pins are `PinKind::Power` named `GND` / `VCC`; unused positions are `PinKind::NoConnect` named `NC<number>`; all other pins TTL family.

## 3. Kernel Change — Power and No-connect Pins

```cpp
// logic.hpp
enum class PinKind : std::uint8_t { Input, Output, OpenCollector, Power, NoConnect };
constexpr bool IsDriver(PinKind kind);  // Output or OpenCollector
```

| File | Change |
|---|---|
| `src/sim/logic.hpp` | add the two kinds and `IsDriver`; `DriveOf` already returns `None` via `default` |
| `src/sim/circuit.cpp` `finalize()` | `if (info.kind != PinKind::Input)` → `if (IsDriver(info.kind))` for drivers; readers only for `Input`; `Power`/`NoConnect` are neither |
| `src/sim/simulator.cpp` `powerOn()` | random initial value only `if (IsDriver(kind))` |
| `src/sim/simulator.cpp` `PowerOnContext::setOutput`, `EvalContext::drive` | assert `IsDriver(kind)` |

Cost: none in the run loop. VCC and GND nets get no drivers from power pins, so they stay as cheap as before.

## 4. Public Interface — Part Models

```cpp
// gate_package.hpp
struct GateUnit {
    GateKind kind;
    std::uint8_t output;                  // package pin
    std::array<std::uint8_t, 8> inputs;   // package pins
    std::uint8_t inputCount;
};

struct GatePackageSpec {
    std::string_view part;
    std::uint8_t pinCount;                     // 14
    std::span<const std::string_view> names;   // index = pin - 1; "GND", "VCC", "NC<n>" included
    std::span<const GateUnit> units;
    Delays delays;
};

const GatePackageSpec& GatePackageSpecOf(std::string_view part);  // Check()s

class GatePackage final : public Component {
public:
    GatePackage(std::string reference, const GatePackageSpec& spec);
    void evaluate(EvalContext& ctx) override;  // all units, GateFunction() per unit
private:
    const GatePackageSpec& spec_;
};

// ls74.hpp — pins per datasheet: 1 1CLR_N, 2 1D, 3 1CLK, 4 1PRE_N, 5 1Q, 6 1Q_N, 7 GND,
//            8 2Q_N, 9 2Q, 10 2PRE_N, 11 2CLK, 12 2D, 13 2CLR_N, 14 VCC
class Ls74 final : public Component {
public:
    explicit Ls74(std::string reference);
    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;
private:
    std::array<LogicValue, 2> state_{};
};

// ls161.hpp — 1 CLR_N, 2 CLK, 3 A, 4 B, 5 C, 6 D, 7 ENP, 8 GND, 9 LOAD_N, 10 ENT,
//             11 QD, 12 QC, 13 QB, 14 QA, 15 RCO, 16 VCC
class Ls161 final : public Component {
public:
    explicit Ls161(std::string reference);
    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;
private:
    std::uint8_t count_ = 0;
    bool unknown_ = false;  // count is X
};

// oscillator.hpp — 1 NC1, 7 GND, 8 OUT, 14 VCC; other positions NC<n>
class Oscillator final : public Component {
public:
    Oscillator(std::string reference, Time period);  // Check(period >= 2)
    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;        // same algorithm as ClockSource
private:
    Time highTime_, lowTime_;
    LogicValue level_ = LogicValue::Zero;
};
```

## 5. Datasheet Delays

`delays.hpp`, `constexpr` values in picoseconds, TI typical at 5 V, 25 °C, CL = 15 pF. Implementation checks each against the TI datasheet and corrects this table if it differs.

| Part / path | rise (tPLH) | fall (tPHL) |
|---|---|---|
| 74LS00, 74LS20 | 9 ns | 10 ns |
| 74LS02 | 10 ns | 10 ns |
| 74LS04 | 9 ns | 10 ns |
| 74LS08 | 8 ns | 10 ns |
| 74LS30 | 8 ns | 13 ns |
| 74LS32 | 14 ns | 14 ns |
| 74LS86 | 12 ns | 10 ns |
| 74LS74 CLK, PRE_N, CLR_N → Q, Q_N | 13 ns | 25 ns |
| 74LS161 CLK → Q | 13 ns | 15 ns |
| 74LS161 CLR_N → Q | — | 20 ns |
| 74LS161 CLK → RCO | 20 ns | 18 ns |
| 74LS161 ENT → RCO | 9 ns | 9 ns |

```cpp
namespace delays {
inline constexpr Delays kLs00{9'000, 10'000};
// … one constant per row
}
```

## 6. Public Interface — Netlist

```cpp
// diagnostic.hpp
enum class Severity : std::uint8_t { Error, Warning };
struct Diagnostic {
    std::string file;
    std::uint32_t line;  // 1-based; 0 = whole file
    Severity severity;
    std::string message;
};
std::string Format(const Diagnostic& d);  // "file:12: error: message"

// units.hpp
std::expected<Time, std::string> ParsePeriodFromFrequency(std::string_view text);  // "14.31818MHz" → ps
std::expected<Time, std::string> ParseDuration(std::string_view text);  // "10ms", "250ns", "2s"

// parser.hpp — syntax only, no Part knowledge
struct PartDecl { std::uint32_t line; std::string reference, part; std::vector<PartParam> params; };
struct PinRef   { std::string reference, pin; };
struct NetDecl  { std::uint32_t line; std::string name; std::vector<PinRef> pins; };
struct Netlist  { std::string file; std::vector<PartDecl> parts; std::vector<NetDecl> nets; };
std::expected<Netlist, std::vector<Diagnostic>> ParseNetlist(std::string_view text,
                                                             std::string file);

// builder.hpp
struct LoadedCircuit {
    Circuit circuit;  // finalized
    std::map<std::string, NetId, std::less<>> nets;              // includes GND, VCC
    std::map<std::string, Component*, std::less<>> components;   // chips only
    std::vector<Diagnostic> warnings;
};
std::expected<std::unique_ptr<LoadedCircuit>, std::vector<Diagnostic>> BuildCircuit(
    const Netlist& netlist);
std::expected<std::unique_ptr<LoadedCircuit>, std::vector<Diagnostic>> LoadNetlist(
    std::string_view text, std::string file);
std::expected<std::unique_ptr<LoadedCircuit>, std::vector<Diagnostic>> LoadNetlistFile(
    const std::filesystem::path& path);
```

`std::unique_ptr` because a `Simulator` keeps a `Circuit&`: the circuit must not move after loading. `std::map` keeps net order stable for `--record-all` and VCD output.

## 7. Internal State

- `catalogue.cpp`: `constexpr std::array<PartInfo, 12> s_parts`, sorted by name; `FindPart` is a linear scan (12 entries, load time only).
- `gate_package.cpp`: one `constexpr` pin-name array, unit array and `GatePackageSpec` per gate Part, from the datasheet pinouts (74LS00/08/32/86 share one layout; 74LS02, 74LS04, 74LS20, 74LS30 have their own).
- `builder.cpp` per build (local to `BuildCircuit`):

```cpp
struct PlacedPart { const PartDecl* decl; const PartInfo* info; Component* component; };
std::map<std::string, PlacedPart, std::less<>> placed;              // by reference
std::map<std::pair<std::string, PinIndex>, const NetDecl*> pinNet;  // pin → first NetDecl
std::vector<Diagnostic> errors, warnings;
```

## 8. Key Algorithms

### 8.1 Parser

```
for each line (1-based):
  strip from '#'; split on whitespace; skip if empty
  "part": need ≥ 3 tokens; ref ~ [A-Z]+[0-9]+; part ~ [A-Za-z0-9]+; extra tokens k=v (non-empty both sides)
  "net":  need ≥ 3 tokens; name ~ [A-Za-z_][A-Za-z0-9_]*; each pin token "ref.pin" (one '.', both non-empty)
  else: error "unknown statement '<tok>'"
collect every error; any error → unexpected(errors)
```

### 8.2 Pin lookup

```
ResolvePin(component, text):
  all digits → n; valid if 1 ≤ n ≤ pins().size(); index n - 1
  else index of pins()[i].name == text
  none → error "<ref> (<part>) has no pin <text>"
```

Resistors (no Component) accept pins `1` and `2` only.

### 8.3 Build

```
1. circuit.addNet("GND"), addNet("VCC"); tie(GND, Zero, Supply); tie(VCC, One, Supply)
2. for each PartDecl: duplicate ref → error; FindPart → unknown → error
     Chip: create(circuit, ref, params) → error text → error at decl.line
     Resistor: only "value" param allowed; record pin slots
3. for each NetDecl, for each PinRef:
     unknown ref → error; ResolvePin → error
     NoConnect → error "<ref>.<pin> is not connected inside the part"
     Power → allowed only if net name == pin name (GND/VCC), else error
     already in pinNet with another net name → error "<ref>.<pin> on nets A and B"
     record; create the Net on first use (GND/VCC exist)
4. for each chip Power pin not yet recorded: record on its rail
5. for each resistor:
     both pins on rails, or neither, or a pin unconnected → error "R1: only pull-up/pull-down resistors are supported"
     else tie(otherNet, rail == VCC ? One : Zero, Weak)
6. if errors: return them
7. connect every recorded chip pin; for each chip Input pin not recorded: warning
     "<ref>.<pin> (<name>) is unconnected: floats high"
8. circuit.finalize(); return LoadedCircuit
```

Errors in steps 2–5 are all collected before returning. Diagnostics are sorted by line.

### 8.4 Gate package evaluate

```
for unit in spec.units:
  inputs[i] = ctx.read(unit.inputs[i] - 1)
  v = GateFunction(unit.kind, inputs[0..inputCount))
  ctx.drive(unit.output - 1, v, DelayFor(spec.delays, v))   // no-op if unchanged (inertial)
```

`DelayFor` moves from `primitives.cpp` (anonymous namespace) to `primitives.hpp` as a public free function, so Parts reuse it.

### 8.5 74LS74 (per flip-flop)

```
pre = read(PRE_N); clr = read(CLR_N)
if pre == X or clr == X:     state = X
elif pre == 0 and clr == 0:  drive Q = 1, Q_N = 1; state = One; continue
elif pre == 0:               state = One
elif clr == 0:               state = Zero
elif rose(CLK):              state = readBefore(D)
drive Q = state, Q_N = Not(state)   // delays kLs74
powerOn: state = random bit each; setOutput Q / Q_N to match
```

### 8.6 74LS161

```
clr = read(CLR_N); edge = rose(CLK)
if clr == 0:          count = 0; unknown = false; qDelay = kLs161Clear
elif clr == X:        unknown = true
elif edge:
  load = readBefore(LOAD_N); enp = readBefore(ENP); ent = readBefore(ENT)
  if load == 0:       data = readBefore(A..D); unknown = any X; count = bits
  elif load == X:     unknown = true
  elif enp == 1 and ent == 1: if !unknown: count = (count + 1) & 15
  elif (enp == X or ent == X) and enp != 0 and ent != 0: unknown = true
drive QA..QD = unknown ? X : count bits                     // kLs161ClockToQ (or clear delay)
rco = GateFunction(And, { read(ENT), unknown ? X : (count == 15 ? One : Zero) })
drive RCO = rco, delay edge ? kLs161ClockToRco : kLs161EntToRco
powerOn: count = 4 random bits; set QA..QD, RCO (ENT unknown → set RCO = Zero)
```

## 9. CLI

`src/cli/main.cpp`, executable `bw` → `bw_netlist`.

```
bw run <file.bwn> [--time 10ms] [--seed 1] [--record A,B | --record-all] [--vcd out.vcd]
```

```
parse args (unknown option, missing value, bad duration → usage on stderr, exit 2)
LoadNetlistFile → errors: print Format() each to stderr, exit 1
print warnings to stderr
Simulator sim(loaded->circuit); sim.powerOn(seed)
record: --record-all → every entry of loaded->nets except GND/VCC; --record → look up each (unknown → exit 2)
t0 = steady_clock; sim.runFor(time); wall = elapsed
if --vcd: WriteVcd(file, circuit, sim.recorder(), sim.now())
print to stdout: simulated_s, wall_s, ratio, applied_events, evaluations
```

Defaults: `--time 1ms`, `--seed 1`, no recording, no VCD.

## 10. Sample Netlist

`netlists/counter.bwn`:

- `X1 OSC freq=1MHz` → net `CLK`.
- `U1`, `U2` 74LS161 cascaded: `U1.RCO → U2.ENT`, both `ENP`, `U1.ENT`, `LOAD_N` on `VCC`; `CLR_N` on net `RESET_N` pulled up by `R1` to `VCC`. Outputs `C0..C7`.
- `U3` 74LS74: flip-flop 1 toggles on `C7` (`1Q_N → 1D`), output `DIV`; preset/clear on `VCC`.
- `U4` 74LS00: `TOP_N = NAND(C6, C7)`; remaining gate inputs on `GND`.

## 11. Reused Infrastructure

| Existing | Used for |
|---|---|
| `Component`, `PinSpec`, `EvalContext`, `PowerOnContext` | all Part models |
| `GateFunction`, `Delays`, `DelayFor` (made public) | gate packages, 74LS161 RCO |
| `Circuit::add`, `connect`, `tie`, `finalize` | builder |
| `ClockSource` algorithm | `Oscillator` (same 6 lines; a different pinout, so not a subclass) |
| `Simulator`, `Recorder`, `WriteVcd` | CLI, tests |
| `Check` | constructor misuse in Part models |

## 12. Build Integration

- Top-level `CMakeLists.txt`: `add_subdirectory(src/parts)`, `add_subdirectory(src/netlist)`, `add_subdirectory(src/cli)` after `src/sim`.
- `src/parts/CMakeLists.txt`: `add_library(bw_parts STATIC …)`, `target_link_libraries(bw_parts PUBLIC bw_sim PRIVATE bw_warnings)`.
- `src/netlist/CMakeLists.txt`: `bw_netlist` → `PUBLIC bw_parts`.
- `src/cli/CMakeLists.txt`: `add_executable(bw main.cpp)` → `bw_netlist`.
- `tests/CMakeLists.txt`: add `parts/*_test.cpp`, `netlist/*_test.cpp`; link `bw_netlist`; `target_compile_definitions(bw_tests PRIVATE BW_NETLIST_DIR="${PROJECT_SOURCE_DIR}/netlists")` (STYLE.md: macros only where the language can't help — a source path is one).

## 13. Dependency Diagram

```
bw_sim  ←  bw_parts  ←  bw_netlist  ←  bw (CLI)
   ↑           ↑            ↑
   └───────── bw_tests ─────┘
```

## 14. Testing

| File | Covers |
|---|---|
| `tests/sim/circuit_test.cpp` (add) | Power / NoConnect pins are neither drivers nor readers |
| `tests/parts/catalogue_test.cpp` | `FindPart` known / unknown; every chip Part: pin count matches package, pin names unique, GND/VCC at datasheet positions, all non-power pins TTL |
| `tests/parts/gate_package_test.cpp` | per gate Part: every unit's truth table through the simulator, rise/fall delay measured on one unit |
| `tests/parts/ls74_test.cpp` | clock edge; preset; clear; both low → both high; same-timestamp D; delays |
| `tests/parts/ls161_test.cpp` | count 0→15→0; RCO at 15 only with ENT; load; async clear mid-cycle; ENP low holds; X on LOAD_N → X outputs until clear; cascade of two; delays |
| `tests/parts/oscillator_test.cpp` | period from frequency; odd period duty |
| `tests/netlist/units_test.cpp` | `1MHz` → 1'000'000 ps; `14.31818MHz` → 69'841 ps; `10ms`; bad unit; missing number |
| `tests/netlist/parser_test.cpp` | comments, blank lines, each syntax error with its line |
| `tests/netlist/builder_test.cpp` | one test per S3 error and warning (exact message); multi-line nets; pins by number and by name are the same pin; implicit power; pull-up / pull-down |
| `tests/netlist/sample_test.cpp` | `counter.bwn` loads with no warnings, counts (C0..C7 after N cycles), `DIV` toggles on each rising `C7`; identical recording to the same circuit built through the C++ API |
| CLI | manual: `./build/debug/src/cli/bw run netlists/counter.bwn --time 1ms --record-all --vcd counter.vcd` |

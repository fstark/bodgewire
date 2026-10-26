# Parts and Netlist — Implementation Plan (P1b)

Design: [PARTS_DESIGN.md](PARTS_DESIGN.md)
Spec: [PARTS.md](PARTS.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Kernel: power / no-connect pin kinds, public `DelayFor` | |
| 2 | Parts library, delays, catalogue, gate packages | |
| 3 | 74LS74 | |
| 4 | 74LS161 | |
| 5 | Oscillator and resistor catalogue entries | |
| 6 | Netlist library: diagnostics and units | |
| 7 | Netlist parser | |
| 8 | Builder: parts, pins, nets, power | |
| 9 | Builder: resistors, warnings, file loading | |
| 10 | Sample netlist | |
| 11 | `bw run` CLI | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug --output-on-failure`

All code follows [STYLE.md](STYLE.md) and [NAMING.md](NAMING.md), namespace `bw`. Each new `.cpp` is added to its library's or the tests' `CMakeLists.txt` in the phase that creates it. Test helpers that build a one-chip circuit with `Stimulus` inputs live in each test file (no shared test library).

---

## Phase 1 — Kernel: power / no-connect pins, public `DelayFor`

Small kernel change the Parts need (design §3, §8.4). First because every Part model depends on it.

### 1.1 — `src/sim/logic.hpp`

```cpp
enum class PinKind : std::uint8_t { Input, Output, OpenCollector, Power, NoConnect };

constexpr bool IsDriver(PinKind kind) {
    return kind == PinKind::Output || kind == PinKind::OpenCollector;
}
```

### 1.2 — Use `IsDriver`

- `src/sim/circuit.cpp` `finalize()`: drivers only `if (IsDriver(info.kind))`; readers (and `netHasTtlInput_`) only `if (info.kind == PinKind::Input)`; other kinds skipped.
- `src/sim/simulator.cpp`: `powerOn()` random initial value only for `IsDriver`; `assert(IsDriver(...))` in `PowerOnContext::setOutput` and `EvalContext::drive` (replacing `!= PinKind::Input`).

### 1.3 — Public `DelayFor`

Move `DelayFor(Delays, LogicValue)` from the anonymous namespace in `src/sim/primitives.cpp` to a declaration in `src/sim/primitives.hpp` (definition stays in the `.cpp`, outside the anonymous namespace).

### 1.4 — Tests

- `tests/sim/logic_test.cpp`: `"logic: IsDriver"` for every kind; `"logic: DriveOf power and no-connect is None"`.
- `tests/sim/circuit_test.cpp`: `"circuit: power and no-connect pins are neither drivers nor readers"` — file-local component with one `Power` and one `NoConnect` pin on a net with one `Input`; `drivers()` empty, `hasTtlInput()` true, `readers()` holds only the input's owner.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"parts: phase 1 — power and no-connect pin kinds"`

---

## Phase 2 — Parts library, delays, catalogue, gate packages

### 2.1 — Library

- `src/parts/CMakeLists.txt`: `add_library(bw_parts STATIC …)`, `target_link_libraries(bw_parts PUBLIC bw_sim PRIVATE bw_warnings)`.
- Top-level `CMakeLists.txt`: `add_subdirectory(src/parts)` after `src/sim`.
- `tests/CMakeLists.txt`: link `bw_parts`.

### 2.2 — `src/parts/delays.hpp`

All constants of design §5 in `namespace bw::delays`: `kLs00`, `kLs02`, `kLs04`, `kLs08`, `kLs20`, `kLs30`, `kLs32`, `kLs86`, `kLs74`, `kLs161ClockToQ`, `kLs161Clear` (rise = fall = 20 ns, only fall is used), `kLs161ClockToRco`, `kLs161EntToRco`.

Check each value against the TI datasheet (`https://www.ti.com/lit/ds/symlink/sn74ls00.pdf`, same pattern for the others, typical column). Correct the table here and in design §5 if they differ. If a datasheet can't be fetched, keep the design value and add a line under this phase listing which ones were not verified.

### 2.3 — `src/parts/gate_package.hpp/.cpp`

`GateUnit`, `GatePackageSpec`, `GatePackageSpecOf()`, `GatePackage` per design §4 and §8.4. Pin kinds from names: `"GND"`/`"VCC"` → `Power`, `"NC…"` → `NoConnect`, unit outputs → `Output`, unit inputs → `Input`; all `Family::Ttl`. Specs (datasheet pinouts):

| Part | Pins (1 → 14) |
|---|---|
| 74LS00 / 08 / 32 / 86 | 1A 1B 1Y 2A 2B 2Y GND 3Y 3A 3B 4Y 4A 4B VCC |
| 74LS02 | 1Y 1A 1B 2Y 2A 2B GND 3A 3B 3Y 4A 4B 4Y VCC |
| 74LS04 | 1A 1Y 2A 2Y 3A 3Y GND 4Y 4A 5Y 5A 6Y 6A VCC |
| 74LS20 | 1A 1B NC3 1C 1D 1Y GND 2Y 2A 2B NC11 2C 2D VCC |
| 74LS30 | A B C D E F GND Y NC9 NC10 G H NC13 VCC |

### 2.4 — `src/parts/catalogue.hpp/.cpp`

Per design §2 and §7. In this phase `s_parts` holds the 8 gate Parts only (later phases add theirs). Each factory: `Check`-free; returns an error string if `params` is non-empty (`"74LS00 takes no parameters"`), else `&circuit.add<GatePackage>(std::move(reference), GatePackageSpecOf(name))`. Descriptions from design S1 table (spec).

### 2.5 — Tests

- `tests/parts/catalogue_test.cpp`: `"catalogue: FindPart known and unknown"`; `"catalogue: chip pinouts are consistent"` — for every `AllParts()` chip, create it in a fresh `Circuit` with no params: pin count = 14 for `DIP-14`, 16 for `DIP-16`; names unique; `GND`/`VCC` are `Power` at 7/14 (DIP-14) or 8/16 (DIP-16); every non-power pin TTL. `"catalogue: parameters rejected"` for a gate Part.
- `tests/parts/gate_package_test.cpp`: `"gate package: <part> truth table"` for each of the 8 parts — every unit, every input combination (74LS20: 16, 74LS30: all-ones and each single zero), driven by `Stimulus` steps 100 ns apart, outputs read after each settle; `"gate package: <part> delays"` for each of the 8 parts — measured rise and fall on unit 1 equal the part's `delays::` constant.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"parts: phase 2 — catalogue and gate packages"`

---

## Phase 3 — 74LS74

### 3.1 — `src/parts/ls74.hpp/.cpp`

`Ls74` per design §4 and §8.5. Pin index constants per flip-flop in a small table:

```cpp
struct FlipFlopPins { PinIndex clr, d, clk, pre, q, qn; };
static constexpr std::array<FlipFlopPins, 2> kUnits{{{0, 1, 2, 3, 4, 5}, {12, 11, 10, 9, 8, 7}}};
```

Catalogue entry `74LS74` (`"dual D flip-flop, preset and clear"`, `DIP-14`).

### 3.2 — Tests: `tests/parts/ls74_test.cpp`

- `"74LS74: captures D on rising CLK"`, `"74LS74: ignores falling CLK"`.
- `"74LS74: preset sets, clear resets, both low gives both high"`.
- `"74LS74: async preset overrides clock"`.
- `"74LS74: D changing with CLK samples old D"`.
- `"74LS74: units are independent"`.
- `"74LS74: delays"` — CLK→Q rise and fall equal `kLs74`.

### Fence

- [ ] Build gate and test gate pass (catalogue test now covers 74LS74 too)
- [ ] Commit: `"parts: phase 3 — 74LS74"`

---

## Phase 4 — 74LS161

### 4.1 — `src/parts/ls161.hpp/.cpp`

`Ls161` per design §4 and §8.6. Pin constants:

```cpp
static constexpr PinIndex kClrN = 0, kClk = 1, kA = 2, kB = 3, kC = 4, kD = 5, kEnp = 6,
                          kLoadN = 8, kEnt = 9, kQD = 10, kQC = 11, kQB = 12, kQA = 13, kRco = 14;
```

Catalogue entry `74LS161` (`"synchronous 4-bit binary counter"`, `DIP-16`).

### 4.2 — Tests: `tests/parts/ls161_test.cpp`

- `"74LS161: counts 0 to 15 and wraps"` (after a clear).
- `"74LS161: RCO high only at 15 with ENT high"`.
- `"74LS161: ENP low holds"`, `"74LS161: ENT low holds and forces RCO low"`.
- `"74LS161: synchronous load"`.
- `"74LS161: asynchronous clear between edges"`.
- `"74LS161: X on LOAD_N gives X outputs until clear"`.
- `"74LS161: two cascaded count to 255"` — `U1.RCO → U2.ENT`.
- `"74LS161: delays"` — CLK→QA, CLR_N→QA, CLK→RCO, ENT→RCO.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"parts: phase 4 — 74LS161"`

---

## Phase 5 — Oscillator and resistor entries

The oscillator factory needs frequency parsing, but units belong to `bw_netlist`. Keep `bw_parts` independent: the factory parses `freq` itself with a small local function (`Hz`/`kHz`/`MHz`, decimal), and phase 6's `ParsePeriodFromFrequency` calls it.

### 5.1 — `src/parts/oscillator.hpp/.cpp`

`Oscillator` per design §4 (pins: 1 `NC1`, 2–6 `NC2`…`NC6`, 7 `GND`, 8 `OUT`, 9–13 `NC9`…`NC13`, 14 `VCC`). Also:

```cpp
// "14.31818MHz" → period in ps, rounded to nearest; error text if malformed or ≤ 0.
std::expected<Time, std::string> PeriodFromFrequency(std::string_view text);
```

Parsing: number = digits with optional `.` and digits; unit one of `Hz`, `kHz`, `MHz` (case-sensitive); `period = llround(1e12 / (value × multiplier))`; `period < 2` → error `"frequency too high"`.

### 5.2 — Catalogue entries

- `OSC` (`"crystal oscillator"`, `DIP-14`, `Chip`): requires exactly `freq`; errors `"OSC needs freq=<frequency>"`, `"OSC: unknown parameter '<name>'"`, or the `PeriodFromFrequency` text.
- `R` (`"resistor"`, `"axial"`, `Resistor`, `create = nullptr`).
- `catalogue_test`: skip `Resistor` kinds; create `OSC` with `freq=1MHz`.

### 5.3 — Tests: `tests/parts/oscillator_test.cpp`

- `"oscillator: PeriodFromFrequency"` — `1MHz` → 1'000'000; `14.31818MHz` → 69'841; `50Hz`; `2.5kHz`; errors: `""`, `"MHz"`, `"10"`, `"10GHz"`, `"0Hz"`.
- `"oscillator: period and duty"` — 1 MHz: high 500 ns, low 500 ns; odd period: construct `Oscillator` directly with 1001 ps → high 500, low 501.
- `"catalogue: OSC parameter errors"`.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"parts: phase 5 — oscillator and resistor"`

---

## Phase 6 — Netlist library: diagnostics and units

### 6.1 — Library

- `src/netlist/CMakeLists.txt`: `bw_netlist` → `PUBLIC bw_parts`, `PRIVATE bw_warnings`.
- Top-level `CMakeLists.txt`: `add_subdirectory(src/netlist)`. `tests/CMakeLists.txt`: link `bw_netlist`.

### 6.2 — `src/netlist/diagnostic.hpp/.cpp`

`Severity`, `Diagnostic`, `Format()` per design §6. `line == 0` → `"file: error: message"`.

### 6.3 — `src/netlist/units.hpp/.cpp`

- `ParsePeriodFromFrequency(text)` → `PeriodFromFrequency(text)` (phase 5).
- `ParseDuration(text)`: number (decimal allowed) + unit `ps`, `ns`, `us`, `ms`, `s`; result rounded to ps; must be ≥ 1 ps.

### 6.4 — Tests

- `tests/netlist/diagnostic_test.cpp`: `"diagnostic: format error and warning"`, `"diagnostic: line 0"`.
- `tests/netlist/units_test.cpp`: `"units: durations"` (`10ms`, `250ns`, `2s`, `1.5us`, `1ps`); `"units: duration errors"` (`10`, `ms`, `10min`, `0ns`); `"units: frequency delegates"` (`1MHz`).

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"parts: phase 6 — diagnostics and units"`

---

## Phase 7 — Netlist parser

### 7.1 — `src/netlist/parser.hpp/.cpp`

`PartDecl`, `PinRef`, `NetDecl`, `Netlist`, `ParseNetlist()` per design §6 and §8.1. Character-class checks are hand-written loops (no `<regex>`). Error messages:

| Case | Message |
|---|---|
| unknown first token | `unknown statement 'xyz'` |
| `part` with < 3 tokens | `part: expected 'part <ref> <part> [key=value ...]'` |
| bad reference | `bad reference 'u1': letters then digits, e.g. U1` |
| bad part name | `bad part name '74-LS00'` |
| bad parameter | `bad parameter 'freq': expected key=value` |
| `net` with < 3 tokens | `net: expected 'net <name> <ref>.<pin> ...'` |
| bad net name | `bad net name '1CLK'` |
| bad pin reference | `bad pin reference 'U1': expected <ref>.<pin>` |

### 7.2 — Tests: `tests/netlist/parser_test.cpp`

- `"parser: parts, params and nets"` — fields and line numbers.
- `"parser: comments and blank lines"` — `#` mid-line, empty lines, tabs.
- `"parser: <case>"` — one per error row above, exact message and line.
- `"parser: reports all errors"` — 3 bad lines → 3 diagnostics, in line order.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"parts: phase 7 — netlist parser"`

---

## Phase 8 — Builder: parts, pins, nets, power

### 8.1 — `src/netlist/builder.hpp/.cpp`

`LoadedCircuit`, `BuildCircuit()`, `LoadNetlist()` per design §6, §7 and §8.2–8.3 steps 1–4, 6–8, **without** resistors (step 5) and the unconnected-input warning (step 7, second half): `R` parts are rejected for now with `"R: resistors not supported yet"` (removed in phase 9). Error messages (all `Severity::Error`, line of the offending statement):

| Case | Message |
|---|---|
| duplicate reference | `U1 already placed at line 3` |
| unknown part | `unknown part '74LS999'` |
| factory error | `U1: <factory text>` |
| unknown reference | `unknown reference 'U9'` |
| unknown pin | `U3 (74LS00) has no pin 15` / `… has no pin XYZ` |
| no-connect | `U5.3 (NC3) is not connected inside 74LS20` |
| power pin | `U1.7 (GND) can only be on net GND` |
| pin on two nets | `U1.3 (1Y) is on nets A and B` |

### 8.2 — Tests: `tests/netlist/builder_test.cpp`

- `"builder: <case>"` for each error row (exact message and line).
- `"builder: parameter errors"` — `OSC` without `freq`, with `foo=1`, with `freq=10GHz` → the factory texts prefixed by `X1: ` (spec S3: missing, unknown, bad value).
- `"builder: reports all errors"`.
- `"builder: pin by number and by name are the same pin"` (`U1.3` then `U1.1Y` on another net → two-nets error).
- `"builder: net spans several lines"`.
- `"builder: power pins connect implicitly"` — `U1` pin 14 net is `VCC`; explicit `net VCC U1.14` accepted.
- `"builder: GND and VCC exist and are supply"` — after `powerOn`, values 0 / 1.
- `"builder: simulates"` — `74LS04` with `1A` on `GND`; after power-on and 100 ns, the `1Y` net is 1.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"parts: phase 8 — netlist builder"`

---

## Phase 9 — Builder: resistors, warnings, file loading

### 9.1 — Resistors and warnings in `builder.cpp`

- Remove the phase 8 `R` rejection. Step 2: `R` accepts only `value` (any text) → else `R1: unknown parameter 'x'`. Pins `1`/`2` only → else `R1 (R) has no pin 3`.
- Step 5 per design §8.3: error `R1: only pull-up/pull-down resistors are supported (one pin on GND or VCC, the other on a signal net)` at the resistor's `part` line.
- Step 7 warning (`Severity::Warning`, line of the chip's `part` statement): `U2.1 (1A) is unconnected: floats high`.

### 9.2 — `LoadNetlistFile(path)`

Reads the whole file (`std::ifstream`, binary); open failure → one diagnostic, line 0, `cannot read file`; file name in diagnostics = `path.filename().string()`.

### 9.3 — Tests (add to `builder_test.cpp`)

- `"builder: pull-up"`, `"builder: pull-down"` — net value with nothing else driving; a chip output overrides it.
- `"builder: resistor errors"` — both on rails, neither on a rail, one pin unconnected, bad pin, bad parameter.
- `"builder: unconnected input warning"` — exact text, circuit still built.
- `"builder: LoadNetlistFile missing file"`.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"parts: phase 9 — resistors, warnings, file loading"`

---

## Phase 10 — Sample netlist

### 10.1 — `netlists/counter.bwn`

Per design §10. Net names `CLK`, `RESET_N`, `C0`…`C7`, `CARRY`, `DIV`, `DIV_N`, `TOP_N`. All unused gate inputs of `U4` on `GND`; unused 74LS74 flip-flop 2: `2PRE_N`, `2CLR_N`, `2CLK`, `2D` on `GND`/`VCC` as needed so nothing is unconnected (no warnings).

### 10.2 — Tests: `tests/netlist/sample_test.cpp`

`tests/CMakeLists.txt`: `target_compile_definitions(bw_tests PRIVATE BW_NETLIST_DIR="${PROJECT_SOURCE_DIR}/netlists")`.

- `"sample: counter.bwn loads without warnings"`.
- `"sample: counter counts"` — power on, settle to 2.25 µs (between clock edges, which fall on multiples of 500 ns), read `C0..C7` as `n0`; run 100 µs (100 cycles); value = `(n0 + 100) mod 256`.
- `"sample: DIV toggles on each rising C7"` — record `C7` and `DIV`; every `DIV` change is 13 or 25 ns after a `C7` rise.
- `"sample: TOP_N"` — low exactly when `C6` and `C7` are high (checked at sample times between edges).
- `"sample: same as API-built circuit"` — build the same circuit with `FindPart(...)->create` and `Circuit::connect`/`tie`, same seed, record all nets by name; both recordings identical. Component creation order must match the netlist's `part` order, and Net creation order must match too (`GND`, `VCC`, then each net in order of first appearance): power-on draws random bits per output Pin and per Net in id order.

### Fence

- [ ] Build gate and test gate pass
- [ ] Commit: `"parts: phase 10 — sample netlist"`

---

## Phase 11 — `bw run` CLI

### 11.1 — `src/cli/CMakeLists.txt`, `src/cli/main.cpp`

`add_executable(bw main.cpp)` → `bw_netlist`, `bw_warnings`. Top-level `add_subdirectory(src/cli)`. Behaviour per design §9 and spec S4. Usage text:

```
usage: bw run <file.bwn> [--time 1ms] [--seed 1] [--record NET,NET... | --record-all] [--vcd FILE]
```

Output (stdout, one per line):

```
simulated_s 0.001
wall_s 0.012
ratio 0.083
applied_events 51234
evaluations 98765
```

### 11.2 — Tests

None automated (thin wrapper over tested code). Manual check below.

### Fence

- [ ] Build gate and test gate pass
- [ ] `./build/debug/src/cli/bw run netlists/counter.bwn --time 1ms --record-all --vcd counter.vcd` exits 0; `bw run` with no file exits 2; with a broken netlist exits 1 and prints `file:line: error: …`
- [ ] Manual: `counter.vcd` looks right in a viewer (spec "Done when")
- [ ] Commit: `"parts: phase 11 — bw run CLI"`

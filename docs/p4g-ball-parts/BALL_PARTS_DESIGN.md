# P4g Ball Parts — Detailed Design

Implements [BALL_PARTS.md](BALL_PARTS.md): the 74LS27, 74LS93 and 74LS107 Parts, and the tests of the bouncing ball. The Machine's netlist, Board, manuals and Puzzles are in [BOUNCING_BALL_DESIGN.md](../machines/060-bouncing-ball/BOUNCING_BALL_DESIGN.md).

Recipes followed: [ADD_PART.md](../recipes/ADD_PART.md) (all three Parts), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md), [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md). This design gives only what those recipes leave open.

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/parts/ls93.hpp, ls93.cpp         74LS93 Component
src/parts/ls107.hpp, ls107.cpp       74LS107 Component
datasheets/74LS27.md, 74LS93.md, 74LS107.md
tests/parts/ls93_test.cpp, ls107_test.cpp
tests/machines/ball_test.cpp
netlists/ball.bwn, manuals/ball/, boards/ball/, puzzles/*.bwp (Machine design)
```

The 74LS27 is a row in `src/parts/gate_package.cpp` (ADD_PART.md, "Gate package") and a row in `tests/parts/gate_package_test.cpp`. Everything else is small changes to existing files (§5).

## 2. 74LS27

In [gate_package.cpp](../../src/parts/gate_package.cpp):

```cpp
constexpr std::array<std::string_view, 14> s_ls27Names{"1A", "1B", "2A", "2B", "2C", "2Y", "GND",
                                                       "3Y", "3A", "3B", "3C", "1Y", "1C", "VCC"};
constexpr std::array<GateUnit, 3> s_ls27Units{{{GateKind::Nor, 12, {1, 2, 13}, 3},
                                               {GateKind::Nor, 6, {3, 4, 5}, 3},
                                               {GateKind::Nor, 8, {9, 10, 11}, 3}}};
```

- `s_specs` grows to 9, `{"74LS27", 14, s_ls27Names, s_ls27Units, delays::kLs27}` after `74LS20` (sorted).
- `delays::kLs27{10'000, 10'000}` (TI typicals, `tPLH` 10 ns, `tPHL` 10 ns).
- Catalogue: `PartInfo{"74LS27", "triple 3-input NOR", "DIP-14", Chip, &CreateGatePackage<"74LS27">}` between `74LS20` and `74LS30`.
- The `GatePackage` header comment lists 27 among the packages.
- `gate_package_test.cpp`: `constexpr GatePart kLs27{"74LS27", GateKind::Nor, 3, "ABC", delays::kLs27};` added to the parts the existing cases loop over.
- Datasheet `74LS27.md`: Function (three NOR gates, each output high only when its three inputs are all low), Pins (`1A`–`1C`, `2A`–`2C`, `3A`–`3C` inputs, `1Y`–`3Y` outputs, `VCC`, `GND`; note gate 1's third input is pin 13, its output pin 12).

## 3. 74LS93

```cpp
// src/parts/ls93.hpp
// 4-bit binary counter: ÷2 (CKA → QA) and ÷8 (CKB → QB..QD), falling edges; asynchronous R0
// (clear). Pins per datasheet: 1 CKB, 2 R01, 3 R02, 4 NC4, 5 VCC, 6 NC6, 7 NC7, 8 QC, 9 QB,
// 10 GND, 11 QD, 12 QA, 13 NC13, 14 CKA.
class Ls93 final : public Component {
public:
    explicit Ls93(std::string reference);
    static constexpr PinIndex kCkb = 0, kR01 = 1, kR02 = 2, kQC = 7, kQB = 8, kQD = 10,
                              kQA = 11, kCka = 13;

    void powerOn(PowerOnContext& ctx) override;  // qa_ random, eight_ 3 random bits
    void evaluate(EvalContext& ctx) override;

private:
    bool qa_ = false;
    std::uint8_t eight_ = 0;  // ÷8 section, QD QC QB as bits 2..0
    bool unknownA_ = false;  // ÷2 section is X
    bool unknownEight_ = false;  // ÷8 section is X
};
```

The [Ls90](../../src/parts/ls90.cpp) shape, without the set-9 inputs and with a plain ÷8:

```
r0 = AND(R01, R02)
if r0 == 1:          qa_ = 0, eight_ = 0, both known      delays: kLs93Clear
else if r0 == X:     both unknown
else:
    if fell(CKA) && !unknownA_:      qa_ = !qa_                    delays: kLs93CkaToQA
    if fell(CKB) && !unknownEight_:  eight_ = (eight_ + 1) & 7     delays: kLs93CkbToQB, …QC, …QD
drive QA, QB, QC, QD (X if unknown)
```

- Pins: `CKA`, `CKB` plain TTL inputs (not `risingEdgeOnly`); `NC4`, `NC6`, `NC7`, `NC13` `PinKind::NoConnect`; outputs `Output`/TTL; `VCC`, `GND` `Power`.
- The ÷8 is modelled as a counter whose outputs each take their datasheet delay from `CKB`: `QB` first, `QD` last, as the real chip's ripple. A `QD` fall reaches a linked `CKA` 34 ns after `CKB`, as it does on the Board (`BALL_QD`).
- An X clock edge is no edge (`fell` needs 1 → 0), as in `Ls90`; an unknown section stays X until a clear.
- `src/parts/delays.hpp`, TI typicals (ps): `kLs93CkaToQA{10'000, 12'000}`, `kLs93CkbToQB{10'000, 14'000}`, `kLs93CkbToQC{21'000, 23'000}`, `kLs93CkbToQD{34'000, 34'000}`, `kLs93Clear{26'000, 26'000}` (only fall used).
- Catalogue: `PartInfo{"74LS93", "4-bit binary counter", "DIP-14", Chip, &CreateChip<Ls93, "74LS93">}` after `74LS90`.
- `tests/parts/catalogue_test.cpp`: `kOffCornerPower{"74LS90", "74LS93"}`.
- Datasheet `74LS93.md`, in the shape of [74LS90.md](../../datasheets/74LS90.md): Function (two counters, falling edges: `CKA` → `QA` ÷2, `CKB` → `QB`–`QD` ÷8, `QB` lowest; wire `QA` to `CKB` for 0–15; `QD` falls when the ÷8 goes from 7 to 0, so it can clock another counter, or the chip's own `CKA`; `R01` and `R02` both high clear it without waiting for a clock; **`VCC` is pin 5 and `GND` pin 10**; pins 4, 6, 7, 13 not connected), Pins (`CKA`, `CKB`, `QA`, `QB`–`QD`, `R01`, `R02`, `NC4`, `NC6`, `NC7`, `NC13`, `VCC`, `GND`).

## 4. 74LS107

```cpp
// src/parts/ls107.hpp
// Dual JK flip-flop, clear, negative-edge clocked. Pins per datasheet: 1 1J, 2 1Q_N, 3 1Q, 4 1K,
// 5 2Q, 6 2Q_N, 7 GND, 8 2J, 9 2CLK, 10 2CLR_N, 11 2K, 12 1CLK, 13 1CLR_N, 14 VCC.
class Ls107 final : public Component {
public:
    explicit Ls107(std::string reference);
    void powerOn(PowerOnContext& ctx) override;  // each state random
    void evaluate(EvalContext& ctx) override;

private:
    std::array<LogicValue, 2> state_{};  // Zero, One or X
};
```

The [Ls74](../../src/parts/ls74.cpp) shape: a `kUnits` table of `{j, k, clk, clr, q, qn}` per flip-flop (`{0, 3, 11, 12, 2, 1}` and `{7, 10, 8, 9, 4, 5}`).

```
per unit:
    clr = read(CLR_N)
    if clr == 0:        state = 0
    else if clr == X:   state = X
    else if fell(CLK):  state = Next(readBefore(J), readBefore(K), state)
    drive Q = state, Q_N = Not(state), DelayFor(kLs107, …)
```

- `Next(j, k, q)`: with all three known, the JK table (0 0 → q, 1 0 → 1, 0 1 → 0, 1 1 → !q). Otherwise the outcomes over every 0/1 value of the X ones (at most 8, no allocation): one value if they all agree, else X. So `J` = X with `K` = 1 and `Q` = 0 gives X (clear → 0, or toggle → 1); `J` = X with `K` = 0 and `Q` = 1 gives 1 (hold or set).
- Pins: `CLK` plain TTL input (not `risingEdgeOnly`: the chip acts on falling edges); `J`, `K`, `CLR_N` TTL inputs; `Q`, `Q_N` outputs; `VCC`, `GND` `Power`.
- `src/parts/delays.hpp`: `kLs107{15'000, 15'000}` (TI typicals, `CLK` or `CLR_N` to `Q`/`Q_N`; TI gives the same 15 ns for both, said so in the commit).
- Catalogue: `PartInfo{"74LS107", "dual JK flip-flop, clear", "DIP-14", Chip, &CreateChip<Ls107, "74LS107">}` after `74LS08` and before `74LS161` (sorted as strings: "74LS10" < "74LS16").
- Datasheet `74LS107.md`: Function (two flip-flops, each changing only when its clock **falls**, from the `J` and `K` it had just before: both low keeps it, `J` high sets `Q`, `K` high clears it, both high toggles; `CLR_N` low clears it at once and holds it; `Q_N` is always the opposite of `Q`; there is no preset), Pins, every name spelled out for the Datasheet check (`1J`, `1K`, `1CLK`, `1CLR_N`, `1Q`, `1Q_N`, `2J`, `2K`, `2CLK`, `2CLR_N`, `2Q`, `2Q_N`, `VCC`, `GND`).

## 5. Integration Points

1. **Catalogue, CMake** — §2–§4; `ls107.cpp`, `ls93.cpp` in `src/parts/CMakeLists.txt` (sorted). All three are DIP-14: no new footprint.
2. **Machine name `ball`** — `LoadMachine` finds `netlists/ball.bwn` and `boards/ball/ball.kicad_pcb` by name (as every Machine since P4a).
3. **Per-Machine loops** (ADD_MACHINE.md step 3) and the manuals list (step 2) gain `"ball"`.
4. **Recipes** — ADD_PART: the 74LS107 as the falling-edge JK reference (`fell`, `J`/`K` read before the edge, X through the enumerated table), the 74LS93 in the off-corner list; ADD_MACHINE: reference commits for a TV Machine with new Parts (this step).

## 6. Reused Infrastructure

- `GatePackage`, `GateFunction`, `DelayFor`, `CreateChip`, `EvalContext::fell` (P4f), the `Ls90` structure for the 74LS93, the `Ls74` structure for the 74LS107.
- The TV on `J2` (P2c), `TvInput` and `TvFrame` for headless picture tests, `ConnectorNet`, `PinNet`, `LogicProbe` (as in `test_pattern_faults_test.cpp`).
- Faults: stuck outputs, Broken Traces placed by pads (P4e), Solder Bridges (P3f).

## 7. Build Integration

```cmake
# src/parts/CMakeLists.txt: + ls107.cpp ls93.cpp
# tests/CMakeLists.txt:     + machines/ball_test.cpp parts/ls107_test.cpp parts/ls93_test.cpp
```

No new dependencies, no new library edges.

## 8. Testing

Catch2, tags as the module.

### 8.1 Parts

| File | Checks |
|---|---|
| `tests/parts/gate_package_test.cpp` | `kLs27` in the looped parts: pin names and kinds, every input combination of each gate, rise and fall delays |
| `tests/parts/ls93_test.cpp` `[parts]` | catalogue row, pins (`VCC` pin 5, `GND` pin 10, four NC); `QA` toggles on `CKA` falling only; `QB`–`QD` count 0..7 on `CKB` falling; with `QA` → `CKB` linked in the Bench: sixteen falls give 0..15, 0; on 7 → 0 `QB` changes first and `QD` last (`kLs93CkbToQB` < `…QD`); R0 both high clears asynchronously (one high: no clear), and a `CKB` fall while R0 is high counts nothing; `QD` → `CKA` linked (the ball's wiring): `QA` rises on the 8th `CKB` fall after a clear; R01 at X with R02 high → outputs X, a clean clear recovers; X clock edge counts nothing. Copy `ls90_test.cpp` |
| `tests/parts/ls107_test.cpp` `[parts]` | catalogue row, pins; for each flip-flop: 1 0 sets, 0 1 clears, 0 0 holds, 1 1 toggles, each on the falling edge only (a rising edge changes nothing); `J` changed at the falling edge's timestamp is not seen (`readBefore`); `CLR_N` low clears at once and wins over a clock; delays `kLs107`; `J` at X with `K` 0 and `Q` 1 → stays 1, with `Q` 0 → X; `CLR_N` at X → X, then a clear recovers; the two flip-flops are independent |
| `tests/parts/catalogue_test.cpp` | `74LS93` in `kOffCornerPower` |

### 8.2 The bouncing ball

`tests/machines/ball_test.cpp` (copy the shape of `test_pattern_faults_test.cpp`). Helpers:

- `Watch(LoadedCircuit&, seed, Time from, Time to, onFrame, probes)`: runs from power-on in 1 ms steps (shorter than a frame: `Tv` keeps only the last completed frame), and each time `completedFrames()` grows and the frame started at or after `from`, calls `onFrame(const TvFrame&)` so the frame is read at once, not stored (a frame is 640 kB; 70 of them under ASan is too much). Returns the probes' lamps at the end.
- `Picture Read(const TvFrame&)`: the box from the frame itself: the side walls are the outermost lit 16-column runs that are lit on every row between the top and bottom walls; the top and bottom walls are the 16-row runs lit across the whole box width. The net is read only from its own columns (dots 48–49 → the 8 columns starting 176 TV columns (44 dots) after the inside edge of the left wall): which box rows are lit. `ball`: the bounding box of the cells lit (≥ 0.5) inside the box that are not wall or net cells, if any. One dot is 4 TV columns (0.5 µs / 125 ns), one line one row.
- `Track`: the ball's position per frame, `nullopt` when hidden. A ball is hidden for one frame at each bounce: one step (8 lines) inside the top or bottom wall, since the vertical reload comes before the hit; at dots 2–3 or 92–93 inside a side wall. Steps are taken between consecutive visible frames; across a hidden frame the step is twice as long. A turn is paired with the last visible position next to the wall.
- Until the Board exists (phase 4) the fault-free circuit is `LoadNetlistFile("ball.bwn")`; from phase 5 the reference is `LoadMachine` on `machine ball`, as `FaultFree()` there.

| Case | Checks |
|---|---|
| `ball: netlist loads without warnings` | `LoadNetlistFile` diagnostics empty |
| `ball: Puzzles have a title and a brief` | the five files |
| `ball: box and dashed net` (seeds 1, 2) | a frame after 100 ms is locked; side walls 16 columns wide, 352 columns apart (88 dots) on the inside; top and bottom walls 16 rows; the net 8 columns wide at dots 48–49, lit on 4 rows in 8 down the box; same box for both seeds |
| `ball: one ball moving diagonally` `[slow]` (seeds 1, 2) | the ball may stay out of sight for up to a second after power-on; then over 10 consecutive visible frames away from the walls: 8 × 8 cells, moving 8 columns and 4 rows per frame (signs constant between bounces) |
| `ball: bounces stay in the box` `[slow]` (seed 1) | over 0.2–1.6 s: the horizontal and vertical steps each change sign at least once, each turn paired with the ball last seen touching the wall it turns from; the ball is never seen outside the box |
| one case per Puzzle (BOUNCING_BALL.md, Puzzles), Symptom then Repair | `wide_net`: the net drawn 16 dots wide, probe `U12.3Y` pulses and `U15.2A` shows nothing (`"-"`, floating), mend → 2 dots; `wraparound` `[slow]`: probe `U26.1Q` `HIGH`, the ball's horizontal step always +8 columns over its run and it is seen both at the right wall and back at the left wall, swap `U26` → a −8 step; `stripe`: no 8-row ball, a lit 8-column bar from the top wall to the bottom wall, swap `U23` → a ball; `jumping_ball`: some vertical step between visible frames is neither ±4 nor ±8 rows, or the ball is missing from frames away from the walls, probe `U20.LOAD_N` shows nothing (`"-"`), mend → only ±4 steps; `net_bounce` `[slow]`: the ball is not seen right of the net over a window after its first meeting with the net, cut the bridge → it is |

Debug runs at about 0.03× real time on this Machine (0.086× measured on the 050 test pattern, about 3× the events here): some 30–35 s per simulated second. The cases above simulate about 10–12 s in all, some 6 CPU-minutes, about 2 minutes of wall time with `catch_discover_tests` running cases in parallel. Times and seeds are set from a first run (a seed whose ball reaches the net early, etc.) and written in the plan's notes. If the suite grows by more than about two minutes, the `[slow]` cases check the flip-flop and hit Nets with probes over shorter runs instead of whole crossings.

### 8.3 Session, Board, release

| File | Checks |
|---|---|
| `tests/session/session_test.cpp` `[session][slow]` | `session: ball with every instrument`: 8 Clips (`U23` `QA`–`QD`, `U26` `1Q`, `2Q_N`, `U25` `1Y`, `2Y`), 2 Hooks (`U19.RCO`, `U24.3Y`), probe, meter lead; `powerOn(1, 1_s)`, `waitIdle`; the TV picture locked; nothing aborts (watch budget) |
| existing loops | `boards_test`, `electrical_test`, `board_scene_test`, `manuals_test` gain `"ball"`; `drc_ball` |

UI check (release build): `bw_ui puzzles/net_bounce.bwp --screenshot /tmp/ball.png --after 1s` shows the box, the dashed net and the ball; the status line stays 1.00× with every instrument attached (`--probe`, `--clip`, `--hook`, `--meter`). The Machine has about 2.5–3× the 050 test pattern's events per second (twice the dot rate, more counters, the 74LS107s evaluated on both `CLK` edges); 050 runs at 5.4× in release, so this one should run near 1.8×: `bw run netlists/ball.bwn --time 2s` in release is timed as soon as the netlist exists (phase 4), before the Board is laid out.

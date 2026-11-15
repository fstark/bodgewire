# P5u BW-80 chips — Detailed Design

Implements [BW80_CHIPS.md](BW80_CHIPS.md): the series resistor (a network Pin kind in the kernel, a `ResistorNetwork` Component, the builder's rules, the Board's electrical netlist), the `2332`, four TTL Parts and a bench. No Machine, no UI change beyond the Net panel's role for a series resistor.

Recipes: [ADD_PART.md](../recipes/ADD_PART.md) (Files table, Which kind, rules; each Part below names only what differs). All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

Sources, cited by these short names:

- **ZX80**: Grant Searle's redrawn ZX80 circuit (`zx80.pdf`, issue 2), its parts list and "Circuit operation" (searle.x10host.com/zx80/zx80.html); **ROMPIN**: his ROM pinout figure (`ROMPinouts.gif`: 2732, the ZX80's ROM, 2532).
- **MOS2332**: MOS 2332 4K mask ROM data sheet (6502.org `documents/datasheets/`); where it cannot be fetched, the 2316B's values (same family and process), said so in the commit.
- **TI373**, **TI365**, **TI05**, **TI10**: TI SN74LS373, SN74LS365A, SN74LS05, SN74LS10 data sheets, typical values (TI PDFs can't be fetched: ADD_PART, rules).
- **P5i**: [SWITCHES_DESIGN.md](../p5i-switches/SWITCHES_DESIGN.md) §1–2, the switch network this step mirrors.

---

## 1. Kernel — `src/sim/`

The switch network (P5i) already gives one Component a Pin on each of its Nets that drives the Net, reads nothing through the Net's value, and is woken when the Net's *other* drives change (`external`, `externalChanged`). The series resistor needs the same thing at Weak strength. So the kernel's "switched Net" becomes a "network Net", with two Pin kinds.

### 1.1 Pin kind — `logic.hpp`

- `PinKind::Joined`, after `PullUp`. Comment: "a Pin of the resistor network (P5u): drives its Net at Weak strength, reads nothing through the Net's value; its Component is woken when the Net's other drives change, as Switched."
- `IsDriver`: true for `Joined`. `IsReader`: false.
- `DriveOf`: `Joined` → `{value, Z ? None : Weak, family}`.
- `constexpr bool IsNetworkPin(PinKind kind)`: `Switched` or `Joined`.

### 1.2 Circuit — `circuit.{hpp,cpp}`

- `netSwitchPin_` → `netNetworkPin_`, `switchPin(net)` → `networkPin(net)`, and `netNetworkPin2_`, `secondNetworkPin(net)`: a Net holds at most one Switched and one Joined Pin (a key on a joined Net: the user's rule, Spec). `buildTables` tests `IsNetworkPin(info.kind)`; two of one kind on a Net `Check`s "circuit: two network pins of one kind on one net".

### 1.3 Simulator — `simulator.{hpp,cpp}`, `component.hpp`

- `NetState::switched` → `networked`; `External` becomes per network Pin: `external_` for a Net's first, `external2_` for its second, each the Net's drives but that Pin's own. `EvalContext::external` and `externalChanged` pick by the Pin (`secondNetworkPin(net) == pin`).
- `externalDrives(net, own)` leaves out `own` only; `runUntil` updates each network Pin's view through `updateExternal`, waking its owner when that view changed (other drives only, no echo; on a Net with both, each wakes on the other's drive, and they settle in one exchange: the switch network passes the merged strong drive, the resistor network then sees a source there and drives Z). `resolveNet` keeps its fast path for a Net with one network Pin and takes the general path (every Driver) for a Net with two.
- `powerOn`: `Joined` is released at power-on like `Switched` (no random draw).
- `EvalContext::external`, `externalChanged` (`component.hpp`): "Switched and Joined Pins only".
- `tests/sim/switched_test.cpp` keeps its cases; a new `tests/sim/joined_test.cpp` checks `DriveOf(Joined, …)` (Weak, None at Z), `IsDriver`, `!IsReader`, `IsNetworkPin`, and that a Joined Pin's own drive does not wake its Component while another driver's does (the P5i wake test, copied with the kind changed).

Every other `PinKind::Switched` site (`src/parts/switch.cpp`, the switched test) is unchanged.

## 2. `ResistorNetwork` — `src/parts/resistor_network.{hpp,cpp}` (new)

```cpp
// ps: the network drives a Net this long after a change of another Net in its group.
inline constexpr Time kJoinDelay = 1;

// The Circuit's series resistors as one Component (P5u): a Joined Pin on each Net a series
// resistor is on, named after the Net. A group (Nets linked by series resistors) is resolved as
// a whole: a Net with any drive of its own is a source; every other Net takes the value of its
// nearest source or pull resistor, nearest counted in ohms, at Weak strength. Equal distances
// that disagree give X; no source in reach gives Z (the Net floats).
class ResistorNetwork final : public Component {
public:
    explicit ResistorNetwork(std::span<const std::string> netNames);
    void join(PinIndex a, PinIndex b, std::uint32_t ohms);           // builder: a ≠ b
    void pull(PinIndex pin, LogicValue value, std::uint32_t ohms);   // builder: Zero or One

    void powerOn(PowerOnContext& ctx) override;  // driven values back to Z, resolve all next
    void evaluate(EvalContext& ctx) override;

private:
    void build();                               // groups, once, at the first power-on
    void resolve(EvalContext& ctx, std::uint32_t group);
    ...
};
```

### 2.1 Groups

Built once, at the first power-on, as `SwitchNetwork::rebuild` does (union-find over Pins, then `order_`/`start_` per group), but static: series resistors never open. Per Pin: its links, each edge stored in both directions (`{other Pin, ohms}`, CSR arrays `linkStart_`, `links_`), and its pulls (`{value, ohms}`, the same shape). Every array is sized in `build`, so `resolve` allocates nothing. `powerOn` (every power-on: a bench or a Session powers the same Circuit again) sets the last driven value of each Pin back to Z, as the kernel releases Joined Pins, and makes the next evaluation resolve every group.

### 2.2 Resolution

`evaluate`: after a power-on, resolve every group. After that, resolve each group in which a Pin has `ctx.externalChanged(pin)`, as `SwitchNetwork::evaluate`.

`resolve(group)`, over the group's Pins, with per-Pin scratch `distance_` (`std::uint64_t`, ohms; `kFar` = none) and `value_` (0, 1 or X):

1. **Sources**: a Pin whose `ctx.external(pin).strength()` is not `None` (a chip output, a port, a stuck pin, a switch: anything but this network) gets distance 0 and the value `external(pin).finish(false, Zero).value`.
2. **Pulls**: every other Pin starts at its nearest pull (its smallest ohms; two pulls at the same ohms that disagree give X), or `kFar`.
3. **Relax** (Bellman–Ford, at most group size − 1 passes, stopping early when no distance or value changed, a turn to X counting as a change): for each link `a`–`b` with `a` not at `kFar` and `b` not a source, the candidate `distance_[a] + ohms` with `value_[a]` replaces `b`'s when shorter; at an equal distance with another value, `b` becomes X.
4. **Drive**: a source Pin drives Z (its Net keeps its own drivers); any other Pin drives `value_` (or Z at `kFar`) with `kJoinDelay`. A Pin whose value has not changed is not driven again (keep the last driven value per Pin, as the 74LS244's `driven_`).

Limitations, in the class comment and the `R` datasheet's spirit: any drive of a Net's own makes it a source at distance 0, a weak one included (an NMOS port's pull-up 1, a `PullUp` Pin), so through a 1k it would beat a strong output on the far side; the ZX80 has no such case. When a source lets go of its Net, the Net floats for `kJoinDelay` (1 ps) before the network drives it: a TTL input reads 1 for that picosecond. Inertial delays swallow it on data inputs; an edge-sensitive input on a joined Net would see it (P5v: HSYNC through R24).

This is "the lowest resistance to a driven point wins", a stand-in for the voltage divider; it gives the ZX80's cases their real outcome (Spec, the `D6'` example) and an X where a real divider would sit near the threshold. A source's own value is the merged drives of its Net, so contention there (X) spreads as X. The network never reads the Net's value, so a Net it drives Weak is not floating, as with a pull-up.

### 2.3 Speed

Each group is a pair of Nets on the ZX80 (the data bus), so a change on `D3` costs one wake of the network, one pass over a group of two, and one drive of `D3'`. The bench measures it (§6, Release); the cost on P5v's full circuit is P5v's measure.

## 3. Builder — `src/netlist/builder.cpp`, `src/netlist/units.{hpp,cpp}`

### 3.1 Resistance — `units.{hpp,cpp}`

`std::expected<std::uint32_t, std::string> ParseOhms(std::string_view text)`: digits, then optionally one of `R`, `k`, `K`, `M` and more digits, the letter standing for the decimal point and the multiplier: `470`, `470R`, `4R7` (rounded to 5), `1k`, `1K`, `2k2`, `47K`, `1M`. Error: `"bad resistance '<text>': expected e.g. 470, 1k, 4k7 or 1M"`. Every resistor value in the repo's netlists (`330`, `1k`, `2k4`, `3k3`, `4k7`, `10k`) parses; `tests/netlist/units_test.cpp` checks them, the forms above and the errors.

### 3.2 Resistor kinds — `tieResistors` becomes `wireResistors`

Each `R` with both pins connected is, by its two Nets:

| Nets | Kind | Today |
|---|---|---|
| one rail, one signal | pull | tied, as now, unless the signal Net is joined (below) |
| two different signals | **series** | error |
| both rails | error: `"R5: a resistor between two rails"` | error |
| one Net | error: `"R5: both ends on net D3"` | error |
| an end unconnected | error: `"R5: a resistor needs both pins connected"` | error |

The old message ("only pull-up/pull-down resistors are supported…") goes.

A Net that a series resistor touches is **joined**. Then:

- every resistor needs `value=`, parsed by `ParseOhms`: errors `"R5: a resistor needs value= (ohms)"` and `"R5: <ParseOhms error>"` (every netlist in the repo has one; eight test strings gain one);
- a pull on a joined Net is not tied (`circuit.tie`): it goes to the network as `pull(pin, One or Zero, ohms)`, so the network sees every weak source of its Nets;
- one `ResistorNetwork` is added (`circuit.add<ResistorNetwork>(names)`), a Joined Pin connected to each joined Net in NetId order (as `wireSwitches`), then `join` per series resistor.

Pulls on Nets that are not joined are tied as today.

### 3.3 Switches on joined Nets

Allowed (§1.2): `wireSwitches` and `wireResistors` each add their Pin to the Net. Test in `tests/parts/resistor_network_test.cpp`: a key joins an inverter's 0 to a Net that a 1k joins to a 47k-pulled Net; key up both read 1, key down both 0, up again 1.

### 3.4 Loaded parts

`loaded_->parts` keeps a resistor's `value` text as now (the Net panel shows it).

## 4. Board — `src/machine/electrical.cpp`, `src/netlist/net_pins.{hpp,cpp}`

### 4.1 Electrical netlist

`ElectricalNetlist` keeps a resistor while its two ends are on named islands that are **different** and **not both rails** (today: exactly one rail). So a series resistor stays between its two islands; with a bridge across it (both ends on one island) or both ends on rails it is left out (`leftOut`), as a cut pull-up is today. `NameIslands` is unchanged (resistors never name islands). Tests in `tests/machine/electrical_test.cpp`: on a small Board (§4.3), a series resistor between two signal islands is kept, and left out once its two islands are bridged.

### 4.2 Net panel — `net_pins.{hpp,cpp}`

`PinRole::Series`: a resistor end whose other end is on a signal Net. `DescribeNet` gives it with the resistor's value, in `drivers` (it drives the Net, weakly, as a pull does), so `board_scene` marks its pad as a Driver with no change (its `drivers` comment says "pull and series resistors"). `FormatNetPin`: `R4 pin 1 · 1k series`. `tests/netlist/net_pins_test.cpp`: a series resistor listed on both its Nets as `Series`.

### 4.3 Test Board

`tests/data/series_bench.bwn` (new): one 74LS04 (`U1`) driving `A` (1Y) whose `R1 value=1k` joins `B`, which goes to `U1.2A`; `R2 value=47k` pulls `B` up; `POWER`. No KiCad Board: the electrical tests load real Boards from `boards/` (`LoadDesign`, which reads only `netlists/`), and a new one would need the user's layout, so this test has its own loader for `tests/data/series_bench.bwn` and builds its `Board` in code, copying the pattern of `tests/machine/crack_test.cpp`'s `Joined()` (anonymous there): one `BoardComponent` per Part with a pad for every listed pin (J1's pads 1 and 2, which `NameIslands` presets to VCC and GND; both pads of R1 and R2; U1's used pins; power pins may be left out), each Net a row of pads on one front `Trace`, and the bridge as a `BodgeWire` between R1's two pads (a Solder Bridge).

## 5. Parts

### 5.1 `2332` — `src/parts/rom.cpp`, `delays.hpp`, `catalogue.cpp`

A ROM spec row (ADD_PART, Which kind: ROM), `selectable` as the 2316B. Pinout ROMPIN and MOS2332 (the ZX80's socket, ZX80 IC2):

| Pins | Names |
|---|---|
| 1–8 | `A7`–`A0` |
| 9–11, 13–17 | `O0`–`O7` |
| 12, 24 | `GND`, `VCC` |
| 18, 19, 22, 23 | `A11`, `A10`, `A9`, `A8` |
| 20, 21 | `CS1`, `CS2` |

Address list (bit 0 first, index = pin − 1): `7, 6, 5, 4, 3, 2, 1, 0, 22, 21, 18, 17`. `s_specs` grows from 3 to 4 rows. `select=` checking in `CreateRom` (catalogue.cpp) builds its messages from the number of enables: `"2332 needs select=<CS1 CS2: L or H each>, e.g. select=LH"`, `"2332: bad select 'LLH' (two of L or H)"`, the 2316B's unchanged; `catalogue_test` passes one `L` per enable to every selectable ROM. Data: the 2716's. Enables in the order `CS1` (index 19), `CS2` (index 20), so `select=` reads `CS1` then `CS2`. Delays (MOS2332, maximums as for the 2316B): `k2332Access` 450 ns, `k2332Select` 120 ns, `k2332Float` 100 ns. Catalogue: `PartInfo{"2332", "4K x 8 mask ROM", "DIP-24", Chip, &CreateRom<"2332">}`. Datasheet `datasheets/2332.md` after `2316B.md`: the ZX80's parts list calls it 8332; `select=LL` on the ZX80, `CS2` on A12. Tests in `tests/parts/rom_test.cpp`, as the 2316B's: pin names, the four `select=` combinations each selecting only at its levels, the three delays, the 4096 words of a `.rom` and a 4096-byte `.bin`, errors (`select=` missing, of length 3, with a letter other than `L`/`H`).

### 5.2 `74LS05` — gate package

`src/parts/gate_package.cpp`: `s_ls05Units` = six `GateKind::Not` units on the 74LS04's pins (`s_ls04Names`), the row `{"74LS05", 14, s_ls04Names, s_ls05Units, &EvaluatePackage<s_ls05Units, delays::kLs05>, true}` (open collector, as the `7417`); `s_specs` grows from 11 to 13 rows, `gate_package.hpp`'s list of parts gains them. Tests in `tests/parts/gate_package_test.cpp`: `GatePart` rows `kLs05`, `kLs10` with truth table and delays, and "74LS05 releases its 1" copied from the 7417's. `delays::kLs05{17'000, 15'000}` (TI05, R_L 2 kΩ). Datasheet: six inverters that can only pull down; on the ZX80 eight of them force a NOP onto the data bus and two in parallel drive the CPU clock with R20.

### 5.3 `74LS10` — gate package

`s_ls10Units`: three 3-input `Nand` units on the 74LS27's pins (`s_ls27Names`: 1A 1, 1B 2, 1C 13, 1Y 12; 2A 3, 2B 4, 2C 5, 2Y 6; 3A 9, 3B 10, 3C 11, 3Y 8). `delays::kLs10{9'000, 10'000}` (TI10, the 74LS00's).

### 5.4 `74LS365` — `src/parts/ls365.{hpp,cpp}` (new)

Hex buffer, three-state, outputs on while `G1_N` **and** `G2_N` are low. Pins (TI365): 1 `G1_N`, 2 `A1`, 3 `Y1`, 4 `A2`, 5 `Y2`, 6 `A3`, 7 `Y3`, 8 `GND`, 9 `Y4`, 10 `A4`, 11 `Y5`, 12 `A5`, 13 `Y6`, 14 `A6`, 15 `G2_N`, 16 `VCC`. The class follows `Ttl125` (one `driven_` per buffer, choosing the data, enable or disable delay as the 74LS244), with one enable for all six: X on either enable while the other is low gives X on the outputs. Delays (TI365): `kLs365Data{10'000, 9'000}`, `kLs365Enable{19'000, 24'000}` (tPZH, tPZL), `kLs365Disable{15'000, 20'000}` (tPHZ, tPLZ). Test `tests/parts/ls365_test.cpp` with `chip_bench.hpp`: the enable table (only both low drives), data follows, X cases, the three delays.

### 5.5 `74LS373` — `src/parts/ls373.{hpp,cpp}` (new)

Octal transparent latch, three-state. Pins (TI373): 1 `OE_N`, 2 `1Q`, 3 `1D`, 4 `2D`, 5 `2Q`, 6 `3Q`, 7 `3D`, 8 `4D`, 9 `4Q`, 10 `GND`, 11 `LE`, 12 `5Q`, 13 `5D`, 14 `6D`, 15 `6Q`, 16 `7Q`, 17 `7D`, 18 `8D`, 19 `8Q`, 20 `VCC`. DIP-20 exists (74LS244).

- State: the eight latched bits, random at power-on (ARCHITECTURE, power-on state).
- `evaluate`: while `LE` is 1, each bit follows its `D` (the `D`s are not `sampled`: they act while `LE` is high); when `LE` falls (`ctx.fell`), each bit keeps `readBefore(D)`, the value just before the edge (hold time 10 ns, setup 0, TI373; the model has no window); `LE` at X makes a bit whose `D` differs from it X. Outputs: the bits while `OE_N` is 0, Z while 1, X at X.
- Delays (TI373): `kLs373Data{12'000, 12'000}` (D → Q while transparent), `kLs373Latch{20'000, 18'000}` (LE high → Q), `kLs373Enable{15'000, 25'000}` (tPZH, tPZL), `kLs373Disable{15'000, 12'000}` (tPHZ, tPLZ). The delay is chosen by what changed, as the 74100 (`ttl100.cpp`) and the 74LS244 do: the output enable when `OE_N` moved, else LE when `LE` rose, else data.
- Test `tests/parts/ls373_test.cpp` with `chip_bench.hpp`: transparent, hold on the fall, ignoring `D` while latched, outputs off and back on with the latched value, X on `LE`, each delay, `D` pins not sampled.

### 5.6 Catalogue, datasheets, tests

Each new Part: a row in `s_parts` (sorted by name), `datasheets/<name>.md` per the Files table, its test file in `tests/CMakeLists.txt` (sorted). `74LS373` (DIP-20) is a grid package already (`IsGridPackage`); `74LS365` DIP-16, `74LS05`/`74LS10` DIP-14. `datasheets/R.md`'s Function gains the series case: "Between two signal lines (the ZX80's data bus), it keeps them apart: a line that nothing else drives takes the other's value, weakly, while a chip driving a line still wins on its own side. The value (`value=`, ohms: `470`, `1k`, `4k7`) is then required, and decides which of two weak pulls wins."

## 6. Bench — `tests/data/bw80_bench.bwn`, `roms/bw80_bench.asm`, `tests/netlist/bw80_bench_test.cpp`

### 6.1 Circuit

The ZX80's CPU, memory and data bus at their reference designators (ZX80), with what the bench leaves out said in its header comment: no video (IC6–IC9, IC11's VSYNC latch, IC19–IC21: P5v), so the ROM's address comes straight from `A0`–`A11` and not through the 74LS157s; and one bench-only gate, IC17's fourth OR, giving `ROM_CS_N` = `A14` + `MREQ_N` (on the ZX80 that select comes through IC6, P5v). Every unused TTL input is tied to `GND` (the bench test checks there are no warnings). Nets:

| Part | Wiring |
|---|---|
| `X1 OSC freq=6.5MHz` | `DOT` |
| IC18 74LS74 | 2CLK ← `DOT`, 2D ← `CLK_Q_N`; 2Q → `CLK_Q`, 2Q_N → `CLK_Q_N`; 2PRE_N, 2CLR_N ← VCC; flip-flop 1's inputs tied |
| IC15 74LS05 | units 5, 6: `CLK_Q_N` → `CLK` (outputs paralleled); unit 1: `NOP_N` → `NOP_EN`; units 2, 3: `NOP_EN` → `D6`, `D7`; unit 4 tied |
| R20 330 | `CLK` pull-up (the CPU clock) |
| IC1 Z80 | `CLK`; `RESET_N`; `INT_N` ← `A6` (as the ZX80); `NMI_N`, `WAIT_N`, `BUSRQ_N` ← R12, R22, R23 (1k pull-ups); A, D, strobes on their Nets |
| `RST1 POR time=1ms`, R21 220k | `RESET_N` ← `RST1.RST_N`, pulled up (bus_load's reset; a reference is letters then digits) |
| IC2 2332 `select=LL contents=../../roms/bw80_bench.rom` | `A0`–`A11`; `CS1` ← `ROM_CS_N`, `CS2` ← `A12`; `O0`–`O7` → `D0'`–`D7'` |
| IC3 2114 `access=300ns` | `A0`–`A9`; `CS_N` ← `RAM_CS_N`; `WE_N` ← `WR_N`; IO1–IO4 = `D7'`, `D1'`, `D0'`, `D2'` (ZX80) |
| IC4 2114 `access=300ns` | the same, IO1–IO4 = `D4'`, `D3'`, `D5'`, `D6'` |
| R4–R11 1k | `D4'`–`D4`, `D3'`–`D3`, `D5'`–`D5`, `D6'`–`D6`, `D2'`–`D2`, `D7'`–`D7`, `D0'`–`D0`, `D1'`–`D1` |
| R28 47k | `D6'` pull-up |
| IC13 74LS04 | `MREQ_N` → `MREQ`; `LE_N` → `LE`; `CH6` → `CH6_N`; three units tied |
| IC12 74LS00 | unit 3: `MREQ`, `A14` → `RAM_SEL_N`; three units tied |
| R19 2k2 | `RAM_SEL_N`–`RAM_CS_N` |
| IC16 74LS10 | unit 1: `RFSH_N`, `CLK_Q_N`, `MREQ` → `LE_N`; unit 2: `CH6_N`, `A15`, `HALT_N` → `NOP_COND_N`; unit 3 tied |
| IC5 74LS373 | `OE_N` ← GND, `LE` ← `LE`; 1D–8D = `D4'`, `D5'`, `D0'`, `D6'`, `D7'`, `D1'`, `D2'`, `D3'` (ZX80); 1Q–8Q = `CH4`, `CH5`, `CH0`, `CH6`, `CH7`, `CH1`, `CH2`, `CH3` |
| IC17 74LS32 | `RD_N` + `IORQ_N` → `IORD_N`; `IORD_N` + `A0` → `KBD_N`; `NOP_COND_N` + `M1_N` → `NOP_N`; `A14` + `MREQ_N` → `ROM_CS_N` (bench only) |
| IC14 74LS05 | six units: `NOP_EN` → `D0`–`D5` |
| R3 1k | `NOP_EN` pull-up |
| IC10 74LS365 | `G1_N`, `G2_N` ← `KBD_N`; A1 `EAR` → Y1 `D7`; A3 `KB0` → `D0`, A4 `KB1` → `D1`, A5 `KB2` → `D2`, A2 `KB3` → `D3`, A6 `KB4` → `D4` (ZX80) |
| R13–R17 47k | `KB0`–`KB4` pull-ups |
| R1 1k | `EAR` pull-down |
| K1, K2 KEY | `A8`–`KB0`, `A9`–`KB1` (no diodes: Spec) |
| J1 POWER | |

The ZX80's eight NOP gates are spread over IC14 and IC15 in an order this design does not pin down (the schematic's crossing lines); the bench gives IC14 `D0`–`D5` and IC15 `D6`, `D7`, and P5v takes the exact order from the schematic.

**NOP timing** (an M1 fetch from `$C1xx`, ns after T1↑, Z80A at 3.25 MHz, T = 307.7 ns), on the model: the address is out at 110 while the RAM is deselected; `MREQ_N` falls at T1↓ + 85 = 239, `RAM_CS_N` ≈ 259 (IC13, IC12, R19), and the 2114 model, timing its output from the input that changed, gives the data at 259 + 120 = 379 whatever its `access=` (the 2332 model alike). `CLK` lags `CLK_Q_N` by the 74LS05 (15–17 ns), so `LE` (high while `CLK` is low, `RFSH_N` high and `MREQ_N` low) rises at about T2↓ − 5 ≈ 456 + IC16 and IC13 ≈ 476, and the latch passes `MD6`: `CH6` ≈ 488, `CH6_N` 498, `NOP_COND_N` 508, `NOP_N` 522, `NOP_EN` 539, `D0`–`D7` low at ≈ 554. The Z80 samples at T3↑ = 615: about 60 ns to spare. A mutation with the 450 ns grade still passes, so the bench cannot show the real tAA margin: `access=300ns` is for realism, and the real check (the address settling while selected, or a model change) is P5v's. The real ZX80's IC4 is a 2114-1.

### 6.2 Program — `roms/bw80_bench.asm`

`.cpu z80`, `.rom $0000, $1000`, at `$0000`, assembled by `bw asm` into `roms/bw80_bench.rom` (the ROM test over `roms/*.asm` checks it is up to date). RAM at `$4000`–`$43FF`, mirrored to `$C000`. Results at:

| Address | Meaning |
|---|---|
| `$4000` `STATUS` | `$55` all passed; `$E1` RAM, `$E2` NOP line, `$E3` read of the line not forced, `$E4` ROM read after the ROM write |
| `$4001` `KEYS8` | last `IN A,(C)` with `BC` = `$FEFE` (A8 low) |
| `$4002` `KEYS9` | last `IN A,(C)` with `BC` = `$FDFE` (A9 low) |

1. `DI`, `IM 1` (unused).
2. RAM test, inline (no stack yet): each address `$4000`–`$43FF` written with `L xor H`, then read back; then the complement; a mismatch stores `$E1` at `$4000` and stops in `JR *` (`*` is the current address; `$` the hex prefix). The status byte itself is tested too: its result is written after.
3. `LD SP,$4400`. The line at `$4100`: `$01`–`$10` (character codes: bit 6 clear) then `$C9` (`RET`, bit 6 set). `LD HL,$1234`, `LD BC,$5678`, `CALL $C100`: each code is fetched by M1 in the `$C000` mirror, forced to `$00`, and executed as a NOP; the `RET` is read as itself. Executed as itself, `$01` would load `BC`, `$09` add it to `HL`: after the return `HL` = `$1234` and `BC` = `$5678`, else `$E2`.
4. `LD A,($C100)`: a data read is not an M1, so not forced: `A` = `$01`, else `$E3`.
5. `LD A,$AA`, `LD ($0800),A`: a write into the ROM's space (`ROM_CS_N` low, the 2332 drives `D'` while the CPU drives `D`). Then `LD A,($0800)` reads the ROM's byte there (`.org $0800` `.byte $3C`), else `$E4`.
6. `STATUS` = `$55`, then forever: `LD BC,$FEFE`, `IN A,(C)`, `LD ($4001),A`, `LD B,$FD`, `IN A,(C)`, `LD ($4002),A`.

### 6.3 Test — `tests/netlist/bw80_bench_test.cpp`

The P5j bench test's shape (`Bench` builds the netlist with `ResolveContentsPaths`, checks no warnings, powers on with a seed). RAM bytes are read through `Ram2114::word`, unscrambled by the wiring above (`ReadRam(address)`: IC3's IO3, IO2, IO4, IO1 are bits 0, 1, 2, 7; IC4's IO2, IO1, IO3, IO4 bits 3, 4, 5, 6). `kDone` is the time `STATUS` is `$55` plus a margin, measured in the implementation: the three RAM passes take ≈ 161 T-states a byte, ≈ 165K T-states, ≈ 51 ms; with the 1 ms reset, about 52 ms. A debug run of the bench costs ≈ 0.1 s a simulated ms.

- **"bw80 bench: the program passes"**, seeds 1–3: `STATUS` = `$55` at `kDone`.
- **"bw80 bench: the latch holds each character code"**, seed 1: `CH0`–`CH6`, `RFSH_N` and `A15` recorded; the window is each `RFSH_N` fall whose M1 had `A15` high (read 200 ns before the fall, in T2): the 17 fetches in the mirror, from `$C100` to the RET at `$C110`. `CH0`–`CH5` read `$01`, `$02`, … `$10` with `CH6` = 0, then `$09` with `CH6` = 1 (the RET, `$C9`, read as itself).
- **"bw80 bench: a write into the ROM leaves the CPU side clean"**, seed 1: `D0`–`D7` and `D0'`–`D7'` recorded; while `WR_N` is low with `ROM_CS_N` low, `D` holds `$AA` and is never X, `D'` holds the ROM's byte at `$0800` (`$3C`).
- **"bw80 bench: keys"**, seed 1: after `kDone` + 2 ms, `KEYS8` and `KEYS9` (masked `$DF`: `D5` floats on a keyboard read) read `$5F` (D6 high through R7 and R28, D7 low through R1); press K1 (`setPressed`, `wake`), 2 ms later `KEYS8` reads `$5E` and `KEYS9` `$5F`; release, 2 ms later both `$5F`.
- **"bw80 bench: a stuck RAM cell fails the RAM test"**, seed 1: `StuckCell{"IC3", <IC3's pinIndex("IO3")> (index 11, pin 12, `MD0`), word 5, One}` (bit 0 of `$4005`: `$45`, then `$BA` read as `$BB`): `STATUS` = `$E1` at `kDone`.

### 6.4 Release

Headless ratio of the bench (`bw run --time 100ms`, release, median of 3); `bus_load`, `bus_load_z80`, 110 and BW-1 before and after the kernel change, side by side (a worktree of the commit before phase 1), as P5n did.

## 7. Docs

- `docs/GLOSSARY.md`: **Series resistor** — "A resistor between two signal Nets (the ZX80's data bus): a Net that nothing else drives takes the other's value through it, weakly; a Net with a driver keeps its own. One resistor network resolves them all; less resistance wins between weak values." **Driver** and **Tie** mention the series resistor; **Joined Pin** — "the resistor network's Pin on a joined Net (P5u), as a Switched Pin is the switch network's."
- `docs/ARCHITECTURE.md` (finish): the Pins, nets, drivers paragraph on resistors points to this step; the P5u row.
- `docs/recipes/ADD_PART.md` (finish): reference commits (2332 as a ROM row; 74LS05 as an open-collector gate row; 74LS373; 74LS365). `docs/recipes/ADD_MACHINE.md`: every resistor needs `value=`; a series resistor joins two signal Nets.

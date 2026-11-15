# P5u BW-80 chips — Implementation Plan

Design: [BW80_CHIPS_DESIGN.md](BW80_CHIPS_DESIGN.md)
Spec: [BW80_CHIPS.md](BW80_CHIPS.md)

Worked in the worktree `../bodgewire-p5u`, branch `p5u-bw80-chips` (another agent works on `main`), merged at the end.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | |
| 1 | Kernel: Joined Pin, network Nets | |
| 2 | `ResistorNetwork`, `ParseOhms`, builder | |
| 3 | Board: electrical netlist, Net panel, `R` datasheet, glossary | |
| 4 | `2332`, `74LS05`, `74LS10`, `74LS365`, `74LS373` | |
| 5 | Bench, its program and test | |
| 5b | A key on a joined Net; `value=` on every resistor (the user, after the review) | |
| 6 | Release check, recipes, roadmap | |

## Phases

### 1. Kernel (design §1)

`PinKind::Joined`, `IsNetworkPin`, `DriveOf`; `networkPin` and `networked` in place of `switchPin` and `switched`; Joined released at power-on; `tests/sim/joined_test.cpp`.

Fence: debug build; `ctest --preset debug -j 12 -R "simulator|switched|joined|circuit|logic|resolve"`; then the full debug suite (kernel change). Commit: `sim: Joined Pins; the switched Net becomes a network Net (P5u)`.

### 2. Resistor network and builder (design §2, §3)

`src/parts/resistor_network.{hpp,cpp}` (CMake sorted); `ParseOhms` in `units`; `wireResistors` (kinds, errors, pulls on joined Nets to the network, the switch check). Tests: `tests/parts/resistor_network_test.cpp` (built through small netlists with `LoadNetlist`: each Spec case of the series resistor, the 1 ps follow, a chain of three, no source, X), `tests/netlist/units_test.cpp` (`ParseOhms`), `tests/netlist/builder_test.cpp` (the "resistor errors" case rewritten for §3.2's messages, a new "series resistor errors" case for §3.2–3.3; every netlist under `netlists/` still builds, which the existing netlist tests cover).

Fence: `ctest --preset debug -j 12 -R "resistor network|units|builder|switch"`; the full debug suite (builder change: every Machine). Commit: `parts: ResistorNetwork, series resistors in the builder (P5u)`.

### 3. Board (design §4, §7 glossary)

`ElectricalNetlist` keeps series resistors; `PinRole::Series`; `tests/data/series_bench.bwn`; electrical and net-pins tests; `datasheets/R.md`; `docs/GLOSSARY.md`.

Fence: `ctest --preset debug -j 12 -R "electrical|net pins|net drivers|manual|board|machine"`. Commit: `machine: series resistors on the Board and in the Net panel (P5u)`.

### 4. Parts (design §5)

`2332` row, delays, datasheet, rom tests; `74LS05`, `74LS10` gate rows; `ls365.{hpp,cpp}`, `ls373.{hpp,cpp}`; catalogue rows; datasheets; tests. Data sheet values checked where a sheet can be fetched (MOS2332 on 6502.org); otherwise the design's, said so in the commit.

Fence: `ctest --preset debug -j 12 -R "gate package|74LS365|74LS373|rom|catalogue|manual|footprint"`. Commit: `parts: 2332, 74LS05, 74LS10, 74LS365, 74LS373 (P5u)`.

### 5. Bench (design §6)

`tests/data/bw80_bench.bwn`, `roms/bw80_bench.asm` and its `.rom` (`bw asm`), `tests/netlist/bw80_bench_test.cpp`; `kDone` measured and set.

Fence: `ctest --preset debug -j 12 -R "bw80|asm"`. Commit: `tests: the BW-80 chips bench (P5u)`.

### 5b. No repair may stop the game (design §1.2–1.3, §3.2–3.3)

Asked by the user after the review (2026-11-15): a Net holds a Switched and a Joined Pin, each with its own view of the Net; the builder's switch check goes; every resistor needs `value=`.

Fence: the full debug suite (kernel). Commit: `sim: a key on a joined Net; every resistor needs value= (P5u)`.

### 6. Finish

Release build: the bench's headless ratio; the four baselines again, side by side with a worktree of the commit before phase 1; `bw_ui` on `netlists/bus_load_z80.bwn` with a screenshot viewed (unchanged picture). Recipes (design §7), ARCHITECTURE row done with the date and links, this plan cut down to the table and notes. Full debug suite. Commit: `docs: P5u implemented`. Then merge into `main`.

## Notes

- Review (before code), fixed in the docs: the catalogue test and `CreateRom`'s `select=` messages assumed three selects; `ResistorNetwork::powerOn` resets its driven values (a Circuit is powered on again); `JR *`, not `JR $`; `.rom $0000, $1000`; `RST1` (a reference is letters then digits); fence regexes by test name (`net pins`, `net drivers`, `manual`, `gate package`, `joined`); the NOP timing on the model (the 2114 times its output from `CS_N`, so `access=` cannot show the margin: P5v); the RAM test ≈ 52 ms, not 25; the unconnected-end message; links stored both ways, `kFar` skipped, a turn to X a change; weak sources count as sources and the 1 ps float, noted; `FormatNetPin`'s series text; the test Board's own loader; eight resistors, not nine; the reachable builder errors noted for P5x.
- Phase 1–4 code was written while the review ran, then checked against its findings (`powerOn`, the select messages and `JR *` were already as the review asks).
- Baselines: taken side by side at phase 6 against a worktree of the commit before phase 1 (the release build here had started after the kernel edit).


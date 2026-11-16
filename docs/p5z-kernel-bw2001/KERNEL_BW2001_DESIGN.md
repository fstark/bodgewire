# P5z Kernel for the BW-2001 — Design

Spec: [KERNEL_BW2001.md](KERNEL_BW2001.md). Plan: [KERNEL_BW2001_PLAN.md](KERNEL_BW2001_PLAN.md). Recipe: [ADD_PART.md](../recipes/ADD_PART.md) (its Files row for `lsNN.cpp` and the Rules on falling-edge clocks and sampled inputs). Methods: P5b's design ([KERNEL_BUSES_DESIGN.md](../p5b-kernel-buses/KERNEL_BUSES_DESIGN.md), Trace check and Measurement), not repeated here.

## Files

| File | Change |
|---|---|
| `bench/trace_check.sh` | S1: the new runs, recording a list of Nets per run, default reference `f849080` |
| `bench/measure.sh` | S2: `bw2001` as the headline, evaluations per event |
| `src/sim/component.hpp` | Z1: `PinSpec::fallingEdgeOnly`. Z2: `EvalContext::quiet`, `EvalContext::lastChange` |
| `src/sim/circuit.{hpp,cpp}` | Z1: the fan-out entry's edge mask. Z2: each Pin's fan-out entry index, each entry's Pin count |
| `src/sim/simulator.{hpp,cpp}` | Z1: the fan-out test in `runUntil`. Z2: the quiet state, sized in the constructor, reset in `powerOn` |
| `src/parts/ls107.cpp`, `ls93.cpp`, `ls90.cpp` | Z1: clock Pins falling-edge-only; the 107's `J`, `K` sampled |
| `src/parts/ram2114.{hpp,cpp}` | Z2 |
| `src/parts/rom.cpp` | Z2: `EvaluateRom` |
| `tests/sim/simulator_test.cpp` | Z1: wake cases |
| `tests/sim/circuit_test.cpp` | Z1: the `risingEdgeOnly` checks (lines 186–217) rewritten for the edge mask, merge cases |
| `tests/sim/quiet_test.cpp` (new), `tests/CMakeLists.txt` | Z2: the file added to the explicit sim test sources |
| `tests/parts/ram2114_test.cpp`, `tests/parts/rom_test.cpp` | Z2 |
| `docs/recipes/ADD_PART.md` | Z1, Z2 (below) |
| `docs/ARCHITECTURE.md`, `docs/p1d-kernel-perf/OPTIMISATION_IDEAS.md` | last phase |

## Trace check

The P5b script with these changes:

- **Reference:** default commit `f849080`. P5b's `0757c02` stays possible as an argument; runs whose netlist it lacks are skipped with a note.
- **Runs:** the spec's S1 table.
  - `run_args` gives each run's options, and `run_netlist` maps a run's name to its netlist (`bw2001_long` → `bw2001`).
  - A run whose options include `--record` records that list; otherwise `--record-all` is added.
  - The options are expanded with globbing off (`set -f` around the call), so typed text may hold `?` or `*`. It holds no space; `\n` stays literal through bash 3.2's `echo`, and `--type` turns it into Return.
- **Cache:** the reference's `.txt` gains a first line with the run's options. A reference whose options differ from the script's is recorded again, so tuning a run in phase 1 never compares against a stale file.
- **Typing:** the BW-2001 matrix takes 80 ms a key (`press_queue.hpp`: 40 ms hold, 40 ms gap).
  - BW-2001: `READY` by 1.5 s (`docs/machines/130-bw2001/BW2001_DESIGN.md`, stage 2). A short line and `\n` typed from about 1.6 s, the run ending about 0.4 s after the last key so the echo and the answer reach the screen. Our firmware has no `?` for PRINT (`bw2001_basic.asm` uses it only as an error mark), so the line is a short statement without spaces that the Tiny BASIC accepts, or a line number alone.
  - BW-80: the `K` cursor by about 200 ms (`tests/machines/bw80_test.cpp`), one key at 400 ms as that test does, the run about 1 s.
  - Keyboards: `J5` (`MATRIXKB`, BW-2001), `J3` (`MEMBRANEKB`, BW-80).
  - Phase 1 runs the reference with `--tv`, views the BMP (the typed text on screen) and writes the final times and text in the plan's notes.
- **Long-run Net lists**, one variable per Machine in the script:
  - BW-2001: `BA0`–`BA11`, `BBD0`–`BBD7`, the CPU side's data, `SA*`, `SD*`, `RAM0_N`–`RAM7_N`, `SEL*_N`, `SEL8`, `TV_VIDEO`, `HORZ_DRIVE`, `VERT_DRIVE`.
  - BW-80: the address, data and memory-data buses, `ROM_CS1`, `RAM_CS_N`, `VIDEO` and the sync Nets.
  - Exact names are read from `netlists/bw2001.bwn` and `netlists/bw80.bwn`. The aim is VCDs under about 300 MB each; sizes are logged in phase 1.

Disk: about 1–1.5 GB per side under `build/`. A reference side is recorded once per set of options.

## Measurement

- `bench/measure.sh`: the `/usr/bin/time -l` line runs on `bw2001` (headline) and `bus_load` (P5b's row, for continuity), each 1 s. It prints instructions, cycles and evaluations per applied event (`bw run` prints `evaluations`).
- The summary line has the idea log's columns: `bw2001` ratio, instr./event, cycles/event, eval./event, `bw80`, `bus_load`. Then one line for every other netlist, with its ratio against the previous row's, so a slowdown over 5 % stands out (spec, Guard 3).

## Z1: falling-edge-only Pins

- **`PinSpec`:** `fallingEdgeOnly` (default false) is **appended after `sampled`**. Parts and tests build `PinSpec` positionally (`PinSpec{name, Input, Ttl, false, true}` for a sampled Pin in `ls74.cpp`, `ls175.cpp`, `ls273.cpp`, `mos6502.cpp`, `z80.cpp`, `mos6522.cpp`, `connector.cpp`, `circuit_test.cpp`), so placing it anywhere else would silently change them. `Circuit::add` copies it into `PinInfo` as it does `risingEdgeOnly`, and `Check`s that a Pin is not both.
- **Edge mask:** each reader Pin has a wake mask of three one-bit kinds: rise (0 → 1), fall (1 → 0) and other (any other value change).
  - An ordinary Pin is rise | fall | other, a `risingEdgeOnly` Pin rise, a `fallingEdgeOnly` Pin fall. A sampled Pin has no entry at all, as today.
  - `FanOut { ComponentId component; bool risingEdgeOnly; }` becomes `{ ComponentId component; std::uint8_t edges; }`, still 8 bytes (`static_assert`).
  - `buildTables` ORs the masks of a Component's Pins on one Net into its one entry. They are contiguous (`circuit.cpp:93-98`): one ordinary Pin makes the entry wake on everything, as today's `&=` of `risingEdgeOnly` does.
- **`runUntil`:** the change kind of a changed Net is computed once, as one bit, and each entry is tested with `f.edges & kind`. This replaces `rose || !f.risingEdgeOnly`.
- **Parts.** Each was checked against its `evaluate`: the Pin enters the result only through `fell(pin)`, nothing reads it as a level, there is no X-clock path, and every skipped evaluation's drives equal what is current or pending.
  - 74LS107: `1CLK`, `2CLK` falling-edge-only; `1J`, `1K`, `2J`, `2K` sampled (read only via `readBefore` under `fell`).
  - 74LS93: `CKA`, `CKB` falling-edge-only.
  - 74LS90: its two clocks, by the same check, made again in phase 2 against the code.
  - Not converted: the 74177, whose X clock makes its counts X (`ttl177.cpp`, the test "74177: X clock and random power-on"), and Parts whose `fell` is on a level Pin (decoders' enables, the 74LS157's select, CPU and VIA phase inputs).
- **Tests:**
  - `simulator_test.cpp`: wake on 1 → 0, none on 0 → 1, X → 0, 0 → X. The last two are why "fell" is the single 1 → 0 transition, as `EvalContext::fell` defines it.
  - `circuit_test.cpp`: the entry masks, a merged ordinary and falling-edge-only Pin waking on everything, and the TTL load kept.

## Z2: quiet Pins

**API.** Declared in `EvalContext` (`src/sim/component.hpp`), defined inline in `simulator.hpp` beside `read`:

- `void quiet(PinIndex pin, bool on)`: while on, a change of this Pin's Net does not mark this Component dirty.
  - It does not touch the Pin's value, its drive, its TTL load, or what `read`, `readBefore` and `readBit` return.
  - It is idempotent. On a Pin with no fan-out entry (sampled, power, output) it does nothing.
- `Time lastChange(PinIndex pin) const`: the Net's `changedAt`, the time `value` last changed (−1 since power-on).

**Kernel:**

- `Circuit::buildTables` records:
  - for each reader Pin, the global index of its fan-out entry (`pinFanOut_`, a `std::uint32_t` per PinId; `kNoFanOut` for Pins with none);
  - for each entry, the number of Pins merged into it, with a `Check` that it fits the count's type.

  Accessors: `Circuit::fanOutIndex(PinId)`, `Circuit::fanOutPins(entry)`, and `fanOutBase(NetId)`, the global index of a Net's first entry, so that `runUntil` walks entries by index.
- `Simulator` keeps two arrays, both sized in the constructor:
  - `fanOutLoud_`: one `std::uint8_t` per entry, its Pins not quiet;
  - `pinQuiet_`: one byte per PinId, so that two `quiet(pin, true)` calls decrement once.

  `powerOn` resets both **before** its loop that evaluates every Component, so quiets set at that first evaluation hold.
- `runUntil`'s fan-out loop also requires `fanOutLoud_[base + i] != 0`. The extra load is one byte per entry. Its cost on every netlist is measured on its own first (phase 3, step 1: the kernel change with no Part using it) and logged.
- **Masks and quiet:** an entry's edge mask is the OR over all its Pins, quiet or not. A quiet ordinary Pin merged with a loud falling-edge-only Pin makes the entry wake on any change. That is safe, an evaluation the Part already tolerates today, and it does not arise in the Parts converted here.

**2114** (`src/parts/ram2114.cpp`):

- `quiet_`, a member, is true while the chip's Pins are quiet. The chip calls `quiet` only when that changes, never on every evaluation.
- The write-in-progress check at the start of `evaluate` runs as today. Its `readBitBefore` of the select draws only when `CS_N` was X before, and then the chip was loud.
- **Going quiet:** at the end of an evaluation where `ctx.read(kCsN) == One` (a definite 1), after driving the data Pins Z as today. Quiet: the ten address Pins, the four data Pins, `WE_N`. `CS_N` stays loud.
  - An X select stays loud: today `selected = !readBit(kCsN)` draws a fresh bit at each evaluation, and the chip may select and drive.
  - With this rule, an evaluation skipped while quiet would have drawn only for X address bits (`readBit` on the address, line 79). That is the spec's one admitted exception.
  - The write-in-progress check cannot fire in a skipped evaluation, because `readBefore(CS_N)` is 1 there.
- **Waking:** at an evaluation where `quiet_` is true and `CS_N` is no longer a definite 1, all Pins are made loud first. `dataReady_` is then recomputed as the latest `lastChange` of `A0`–`A9` plus `access_`, or 0 if none is ≥ 0. After that the evaluation proceeds as today.
- **Why this matches the old code:** it set `dataReady_ = now + access` at every evaluation where an address Pin had `changedAt == now`, and it was woken at every address change. So its `dataReady_` was always the latest address change plus access, or 0 before any.
- While loud, `addressMoved` and `dataReady_` work as today.

**ROM** (`EvaluateRom<Layout>` in `src/parts/rom.cpp`), following the 2114:

- `quiet_` is a member of `Rom`, passed in by reference as `dataReady` is.
- The ROM quiets its address Pins exactly when `off`, as computed today (some enable at its definite inactive level; one inactive enable with another X is off). Its enables stay loud. The ROM draws no random bits.
- **Waking:** when no longer `off`, all address Pins are made loud, then `dataReady` is recomputed. T is the latest `lastChange` over the address Pins. `dataReady` is T plus `addressDelay[0]` for a one-delay layout, or T plus the largest `addressDelay[i]` among the Pins whose `lastChange == T`. It is 0 if T < 0. This matches today's `addressDelay = max over the Pins changed at this timestamp`.

Every other Part keeps all its Pins loud; nothing changes for it.

**Tests:** `quiet_test.cpp` builds its own small counting Components on the pattern of `simulator_test.cpp`'s (the `circuit_test.cpp` ones are in an anonymous namespace and do not evaluate). Cases: the spec's Z2 list, plus `quiet` on a Pin without an entry doing nothing.

## Z3 and Z4

Designed in the plan's idea log from the profile after Z2, as P5b did for its later ideas. Starting points:
- **Z3:**
  - the 74LS107 and 74LS93 written for their known Pins, with no `Next()` loop over X when `J`, `K` and the state are known;
  - the 74LS244 reading each `OE_N` once (P5b 4.4 found that alone below the bar);
  - the 2114's selected path reading its Pins with `read` and checking for X once, before any `readBit`.
- **Z4:** the fan-out marks: `markComponentDirty` per entry, and the `componentDirty_` test.

## ADD_PART

- Files row for `src/parts/lsNN.cpp` (line 22): falling-edge clocks `PinSpec{"CLK", Input, Ttl, false, false, true}` = falling-edge-only, beside the rising-edge and sampled forms.
- Rules, "Falling-edge clocks" (line 38) rewritten: `ctx.fell(pin)` and `fallingEdgeOnly = true`, only when `evaluate` reads the Pin through `fell` alone. A Part that reacts to an X clock (the 74177) leaves it false.
- Rules, "Sampled inputs" (line 39): the 74LS107's `J`/`K` added to the examples.
- A new rule, "Quiet Pins":
  - when a Part may quiet Pins: a definite deselect that makes every other input irrelevant;
  - what it keeps loud: the selects, and anything when a select is X;
  - toggling only on change;
  - timing measured from an input's change uses `lastChange`, not a member updated on wake;
  - the 2114 and the ROM as the examples.

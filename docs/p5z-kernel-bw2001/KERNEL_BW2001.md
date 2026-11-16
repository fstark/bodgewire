# P5z Kernel for the BW-2001 — Specification

Roadmap row P5z ([ARCHITECTURE.md](../ARCHITECTURE.md), Roadmap). Design: [KERNEL_BW2001_DESIGN.md](KERNEL_BW2001_DESIGN.md). Plan: [KERNEL_BW2001_PLAN.md](KERNEL_BW2001_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md). Before: P5b ([KERNEL_BUSES.md](../p5b-kernel-buses/KERNEL_BUSES.md)), [OPTIMISATION_IDEAS.md](../p1d-kernel-perf/OPTIMISATION_IDEAS.md).

## Why

The BW-2001 (Machine 130, the PET 2001's board 320137) runs at **0.22×** real time headless and 0.20× in `bw_ui`. P5z was deferred to the end of P5 so that the BW-80 could be measured too. Measured 2026-11-16 (release, 1 s simulated, `/usr/bin/time -l`):

| Netlist | Ratio | Applied events | Evaluations | Eval./event | Instr./event |
|---|---|---|---|---|---|
| `bw2001` | 0.22× | 128 M | 275 M | 2.14 | 855 |
| `bw80` | 0.25× | 193 M | 189 M | 0.98 | 492 |
| `bw1` | 2.59× | 26 M | 18 M | 0.68 | 408 |
| `bus_load` | 0.37× | 118 M | 114 M | 0.96 | 457 |

The BW-2001 does twice the evaluations per event of the other computers, and each event costs almost twice as much. Evaluations by Part kind over 200 ms (temporary counter):

| Part | `bw2001` | `bw80` |
|---|---|---|
| gate packages | 23.7 % | 40.1 % |
| 2114 | **21.6 %** | 4.1 % |
| 74LS107 | 12.8 % | — |
| 74LS244 | 8.5 % | — |
| 74LS93 | 8.3 % | — |
| oscillator | 6.8 % | 7.1 % |
| ROM | 6.6 % | 2.6 % |
| 74LS165 | 4.6 % | 5.4 % |
| capacitor | — | 21.4 % |

The profile (`sample`, by function, about 5,000 samples each) agrees:
- **BW-2001:** `runUntil` 18 %, `readBit` 13 %, `Ram2114::evaluate` 11 %, Net resolution 8 %, `GateFunction` 6 %, 74LS244 6 %, the event queue 10 %.
- **BW-80:** `runUntil` 29 %, the event queue 18 %, `GateFunction` 8 %, Net resolution 8 %.

Two causes stand out in the BW-2001:

1. **Fan-out on the address and data buses.**
   - `BA0`–`BA9` reach sixteen main-RAM 2114s and seven ROMs (`BA10` reaches the ROMs too), and `BBD0`–`BBD7` reach the RAMs. Every change of one of these lines wakes about 23 chips, almost all of them deselected.
   - Each deselected 2114 then reads its select and ten address Pins through `readBit`, which draws a random bit for X, and about twenty more through `read`/`readBefore`. It drives Z again before finding nothing to do.
2. **Clock edges nobody acts on.** The 74LS107 and 74LS93 of the video chain act only on falling clock edges, and the 107's `J` and `K` only at that edge, yet every change of those Pins wakes the chip.

The row's own figure ("the 8 MHz dot clock and its gated copy are 28 % of all Net changes") counts Net changes, not cost. The clocks themselves stay: they are the netlist, which is Commodore's.

The BW-80's cost is the generic per-event path that P5b already worked on: the run loop and the queue. It has no comparable waste of its own.

## Aim and terms (set 2026-11-16)

- **Aim:** the BW-2001 as fast as the ideas below allow, with no fixed target. Real time would need about 4.6×, which nothing within these terms reaches. The estimate, not measured, is 0.35–0.45×.
- **Guard 1, the trace check:** every run of S1 byte-identical to the reference kernel (`f849080`, the last commit before P5z), with the same `applied_events`. `evaluations` is printed, not compared: the point of P5z is fewer of them. The only accepted exception is the random bits a deselected chip drew for X inputs it never used (S3, Z2). If a trace check run shows one, it is shown to the user and explained before the idea is kept.
- **Guard 2, one kernel path; the Part API may grow:** additions to `PinSpec` and `EvalContext` are allowed (the user, 2026-11-16). Nothing existing changes meaning, every Part not converted keeps working unchanged, and there is no second kernel path. Also out: Part-specific code in the kernel, cuts to timing or to which Nets are simulated, Bus-as-one-entity (OPTIMISATION_IDEAS idea 8), two cores (idea 6) and generated code (idea 2).
- **Guard 3, one idea at a time:** one commit each, kept only if it gains ≥ 5 % on `bw2001` in both ratio and cycles elapsed, with instructions logged. The bar applies to an idea as a whole: a pin flag counts with all the Parts it is applied to. Every other netlist is measured and reported each time. Any netlist slower by more than noise (5 %) is shown to the user before the idea is kept.
- **Stop:** when nothing left reaches the 5 % bar alone (the user, 2026-11-16), as P5b did.
- **Progress:** the plan's idea log has one row per idea tried, kept or not.

## User decisions (2026-11-16)

| Question | Decision |
|---|---|
| Which Machines P5z aims at | the BW-2001. The BW-80 and every other netlist are measured before and after, and keep whatever the shared changes give them |
| Rules for the kernel work | traces exact; one kernel path; the Part API may grow (additions only) |
| When it stops | when no idea left gives 5 % alone; no fixed target |
| The UI's frame cost | measured, and raised with the user only if it is significant or limits the simulation. The UI runs on its own thread and is still changing, so no UI work here. Measured 2026-11-16: `bw_ui` on healthy BW-2001 Puzzles 0.20× (headless 0.22×), UI 2.5 ms a frame; on the BW-80 0.25×, UI 0.9 ms. Not significant |

## Behaviour

Nothing a player, a Puzzle or an instrument sees changes. What is new:

### S1 Trace check

`bench/trace_check.sh`, reference `f849080`. It keeps P5b's three runs and adds the computers. A long run records the Nets that show the computer working, not every Net: a full trace is about 1.4 GB per simulated second.

| Run | Time | Input | Recorded |
|---|---|---|---|
| `bus_load` | 50 ms | none | all |
| `test_pattern` | 100 ms | none | all |
| `typewriter` | 300 ms | `--type J3:100ms:HI` | all |
| `bw2001` | 150 ms | none | all (power-on, RAM sizing, first frames) |
| `bw2001` long | about 2.5 s | a short line and Return (`\n`) typed after `READY` (80 ms a key) | CPU address and data, RAM data, video RAM, every RAM and ROM select, video out |
| `bw80` | 100 ms | none | all |
| `bw80` long | about 1 s | a key typed after the `K` cursor (shown by about 200 ms) | same kind of Nets as the BW-2001 long run |
| `bw1` | 200 ms | none | all |

The check passes when every VCD is byte-identical and each run prints the same `applied_events`. If a long run's text shows no effect on the TV (`--tv` against the reference), the design's typing time is wrong. That is fixed in phase 1, before the reference is recorded.

### S2 Measurement

`bench/measure.sh`, release build, the machine otherwise idle (P5b S2). The headline netlist is `bw2001`: its ratio (best of 3, 2 s), and its instructions and cycles per applied event (1 s). The `bw80` and `bus_load` figures come next, then every other netlist on one line each.

### S3 Ideas, in expected order of gain

1. **Z1, falling-edge-only and sampled Pins on the dot-clock chain.**
   - `PinSpec::fallingEdgeOnly` is the counterpart of `risingEdgeOnly`: a Pin whose changes other than 1 → 0 never alter `evaluate`'s result, so they do not wake the Component.
   - It goes on the clocks of the 74LS107 and 74LS93 (the BW-2001's video chain). It also goes on the 74LS90 (not on the BW-2001; tennis, stopwatch, knob), so that ADD_PART's rule holds for every falling-edge Part. Each was checked to be a no-op on any other change of that Pin.
   - The 74177 stays as it is: an X on its clock makes its counts X (`ttl177.cpp`), so it must wake on 0 → X and 1 → X.
   - The 107's `J` and `K`, read only through `readBefore` at the falling edge, become `sampled` (P5b idea 5).
   - A skipped evaluation would have driven only what is already driven or pending (the inertial `drive` makes that a no-op), so traces stay identical.
2. **Z2, quiet Pins while deselected.** Two additions to `EvalContext`:
   - `quiet(pin, bool)`: while a Pin is quiet, a change of its Net does not wake the Component. All Pins are loud again at power-on.
   - `lastChange(pin)`: the time of the last change of the Pin's Net's value (−1 if none since power-on).

   - A 2114 whose `CS_N` reads a definite 1 quiets its address, data and `WE_N` Pins and listens only to `CS_N`. An X select stays loud: today it draws a fresh select bit at each evaluation, and it may select.
   - Selected again, the 2114 takes the data-ready time from the address Pins' `lastChange`. The timing is then the same as if it had been woken by every address change.
   - The ROMs (2316B, 2332, 2716, 2513) quiet their address Pins while off (an enable at its definite inactive level) in the same way.
   - A skipped evaluation drove nothing, so traces stay identical. The one difference, by construction: a deselected 2114 no longer draws random bits for X on its address Pins (Guard 1). In a healthy run those Pins are never X, so the trace check is expected to pass.
   - ADD_PART gains a section on quiet Pins.
3. **Z3, specialised evaluation** of the Parts hottest after Z1 and Z2, in profile order, one idea each (P5b idea 4): the 74LS107, 74LS93, 74LS244, and the 2114's selected path.
4. **Z4, the run loop**, from a fresh line profile. The fan-out marks per changed Net are the likely candidate, since each address change still marks the chips listening to it.

Each idea after Z2 is designed in the plan's idea log when it is tried, because the next idea depends on the last profile.

## Tests

- `bench/trace_check.sh` passes after every kept idea (S1).
- Z1: in `tests/sim/simulator_test.cpp` (beside "a sampled input does not wake its component"), a falling-edge-only Pin wakes its Component on 1 → 0 and not on 0 → 1, X → 0 or 0 → X. In `tests/sim/circuit_test.cpp`, the fan-out entry's edge mask (its `risingEdgeOnly` checks rewritten for the mask): merged Pins OR their masks, and a falling-edge-only Pin still counts as a TTL load. Each converted Part's existing tests pass unchanged; the 74177's X-clock test is the reason it is not converted.
- Z2: in `tests/sim`:
  - a quiet Pin's Net change does not wake its Component, and the same Pin made loud again does;
  - a Component listening on another Pin of the same Net is still woken;
  - power-on makes every Pin loud;
  - `lastChange` gives the time of the last value change, −1 before any;
  - a Pin with a stuck-input Fault (its own Net) behaves as any other.

  In `tests/parts/ram2114_test.cpp` and `tests/parts/rom_test.cpp`, the select-after-address cases already there (2114 line 344, ROM line 518) stay. New cases:
  - an address change at the same timestamp as the select;
  - a select earlier than the access time after power-on, with no address change yet;
  - for a 2114, an X on `CS_N` (it stays loud and is evaluated);
  - a 2513 with row and character addresses of different delays changing at the latest time;
  - a deselected chip is not evaluated on address or data changes (evaluation count).
- The full debug suite after each kept kernel idea (AGENTS.md: shared code). For a Part-only idea, the tests of every Machine using that Part, in debug, plus the `bw2001` and `bw80` release cases.

## Done when

- Z1 and Z2 tried; then Z3 and Z4 from the profiles until nothing reaches the bar. Each try logged; the kept ones committed under the three guards.
- The final measurement is recorded here (Results) and in ARCHITECTURE (Performance, row P5z), and in OPTIMISATION_IDEAS (Target, History, Next steps). ADD_PART is updated for any new Pin flag.
- The full debug suite is green, and release screenshots of a BW-2001 and a BW-80 Puzzle have been viewed (pictures unchanged, ratio printed).

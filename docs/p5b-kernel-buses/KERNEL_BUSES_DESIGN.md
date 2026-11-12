# P5b Kernel for buses — Design

Spec: [KERNEL_BUSES.md](KERNEL_BUSES.md). Plan: [KERNEL_BUSES_PLAN.md](KERNEL_BUSES_PLAN.md). Recipe: [ADD_PART.md](../recipes/ADD_PART.md) (only when Parts are converted, idea 4).

## Files

| File | Change |
|---|---|
| `bench/trace_check.sh` | S1 (below) |
| `bench/measure.sh` | S2 (below) |
| `src/sim/event_queue.{hpp,cpp}` | idea 1: the queue's inside; its interface (`push`, `top`, `pop`, `empty`, `size`, `clear`) and pop order unchanged. Idea 1f: `top`, `pop` and `size` give way to `nextTime` and `takeNext` (a whole timestamp at once), the order unchanged |
| `tests/sim/event_queue_test.cpp` | idea 1: cases for the new structure's edges |
| `src/sim/simulator.{hpp,cpp}` | idea 2: `runUntil` and its helpers; `EvalContext`'s declared members unchanged. Idea 1f: the update phase loops over one `takeNext()` |
| `src/sim/component.hpp`, `src/sim/circuit.{hpp,cpp}` | idea 5: `PinSpec::sampled`, carried into `PinInfo`; `buildTables` leaves sampled Pins out of the fan-out |
| `src/parts/ls74.cpp`, `src/parts/ls175.cpp` | idea 5: the `D` Pins sampled |
| `CMakePresets.json` | idea 3: `CMAKE_INTERPROCEDURAL_OPTIMIZATION` `ON` in the `release` preset only |
| `src/parts/rom.{hpp,cpp}` | idea 4: specialised evaluation (below) |
| later Parts' `.cpp`/`.hpp` | idea 4, after the user's OK |
| `docs/recipes/ADD_PART.md` | idea 4, if Parts are written differently |
| `docs/ARCHITECTURE.md`, `docs/p1d-kernel-perf/OPTIMISATION_IDEAS.md` | last phase |

## Trace check (`bench/trace_check.sh`)

Bash, `set -euo pipefail`, run from the repository root. Argument: the reference commit, default `0757c02` (the P5a kernel, last commit before P5b's code).

1. Reference build, once per commit: `git worktree add --detach build/trace-ref/<commit>/src <commit>` if missing; `cmake -S build/trace-ref/<commit>/src -B build/trace-ref/<commit>/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBW_BUILD_TESTS=OFF -DBW_BUILD_UI=OFF` (no Catch2, SDL3 or imgui fetched); build target `bw`. The worktree lives under `build/` (ignored by git); `git worktree prune` is not needed while it stays.
2. Reference traces, once per commit: the three runs of the spec's S1 table with the reference `bw`, into `build/trace-ref/<commit>/{bus_load,test_pattern,typewriter}.{vcd,txt}`; `.txt` keeps the `applied_events` and `evaluations` lines (`ratio` and `wall_s` vary and are dropped).
3. Current traces: the same runs with `build/release/src/cli/bw` into `build/trace-now/`.
4. Compare: `cmp` each VCD and `diff` each `.txt`. On a VCD difference, print the line number from `cmp` and that line with the nearest preceding `#time` line of both files (`sed -n`). Exit 1 on any difference, 0 with "trace check: identical (3 runs)" otherwise.

Each side runs on its own tree's netlists: the reference `bw` on `build/trace-ref/<commit>/src/netlists/…`, the current one on `netlists/…`. ROM contents resolve relative to the netlist file (`ResolveContentsPaths`, `src/netlist/builder.cpp`), so each finds its own ROMs. P5b changes no netlist or ROM, so the inputs are the same bytes.

Disk: ≈ 55 + 8 + 150 MB per side, under `build/`.

## Measurement (`bench/measure.sh`)

Bash. Builds nothing (the caller builds release first).

- Three rounds; in each, `bw run` on every `netlists/*.bwn` `--time 2s` in turn; keeps each netlist's best `ratio` and its `applied_events`, `evaluations`.
- `/usr/bin/time -l build/release/src/cli/bw run netlists/bus_load.bwn --time 1s`: "instructions retired" and `applied_events`, printed with their quotient.
- Prints the idea log's columns (`bus_load`, test pattern, typewriter) and one line per other netlist for the notes.

The profile build stays manual as in P1d (`cmake -S . -B build/profile -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -g -fno-omit-frame-pointer"`, then `sample bw 8`), commands in the plan's notes.

## Idea 1: the event queue

Measured first, before any code: the queue's size distribution on `bus_load` (temporary counter, `git checkout` after) and the spread of event delays, to size the structure. Starting design, adjusted from those numbers and logged:

- **Timing wheel with sorted slots.** `kSlots` (power of two) slots of `kSlotWidth` ps (power of two), each a `std::vector<Event>` that keeps its capacity. An event at `time` goes to slot `(time / kSlotWidth) % kSlots` if `time < horizon` (the end of the wheel's current turn), else to an overflow min-heap (today's heap code) drained into slots as the wheel turns.
- The current slot is sorted once by `(time, target, stamp)` when the wheel reaches it and popped from the front (an index, not erase). An event pushed into the current slot (a delay shorter than the slot width) is inserted at its sorted place after the read index (`std::upper_bound`): slots hold few events, so the insertion is short. Same order as the heap's: traces identical by construction.
- `top()` finds the next non-empty slot; an occupancy bitmap (`std::uint64_t` words, `std::countr_zero`) skips empty slots. `top()` stops being `const` (the queue is private to `sim`).
- **Push invariant:** a push may carry any `time ≥` the last *popped* time, not only `≥ top().time`. `runUntil` calls `top()` on an event past `end`, then sets `now_ = end`; callers then push at `now_ + 0…` (`Simulator::wake` from `src/cli/main.cpp`, `src/runner/runner.cpp`, `src/session/session.cpp` button and keyboard jitter). So `top()` may advance the cursor and sort a slot, and a push earlier than the current slot's start goes into the current sorted slot at its sorted place (before everything there, since nothing earlier is pending).
- **Overflow invariant:** every event with `time < horizon` is in a slot; the overflow holds only events `≥ horizon`. The horizon is a fixed turn boundary (the end of the wheel turn holding the cursor). When the cursor enters a new turn, the horizon moves one turn on and overflow events below it move into their slots *before* any slot of that turn is sorted.
- `clear()` empties the slots, the bitmap and the overflow, keeping capacity, and resets the cursor.
- Tests: push A at 1000, `top()`, push B at 10: B pops first; an overflow event that lands in the current slot; an event a whole turn ahead; the bitmap empty after `clear()`.

P1d's wheel lost 9–12 % on a heap of 8–15 events; this one is kept only if `bus_load` gains ≥ 5 %. If it loses, a 4-ary heap (same interface, shallower) is the second try, one row of the log each.

## Idea 1f: the queue as runs (added 2026-11-12)

Measured on `bus_load` (100 ms, temporary counters): 13.7 M pops in 6.36 M timestamps, 2.16 a timestamp (one pop 55 % of timestamps, two 28 %, eight or more 7 %); 43 % of pushes carry the time of the push before (a Part driving its bus Pins in one evaluation); about 5 distinct times pending. The heap paid a sift for each event; what the load offers is events that arrive and leave in groups of one time.

- **Run:** the events of consecutive pushes for one time, side by side in one flat array (`keys_`). An event is stored without its time, as `target << 32 | stamp` (`EventKey`), so integer order is `(target, stamp)`.
- **Push:** the time of the open run (the last one pushed to): append the key. Another time: close the open run, open a new one, put at its place in `runs_`, which is kept in time order (a scan from the latest; a run of the same time stays before the new one). Opening is linear in the number of runs, about 5 here and never more than the Circuit's outputs and wakes pending.
- **Take** (`takeNext()`): the first run, or the first few when they share a time (gathered into `batch_`), sorted by key (insertion sort: 1 or 2 events, 8 or 16 for a bus). The same `(time, target, stamp)` order as the heap: traces identical by construction. The span is valid until the next push or take; `runUntil`'s update phase reads it before any evaluation pushes.
- **`nextTime()`:** the first run's time. `runs_[tail_].time` is `kNever` (`INT64_MAX`), so an empty queue answers `kNever` and scans forward need no bound.
- **Room:** taken runs leave holes in `keys_`. When it is full, the pending runs move to the front of a second buffer (the two swap; no allocation once grown), the open run last so that it can still grow; the buffer doubles when pending events fill half of it. `runs_` moves its pending runs down when its end is reached, and doubles when all of it is pending. An empty queue starts again from the front.
- **Push invariant** as for the wheel: a push may carry any time ≥ the last taken time, also earlier than `nextTime()` and equal to the time just taken (`Simulator::wake(…, 0)` between two `runUntil` calls); it opens a run at its place.
- Tests: a take gives every event of the earliest time; one time pushed in separate runs is one sorted take; a far event behind near ones through many refills; a push earlier than the pending events and one for the time just taken; more pending times than the first buffer holds; the same takes as a sorted list over 60,000 random steps with bursts, repeated times and clears.

## Idea 2: the run loop

From a line-level profile of `runUntil` after idea 1. Candidates, each its own try only if the profile points at it: the update loop's `queue_.top()`/`pop()` pair (one call that pops a whole timestamp into a small vector); stale events left by inertial cancellation (`staleEvents` read first, through a temporary print in `bw run`, not committed: if large, cancellation that removes the event instead of leaving it is a queue change, tried under idea 1's rules); the `recorder_.isRecording` test per changed Net; the `wakes` and `staleEvents` counters (`appliedEvents` and `evaluations` stay: the trace check and `measure.sh` use them). Evaluation order and RNG use stay as today (they set the traces).

## Idea 5: sampled inputs (added 2026-11-12)

Measured on `bus_load` (100 ms, temporary counters): 12.9 M evaluations, 57 % of them without a `drive` that changes anything or a wake. Among them: the 74LS74 that halves the clock is evaluated again when its own `Q_N` comes back on its `D` (half of its evaluations), and the 74LS175s of the character latch at every change of the memory data bus (85–92 % of theirs).

- `PinSpec::sampled` (default false): an Input that `evaluate` reads only when another Pin acts. `Circuit::add` copies it into `PinInfo`; `Circuit::buildTables` skips such a Pin when it builds the fan-out, after counting it as a TTL load (`netHasTtlInput_`: it still pulls a floating Net high). A Net whose only readers are sampled Pins has an empty fan-out; a Component with a sampled Pin and another Pin on one Net keeps the entry of the other Pin.
- Nothing changes in `Simulator` or `EvalContext`: `read` and `readBefore` go through the Net, whoever was woken.
- Marked: `1D`, `2D` of the 74LS74; `1D`–`4D` of the 74LS175. Both read `D` with `readBefore` under `ctx.rose(clk)` and nowhere else. Not marked, because they act by themselves: `PRE_N`, `CLR_N`. Other Parts with inputs of this kind (74LS107 `J`/`K`, 74LS161 data and `LOAD_N`, 74LS164 `A`/`B`, 74LS165 `SER`, the shift registers' inputs) are left for when a profile asks: each needs its `evaluate` read through first.
- Traces: an evaluation that is skipped had no clock edge, so it would have driven every output to the value it already had (`drive` does nothing then) and read no random bit (neither Part uses `readBit`). `bench/trace_check.sh` compares VCDs and `applied_events`, and prints `evaluations` of both sides.
- Recipe: ADD_PART gains the field and the rule for when it may be set.

## Idea 3: LTO

`"CMAKE_INTERPROCEDURAL_OPTIMIZATION": "ON"` in the `release` configure preset's `cacheVariables`. The top `CMakeLists.txt` gets `cmake_policy`/`CheckIPOSupported` only if CMake asks for it. Debug unchanged. The cache variable also puts LTO on the fetched SDL3, imgui and Catch2: the clean release build time is reported before and after including them (build complexity was P1d's reason to reject LTO); if that cost is what tips it, `INTERPROCEDURAL_OPTIMIZATION` on bodgewire's own targets only is the variant tried. Kept under guard 3 like any idea. Once kept, profiles come from `build/profile-lto` (the profile flags plus `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`), so they show what runs.

## Idea 4: specialised evaluation, the ROM

Within today's `EvalContext` (guard 2), and with `class Rom`, its constructor, `size()`, `RomSpec`, `FindRomSpec`/`RomSpecOf` and the catalogue's `circuit.add<Rom>(…, spec, words)` unchanged (so `tests/parts/rom_test.cpp`, which builds `Rom` directly, stays as it is):

- `template <std::size_t A, std::size_t D, std::size_t E> struct RomLayout` holds the compile-time part of a spec as `std::array`s (address pins and delays, data pins, enables, off delay), with `constexpr` `kLayout2716` and `kLayout2513` next to `s_specs`, built from the same arrays.
- `Rom::evaluate` dispatches once on `&spec_` to `EvaluateRom<kLayout2716>(ctx, words_)` or `EvaluateRom<kLayout2513>(…)` (`template <const auto& L>`), a function in `rom.cpp`'s unnamed namespace.
- In it: loops over arrays of known size (unrolled), the longest address delay a constant; the unknown-address path (X bits) moved into a separate function called only when a bit is not 0/1. For a layout with one address delay (2716), the changed-address check stops at the first changed bit; otherwise (2513, row and character access differ) it keeps the max over changed bits.
- Expected: Rom's share (7 %) roughly halved, ≈ 3–4 % overall: below the 5 % bar alone, which is why the bar applies to the summed conversion (spec, Guard 3). It is the pattern shown to the user with its numbers.
- If the profile shows the cost is in `read`/`readBefore` themselves (a Net index fetch per Pin), the fix is an `EvalContext` addition such as reading a list of Pins in one call: a Part API change, described to the user with the ROM and not made without an OK. This is the likely outcome (spec, S3.4).

The ROM's tests (`tests/parts/rom_test.cpp`, the machines that use a 2716 or 2513) are its behaviour check, unchanged.

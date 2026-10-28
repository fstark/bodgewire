# TV — Implementation Plan (P2c)

Design: [TV_DESIGN.md](TV_DESIGN.md)
Spec: [TV.md](TV.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | `Recorder::trim`, `Simulator::trimRecording`, `ConnectorNet()` in the netlist library | |
| 2 | `bw_devices` module, `Tv` sync separation and deflection (S1, S2) | |
| 3 | `Tv` picture (S3) and `WriteGreyscaleBmp()` | |
| 4 | `TvInput` and the test-pattern picture check (S5) | |
| 5 | `bw run --tv` (S4), release run, docs | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate: `ctest --preset debug --output-on-failure`

`clang-format -i` every changed C++ file (never CMakeLists.txt). One commit per phase.

---

## Phase 1 — Groundwork in existing modules

The two small additions the TV needs from `sim` and `netlist`, so later phases only add files.

### 1.1 — Recorder trimming

[src/sim/recorder.hpp](../../src/sim/recorder.hpp) / `.cpp`:

```cpp
void trim(NetId net);  // keeps only the last Sample (the current value); no-op if not recording
```

Implementation: `auto& s = slots_[slot]; s.erase(s.begin(), s.end() - 1);` (a slot always holds ≥ 1 Sample).

[src/sim/simulator.hpp](../../src/sim/simulator.hpp), next to `recorder()`:

```cpp
void trimRecording(NetId net) { recorder_.trim(net); }  // bounded memory for long runs
```

### 1.2 — `ConnectorNet()`

[src/netlist/builder.hpp](../../src/netlist/builder.hpp) (add `<optional>`, `<string_view>` already there):

```cpp
// Net on `pin` of the first `part` connector, e.g. ("VIDEO", "SYNC_N"); nullopt if none.
std::optional<NetId> ConnectorNet(const LoadedCircuit& loaded, std::string_view part,
                                  std::string_view pin);
```

[src/netlist/builder.cpp](../../src/netlist/builder.cpp): body of the test helper in [tests/machines/test_pattern_test.cpp](../../tests/machines/test_pattern_test.cpp) (`loaded.components.at(p.reference)`, `firstPin()`, `pinIndex(pin)`, `circuit.netOf`), returning `std::nullopt` instead of `FAIL`. (`LoadedCircuit::components` is commented "chips only" but holds connectors too — the current test relies on it; fix the comment.)

In test_pattern_test.cpp delete the local `ConnectorNet` and use a small local wrapper that `REQUIRE`s the optional.

### 1.3 — Tests

- [tests/sim/recorder_test.cpp](../../tests/sim/recorder_test.cpp): `"recorder: trim keeps the last sample"`, `"recorder: trim of an unrecorded net is a no-op"`.
- [tests/sim/simulator_test.cpp](../../tests/sim/simulator_test.cpp): `"simulator: recording continues after trimRecording"` (record, run, trim → 1 Sample equal to `value(net)`, run, new Samples appended after it).
- [tests/netlist/builder_test.cpp](../../tests/netlist/builder_test.cpp): `"builder: ConnectorNet finds the VIDEO pins"` (test_pattern.bwn: SYNC_N and VIDEO Nets differ, both present); `"builder: ConnectorNet without the connector"` (counter.bwn, `"VIDEO"` → nullopt).

### Fence

- [ ] `trim`, `trimRecording`, `ConnectorNet` exist; test_pattern_test uses the library `ConnectorNet`
- [ ] Build and test gates green
- [ ] Commit: `"tv: phase 1 — recorder trim, ConnectorNet"`

---

## Phase 2 — `Tv` deflection

New module and the timing half of `Tv` (design §2, §4, §5.1, §5.2). Painting is a no-op until phase 3.

### 2.1 — Module

- `src/devices/CMakeLists.txt` as design §7 (only `tv.cpp` for now).
- Root [CMakeLists.txt](../../CMakeLists.txt): `add_subdirectory(src/devices)` after `src/sim`.
- [tests/CMakeLists.txt](../../tests/CMakeLists.txt): add `devices/tv_test.cpp`; link `bw_devices`.

### 2.2 — `src/devices/tv.hpp` / `tv.cpp`

Public interface exactly as design §2 (`kTvColumns`, `kTvRows`, `TvFrame`, `TvFrameStats`, `Tv`), private state as §4. Constants use `bw::literals` (`using namespace literals;` inside a helper scope, or plain picosecond values with a unit comment).

- `advanceTo`, `setSyncN` per §5.1; `setVideo` = `advanceTo(time); video_ = value;`.
- `startLine`, `startFrame` per §5.2 (private).
- `paint(Time to)`: for now only `now_ = to;`.
- `assert(time >= now_)` in `advanceTo`.

### 2.3 — Tests: [tests/devices/tv_test.cpp](../../tests/devices/tv_test.cpp)

Helper `void FeedSync(Tv& tv, Time from, Time to, bool horizontal, bool vertical)`: SYNC_N 1, with 4 µs low pulses every 64 µs if `horizontal`; if `vertical`, every 20 ms SYNC_N goes low for 160 µs (horizontal pulses suppressed there). Starts with `setSyncN(from, One)`.

- `"tv: 50 Hz sync locks"`: 200 ms → `isLocked()`, `completedStats().verticalSynced`, `lines` 312 or 313, `syncedLines * 10 >= lines * 9`, consecutive `completedStats().start` 20 ms apart.
- `"tv: missing vertical sync rolls"`: horizontal only, 200 ms → frames 21 ms apart, not locked.
- `"tv: missing horizontal sync"`: vertical only → `lines` = 20 ms / 66 µs (303), `syncedLines` 0, not locked.
- `"tv: no signal"`: `advanceTo(105 ms)` with nothing fed → `completedFrames()` 5, not locked.
- `"tv: sync at the free-running end is accepted"`: line start at t (accepted sync), next falling edge at exactly t + 66 µs → counted as synced (check via `syncedLines`).
- `"tv: early vertical sync is ignored"`: vertical sync 10 ms after a frame start does not start a frame.

### Fence

- [ ] `bw_devices` builds; `tv_test` cases pass
- [ ] Build and test gates green
- [ ] Commit: `"tv: phase 2 — sync separation and deflection"`

---

## Phase 3 — Picture and BMP

### 3.1 — Painting

`Tv::paint` per design §5.3: weight 1 / 0.5 (X) / 0 (0, Z); columns clipped to `kTvColumns`, rows to `kTvRows`; `startFrame` swaps and clears as §5.2.

### 3.2 — `src/devices/bmp.hpp` / `bmp.cpp`

`void WriteGreyscaleBmp(std::ostream& out, const TvFrame& frame);` per design §5.6. Little-endian helpers in an anonymous namespace (`PutU16`, `PutU32`). Add `bmp.cpp` to `bw_devices`, `devices/bmp_test.cpp` to the tests.

### 3.3 — Tests

- tv_test.cpp:
  - `"tv: video paints its columns"`: locked 50 Hz sync; on one line VIDEO 1 from line start + 10 µs to + 20 µs → that row's columns 80–159 are 1.0, 79 and 160 are 0, in `completedFrame()` after the frame ends.
  - `"tv: X paints grey, half a column paints half"`.
  - `"tv: no signal gives black frames"`: every cell 0.
- [tests/devices/bmp_test.cpp](../../tests/devices/bmp_test.cpp): `"bmp: header and size"` (`BM`, file size 14 + 40 + 1024 + 163 840, width 512, height 320, 8 bpp, pixel offset 1078); `"bmp: rows are bottom-up"` (cell (0, 0) = 1 → byte at offset 1078 + 319 · 512 is 255).

### Fence

- [ ] Picture and BMP tests pass
- [ ] Build and test gates green
- [ ] Commit: `"tv: phase 3 — frame buffer and BMP"`

---

## Phase 4 — `TvInput` and the test pattern on the TV

### 4.1 — `src/devices/tv_input.hpp` / `tv_input.cpp`

Interface as design §2, state as §4, `update()` per §5.4. The constructor calls `sim.record(video)` and `sim.record(syncN)`. Add to `bw_devices` and `devices/tv_input_test.cpp` to the tests.

### 4.2 — Tests

- [tests/devices/tv_input_test.cpp](../../tests/devices/tv_input_test.cpp): `"tv input: trim keeps the recorder bounded"` and `"tv input: trim and keep draw the same picture"` (test_pattern.bwn, seed 1, 45 ms in 1 ms slices; `Trim` → both Nets have 1 Sample after each `update()`; `Keep` → grows; completed frames equal cell by cell).
- [tests/machines/test_pattern_test.cpp](../../tests/machines/test_pattern_test.cpp): `"test pattern: TV shows the grid"`, seeds 1–3, 50 ms in 1 ms slices → `isLocked()`; per design §5.5 for v = 1..287, h = 0..63, columns 8·(h+12)+1..+6 of row v+4 equal `Decode(h, v).video ? 1.0f : 0.0f` (`Catch::Approx` or abs < 1e-6); rows 0–3 and 292–319 all 0. Report the first 10 mismatches like the golden test.

If the row/column offset of §5.5 turns out wrong, fix the derivation in the design from the observed frame (dump with a temporary BMP), not the TV rules.

### Fence

- [ ] TvInput and test-pattern TV tests pass for all seeds
- [ ] Build and test gates green
- [ ] Commit: `"tv: phase 4 — TvInput, test pattern on the TV"`

---

## Phase 5 — CLI, release, docs

### 5.1 — `bw run --tv`

[src/cli/main.cpp](../../src/cli/main.cpp) per design §3.3: `Options::tv`, `--tv` in `ParseOptions`, `kUsage`, VIDEO connector lookup (`bw: netlist has no VIDEO connector`, exit 1), `TvInput` with `History::Keep` when `--vcd` else `Trim`, 1 ms slice loop, write BMP, print `tv_locked true|false`. [src/cli/CMakeLists.txt](../../src/cli/CMakeLists.txt): link `bw_devices`.

### 5.2 — Smoke checks

- `./build/debug/src/cli/bw run netlists/test_pattern.bwn --time 100ms --tv /tmp/tv.bmp` → `tv_locked true`; open `/tmp/tv.bmp` (`open /tmp/tv.bmp`) and ask the user to confirm the grid in Preview.
- `bw run netlists/counter.bwn --tv /tmp/x.bmp` → error, exit 1.
- Release: `cmake --preset release && cmake --build --preset release && ctest --preset release --output-on-failure`.

### 5.3 — Docs

- ARCHITECTURE.md roadmap row P2c: `done: …` with the plan link.
- Prune this plan to a summary (commit range, phases, follow-ups), as in [CONNECTORS_PLAN.md](../p2b-connectors/CONNECTORS_PLAN.md).
- GLOSSARY.md: add terms only if the user agrees (e.g. Lock).

### Fence

- [ ] `bw run … --tv` works; counter netlist rejected
- [ ] Debug and release gates green
- [ ] User confirmed the picture
- [ ] Commit: `"tv: phase 5 — bw run --tv"` (docs in the same commit)

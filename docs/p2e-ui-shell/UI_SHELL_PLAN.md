# UI Shell — Implementation Plan (P2e)

Design: [UI_SHELL_DESIGN.md](UI_SHELL_DESIGN.md)
Spec: [UI_SHELL.md](UI_SHELL.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | `SpscRing`, `TripleBuffer` (header-only) with tests | |
| 2 | `Recorder::stop`, `Consumer`, `Dispatcher`, `TvConsumer`, `bw_runner` | |
| 3 | `Runner`: simulation and devices threads, commands, status | |
| 4 | `Machine::name()`, SDL3/ImGui fetch, `bw_ui` skeleton window | |
| 5 | `bw_ui`: TV panel, controls, status line; smoke test; docs | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate: `ctest --preset debug --output-on-failure`

`clang-format -i` every changed C++ file (never CMakeLists.txt). One commit per phase.

---

## Phase 1 — Ring and triple buffer

### 1.1 — `src/runner/spsc_ring.hpp`

Per design §2 and §4. `Check()` in the constructor that capacity is a power of two ≥ 2. `tryPush`: `head = head_.load(relaxed)`, `if (head - tail_.load(acquire) == capacity) return false`, write `slots_[head & mask_]`, `head_.store(head + 1, release)`. `tryPop` mirrors with `tail_`. `static_assert(std::is_trivially_copyable_v<T>)`.

### 1.2 — `src/runner/triple_buffer.hpp`

Per design §4. `publish()`: `back_ = middle_.exchange(back_ | kFresh, acq_rel) & kIndex`. `update()`: if `!(middle_.load(relaxed) & kFresh)` return false; `front_ = middle_.exchange(front_, acq_rel) & kIndex`; return true. `kFresh = 0x4`, `kIndex = 0x3`.

### 1.3 — Tests

Add to [tests/CMakeLists.txt](../../tests/CMakeLists.txt): `runner/spsc_ring_test.cpp`, `runner/triple_buffer_test.cpp` (headers only: no new link yet). Cases per design §9 (`[runner]`): `"spsc ring: fifo"`, `"spsc ring: full and empty"`, `"spsc ring: wrap-around"`, `"spsc ring: two threads keep order"` (10 M `Sample`s, release build fast; debug acceptable), `"triple buffer: nothing before publish"`, `"triple buffer: reader gets the latest"`, `"triple buffer: never torn"` (struct of 64 equal `std::uint64_t`, two threads).

### Fence

- [ ] Build and test gates green
- [ ] Commit: `"ui shell: phase 1 — spsc ring, triple buffer"`

---

## Phase 2 — Devices side: Consumer, Dispatcher, TvConsumer

### 2.1 — `Recorder::stop`, `Simulator::stopRecording`

[src/sim/recorder.hpp](../../src/sim/recorder.hpp) / `.cpp`: `void stop(NetId net);  // no-op if not recording` — move the last slot into the stopped one (fix its `slotOfNet_` entry), pop, `slotOfNet_[net] = -1`, erase from `nets_`. [src/sim/simulator.hpp](../../src/sim/simulator.hpp): `void stopRecording(NetId net) { recorder_.stop(net); }`. Test in [tests/sim/recorder_test.cpp](../../tests/sim/recorder_test.cpp): `"recorder: stop"` (stop middle of three, others keep their Samples; restart gives a fresh first Sample).

### 2.2 — `src/runner/consumer.hpp`, `dispatcher.hpp/.cpp`

`Consumer` per design §2. `Marker`/`MarkerKind` and `kMaxWatched = 16` in `dispatcher.hpp` (the Runner reuses them). `Dispatcher`:

```cpp
class Dispatcher {
public:
    explicit Dispatcher(std::vector<Consumer*> consumers);
    void stage(std::uint8_t slot, Sample sample);  // drained from a ring
    void apply(const Marker& marker);              // design §5.2 switch
};
```

SliceEnd merge: repeatedly pick the active slot whose first unsent staged Sample has the smallest `(time, slot)` and `time <= marker.time`; send to each consumer whose `nets()` contains `slotNet_[slot]`; then erase the sent prefix of each `staged_` vector; then `advanceTo(marker.time)` on every consumer.

### 2.3 — `src/runner/tv_consumer.hpp/.cpp`

`TvPicture`, `TvConsumer` per design §2, §5.3. `nets()` returns `{video, syncN}` (that order).

### 2.4 — Module

`src/runner/CMakeLists.txt` with `add_library(bw_runner STATIC dispatcher.cpp tv_consumer.cpp)` (design §7; `runner.cpp` comes in phase 3); root `add_subdirectory(src/runner)` after `src/devices`; tests link `bw_runner`.

### 2.5 — Tests: [tests/runner/dispatcher_test.cpp](../../tests/runner/dispatcher_test.cpp)

- `RecordingConsumer` (test helper): logs `"on"`, `"N t v"`, `"adv t"`, `"off"`.
- `"dispatcher: merges by time then slot"`, `"dispatcher: keeps Samples past the slice end"`, `"dispatcher: two consumers on one Net"`, `"dispatcher: power on clears staging"`, `"dispatcher: unwatch"`.
- `"dispatcher: TV frame equals TvInput"`: test pattern seed 1, single thread: each 1 ms, copy recorder Samples (time > fedUntil) of VIDEO, SYNC_N into `stage(0|1, …)`, `apply(SliceEnd)`, `trimRecordings()`; at 60 ms `TvConsumer` picture frame == `TvInput` frame from a second run.

### Fence

- [ ] Build and test gates green
- [ ] Commit: `"ui shell: phase 2 — dispatcher, tv consumer"`

---

## Phase 3 — Runner

### 3.1 — `src/runner/runner.hpp/.cpp`

`Speed`, `RunState`, `RunnerStatus`, `Runner` per design §2, state §4, algorithms §5.1–5.2. Commands: `struct Command { CommandKind kind; std::uint64_t value; NetId net; }` with `enum class CommandKind : std::uint8_t { PowerOn, PowerOff, Pause, Resume, SetSpeed, PauseAt, Watch, Unwatch }`; each public method locks `mutex_`, appends, `wake_.notify_one()`. The constructor queues `Watch` for each consumer's `nets()` in order (skipping Nets already watched), then starts `devices_`, then `simulation_` (`std::jthread`, loops check `stop_token`; the destructor requests stop, notifies `wake_` and `markersPushed_`). Pacing uses `std::chrono::steady_clock`. `status().ringWaitShare = waitedNs_ / max(1, runNs_)`. `waitIdle()`: loop until no queued commands, `state_ != Running` and `markersDone_ == markersPushed_` (wait on `markersDone_` with `std::atomic::wait`). Add `runner.cpp` to `bw_runner`.

### 3.2 — Tests: [tests/runner/runner_test.cpp](../../tests/runner/runner_test.cpp)

Per design §9 (`[runner]`): `"runner: frame equals TvInput"` (GENERATE seeds 1, 2), `"runner: tiny ring gives the same frame"` (capacity 4), `"runner: second consumer on VIDEO"`, `"runner: pause and resume"`, `"runner: power cycle"`, `"runner: power off shows black"`, `"runner: watch and unwatch"`. Every test: `setSpeed(Max)`, `pauseAt(t)`, `powerOn(seed)`, `waitIdle()`, then read `pictures().update()` / `front()`. Headless reference helper: `TvInput` loop to 60 ms with `trimRecordings()` (as in `tests/machines/test_pattern_faults_test.cpp`).

### Fence

- [ ] Runner tests pass in debug (ASan/UBSan) and release, 3 runs in a row (`ctest --repeat until-fail:3 -R runner`)
- [ ] Build and test gates green
- [ ] Commit: `"ui shell: phase 3 — threaded runner"`

---

## Phase 4 — Build integration and window skeleton

### 4.1 — `Machine::name()`

[src/machine/machine.hpp](../../src/machine/machine.hpp) / `.cpp`: member `std::string name_`, set in `LoadMachine` from `puzzle.machine`; accessor per design §3.2. Extend `"machine: no faults builds the design"` with `CHECK(machine->name() == "test_pattern")`.

### 4.2 — Fetch SDL3 and ImGui

`src/ui/CMakeLists.txt` per design §7 (SDL3 `release-3.4.18`, ImGui `v1.93.0`). Root: `option(BW_BUILD_UI "Build bw_ui" ON)`, `if(BW_BUILD_UI) add_subdirectory(src/ui) endif()` after `src/cli`. If SDL's CMake needs other cache options to build statically on macOS, add them here and note them in the design.

### 4.3 — `src/ui/main.cpp` skeleton

Load per design §5.4 (errors, VIDEO check, title); `SDL_Init(SDL_INIT_VIDEO)`, `SDL_CreateWindow(title, 1024, 768, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)`, `SDL_CreateRenderer(window, nullptr)`, `SDL_SetRenderVSync(renderer, 1)`; ImGui context + both backends; loop: events (`ImGui_ImplSDL3_ProcessEvent`, `SDL_EVENT_QUIT` / window close → exit), new frame, one ImGui text "Bodgewire", render. Usage error (no file) → `usage: bw_ui <file.bwn|file.bwp>`, exit 2.

### Fence

- [ ] Debug and release build with `bw_ui`; tests green
- [ ] `build/debug/src/ui/bw_ui netlists/test_pattern.bwn` opens a window titled `Bodgewire — test_pattern`; `bw_ui missing.bwn` prints the error, exit 1
- [ ] Commit: `"ui shell: phase 4 — sdl3/imgui build, bw_ui window"`

---

## Phase 5 — Live TV, controls, status

### 5.1 — `src/ui/tv_view.hpp/.cpp`

Per design §2 and §5.4: streaming `SDL_PIXELFORMAT_RGBA32` texture `kTvColumns × kTvRows`, linear scale mode, `show()` converts and uploads, `draw(available)` fits 4:3 (width = min(available.x, available.y · 4/3)), centred, `ImGui::Image(reinterpret_cast<ImTextureID>(texture_), size)` (check the v1.93 `ImTextureID`/`ImTextureRef` signature).

### 5.2 — `main.cpp`: runner and panels

`TvConsumer`, `Runner`, `powerOn(1)`; controls row, TV, status line per design §5.4 (Power toggles `powerOn(seed)` / `powerOff()`; Pause toggles; Speed radio `1×` / `Max`; seed `ImGui::InputScalar` `ImGuiDataType_U64`). Ratio over a 1 s sliding window of `(wall, simTime)` pairs. Status format: `t {:.3f} s  {:.2f}×  locked|no lock  running|paused|off  ring wait {:.0f}%`.

### 5.3 — Smoke test (manual, ask the user)

- `bw_ui netlists/test_pattern.bwn`: grid, locked within 0.5 s, ≈ 1.00×, ring wait < 25%.
- `bw_ui puzzles/stuck_gate.bwp`: white box; title `Bodgewire — test_pattern`.
- Pause/Resume, Power off (black) / on, seed change, `Max` ratio ≥ 80% of `bw run netlists/test_pattern.bwn --time 5s --tv /tmp/x.bmp` (release builds).
- Drag/resize the window for 2 s at `1×`: simulated time keeps pace afterwards (ratio stays ≈ 1.00).

### 5.4 — Docs

- ARCHITECTURE.md roadmap row P2e: `done: …`, link to this plan.
- Prune this plan to a summary.

### Fence

- [ ] User confirms the smoke test
- [ ] Debug and release gates green
- [ ] Commit: `"ui shell: phase 5 — live tv, controls, status"`

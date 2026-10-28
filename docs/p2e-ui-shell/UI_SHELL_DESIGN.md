# UI Shell — Detailed Design

Implements the specification in [UI_SHELL.md](UI_SHELL.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/runner/                  new module bw_runner: live run on threads, no SDL/ImGui
  spsc_ring.hpp              SpscRing<T>: lock-free single-producer/single-consumer ring
  triple_buffer.hpp          TripleBuffer<T>: latest-value handoff, never torn
  consumer.hpp               Consumer: what the devices thread feeds (TV now, instruments later)
  dispatcher.hpp/.cpp        Dispatcher: devices-side staging, time merge, fan-out (single-threaded)
  tv_consumer.hpp/.cpp       TvConsumer: Tv as a Consumer, publishes TvPicture
  runner.hpp/.cpp            Runner: simulation thread + devices thread, commands, status
src/ui/                      bw_ui executable (SDL3 + Dear ImGui)
  main.cpp                   load, window, main loop
  tv_view.hpp/.cpp           TvView: TvPicture → SDL texture, 4:3 fit
tests/runner/                spsc_ring_test, triple_buffer_test, dispatcher_test, runner_test
```

`runner` is headless and unit-tested; `ui` is a thin shell over it (manual smoke test only, S6).

## 2. Public Interface

```cpp
// src/runner/spsc_ring.hpp — capacity is a power of two; T trivially copyable
template <class T>
class SpscRing {
public:
    explicit SpscRing(std::size_t capacity);
    bool tryPush(const T& value);  // producer; false if full
    std::optional<T> tryPop();     // consumer; nullopt if empty
    bool empty() const;            // either side, a snapshot
};

// src/runner/triple_buffer.hpp — one writer, one reader
template <class T>
class TripleBuffer {
public:
    T& back();         // writer: fill, then publish()
    void publish();
    bool update();     // reader: true if a newer value was taken
    const T& front() const;
};
```

```cpp
// src/runner/consumer.hpp — runs on the devices thread only
class Consumer {
public:
    virtual ~Consumer() = default;
    virtual std::span<const NetId> nets() const = 0;  // watched in this order (ties, 5.2)
    virtual void powerOn() = 0;                        // time restarts at 0
    virtual void sample(NetId net, Sample sample) = 0; // in time order across all its Nets
    virtual void advanceTo(Time time) = 0;             // end of a slice
    virtual void powerOff() = 0;
};

// src/runner/tv_consumer.hpp
struct TvPicture {
    TvFrame frame;  // black when off or before the first frame
    bool locked = false;
};
class TvConsumer final : public Consumer {
public:
    TvConsumer(NetId video, NetId syncN);
    TripleBuffer<TvPicture>& pictures() { return pictures_; }  // the UI reads
    // Consumer overrides
};
```

```cpp
// src/runner/runner.hpp
enum class Speed : std::uint8_t { RealTime, Max };
enum class RunState : std::uint8_t { Off, Running, Paused };

struct RunnerStatus {
    RunState state;
    Time simTime;              // ps
    std::uint64_t seed;
    double ringWaitShare;      // ring waits / (runUntil + ring pushes); pacing sleeps excluded (S5)
};

// Owns a Simulator on the Circuit; consumers and Circuit must outlive it. Starts Off.
class Runner {
public:
    static constexpr std::size_t kRingCapacity = 1 << 16;  // Samples per watched Net

    Runner(Circuit& circuit, std::vector<Consumer*> consumers,
           std::size_t ringCapacity = kRingCapacity);
    ~Runner();  // stops and joins both threads

    // Commands: any thread, applied at the next slice boundary (S3).
    void powerOn(std::uint64_t seed);
    void powerOff();
    void pause();
    void resume();
    void setSpeed(Speed speed);
    void pauseAt(Time time);  // one-shot: pause at the first slice end >= time (queue before powerOn)
    void watch(NetId net);    // counted: the Net's ring is written while any watch is held
    void unwatch(NetId net);  // undoes one watch(); the slot is freed when none remain

    void waitIdle();  // blocks until all commands applied, not Running, devices caught up
    RunnerStatus status() const;
};
```

```cpp
// src/ui/tv_view.hpp
class TvView {
public:
    explicit TvView(SDL_Renderer* renderer);  // streaming 512×320 texture, linear filtering
    ~TvView();
    void show(const TvPicture& picture);       // upload if the frame changed
    void draw(ImVec2 available);               // ImGui::Image, largest 4:3 rect, centred
};
```

## 3. Integration Points

1. **`Recorder::stop(NetId)` / `Simulator::stopRecording(NetId)`** — [src/sim/recorder.hpp](../../src/sim/recorder.hpp): removes the Net's slot (swap with the last slot, fix `slotOfNet_`, erase from `nets_`). Needed by `unwatch`. Not on the hot path; `isRecording` unchanged, so zero cost in `runUntil`.
2. **`Machine::name()`** — [src/machine/machine.hpp](../../src/machine/machine.hpp): `std::string_view name() const { return name_; }`, set from `Puzzle::machine` in `LoadMachine`. The window title must not use the `.bwp` file name (it names the Fault).
3. **Root CMake** — `add_subdirectory(src/runner)` after `src/devices`; `option(BW_BUILD_UI "Build bw_ui" ON)` and `if(BW_BUILD_UI) add_subdirectory(src/ui) endif()`. Tests link `bw_runner`.

No change to `Simulator::runUntil`: the Recorder is the per-slice staging buffer, copied into the rings at each slice end (5.1). Cost per Sample: one Recorder append (as today) + one ring push (~2 ns).

## 4. Internal State

```cpp
// SpscRing: head_ written by the producer, tail_ by the consumer, on separate cache lines.
std::vector<T> slots_; std::size_t mask_;
alignas(64) std::atomic<std::size_t> head_{0};
alignas(64) std::atomic<std::size_t> tail_{0};

// TripleBuffer: three T; writer owns back_, reader owns front_; middle_ swapped atomically.
std::array<T, 3> buffers_;
std::uint8_t back_ = 0, front_ = 1;
std::atomic<std::uint8_t> middle_{2};  // bit 0x4 = fresh (published, not yet taken)
```

```cpp
// Control markers, simulation → devices, one SpscRing<Marker> (capacity 1024). In dispatcher.hpp.
inline constexpr std::size_t kMaxWatched = 16;  // ring slots
enum class MarkerKind : std::uint8_t { PowerOn, SliceEnd, Watch, Unwatch, PowerOff };
struct Marker {
    MarkerKind kind;
    std::uint8_t slot;  // Watch/Unwatch
    NetId net;          // Watch
    Time time;          // SliceEnd: slice end, ps
};

class Runner {
    Simulator sim_;                                     // simulation thread only
    std::vector<std::unique_ptr<SpscRing<Sample>>> rings_;  // kMaxWatched slots
    SpscRing<Marker> markers_{1024};
    Dispatcher dispatcher_;                             // devices thread only

    // simulation thread
    struct Watched { NetId net; int count; Time pushedUntil; };  // Samples up to here pushed
    std::array<std::optional<Watched>, kMaxWatched> watched_;
    std::array<std::atomic<bool>, kMaxWatched> slotFree_;  // all true; false on Watch, true again
                                                           // once devices has applied Unwatch
    std::optional<Time> pauseAt_;

    std::mutex mutex_; std::condition_variable wake_;   // commands
    std::vector<Command> commands_;                     // guarded by mutex_
    std::uint64_t commandsQueued_ = 0;                  // guarded by mutex_
    std::atomic<std::uint64_t> commandsApplied_{0};     // waitIdle
    std::atomic<std::uint64_t> markersPushed_{0}, markersDone_{0};  // waitIdle, power barrier
    std::atomic<std::uint64_t> devicesWake_{0};         // bumped on each marker and full ring
    std::atomic<RunState> state_{RunState::Off};
    std::atomic<Time> simTime_{0};
    std::atomic<std::int64_t> waitedNs_{0}, runNs_{0};
    std::jthread simulation_, devices_;
};

class Dispatcher {
    std::vector<Consumer*> consumers_;
    std::array<NetId, kMaxWatched> slotNet_;
    std::array<std::vector<Sample>, kMaxWatched> staged_;  // drained, not yet dispatched
    std::vector<bool> active_;
};
```

## 5. Key Algorithms

### 5.1 Simulation thread (S3)

```
loop until stop:
  apply commands (under mutex_), in order:
    PowerOn(seed): wait markersDone_ == markersPushed_   (all old Samples consumed)
                   sim_.powerOn(seed); re-record watched Nets, pushedUntil = -1
                   push PowerOn; state Running; rebase pacing; seed saved
    PowerOff:      push PowerOff; state Off
    Pause/Resume:  state Paused / Running (Resume rebases pacing)
    Speed:         set; rebase pacing
    PauseAt(t):    pauseAt_ = t
    Watch(net):    already watched: count += 1
                   else lowest s with slotFree_[s]: slotFree_[s] = false; sim_.record(net);
                   watched_[s] = {net, 1, -1}; push Watch(s, net)
    Unwatch(net):  count -= 1; at 0: push Unwatch(s); sim_.stopRecording(net); watched_[s] = nullopt
    then commandsApplied_ += 1 per command (notify)
  if state != Running: wait on wake_; continue
  if speed == RealTime:
      due = wallBase + (sim_.now() + 1 ms − simBase)      (as wall duration)
      if due < wall now − 100 ms: rebase                  (fell behind: never catch up in a burst)
      else wait_until(due) on wake_ (commands interrupt)
  sim_.runUntil(sim_.now() + 1 ms)
  for each watched slot s: for Samples of net with time > pushedUntil:
      while !rings_[s].tryPush(sample): sleep 50 µs, waitedNs_ += …   (never drop, S3)
      pushedUntil = sim_.now()
  push SliceEnd(sim_.now()) (same wait); sim_.trimRecordings(); simTime_ = sim_.now()
  runNs_ += time of runUntil + pushes (waits included)
  if pauseAt_ && sim_.now() >= *pauseAt_: state Paused; pauseAt_ = nullopt
```

The devices thread drains rings into `staged_` without limit, so the real bound on Samples in flight is the marker ring: at most 1024 slices (≈ 1 s simulated) ahead of the devices thread.

`markersPushed_` is incremented after each marker push. `devicesWake_` is bumped and notified after each marker push and on every full-ring wait; the devices thread waits on it when idle, so a full ring always gets drained (no deadlock with a devices thread waiting for a marker).

### 5.2 Devices thread and Dispatcher (S4)

```
loop until stop:
  seen = devicesWake_; if stop: return
  for each active slot: drain ring into staged_[slot]          (keeps the producer unblocked)
  m = markers_.tryPop(); if none: devicesWake_.wait(seen); continue
  for each active slot: drain ring again                       (acquire: every Sample before m)
  switch m.kind:
    Watch:    slotNet_[slot] = net; active
    Unwatch:  inactive; staged_[slot].clear(); slotFree_[slot] = true
    PowerOn:  every consumer powerOn()   (don't clear staged_: it may already hold the new run's
              Samples; nothing stale is left, as the Runner waits for the devices thread first)
    PowerOff: every consumer powerOff()
    SliceEnd: merge staged Samples with time <= m.time over active slots, by (time, slot);
              each goes to every consumer whose nets() contains its Net;
              erase them from staged_; every consumer advanceTo(m.time)
  markersDone_ += 1 (notify)
```

Ties at the same time go in slot order. Slots are allocated lowest-free first and the Runner watches each consumer's `nets()` in order at construction (never unwatched while the Runner lives), so `TvConsumer` gets VIDEO before SYNC_N at equal times, exactly like `TvInput::update()` (`video.time <= syncN.time`). The Tv thus receives the same calls as on the headless path: identical frames (S4).

The ring wait cannot deadlock: the devices thread drains rings without waiting for a marker.

### 5.3 TvConsumer

```
powerOn():   tv_ = Tv{}; lastFrames_ = 0
sample():    net == video_ ? tv_.setVideo(...) : tv_.setSyncN(...)
advanceTo(t): tv_.advanceTo(t);
             if tv_.completedFrames() != lastFrames_: back() = {completedFrame, isLocked}; publish()
powerOff():  back() = {TvFrame{}, false}; publish()       (black, S3)
```

### 5.4 UI main loop (S1, S2)

```
load: .bwp → LoadPuzzleFile + build, title name = machine.name(); else LoadNetlistFile, name = stem
      errors → stderr, exit 1; no VIDEO connector → "bw_ui: netlist has no VIDEO connector", exit 1
SDL_Init(VIDEO); window "Bodgewire — NAME" (resizable, high pixel density); renderer, vsync 1
ImGui: ImGui_ImplSDL3_InitForSDLRenderer, ImGui_ImplSDLRenderer3_Init
TvConsumer tv(video, syncN); Runner runner(circuit, {&tv}); runner.powerOn(1)
each frame: poll events (quit → break)
  if tv.pictures().update(): view.show(tv.pictures().front())
  one full-window ImGui window, no decoration:
    row: [Power on/off] [Pause/Resume] Seed (InputScalar u64) Speed (1× | Max radio)
    TV: view.draw(remaining size minus one status line)
    status: "t 12.345 s  1.00×  locked  running  ring wait 3%"
  speed ratio: Δ simTime / Δ wall over a 1 s sliding window, computed on the UI thread
render
shutdown: runner destroyed first (joins threads), then ImGui, SDL
```

`TvView::show()` converts cells to bytes (`std::lround(cell * 255)`, grey RGBA) into a reusable buffer and `SDL_UpdateTexture`; `SDL_SetTextureScaleMode(SDL_SCALEMODE_LINEAR)`.

## 6. Reused Infrastructure

- `Simulator` (`powerOn`, `runUntil`, `record`, `recorder()`, `trimRecordings`): unchanged run loop; the Recorder stages each slice.
- `Tv` (`setVideo`, `setSyncN`, `advanceTo`, `completedFrame`, `isLocked`): the same core as `TvInput`; `TvConsumer` replaces only its Recorder adaptor.
- `LoadNetlistFile`, `LoadPuzzleFile`, `Machine::build`, `ConnectorNet`, `Format(Diagnostic)`: loading and errors as in `bw run`.
- `Sample` (recorder.hpp) is the ring element.

## 7. Build Integration

```cmake
# src/runner/CMakeLists.txt
add_library(bw_runner STATIC dispatcher.cpp runner.cpp tv_consumer.cpp)
target_link_libraries(bw_runner PUBLIC bw_devices PRIVATE bw_warnings)

# src/ui/CMakeLists.txt
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.4.18)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.93.0)
FetchContent_MakeAvailable(SDL3 imgui)
add_library(bw_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
target_include_directories(bw_imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(bw_imgui PUBLIC SDL3::SDL3-static)
add_executable(bw_ui main.cpp tv_view.cpp)
target_link_libraries(bw_ui PRIVATE bw_machine bw_runner bw_imgui bw_warnings)
```

ImGui is included as SYSTEM so `-Werror` applies to our code only. Tests never link SDL or ImGui.

## 8. Dependency Diagram

```
ui ──► runner ──► devices ──► sim
 └───► machine ──► board ──► netlist ──► parts ──► sim
 └───► bw_imgui ──► SDL3
```

## 9. Testing

- **spsc_ring_test**: push/pop FIFO, full at capacity, empty, wrap-around over many cycles; two threads, 10 M Samples with increasing time, consumer checks order and content.
- **triple_buffer_test**: `update()` false before any publish; after publish → true, front is it; two publishes before a read → the latest; two threads, writer publishes structs whose fields must all agree, reader never sees a mix.
- **dispatcher_test** (single-threaded): Watch + Samples + SliceEnd feed a recording Consumer in (time, slot) order; Samples past the SliceEnd stay staged; two consumers on one Net get the same Samples; PowerOn keeps the new run's staged Samples; Unwatch frees the slot.
- **runner_test** (`[runner]`, test pattern, `Speed::Max`, `pauseAt` + `waitIdle`, no wall clock):
  - seeds 1–2: TvConsumer frame at 60 ms == `TvInput` frame at 60 ms (headless helper as in test_pattern_faults_test);
  - ring capacity 4: same frame;
  - a second Consumer on VIDEO sees the same Samples as a Recorder-based reference;
  - pause at 20 ms, resume, pause at 60 ms: same frame; power on seed 2, run to 30 ms, power on seed 1, pause at 60 ms: frame of a fresh seed-1 run;
  - power off → published picture black, not locked;
  - watch a Net mid-run (a test Consumer whose `nets()` lists a Net no other Consumer uses, unwatched once right after construction) → it gets the initial Sample at the watch time, then changes; unwatch → nothing more;
  - watch VIDEO a second time then unwatch it once → the TV still gets VIDEO (counted watches).
- **Manual (bw_ui)**: Done-when list of UI_SHELL.md, plus: drag/resize the window for 2 s at `1×`, simulated time keeps pace (S5); status line ring wait < 25% at `1×`; `Max` ratio ≥ 80% of `bw run --tv`.

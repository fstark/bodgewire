# TV — Detailed Design

Implements the specification in [TV.md](TV.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/devices/            new module bw_devices: Output Devices, outside the Circuit
  tv.hpp, tv.cpp        Tv: sync separation, deflection, frame buffer (S1–S3); pure, fed with edges
  tv_input.hpp/.cpp     TvInput: feeds a Tv from two recorded Nets of a running Simulator
  bmp.hpp, bmp.cpp      WriteGreyscaleBmp(): TvFrame → 8-bit BMP (S4)
tests/devices/
  tv_test.cpp           synthetic sync (S5 second bullet), picture integration
  tv_input_test.cpp     feeding from a Simulator, bounded Recorder
  bmp_test.cpp          header and pixel bytes
```

Split rationale: `Tv` knows nothing about the Simulator, so synthetic tests drive it with exact edge times. `TvInput` is the only code that touches both. Future Output Devices (LED displays) join `src/devices/`.

## 2. Public Interface

```cpp
// src/devices/tv.hpp
inline constexpr int kTvColumns = 512;
inline constexpr int kTvRows = 320;

struct TvFrame {
    std::vector<float> cells = std::vector<float>(kTvRows * kTvColumns);  // row-major, 0 black..1 white
    float at(int row, int column) const { return cells[std::size_t(row) * kTvColumns + column]; }
};

struct TvFrameStats {
    Time start = 0;                // ps
    bool verticalSynced = false;   // started on an accepted vertical sync
    int lines = 0;                 // lines started during the frame
    int syncedLines = 0;           // of which on an accepted horizontal sync
};

// Monitor on a VIDEO connector (TV.md). Time only moves forward; at t = 0 both inputs are Z.
class Tv {
public:
    static constexpr Time kColumnTime = 125_ns;
    static constexpr Time kLineFreeRun = 66_us, kLineAccept = 56_us;
    static constexpr Time kFrameFreeRun = 21_ms, kFrameAccept = 16_ms;
    static constexpr Time kVerticalSyncLow = 16_us;

    void setVideo(Time time, LogicValue value);  // advanceTo(time), then apply
    void setSyncN(Time time, LogicValue value);  // advanceTo(time), then apply
    void advanceTo(Time time);                   // runs deflection and paints up to `time`
    const TvFrame& completedFrame() const { return completed_; }  // black before the first
    const TvFrameStats& completedStats() const { return completedStats_; }
    std::uint64_t completedFrames() const { return completedFrames_; }
    bool isLocked() const { return locked_; }
};
```

```cpp
// src/devices/tv_input.hpp
// Plugs a Tv into two Nets of a Simulator. Construct after powerOn() (it clears recordings).
class TvInput {
public:
    enum class History : std::uint8_t { Trim, Keep };  // Keep when a VCD needs the samples
    TvInput(Simulator& sim, NetId video, NetId syncN, History history);
    void update();  // feeds the changes recorded since the last update, then advanceTo(sim.now())
    const Tv& tv() const { return tv_; }
};
```

```cpp
// src/devices/bmp.hpp
void WriteGreyscaleBmp(std::ostream& out, const TvFrame& frame);  // kTvColumns × kTvRows, 8 bit
```

```cpp
// src/netlist/builder.hpp (moved from tests/machines/test_pattern_test.cpp)
// Net on `pin` of the first `part` connector, e.g. ("VIDEO", "SYNC_N"); nullopt if none.
std::optional<NetId> ConnectorNet(const LoadedCircuit& loaded, std::string_view part,
                                  std::string_view pin);
```

## 3. Integration Points

1. **Recorder trimming** — [src/sim/recorder.hpp](../../src/sim/recorder.hpp): add `void trim(NetId net);` keeping only the last Sample (the current value). [src/sim/simulator.hpp](../../src/sim/simulator.hpp): add `void trimRecording(NetId net) { recorder_.trim(net); }`. Not on the run loop; zero cost when unused.
2. **Connector lookup** — `ConnectorNet()` added to [src/netlist/builder.cpp](../../src/netlist/builder.cpp); the local copy in [tests/machines/test_pattern_test.cpp](../../tests/machines/test_pattern_test.cpp) is deleted and the test calls the library one (`REQUIRE` on the optional).
3. **CLI** — [src/cli/main.cpp](../../src/cli/main.cpp):
   - `Options::tv` (path); `--tv` joins the value options in `ParseOptions()`; `kUsage` gains `[--tv FILE.bmp]`.
   - In `Run()`, after `sim.powerOn()` and the `record()` loop: if `--tv`, look up `ConnectorNet(loaded, "VIDEO", "VIDEO")` and `"SYNC_N"`; none → `bw: netlist has no VIDEO connector`, exit `kExitNetlistErrors`. Build `TvInput` with `History::Keep` if `--vcd` is given, else `Trim`.
   - Replace `sim.runFor(options.time)` with slices when a TV is attached:

     ```cpp
     const Time end = sim.now() + options.time;
     while (sim.now() < end) {
         sim.runUntil(std::min(end, sim.now() + 1_ms));
         tvInput->update();
     }
     ```
   - After the run: write the BMP (`cannot write FILE` → usage error, as for `--vcd`) and print `tv_locked true|false`.
   - `bw` links `bw_devices`.

## 4. Internal State

```cpp
class Tv {
    // inputs
    LogicValue video_ = LogicValue::Z, syncN_ = LogicValue::Z;
    Time lowSince_ = 0;              // ps, last 1→0 of SYNC_N
    bool verticalSyncSeen_ = true;   // current low period already produced a vertical sync
    // deflection
    Time now_ = 0;                   // painted up to here
    Time lineStart_ = 0, frameStart_ = 0;
    int row_ = 0;                    // lines since frame start
    TvFrameStats stats_;             // frame being drawn
    // picture
    TvFrame drawing_, completed_;
    TvFrameStats completedStats_;
    std::uint64_t completedFrames_ = 0;
    bool previousVerticalSynced_ = false, locked_ = false;
};

class TvInput {
    Simulator& sim_;
    NetId video_, syncN_;
    History history_;
    std::size_t videoFed_ = 0, syncNFed_ = 0;  // Samples already fed, per Net
    Tv tv_;
};
```

Memory: two frames of 512 × 320 floats (1.3 MB) plus at most one slice of Samples; constant over any run length (S3).

## 5. Key Algorithms

### 5.1 Advancing (S1, S2)

Internal events strictly before `time` are handled in order; an input change at `time` is applied before internal events at `time` (so a sync arriving exactly when the free-running period ends is accepted).

```
advanceTo(time):
  loop:
    lineEnd  = lineStart_ + kLineFreeRun
    frameEnd = frameStart_ + kFrameFreeRun
    vsyncAt  = syncN_ == 0 && !verticalSyncSeen_ ? lowSince_ + kVerticalSyncLow : ∞
    next = min(lineEnd, frameEnd, vsyncAt)
    if next >= time: break
    paint(next)
    if next == vsyncAt:
        verticalSyncSeen_ = true
        if next - frameStart_ >= kFrameAccept: startFrame(next, synced = true)
    else if next == lineEnd:  startLine(next, synced = false)
    else:                     startFrame(next, synced = false)
  paint(time)

setSyncN(time, value):
  advanceTo(time)
  if syncN_ == 1 && value == 0:                        // falling edge = 1 → 0 only
      lowSince_ = time; verticalSyncSeen_ = false
      if time - lineStart_ >= kLineAccept: startLine(time, synced = true)
  syncN_ = value
```

### 5.2 Line and frame starts (S2, S3)

The two oscillators are independent: a frame start resets the row but not the column, as on a real set.

```
startLine(t, synced):  lineStart_ = t; ++row_; ++stats_.lines; stats_.syncedLines += synced
startFrame(t, synced):
  locked_ = synced && stats_.verticalSynced && stats_.syncedLines * 10 >= stats_.lines * 9
  swap(drawing_, completed_); fill(drawing_, 0); completedStats_ = stats_; ++completedFrames_
  frameStart_ = t; row_ = 0; stats_ = {t, synced, 0, 0}
```

### 5.3 Painting (S3)

`paint(to)` integrates VIDEO over `[now_, to)`, which lies inside one line because every line start calls `paint` first.

```
paint(to):
  weight = video_ == 1 ? 1 : video_ == X ? 0.5 : 0      // 0 and Z paint nothing
  if weight > 0 && row_ < kTvRows:
    for each column c in [(now_ - lineStart_) / kColumnTime, (to - lineStart_ - 1) / kColumnTime], c < kTvColumns:
      overlap = |[now_, to) ∩ [lineStart_ + c·kColumnTime, +kColumnTime)|
      drawing_.cells[row_, c] += weight · overlap / kColumnTime
  now_ = to
```

### 5.4 Feeding from the Simulator

```
update():
  v = sim_.recorder().samples(video_)[videoFed_..]; s = ...samples(syncN_)[syncNFed_..]
  merge v and s by time (video first on ties): tv_.setVideo / tv_.setSyncN
  tv_.advanceTo(sim_.now())
  Trim: sim_.trimRecording(each); videoFed_ = syncNFed_ = 1     // the kept Sample was fed
  Keep: videoFed_ = v.size(); syncNFed_ = s.size()
```

The first `update()` feeds the initial Sample written by `record()`. The connector Pins are TTL inputs, so an undriven VIDEO Net reads 1 (ARCHITECTURE.md, Logic values): a broken video trace shows white, as on a real TTL monitor.

### 5.5 Where the test pattern lands (S5)

Hsync falls at h = 52, so column = 8·((h − 52) mod 64) + offset within the pixel. The frame starts 16 µs into the first vsync line (v = 308) while the line that began at v = 307, h = 52 is row 0. During vsync the falling edges arrive at h = 56: one line free-runs (v = 308, h = 54), then lines start at h = 56 (v = 309–311) and at h = 52 from v = 0 on. So for v ≥ 1 and h < 52: **row = v + 4, column = 8·(h + 12) + 0..7**. Row 4 (v = 0) is shifted 4 µs left, the "hook" a real set shows at the top. Each frame has 312 lines, 311 synced: locked.

### 5.6 BMP (S4)

`BITMAPFILEHEADER` (14 bytes) + `BITMAPINFOHEADER` (40 bytes, height positive = bottom-up) + 256-entry grey palette + 512 × 320 bytes (stride 512, already a multiple of 4). Pixel = `round(clamp(cell, 0, 1) · 255)`. Little-endian fields written byte by byte.

## 6. Reused Infrastructure

- `Simulator::record()` / `Recorder::samples()`: the TV's only view of the Nets; no new hook in `runUntil()`.
- `Time`, literals, `LogicValue` from `src/sim`.
- `LoadedCircuit::parts`, `Component::firstPin()/pinIndex()`, `Circuit::netOf()`: connector lookup, moved from the test into `ConnectorNet()`.
- The test-pattern golden `Decode()` in `test_pattern_test.cpp` for the picture check.
- CLI option parsing, error exits and output-file handling as for `--vcd`.

## 7. Build Integration

```cmake
# src/devices/CMakeLists.txt
add_library(bw_devices STATIC
  bmp.cpp
  tv.cpp
  tv_input.cpp
)
target_link_libraries(bw_devices PUBLIC bw_sim PRIVATE bw_warnings)
```

- Root `CMakeLists.txt`: `add_subdirectory(src/devices)` after `src/sim`.
- `src/cli/CMakeLists.txt`: `bw` links `bw_board bw_devices`.
- `tests/CMakeLists.txt`: add `devices/bmp_test.cpp`, `devices/tv_input_test.cpp`, `devices/tv_test.cpp`; link `bw_devices`.

## 8. Dependency Diagram

```
cli ──► board ──► netlist ──► parts ──► sim
 └────► devices ─────────────────────────┘
```

`devices` depends on `sim` only; nothing depends on `devices` except `cli` and tests.

## 9. Testing

**tv_test.cpp** (synthetic, `[devices]`), with a helper emitting SYNC_N: 4 µs low every 64 µs, plus 160 µs low every 20 ms for vertical sync.
- 50 Hz composite sync: after 3 frames `isLocked()`, `completedStats()` has `verticalSynced`, ~312 lines, all but the vsync lines synced, frames 20 ms apart.
- Vertical sync missing: frames 21 ms apart, not locked.
- Horizontal sync missing (vertical kept): `completedStats().lines` = frame time / 66 µs, `syncedLines` = 0, not locked.
- No signal: frames every 21 ms, completed frame all black, not locked.
- Picture: VIDEO 1 from 10 µs to 20 µs after a line start → columns 80–159 of that row are 1.0, neighbours 0; X paints 0.5; half a column of 1 paints 0.5.
- Sync falling exactly at the free-running end is accepted (5.1 tie rule).

**tv_input_test.cpp**: on `test_pattern.bwn`, `Trim` keeps both recordings at 1 Sample after `update()`, `Keep` grows; both produce an identical completed frame.

**bmp_test.cpp**: file size 14 + 40 + 1024 + 163 840; header fields; a white top-left cell lands at the last pixel row's first byte.

**test_pattern_test.cpp** (S5 first bullet), seeds 1–3, 1 ms slices until locked (at most 500 ms; measured 39–314 ms), then one more frame: for v = 1..287, h = 0..63, columns 8·(h+12)+1..+6 of row v+4 equal `Decode(h, v).video` (1.0 or 0.0); rows 0–3 and 292–319 black. Boundary columns are skipped: video and sync edge delays differ by a few ns.

**builder_test.cpp**: `ConnectorNet()` finds J2's SYNC_N Net in test_pattern.bwn; nullopt for counter.bwn's VIDEO.

**recorder_test.cpp / simulator_test.cpp**: `trim()` keeps the last Sample; recording continues after a trim.

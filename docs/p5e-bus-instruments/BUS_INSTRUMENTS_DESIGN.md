# P5e — Instruments for buses — Design

Spec: [BUS_INSTRUMENTS.md](BUS_INSTRUMENTS.md). Plan: [BUS_INSTRUMENTS_PLAN.md](BUS_INSTRUMENTS_PLAN.md). Recipe: [ADD_INSTRUMENT.md](../recipes/ADD_INSTRUMENT.md): its layers (engine, headless check, consumer, Session, placement, tool, window, CLI) are followed in that order; each section below names the layer it changes. No new instrument, no new consumer.

## Files

| File | Change |
|---|---|
| `src/instruments/logic_analyzer.{hpp,cpp}` | `kChannels` 32, `kDepth` 65 536, `ChannelMask` (`std::uint32_t`), `ChannelBits` and `change()` on it, mask Triggers, `ParseBusPattern`, `FormatBusPattern`, `NameStem` |
| `src/instruments/oscilloscope.{hpp,cpp}` | `kScopeChannels` 4, `ScopeSettings::delay`, `kMaxDelayDivisions`, `Sweep::delay`, `startSweep` with the delay |
| `src/runner/spsc_ring.hpp` | storage allocated without being written |
| `src/runner/dispatcher.{hpp,cpp}` | `kMaxWatched` 96; `sliceEnd` as a heap merge, Consumers per slot once a slice (below) |
| `src/runner/analyzer_consumer.{hpp,cpp}` | masks `ChannelMask` |
| `src/runner/scope_consumer.cpp` | nothing but what `kScopeChannels` changes (masks stay `std::uint8_t`: 4 bits) |
| `src/machine/machine.cpp` | `probeNet` falls back to the pin's Net |
| `src/session/session.{hpp,cpp}` | `ChannelGroup`, `clipGroup`, `group`, `ungroup`, `unclipGroup`, `setGroupTrigger`, `groupTrigger`, `groups`, `groupOf`; `unclip` leaves the group |
| `src/ui/board_view.{hpp,cpp}` | `BoardClick::shift`; `kChannelColours` stays 8 (`ChannelColour(c)`), `kHookColours` 4; `refuse(reason)` drawn at the cursor; `ClipTag::colour` |
| `src/ui/toolbar.cpp` | tooltips on `Probe (P)`, `Clip (C)`, `Hook (O)`, `Meter (V)` |
| `src/ui/analyzer_window.{hpp,cpp}` | rewritten around a row list and one canvas (below) |
| `src/ui/scope_window.{hpp,cpp}` | 4 bands of 70 px, Trigger combo over 4 Channels, delay field, off-screen Trigger arrow |
| `src/ui/main.cpp` | shift-click, group clicks, refusals through one lambda, `--group`, `--hook` ≤ 4, `--delay`, usage |
| `tests/instruments/logic_analyzer_test.cpp`, `oscilloscope_test.cpp` | spec, Tests (engine, pattern, stems, scope) |
| `tests/runner/analyzer_consumer_test.cpp`, `runner_test.cpp` | Captures at 100 MHz (2 ms runs no longer hold a 10 MHz Capture); 96 watched |
| `tests/machines/logic_analyzer_test.cpp` | the 110 bus Triggers; the existing 1 MHz cases now simulate ~60 ms (time measured) |
| `tests/machine/machine_test.cpp` | `probeNet` on the 100's `U1` `RW` |
| `tests/session/session_test.cpp` | groups; probe on `RW`; `led computer with every instrument` widened; existing cases that assumed 8 Channels and 2 Hooks: `clips` (fills the 32 Channels from the Board's pads), `arm before power-on captures` and `a capture is not re-armed by a repair` (100 MHz), `hooks` (4 taken, the fifth refused) |
| `docs/GLOSSARY.md` | **Channel Group** |
| `docs/recipes/ADD_INSTRUMENT.md`, `ADD_BOARD_DEVICE.md`, `ADD_MACHINE.md` | budget 96, rings, groups, tests at 100 MHz, the probe note removed |
| `docs/machines/100-heartbeat/HEARTBEAT_DESIGN.md`, `110-led-computer/LED_COMPUTER_DESIGN.md` | the Session budgets read `kMaxWatched` (96) |
| `docs/ARCHITECTURE.md` | P5e row done; P3c row's Later points to P5e |

## Engine: analyzer (`logic_analyzer.hpp`)

```cpp
inline constexpr std::size_t kChannels = 32;
inline constexpr std::size_t kDepth = 65'536;
using ChannelMask = std::uint32_t;  // bit c: Channel c

struct ChannelBits {
    ChannelMask high = 0, unknown = ~ChannelMask{0};
    bool operator==(const ChannelBits&) const = default;
};

class AnalyzerState {
    ...
    void change(Time time, ChannelMask channels, LogicValue value);
private:
    struct TriggerMasks { ChannelMask low = 0, high = 0, rise = 0, fall = 0; };
    TriggerMasks masks_;  // from settings_.trigger at arm()
};
```

- `arm()` builds `masks_` from the 32 conditions. `triggers(now)`: with `known = ~now.unknown` and `was` the previous point (none right after arm: edges cannot fire, as today), true when `(low | high | rise | fall) & ~known` is empty, `now.high & low` is empty, `~now.high & high` is empty, and rise/fall bits were known on the previous point with the opposite level. Same results as the per-Channel loop (a test keeps that loop as the reference).
- `PreTriggerPoints` unchanged in form: 6553, 32 768, 58 982. `write()` and `freeze()` unchanged. Memory: `ChannelBits` is 8 bytes, the ring 512 KiB; `Capture::points` another 512 KiB; the consumer's `TripleBuffer<Capture>` three copies. A Capture is copied once when it completes.
- `labels` become `std::array<std::string, kChannels>`; `AnalyzerSettings::trigger` 32 entries.

Bus patterns and name stems, beside `ParseTrigger`:

```cpp
// `pattern` over `width` members, bit 0 first in the result: hex digits and x/X (don't care),
// at most ceil(width / 4) digits, fewer padded with leading 0s; a 1 above the width, or any other
// character, is refused. Empty: all Any.
std::optional<std::vector<TriggerCondition>> ParseBusPattern(std::string_view pattern,
                                                             std::size_t width);
// Back from members' conditions (bit 0 first): a nibble of levels is its hex digit, all Any is x,
// anything else (partly x, or an edge) is ?.
std::string FormatBusPattern(std::span<const TriggerCondition> conditions);

struct NameStem {
    std::string stem;    // the pin name with its last digit run replaced by '#': "A#", "#Q"
    std::uint32_t index; // that digit run's value
};
std::optional<NameStem> StemOf(std::string_view pin);  // nullopt: no digit in the name
```

`StemOf` lives in the engine header because the window and the Session both need it; it knows nothing of Parts (the Session excludes power and no-connect pins).

## Engine: scope (`oscilloscope.hpp`)

```cpp
inline constexpr std::size_t kScopeChannels = 4;
inline constexpr Time kMaxDelayDivisions = 1000;

struct ScopeSettings {
    ...
    Time delay = 0;  // ps from the Trigger to the screen's Trigger mark; 0..1000 divisions
};

struct Sweep {
    ...
    Time delay = 0;  // as configured for this Sweep
    // start: the left edge, at Trigger + delay - timebase
};
```

- `startSweep(trigger, kind)`: `start_ = trigger + settings_.delay - settings_.timebase`. `catchUp`, `materialize` and `prune` work on `start_`/`end_` as today: while a Sweep is pending its history is kept from `start_`, and between the Trigger and `start_` nothing older is needed.
- `configure()` clamps the delay to `kMaxDelayDivisions * timebase` (a timebase change can leave a delay too long).
- Auto Sweeps: `startSweep(waitFrom_ + wait(), Auto)` also takes the delay: the free-running screen just runs later. Nothing else changes.
- `Oscilloscope` (headless) and `ScopeConsumer` work over `kScopeChannels` already; the consumer's per-Net masks stay `std::uint8_t`. `last_` is initialised with 4 X (it listed 2).

## Runner: watch budget

- `kMaxWatched` = 96 (`dispatcher.hpp`); `Marker::slot` stays `std::uint8_t`.
- `SpscRing<T>`: `slots_` becomes `std::unique_ptr<T[]>` from `std::allocator<T>{}.allocate(capacity)` with a deleter calling `deallocate`, plus `capacity_`. `T` is trivially copyable (asserted already), so writing a slot by assignment starts its lifetime; untouched pages are never committed. 96 rings of 1 MiB cost only what busy Nets touch. `tryPush`, `tryPop` unchanged.
- `Dispatcher::sliceEnd` scanned every slot for the earliest Sample and every Consumer's `nets()` for each Sample: doubling the slots and adding 32 busy Clips would multiply that. It becomes a min-heap of (time, slot) over the slots holding Samples up to the slice end (equal times in slot order, as the scan gave), and each such slot's Consumers are found once at the slice's start. A Consumer retargeted within the slice may get the old Net's last Samples of that slice: the probe, meter, scope and analyzer each check the Net already (the TV and LEDs never retarget). The 110's ratio with 32 Clips is measured in phase 3, not left to phase 6.

## Consumer (`analyzer_consumer`)

`masks_` become `std::vector<ChannelMask>`, `1u << c` becomes `ChannelMask{1} << c`. Nothing else changes: `configure()` already watches only the Channels whose Net changed.

## Probe on an unconnected pin (`Machine::probeNet`)

After the island lookup: when `IslandNetNames(...)[island]` is empty, look for the first chip pad on that island (`islands.padIsland[c][p] == *island` over `board_.components`) and return `PinNet(built, component.reference, pad.number)`. `PinNet` gives the private Net of an unconnected pin, the same Net the Hooks and Clips use. Any component's pad counts, so an island holding only a power pad (unnamed: the namer skips power pins) reads that pin's private Net, as a Hook or Clip there does. An island with no pad (a stray Trace) still returns nullopt. The meter's `Volts` read goes through `probeNet` too and now reads such a pin; continuity is unchanged (islands).

## Session (`session.hpp`)

```cpp
struct ChannelGroup {
    std::string name;                   // "U1 A"; hand-made: "group N"
    std::vector<std::size_t> channels;  // bit 0 first
};

// The pin's group on its chip (same stem, not power or no-connect, at least 2 pins), each pin
// clipped to the lowest free Channel in bit order; the pins already clipped are moved into the
// group. A pin with no stem partner is clipped alone (a group of none: returns nullopt in
// `group`). Errors: "no such pin", "group U1 A needs 16 channels, 9 free".
struct ClipGroupResult { std::size_t first; std::optional<std::size_t> group; };
std::expected<ClipGroupResult, std::string> clipGroup(std::string_view reference,
                                                      std::string_view pin);
void unclipGroup(std::size_t group);  // unclips every member
// Hand grouping: at least 2 clipped Channels, taken out of their groups; ordered by StemOf index
// when all have one and they differ, else as given (first = most significant). Returns its index.
std::optional<std::size_t> group(std::vector<std::size_t> channels);
void ungroup(std::size_t group);  // members stay clipped, their conditions kept
bool setGroupTrigger(std::size_t group, std::string_view pattern);  // false: refused pattern
std::string groupTrigger(std::size_t group) const;  // FormatBusPattern of the members
std::span<const ChannelGroup> groups() const { return groups_; }
std::optional<std::size_t> groupOf(std::size_t channel) const;
```

- `clipGroup` with some pins already clipped: they keep their Channel, the free-Channel count is over the others only, and they leave any group they were in first (which may dissolve: indices renumber).
- `unclip(c)` removes `c` from its group; a group left with one member is erased (that member stays clipped). Erasing a group renumbers the later ones: callers hold indices only within a frame.
- Groups are UI-side state, like `clips_`: they survive rebuilds and power cycles unchanged, and never reach the consumer. The consumer sees only Channels and their 32 conditions.
- Pins of a stem come from `machine_->designCircuit().components` (`pins()`: kind not `Power` or `NoConnect`), each turned into a `PadLead` by `padLead`. A pin with no pad (none on our Boards) is skipped.
- Name: `std::format("{} {}", reference, stem without '#')`; hand groups `group 1`, `group 2`… (lowest unused).

## Board view and toolbar

- `BoardClick::shift = ImGui::GetIO().KeyShift` at the click.
- `BoardView::refuse(std::string reason)`: stores the reason and the cursor's screen position; `draw` puts it in a small dark red box just below-right of that point for 1.5 s (the banner keeps its own 2 s). `main.cpp` gets one lambda `refuse(reason)` calling `toolbar.refused(reason)` and `boardView.refuse(reason)`; a repair `Refused` (powered), where the banner only flashes, says "Power off first" at the cursor.
- The refusal texts name the new sizes: "Clip: all 32 channels used", "Hook: all 4 channels used" (`std::format` with the constants).
- Clip tags: `ClipTag` gains `ImU32 colour`, set by `main.cpp` (`GroupColour(g)` for a member, else `ChannelColour(c)`); `board_view.cpp` draws that colour. `ChannelColour(c) = kChannelColours[c % 8]`, `GroupColour(g) = kChannelColours[(g + 2) % 8]`.
- `kHookColours`: yellow, cyan, `IM_COL32(255, 60, 220, 255)`, `IM_COL32(110, 160, 255, 255)`.
- Toolbar tooltips (`ImGui::SetItemTooltip`): `Clip (C)`: "Logic Analyzer leads: click a pin; shift-click a whole bus"; `Hook (O)`: "Oscilloscope leads: click a pin"; the meter and probe buttons name what they read.

## Clicks (`main.cpp`)

Tool `Clip`:

| Click | On a clipped pin | On a free pin |
|---|---|---|
| plain | `unclip(c)` | `clip()`; all used: refuse "Clip: all 32 channels used" |
| shift | in a group: `unclipGroup`; else `unclip(c)` | `clipGroup()`; error: refuse "Clip: " + message |

Hook: as today with "Hook: all 4 channels used".

## Analyzer window

State: `shown_` (Capture), `from_`, `to_` (points), `marker_`, `selected_` (set of Channels for hand grouping), `expanded_` (set of group names: a reused hand-group name inherits it, harmless), the drag state, and the run caches.

**Rows**, built each frame from the Session (a Channel of the shown Capture unclipped since is no longer listed; a row whose Clip changed shows the Capture's label faint in its lane): each group (in order of its first Channel) is one row, followed by its members (MSB first) when expanded; then each clipped Channel not in a group; free Channels are not listed (32 grey rows would bury the content), the header says `12 of 32 clipped`. With no Clip: the hint line (spec).

**Layout**: header line (tool button, Arm/Stop, phase, Rate, Trigger at, `Fit`, `Trigger`, `Group` when ≥ 2 Channels are selected); then a child window `rows` filling the rest minus the axis and readout lines, with its own vertical scrollbar. Inside it, per row at fixed height 20 px: the label column (240 px: colour, expand arrow for a group, label, value at cursor or marker, condition combo or pattern box, `×`), and the lane. The lane column of all visible rows is one `InvisibleButton` (`##canvas`) covering the child's lane area at the child's scroll, drawn into with the child's draw list; `ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY)` on it so the wheel zooms instead of scrolling. Over the labels the wheel scrolls the rows (ImGui default).

**Input** is read once, right after the canvas button and before any lane is drawn: wheel zoom about the cursor, drag pan, click marker, double-click fit; then the view is clamped. Every lane of the frame uses the same view.

**Drawing**:
- Lanes are drawn from **runs**: `CaptureRuns RunsOf(capture, channels)` (start point, value, unknown per run of equal value), built once per Capture and Channel list (one Channel, or a group's members) and cached until the next Capture: one pass over 65 536 points. Drawing walks the runs in view; a run narrower than a pixel marks its pixel busy (filled), and the walk jumps by binary search to the run at the next pixel. A lane costs O(runs wide enough to see + pixels × log runs), whatever the zoom, so 32 lanes over a 100 MHz Capture of a 1 MHz bus stay within the UI budget while panning. (The review's one-pass 32-Channel mask cache was the other option; runs also give group values and the value under the cursor.)
- A Channel lane: a level line, an edge where it changes, grey for X.
- A group lane: each run a bus hexagon in the group's colour with its hex value centred when it fits (`X` digits where a member is unknown); busy pixels filled.
- Trigger, marker, cursor lines and the axis as today over the canvas.

**Labels**: a Channel row: `U1.34 RW` and its level under the cursor (else the marker) as `0`/`1`/`X`; a group: `U1 A` and its hex value there. A Channel's condition combo as today (`any`, `0`, `1`, `rise`, `fall`); a group's condition is an `InputText` (6 characters) showing `groupTrigger`; on leaving it after an edit (Enter or click away), `setGroupTrigger`; refused input turns the box red until edited. Its width fits `ceil(members / 4)` digits (8 for a 32-member hand group). Member rows show their condition read-only (grey).

**Selection**: clicking a Channel row's label (not its widgets) toggles it in `selected_`; `Group` calls `session.group(selected in row order)`; a group row's `×` ungroups (members stay), shift-`×` unclips the whole group.

**Readout** under the axis: `t = …   dt = …` as today; the hint line when not hovering: "wheel: zoom   drag: pan   click: marker   double-click: fit   shift-click a pin: whole bus".

## Scope window

- Bands of `kBandHeight` 70 px × 4; labels, rows (`1`–`4`), the `×` buttons over `kScopeChannels`.
- Trigger combo: `ch1 rise` … `ch4 fall` built in a loop.
- Delay: `InputText` (8 characters) parsed with `ParseDuration` (`netlist/units.hpp`, as `--after`; it refuses `0`, so `0` is taken as zero first) and clamped; `-`/`+` buttons (ASCII: the default font has no U+2212) step by one division, `0` resets. Shown as `delay 12.5 us`.
- Time axis: `timeAt(x)` uses `shown_->delay`: `delay - timebase + (x - left) / width * span`. When the delay pushes the Trigger off screen (delay > timebase), the Trigger mark becomes a left-edge arrow with `T -12.4 us` (the time from the screen's left edge back to the Trigger). The readout `t =` is time since the Trigger, as today.
- Empty hint when no Hook: "Hook (O), then click a chip pin."

## CLI (`bw_ui`)

- `--group REF.PIN[:PATTERN],...`: `clipGroup` then `setGroupTrigger`; an unknown pin, a refused group or pattern is a usage error as for `--clip`. Arms like `--clip`.
- `--hook REF.PIN,...` up to 4; `--delay T` (`0` or a duration, at most 1000 divisions of the timebase given or default, checked after `--timebase`).
- Usage text lists them.

## Tests (details beyond the spec)

- Engine tests keep their points relative to `PreTriggerPoints(Early)`. `kFar` (10 depths) grows with `kDepth`: the runs only add points, cheap.
- The existing headless cases at 1 MHz (`crack stops H0`, `stuck gate`, `LINE_END_N every 64 points`) now simulate ~60 ms and check every point; `LogicAnalyzer::update` rescans the recorded Samples each step. Measured in phase 2; if one goes over ~10 s in debug, it moves to 10 MHz or checks a range.
- The headless 110 cases: `Simulator` + `LogicAnalyzer` with Channels `A0`–`A15`, `D0`–`D7`, `RW`, `PHI2` at 10 MHz (5.9 ms simulated past the Trigger at the Early position), run until Captured. The data is read at the last point of the Trigger's cycle with `PHI2` high: the oscillator's edges fall on 500 ns, the points on 100 ns; after `PHI0` falls the CPU releases the data at 90 ns and changes the address at 130 ns, so the last point with the address reads a floating bus (`FF` through the TTL inputs). The `FFFC` case also needs `RW` = 1: while reset is held the core reads at a random address. The reset case triggers about 20 ms after power-on (the POR), the latch case 31 µs later: the latch case runs ~26 ms. Measure the debug time in phase 2 and keep both under ~20 s.
- Runner test: `runner: 96 watched Nets` loads `netlists/tennis.bwn` (`bus_load` has only 88 distinct Nets), watches 96 distinct Nets through a counting Consumer, runs 2 ms and checks every one got at least one Sample. The 97th is a `Check` on the simulation thread: not tested.
- Every existing Dispatcher and runner test (Sample order, Rewatch, power cycles) stays green on the heap merge.
- Session tests use the 100 Heartbeat (`U1` `A0`–`A15` are all routed there) and the 110 for the widened every-instrument case.

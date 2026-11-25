# P8g — The Multimeter held — Design

How [MULTIMETER.md](MULTIMETER.md) is built. Plan: [MULTIMETER_PLAN.md](MULTIMETER_PLAN.md). Numbers in parentheses (§3) are the spec's items.

The kernel, the runner, the meter's engine and Consumer, Parts, Machines and the other instruments are untouched. `MeterSetup` is rewritten around a held red lead and a clipped black one; the meter leaves the bench objects and the Equipment; its window is replaced by an LCD drawn above the Shelf while a press measures.

## Files

| File | Change |
|---|---|
| `src/session/meter_setup.{hpp,cpp}` | rewritten: `mode()` from the power, `hold`, `clip`, `unclip`, `powerChanged` |
| `src/session/session.cpp` | `powerOn` / `powerOff` call `meter_.powerChanged()` |
| `src/view/meter_reading.{hpp,cpp}` (new) | `MeterReading`, `VoltsReading`, `ContinuityReading`, `Segments` |
| `src/view/bench_places.{hpp,cpp}` | `BenchObject::Meter` removed |
| `src/view/focus.{hpp,cpp}` | the meter no longer an Instrument; `Measures(owner)` |
| `src/view/bench_options.{hpp,cpp}` | `meter` out of the bench objects' names; `--meter`'s help |
| `src/view/CMakeLists.txt` | `meter_reading.cpp` |
| `src/ui/board_view.{hpp,cpp}` | `MeterPress`, `meterPress()`; the meter's drag reads, never pans; a held pin link; `framePress_` with `Measures` |
| `src/ui/manual_view.{hpp,cpp}` | `TextFlow::Result::held`; `TargetHits::held` set |
| `src/ui/chip_panel.cpp` | `PinHits` sets `held` |
| `src/ui/tool_click.{hpp,cpp}` | `Owner::Meter`: the clip; `OnClip` |
| `src/ui/meter_lcd.{hpp,cpp}` (new) | `DrawMeterLcd` |
| `src/ui/meter_window.{hpp,cpp}` | deleted |
| `src/ui/instrument_marks.hpp` | `kMeterWindowName` removed |
| `src/ui/shelf.{hpp,cpp}` | the meter taken like the probe; tooltip, hints, `meterAt` |
| `src/ui/shelf_drawings.cpp` | `Meter`: no fake digits |
| `src/ui/board_tags.cpp` | `LeadTagsOf`: red while held, black while clipped |
| `src/ui/bench_setup.cpp` | `--meter`: hold, or clip then hold |
| `src/ui/main.cpp` | the meter each frame; the LCD drawn; `MeterWindow` gone |
| `src/ui/CMakeLists.txt` | `meter_lcd.cpp` in, `meter_window.cpp` out |
| `tests/view/meter_reading_test.cpp` (new), `tests/CMakeLists.txt` | its cases |
| `tests/view/focus_test.cpp`, `bench_places_test.cpp`, `bench_options_test.cpp` | the meter out of the bench objects |
| `tests/session/session_test.cpp`, `tests/machines/bodge_wire_test.cpp`, `tests/machines/shorts_power_test.cpp` | the new `MeterSetup` |
| `docs/GLOSSARY.md`, `docs/recipes/ADD_INSTRUMENT.md`, `docs/ARCHITECTURE.md`, `docs/README_SPEC.md`, `README.md`, `docs/readme/continuity.png`, `tools/readme_shots.py`, `docs/GAME_DESIGN.md` | §6 |

## 1. `MeterSetup` (§2, §3, §4)

### `src/session/meter_setup.hpp`

```cpp
enum class MeterMode : std::uint8_t { Volts, Continuity };

struct TestLead {
    Point at;           // nm
    Layer layer;        // as ProbeTip
    std::string label;  // "U1.14 QA leg", "Trace", "via"
};

// The Multimeter as the player holds it (Session::meter(), P8g). The power is its mode: Volts
// while the Machine is powered (black on GND, implied), Continuity while it is off. The red Test
// Lead is on the Board while a press holds it; in Continuity the black one is clipped and stays,
// across rebuilds and Repairs, until unclipped or the power goes on.
class MeterSetup {
public:
    explicit MeterSetup(const SessionBuild& build) : build_(&build) {}

    MeterMode mode() const;                  // from build_->powered
    void hold(std::optional<TestLead> red);  // nullopt: lifted; the same place: no-op
    bool clip(TestLead black);               // false: no Machine, or powered
    void unclip();
    const std::optional<TestLead>& red() const { return red_; }
    const std::optional<TestLead>& black() const { return black_; }
    std::optional<NetId> net() const { return consumer_->touched(); }
    // Volts: the red Lead's average (0 on bare Copper, a rail's volts); nullopt in Continuity,
    // with no red Lead, or before the first refresh. Run state aside: the LCD shows dashes while
    // paused (main.cpp).
    std::optional<double> volts() const;
    // Continuity, both Leads on: whether they are on one island of the current Copper.
    std::optional<bool> continuity() const;

private:
    friend class Session;
    void bind(MeterConsumer& consumer);
    void copperChanged() { findContinuity(); }
    void powerChanged();  // on: the clip lifted; on or off: follow(), findContinuity()
    void follow();        // the consumer onto the red Lead's Net, in Volts; else lifted
    void findContinuity();

    const SessionBuild* build_;
    MeterConsumer* consumer_ = nullptr;
    std::optional<TestLead> red_, black_;
    std::optional<bool> connected_;
    std::optional<double> rail_;
};
```

`setMode`, `placeLead`, `liftLeads`, `blackNext` go. `hold` compares `at` and `layer` with `red_` and returns early when equal (`hold(nullopt)` with nothing held is a no-op); otherwise it sets `red_`, `follow()`, `findContinuity()`. `unclip()` also calls `findContinuity()`. `main.cpp` calls `hold` only when the pick's Hit or Layer changes (§5), so a slide along one Trace costs no `probeNet` per frame. `follow()` touches the red Lead's Net only when powered (`mode() == Volts`). `volts()` keeps today's rules: no red Lead → nullopt; bare Copper → 0.0; a rail → `rail_`; else the consumer's reading; but with the power off it is nullopt (Continuity), no longer 0.0.

### `src/session/session.cpp`

`Session::powerOn` (after `build_->powered = true`) and `Session::powerOff` (after `build_->powered = false`) call `meter_.powerChanged()`. At power on the runner is already started, so the red Lead's Net is watched just after the start, a live retarget like the probe's (the `with every instrument` cases then watch the meter's Net from there).

### Tests

`tests/session/session_test.cpp`, rewritten on `hold` and `clip` (cases renamed where their subject changed):

- `session: meter volts`: `hold(clk)` before power: `volts()` nullopt (Continuity); powered and run 1 s: 1.85 V; `hold(vcc)`: 5.0; `hold(nullopt)`: nullopt.
- `session: the clip refused with the power on, lifted by power on` (was `continuity refused with the power on`): `clip` false while powered; clipped off, `powerOn`: `black()` nullopt.
- `session: continuity follows a mend`: `clip(qa)`, `hold(far)`: false; mend: true; `hold(nullopt)`: nullopt.
- `session: undo every repair`: as today, the clip and red held; after the power cycle the clip is gone (`black()` nullopt), so it is clipped again before the final `continuity() == false`.
- `session: hooks and leads survive a repair`: `hold(far)` in place of `placeLead`.
- `LeadsAcrossH0` (used by `continuity through a bodge wire` and `continuity across a cut`): `clip` then `hold`.
- The other `placeLead({…})` calls (lines 742–1224: volts on a held red) become `hold({…})`; `REQUIRE(…placeLead…)` becomes the call alone.

`tests/machines/bodge_wire_test.cpp`: `clip(pad("7"))`, `hold(pad("8"))`; after its power cycle (the clip lifted) clipped again before `continuity() == false`. `tests/machines/shorts_power_test.cpp`: `hold({*vcc, …})`, the checks unchanged (both are read powered).

## 2. What the LCD shows (§3, §4, §5)

### `src/view/meter_reading.hpp` (new)

```cpp
// What the Multimeter's LCD shows (P8g): three seven-segment cells and the mode's sign.
struct MeterReading {
    std::string digits;       // the cells' characters, right-aligned in kMeterCells; a '.'
                              // lights the decimal point of the cell before it
    bool continuity = false;  // the •))) sign; else V
    bool beep = false;        // connected: the backlight green
    bool operator==(const MeterReading&) const = default;
};
inline constexpr std::size_t kMeterCells = 3;

// "2.43" (two decimals under 9.995 V), "12.0" (one from there), "---" for nullopt.
MeterReading VoltsReading(std::optional<double> volts);
// "0.0" lit green when connected, "OL" otherwise (nullopt: off copper, or no red Lead).
MeterReading ContinuityReading(std::optional<bool> connected);
// Seven-segment bits, a = 1 … g = 64, for '0'-'9', '-', 'O', 'L', ' '; Check()s anything else.
std::uint8_t Segments(char c);
```

### Tests (`tests/view/meter_reading_test.cpp`, `[view]`)

`meter reading: volts with two decimals` (2.434 → "2.43", 0 → "0.00", 4.996 → "5.00"), `meter reading: ten volts and over with one` (12 → "12.0"), `meter reading: no reading is dashes`, `meter reading: continuity` (true → "0.0", beep; false and nullopt → "OL", no beep), `meter reading: segments` ('8' all seven, '-' g alone, 'L' d e f, ' ' none).

## 3. The meter out of the bench objects (§1, §8)

### `src/view/bench_places.{hpp,cpp}`

`BenchObject::Meter` removed from the enum and from `DefaultPlace`, `NormalSize`, `MinimalSize`; the `kViewLane` comment names the keyboard alone.

### `src/view/focus.{hpp,cpp}`

```cpp
bool IsInstrument(Owner owner);  // Analyzer, Scope: the Equipment, with a window
bool Measures(Owner owner);      // Probe, Meter, or an Instrument: a pin link measures
```

`IsInstrument` drops the meter, and so does its `BenchObject` overload in `focus.cpp` (kept: `outFlag`'s `Check` refuses the Analyzer and the Scope); `ObjectOf` maps Analyzer and Scope. `RouteLink` uses `Measures`. The meter is taken with `Focus::take` like the probe.

### `src/view/bench_options.{hpp,cpp}`

`{"meter", BenchObject::Meter}` leaves the bench objects' names (`kObjects` 7 → 6; so `--pop meter`, `--away meter` are usage errors) and `--pop`'s help string loses `meter`; `{"meter", Owner::Meter}` stays for `--focus`. `--meter`'s help: `REF.PIN[,REF.PIN]` unchanged.

### Tests

`focus_test.cpp`: the cases using the meter as an Instrument (`out(Owner::Meter)` would now assert in `ObjectOf`: lines 12, 52–57, 93) use the Scope; `focus: the meter is taken like the probe` (`take(Owner::Meter)`: the owner, no `BenchObject` out); `focus: a link to the Board measures or frames` gains the meter. `bench_places_test.cpp`: the instrument loops and the meter's place case lose the meter. `bench_options_test.cpp`: `--pop tv,scope`; `--pop meter` is a usage error.

## 4. The press (§3, §6, §7)

### `src/ui/board_view.hpp`

```cpp
// The Multimeter's press (P8g): the copper it began on and what is under it now (nullopt: off
// copper).
struct MeterPress {
    Pick start;
    std::optional<Pick> at;
};

class BoardView {
public:
    // With the meter in hand: the left button held on the Board (a mouse: from the press; a
    // finger: once its loupe is up), or a pin link held in a manual or the chip panel (route()).
    std::optional<MeterPress> meterPress() const;
    …
private:
    std::optional<Pick> meterStart_;  // the press began on copper, the meter in hand
    std::optional<Pick> meterAt_;     // the pick under the pointer this frame, while meterDown_
    bool meterDown_ = false;          // the press is the meter's (see meterPress())
    std::optional<std::pair<Pick, int>> linkPress_;  // a held pin link, the ImGui frame it was set
};
```

In `BoardView::input`:

- At the press (`IsMouseClicked(Left)`), with `owner_ == Owner::Meter`, `pressed_`, off the flip corner and a memory window, and `picked` on copper (`AcceptsLead(LeadKind::Held, kind)`: a pad, Trace or via, not a chip body): `meterStart_ = picked`. A press that starts off copper is no meter press: its drag pans, as with any tool (a trackpad has no middle button). `meterStart_` is reset at the release and by `Touch().cancelled`.
- `meterDown_` is `pressed_ && meterStart_ && (!FingerPointer() || loupe_)`, false on the release frame: a mouse's press reads at once, a finger's once its loupe is up; `meterAt_` is `picked` while it holds.
- A mouse's drag with `meterDown_` sets `dragging_` (so its release is no click) but never pans, as `wirePress_` does; the early `if (dragging_) return;` lets a meter drag through, so the Net under the pointer keeps its highlight.
- A finger's drag before its hold pans, as before (`meterDown_` false).
- `meterPress()` returns `{*meterStart_, meterAt_}` while `meterDown_`, else the held link's `{pad, pad}` when `linkPress_` was set at this frame or the one before (`route()` runs after the Board's draw, in the panels).
- In Continuity, a press that is not measuring (no clip, or begun on the clip) and is dragged does nothing at all: no pan, no clip (its release is no click). Intended: the drag began on copper with the meter.
- `framePress_` (a double-click on a chip frames it) uses `Measures(owner_)`.

In `BoardView::route`, before its early return on `!hits.clicked`: when `hits.held` is a pin and `owner_ == Owner::Meter`, `linkPress_` = the pad's `Pick` (as `LinkAction::Measure` builds it) and the frame count, and the pin flashed.

### `src/ui/manual_view.{hpp,cpp}`, `src/ui/chip_panel.cpp`

`TargetHits` gains `std::optional<BoardTarget> held` (a link held down this frame). `TextFlow::Result` gains `held`: `hovered && IsMouseDown(Left) && inside(MouseClickedPos[Left]) && !PressCancelled() && the drag under the threshold` (the click's own test, before its release). `DrawSpans` sets `hits.held`. `PinHits` sets `held` on the same conditions (`IsItemActive() && IsItemHovered()`, the drag under the threshold, `!PressCancelled()`), so a slide off the row or a finger's scroll is no hold. `ChipPanel::draw` merges the Function section's `held` with its `hovered` and `clicked` (`chip_panel.cpp:301`).

### `src/ui/tool_click.{hpp,cpp}`

```cpp
// The black clip's copper (P8g): the pick at the clip's point is `press`'s Hit.
bool OnClip(const BoardScene& scene, const Pick& press, const TestLead& clip, PickReach reach);
```

`case Owner::Meter` in `ToolClick`: in Continuity only (Volts: a click measured while held, nothing more). No clip: a pick that `AcceptsLead(LeadKind::Held, …)` clips the black lead (`clip({contact, layer, LeadLabel(…)})`). A clip: a click `OnClip` lifts it (`unclip()`); any other click does nothing (its press measured). `BoardView` exposes the reach it picks with for `OnClip` (`PickReach pickReach() const`, today's private `reach()` made public).

## 5. The LCD and the frame (§1, §3–§5)

### `src/ui/meter_lcd.hpp` (new)

```cpp
inline constexpr ImVec2 kMeterLcdSize{150, 64};  // logical px
// The Multimeter's LCD risen from its Shelf drawing (P8g): the yellow case's top, a grey-green
// LCD (lit green on `beep`) with kMeterCells seven-segment cells, their unlit segments faint, the
// decimal points, `V` or the •))) sign. Its bottom centre at `base`, moved sideways to stay
// inside the main viewport.
void DrawMeterLcd(ImDrawList& list, ImVec2 base, const MeterReading& reading);
```

Segments drawn as convex hexagons from `Segments(c)`; ImGui draw lists only (no texture), so the web path needs nothing in `gpu_render_web.cpp`.

### `src/ui/shelf.{hpp,cpp}`, `src/ui/shelf_drawings.cpp`

- `Item::Meter` keeps `Owner::Meter`; the click's `default:` branch takes it (`focus.take`) now that it is not an Instrument; its `look.lit` is the owner's (held), as the probe's. `WindowOf` loses the meter.
- Tooltip (§1): "Multimeter: hold on copper; volts, or continuity with the power off", then `"black clip: " + black()->label + "; a click on it lifts it"` while clipped.
- The hint (§4), with the meter in hand: powered "Meter: hold on copper"; off, no clip, "Meter: clip black lead"; clipped, "Meter: hold red lead" (about 22 characters fit at 1280 px).
- `ImVec2 meterAt{}`: the meter cell's centre x and the Shelf's top edge y, screen px, written by `draw()` (as `leaveAt`).
- `Meter(Pen&, look)` draws its little LCD lit and blank when held.

### `src/ui/board_tags.cpp`

`LeadTagsOf`: red's tag while `red()`, black's while `black()` (only ever set in Continuity).

### `src/ui/main.cpp`

After the Board's click handling, each frame:

```cpp
// The Multimeter (P8g): the red Lead held where its press is; the LCD up while it measures.
const auto press = focus.owner() == Owner::Meter ? boardView.meterPress() : std::nullopt;
if (press || focus.owner() != Owner::Meter) meterHeldAtStart = false;
bool measuring = meterHeldAtStart;
if (press) {
    const auto& black = session.meter().black();
    measuring = session.meter().mode() == MeterMode::Volts ||
                (black && !OnClip(scene, press->start, *black, boardView.pickReach()));
    const auto at = measuring && press->at && AcceptsLead(LeadKind::Held, press->at->hit.kind)
                        ? press->at : std::nullopt;
    if (/* at's Hit or Layer differs from the last one held */) {
        session.meter().hold(at ? lead(*at) : std::nullopt);
    }
} else if (!meterHeldAtStart) {
    session.meter().hold(std::nullopt);
}
```

then, after `shelf.draw`, when `measuring`: the reading (`VoltsReading(paused ? nullopt : meter.volts())`, `paused` the runner's `RunState::Paused`: dashes, whatever was read before the pause, a rail included (the user: "in real life you cannot read a paused board"); or `ContinuityReading(meter.continuity())`) drawn by `DrawMeterLcd(*ImGui::GetForegroundDrawList(ImGui::GetMainViewport()), shelf.meterAt, …)`. `meterHeldAtStart` is `!options.bench.meter.empty()`, cleared by the first press or whenever the owner is not the meter. `MeterWindow`, its `instrument(Owner::Meter, …)` and `takePowerClick` go; `--meter` gives the meter the focus (`focus.take(Owner::Meter)`) instead of `bringOut`, before `--focus` (which, naming another owner, clears the held start).

### `src/ui/bench_setup.cpp`

`--meter A`: `hold(A)`. `--meter A,B`: `clip(A)` (before the power-on, which `main` leaves off for two), `hold(B)`. Labels as today.

## 6. Docs

- GLOSSARY: **Multimeter** (held; the power its mode; the LCD over the Shelf while measuring; the black clip in Continuity), **Test Lead** (red held, black clipped), **Instrument** (the Logic Probe and the Multimeter held, the Analyzer and the Oscilloscope Equipment), the bench objects' list (no Multimeter), the Shelf's hint.
- ADD_INSTRUMENT.md: a held instrument (no window) is one of two shapes; the meter as its second example (press in `BoardView`, the reading drawn over the Shelf).
- GAME_DESIGN.md: its Multimeter lines, where they name a window or a mode switch.
- GLOSSARY also: **View buttons** ("the keyboard's and the Multimeter's default places": the keyboard's alone).
- ARCHITECTURE.md's instruments paragraph (held tips vs attached leads): the meter's red lead held, its black clip on any copper (`AcceptsLead(Held)`), though called a clip.
- ADD_INSTRUMENT.md's CLI row ("these bring the window out without the focus"): `--meter` takes the meter.
- `src/session/session.hpp:101`'s comment: "the Multimeter's held and clipped Test Leads".
- README_SPEC.md line 195 (`--meter A,B`: black first).
- README_SPEC.md item 3 and README.md: "Power off, take the multimeter, clip its black lead on the LED's pad and hold the red one on `U5` pin 11: it reads OL". `tools/readme_shots.py`: `continuity` becomes `--meter D1.1,U5.11` (black first); regenerate `docs/readme/continuity.png`.
- ARCHITECTURE: the P8g row done.

## Checks

- Release: `ctest --preset release -j -R "^(meter|meter reading|session|bodge wire|shorts power|focus|bench places|bench options): "` between steps; the full release suite at each phase's end (shared code).
- Debug: `ctest --preset debug -j -R "^(meter reading|session|bodge wire|shorts power|focus): " -E "bw2001|bw80|tennis"`.
- Screenshots (release, viewed, 040-stopwatch): `--meter U1.12` (Volts, the LCD over the Shelf); `--meter U1.12 --run pause` (dashes); `--meter U1.12,U1.1` (Continuity: QA–CKB, connected) and `--meter U1.12,U2.12` (open); no `--meter`, `--focus meter` (the meter in hand, no LCD, the hint); `--touch --hold X:Y --focus meter --after 2s` (the loupe and the LCD); the 110 with `--meter` for the speed and UI ms; the UI ms during a slide along a 140 Trace (`--drag` with `--touch` off is a finger's: measured by hand in a desktop run if no flag scripts a mouse drag).
- Web: `cmake --build --preset web`.

# P4f Seven-Segment Digits — Detailed Design

Implements [SEVEN_SEGMENT.md](SEVEN_SEGMENT.md): the 74LS90 and 74LS47 Parts, a 7-segment digit Part drawn lit on the Board, LEDs generalised to lamps, and the tests of the stopwatch. The Machine's netlist, Board, manuals and Puzzles are in [STOPWATCH_DESIGN.md](../machines/040-stopwatch/STOPWATCH_DESIGN.md).

Recipes followed: [ADD_PART.md](../recipes/ADD_PART.md) (74LS90, 74LS47), [ADD_BOARD_DEVICE.md](../recipes/ADD_BOARD_DEVICE.md) (output device chain, for the digit), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md), [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md). This design gives only what those recipes leave open.

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/parts/ls90.hpp, ls90.cpp         74LS90 Component
src/parts/ls47.hpp, ls47.cpp         74LS47 Component, Ls47Segments (glyph table)
src/parts/digit.hpp, digit.cpp       Digit Component (passive, like Led)
src/view/digit_shape.hpp, .cpp       DigitFrameOf, LampOutline (Board geometry, headless)
datasheets/74LS90.md, 74LS47.md, DIGIT.md
tests/parts/ls90_test.cpp, ls47_test.cpp, digit_test.cpp
tests/view/digit_shape_test.cpp
tests/machines/stopwatch_test.cpp
```

Everything else is small changes to existing files (§8).

## 2. 74LS90

```cpp
// src/parts/ls90.hpp
// Decade counter: ÷2 (CKA → QA) and ÷5 (CKB → QB..QD), falling edges; asynchronous R0 (clear)
// and R9 (set 9). Pins per datasheet: 1 CKB, 2 R01, 3 R02, 4 NC4, 5 VCC, 6 R91, 7 R92, 8 QC,
// 9 QB, 10 GND, 11 QD, 12 QA, 13 NC13, 14 CKA.
class Ls90 final : public Component {
public:
    explicit Ls90(std::string reference);
    static constexpr PinIndex kCkb = 0, kR01 = 1, kR02 = 2, kR91 = 5, kR92 = 6, kQC = 7,
                              kQB = 8, kQD = 10, kQA = 11, kCka = 13;

    void powerOn(PowerOnContext& ctx) override;  // qa_ random, five_ 3 random bits (0..7)
    void evaluate(EvalContext& ctx) override;

private:
    bool qa_ = false;
    std::uint8_t five_ = 0;  // ÷5 section, QD QC QB as bits 2..0
    bool unknownA_ = false, unknownFive_ = false;  // section is X
};
```

- Pins: `CKA`, `CKB` plain TTL inputs (not `risingEdgeOnly`: the chip acts on falling edges, so it is evaluated on both); `NC4`, `NC13` `PinKind::NoConnect`; outputs `Output`/TTL; `VCC`, `GND` `Power`.
- ÷5 next state, from the chip's three JK flip-flops (B: J = !QD, K = 1; C toggles on B falling; D: J = QB·QC, K = 1): `constexpr std::array<std::uint8_t, 8> kNextFive{1, 2, 3, 4, 0, 2, 2, 0};`. 5 and 6 go to 2, 7 to 0.
- `evaluate`, resets first (asynchronous, level-sensitive), with `GateFunction(GateKind::And, …)` for the two-input conditions:

  ```
  r9 = AND(R91, R92); r0 = AND(R01, R02)
  if r9 == 1:                 qa_ = 1, five_ = 4, both known      delays: kLs90SetNine
  else if r9 == X:            both unknown
  else if r0 == 1:            qa_ = 0, five_ = 0, both known      delays: kLs90Clear
  else if r0 == X:            both unknown
  else:
      if ctx.fell(kCka) && !unknownA_:    qa_ = !qa_              delays: kLs90CkaToQA
      if ctx.fell(kCkb) && !unknownFive_: five_ = kNextFive[five_]  delays: kLs90CkbToQB, …ToQC, …ToQD
  drive QA from qa_ (X if unknownA_), QB/QC/QD from five_ bits 0/1/2 (X if unknownFive_)
  ```

  An X clock edge (1 → X, X → 0) is no edge, as in `Ls161`. An unknown section stays X until a reset. Each output drives with the Delays of what last changed it (reset, set, or its clock); `drive` only schedules a real change.
- `src/parts/delays.hpp`, TI typicals (ps): `kLs90CkaToQA{10'000, 12'000}`, `kLs90CkbToQB{10'000, 14'000}`, `kLs90CkbToQC{21'000, 23'000}`, `kLs90CkbToQD{21'000, 23'000}`, `kLs90Clear{26'000, 26'000}` (only fall used), `kLs90SetNine{20'000, 26'000}` (QA, QD rise; QB, QC fall).
- Catalogue: `PartInfo{"74LS90", "decade counter", "DIP-14", Chip, &CreateChip<Ls90, "74LS90">}`, sorted (after `74LS86`: "74LS9" > "74LS8").
- [catalogue_test.cpp](../../tests/parts/catalogue_test.cpp#L49-L50) asserts `GND` at pin count/2 and `VCC` at the last pin for every Chip: it becomes "exactly one `VCC` and one `GND` Power Pin", with the corner positions still checked except for a listed `kOffCornerPower{"74LS90"}` (comment: its datasheet puts them on pins 5 and 10). ADD_PART.md says so.
- Datasheet `74LS90.md`: Function (two counters in one package: `CKA` → `QA` divides by two, `CKB` → `QB`–`QD` by five, each on the falling edge; link `QA` to `CKB` and feed `CKA` for a 0–9 count on `QA`–`QD`, `QD` falling on 9 → 0, so it clocks the next stage; `R01` and `R02` both high clear to 0, `R91` and `R92` both high set 9, set wins; **`VCC` is pin 5 and `GND` pin 10**), Pins (`CKA`, `CKB`, `R01`, `R02`, `R91`, `R92`, `QA`–`QD`, `NC4`, `NC13`, `VCC`, `GND`).

## 3. 74LS47

```cpp
// src/parts/ls47.hpp
// BCD to 7-segment decoder/driver, open-collector outputs, low = lit. Pins per datasheet:
// 1 B, 2 C, 3 LT_N, 4 BI_N (BI/RBO: blanking input only), 5 RBI_N, 6 D, 7 A, 8 GND,
// 9 SE_N, 10 SD_N, 11 SC_N, 12 SB_N, 13 SA_N, 14 SG_N, 15 SF_N, 16 VCC.
class Ls47 final : public Component {
public:
    explicit Ls47(std::string reference);
    static constexpr PinIndex kB = 0, kC = 1, kLtN = 2, kBiN = 3, kRbiN = 4, kD = 5, kA = 6,
                              kSE = 8, kSD = 9, kSC = 10, kSB = 11, kSA = 12, kSG = 13, kSF = 14;
    void evaluate(EvalContext& ctx) override;
};

// Segments lit (bit 0 = a … bit 6 = g) for inputs given as 0/1. Datasheet function table.
std::uint8_t Ls47Segments(unsigned value, bool lampTestN, bool rippleBlankingInN, bool blankingInN);
```

- Outputs `PinKind::OpenCollector`, TTL; inputs TTL; `VCC`, `GND` `Power`. No state, so the default `powerOn` random outputs are replaced at the first evaluation (power-on evaluates every Component).
- Glyph table, bit 0 = a … bit 6 = g, values 0–15: `0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7C, 0x07, 0x7F, 0x67, 0x58, 0x4C, 0x62, 0x69, 0x78, 0x00` (6 without `a`, 9 without `d`; 10–14 the datasheet's fragments; 15 blank).
- `Ls47Segments`: `!blankingInN` → 0; else `!lampTestN` → `0x7F`; else `!rippleBlankingInN && value == 0` → 0; else the table.
- `evaluate`: read the seven inputs; the X ones (at most 7) are enumerated, no allocation:

  ```
  on = 0x7F, off = 0x7F      // segments lit in every case / dark in every case
  for each assignment of 0/1 to the X inputs:
      s = Ls47Segments(...); on &= s; off &= ~s
  segment k: on bit → drive 0; off bit → drive 1 (open collector: released); else drive X
  ```

  Driving 1 on an `OpenCollector` Pin is no drive (`DriveOf`), so the segment's Net floats unless the resistor's far side holds it.
- Delays: `kLs47{50'000, 50'000}` (TI gives only the 100 ns maximum for `tPHL`/`tPLH` from data; half of it, said so in the commit).
- Catalogue: `PartInfo{"74LS47", "BCD to 7-segment decoder, open collector", "DIP-16", Chip, &CreateChip<Ls47, "74LS47">}` between `74LS32` and `74LS74`.
- Datasheet `74LS47.md`: Function (shows `A`–`D` (BCD, `A` lowest) on a common-anode digit: a segment output goes low to light it, through a resistor; open-collector outputs only sink; the glyph list in words, 6 and 9 without tails, 10–14 fragments, 15 blank; `LT_N` low lights all; `BI_N` low blanks all and wins; `RBI_N` low blanks a 0; on the real chip pin 4 is also the ripple-blanking output, not used here), Pins (`A`–`D`, `LT_N`, `BI_N`, `RBI_N`, `SA_N`–`SG_N` naming segment letters, `VCC`, `GND`).

## 4. Digit Part and Lamps

### 4.1 Lamps in the catalogue

[catalogue.hpp](../../src/parts/catalogue.hpp#L15):

```cpp
enum class LampShape : std::uint8_t { Round, SegmentA, SegmentB, SegmentC, SegmentD, SegmentE,
                                      SegmentF, SegmentG, DecimalPoint };

// One light of a Part: lit while an anode Pin reads a driven 1 and the cathode Pin a driven 0.
struct Lamp {
    LampShape shape;
    std::array<std::string_view, 2> anodes;  // Pin names; the second "" if one
    std::string_view cathode;
};

struct PartInfo {
    ...
    PartFactory create;
    std::span<const Lamp> lamps{};  // empty: the Part does not light
};
```

`PartKind::Led` stays the kind of both the LED and the digit (Parts made of LEDs). The kind's branches keep working unchanged: nothing branches on `Led` except the Session, which now reads `lamps` instead (§6).

In catalogue.cpp, above `using enum PartKind;`:

```cpp
constexpr std::array<Lamp, 1> kLedLamps{{{LampShape::Round, {"A", ""}, "K"}}};
constexpr std::array<Lamp, 8> kDigitLamps{{
    {LampShape::SegmentA, {"CA1", "CA2"}, "A"}, ... {LampShape::SegmentG, {"CA1", "CA2"}, "G"},
    {LampShape::DecimalPoint, {"CA1", "CA2"}, "DP"},
}};
```

Rows: `PartInfo{"LED", "red 5 mm LED", "LED-5mm", Led, &CreateLed, kLedLamps}` and `PartInfo{"DIGIT", "red 0.56-inch 7-segment digit, common anode", "DIGIT-0.56", Led, &CreateDigit, kDigitLamps}` (sorted: `BUTTON` < `DIGIT` < `LED`). `CreateDigit` (no parameters) beside `CreateLed`.

### 4.2 Digit Component

```cpp
// src/parts/digit.hpp
// Red 0.56" common-anode 7-segment digit (LTS-6960HR pinout). Pin index = KiCad pad − 1:
// 1 E, 2 D, 3 CA1, 4 C, 5 DP, 6 B, 7 A, 8 CA2, 9 F, 10 G. Passive like Led: every Pin a
// Family::Mos load, no behaviour; its lamps are in the catalogue.
class Digit final : public Component { ... };
```

- KiCad footprint ([kicad_netlist.cpp](../../src/board/kicad_netlist.cpp#L15)): array size 8, `{"DIGIT-0.56", "Display_7Segment:7SegmentLED_LTS6760_LTS6780"}`: pads 1–5 in one row, 6–10 in the other, 15.24 mm apart; courtyard drawn with `fp_line`s (read since P4d), 19.56 × 13.1 mm.
- Datasheet `DIGIT.md` in the shape of [LED.md](../../datasheets/LED.md): Function (seven bar LEDs `A`–`G` and a decimal point `DP` sharing their anodes; `A` top, `B` top right, `C` bottom right, `D` bottom, `E` bottom left, `F` top left, `G` middle; `CA1` and `CA2` joined inside, both to +5 V; a segment lights while its pin is pulled low through a resistor), Pins (`A`–`G`, `DP`, `CA1`, `CA2` with pad numbers).

## 5. Glow and Consumer

- [led_glow.hpp](../../src/devices/led_glow.hpp): `void setAnode(Sample sample, std::size_t which = 0);` with `std::array<Sample, 2> anodes_` (both start `{0, Z, true}`); `lit()` = any anode `One` and not floating, and the cathode `Zero` and not floating. Everything else unchanged.
- [led_consumer.hpp](../../src/runner/led_consumer.hpp#L12):

  ```cpp
  struct LedNets {
      std::string reference;  // "D1", "DS1"
      std::size_t lamp;       // index into the Part's catalogue lamps
      std::array<std::optional<NetId>, 2> anodes;  // at least one
      NetId cathode;
  };
  ```

  An index, not a `LampShape`: `bw_runner` links only `bw_devices` and `bw_instruments`, and stays off `bw_parts`.

  `nets()` = distinct Nets over every anode and cathode; `sample` feeds `setAnode(sample, i)` for each matching anode slot `i`. One `LedGlow` per entry (per lamp).

## 6. Session

- [session.hpp](../../src/session/session.hpp#L51): `struct LedLight { std::string reference; LampShape shape; float brightness; };`. `leds()`: every lamp of every Part with lamps, in netlist order then lamp order; 0 when not powered or not watched.
- `makeLive` ([session.cpp](../../src/session/session.cpp#L56)): for each Part, for lamp `i` of `part.info->lamps`: cathode and each non-empty anode through `PinNet`, kept only if the Pin is *connected*, i.e. listed in `loaded.connections` (`PinNet` alone never fails: after `finalize` an unconnected Pin has a private Net, so today's "unconnected Pin: never lit" skip at L64 never fires). Push `LedNets{reference, i, …}` when the cathode and at least one anode are connected. No `PartKind` test.
- `leds()` ([L333](../../src/session/session.cpp#L333)): same walk over `machine_->designCircuit()`, one `LedLight` per lamp, `shape` from the catalogue lamp, brightness looked up by `(reference, lamp)` in `live_->leds->leds()`; 0 if not watched.

## 7. Drawing the Digit

### 7.1 Geometry (headless, `bw_view`)

```cpp
// src/view/digit_shape.hpp
// A digit's own axes on the Board: it stands along its courtyard's long side, bottom on the side
// of pad 1. nm; `up` and `right` unit vectors (Board y grows downwards).
struct DigitFrame {
    double centreX, centreY;
    double upX, upY, rightX, rightY;
    double length, breadth;  // courtyard long and short sides
};

// nullopt unless the courtyard has 4 corners.
std::optional<DigitFrame> DigitFrameOf(std::span<const Point> courtyard, Point pad1);
// Convex outline of one segment or the decimal point, Board nm. Check()s shape != Round.
std::vector<Point> LampOutline(const DigitFrame& frame, LampShape shape);
```

- `DigitFrameOf`: corners `c0..c3`; edges `c1 − c0`, `c2 − c1`; the longer gives the up axis and `length`, the other `breadth`; centre = mean of the corners; `up` points away from pad 1 (`dot(centre − pad1, up) > 0`, else negate); `right = (−upY, upX)`, so the digit reads left to right on the front view at any rotation (upright: up = (0, −1), right = (1, 0)).
- `LampOutline`, in digit units (u along `right`, v along `up`, origin the centre, unit `length`): character height `h = 0.62`, width `w = 0.30`, bar thickness `t = 0.075`, centre shifted `u0 = −0.04` (room for the point), slant `u += 0.08·v`.
  - Bars between end points (before the slant): `A` (−w/2, h/2)–(w/2, h/2), `G` at v = 0, `D` at v = −h/2; `F` (−w/2, 0)–(−w/2, h/2), `B` at u = w/2; `E` (−w/2, −h/2)–(−w/2, 0), `C` at u = w/2.
  - Each bar a hexagon: ends pulled in by `0.6 t`, pointed ends `t/2` long, `t` thick.
  - `DecimalPoint`: octagon of radius `0.6 t` at (w/2 + 0.9 t, −h/2).
  - Then u += u0, slant, and to the Board: `centre + length·(u·right + v·up)`.
  On the LTS footprint (19.56 mm): character 12.1 × 5.9 mm, bars 1.5 mm.
- Winding: every outline is listed counter-clockwise in digit units (u right, v up). `(u, v) → u·right + v·up` is a rotation (no mirror), and `Mapper` only scales and translates, so the order stays consistent on screen, as `AddConvexPolyFilled` needs.

### 7.2 Board view and frame loop

- [board_view.hpp](../../src/ui/board_view.hpp#L54): `struct LedTag { std::size_t component; LampShape shape; float brightness; };`.
- Frame loop ([main.cpp](../../src/ui/main.cpp#L495)): `ledTags.push_back({*component, led.shape, led.brightness})`.
- Drawing ([board_view.cpp](../../src/ui/board_view.cpp#L395)), still under the pads:

  ```
  bodyDrawn = none
  for each LedTag:
      Round: as today
      else:
          frame = DigitFrameOf(scene_->courtyard(k), pad "1" of board.components[k]); none → skip
          if bodyDrawn != k: AddConvexPolyFilled(courtyard, kDigitBody); bodyDrawn = k
          glow = clamp(brightness)
          AddConvexPolyFilled(map(each point of LampOutline(frame, shape)), Lerp(kSegmentDim, kLedLit, glow))
  ```

  The body polygon is the courtyard's 4 corners through `map`. `kDigitBody = IM_COL32(28, 28, 30, 255)`, `kSegmentDim = IM_COL32(70, 22, 22, 255)`; `kLedLit` shared with the LED. No halo (segments are close together). Pads are drawn after, on top, as on every Component (see-through Board).

## 8. Integration Points

1. **Falling edge** — [simulator.hpp](../../src/sim/simulator.hpp#L90): `inline bool EvalContext::fell(PinIndex pin) const` (`changedAt == now_ && previous == One && value == Zero`), declared in [component.hpp](../../src/sim/component.hpp#L29) beside `rose`.
2. **Watch budget** — [dispatcher.hpp](../../src/runner/dispatcher.hpp#L16): `kMaxWatched = 48`. Markers carry the slot as `std::uint8_t`: fine. Recipes' "32" updated.
3. **Builder warning** — [builder.cpp](../../src/netlist/builder.cpp#L299): `pins[i].kind == PinKind::Input && pins[i].family == Family::Ttl && …`. LED and digit pins are `Family::Mos`.
4. **Catalogue, footprint, CMake** — §2–§4; `ls47.cpp`, `ls90.cpp`, `digit.cpp` in `src/parts/CMakeLists.txt` (sorted); `digit_shape.cpp` in `src/view/CMakeLists.txt`.
5. **Recipes** — ADD_BOARD_DEVICE: lamps in the catalogue replace the per-kind Session branch, digit drawing via `digit_shape`, budget 48, a digit costs its segments + one anode Net (not 2 per lamp), resistors only rail-to-signal; ADD_PART: `fell` for falling-edge chips, open-collector outputs drive 1 to release, off-corner power Pins in the catalogue test; ADD_MACHINE: reference prefix `DS` for digits; ADD_INSTRUMENT: budget 48.

## 9. Reused Infrastructure

- `GateFunction`, `DelayFor`, `CreateChip`, `PinKind::OpenCollector` and `DriveOf`, resistor groups (a segment Net takes the decoder's 0 across the resistor, weak).
- `LedGlow` windows, `LedConsumer`, the Session's LED path, `BoardScene::courtyard` (lines courtyards since P4d), `componentIndex`.
- Resistors between a rail and a signal only (the builder refuses series resistors): each digit has one anode resistor, its segments sit on the decoder outputs.
- Machine stack unchanged: Broken Traces, stuck outputs (an `OpenCollector` output stuck at 0 ties the Net Strong 0), Powered per build (the 74LS90's `VCC` on pin 5 is just a Power Pin named `VCC`).
- Buttons, `--press`, `ButtonOf`, Session presses (P4d), for Start, Stop and Reset.

## 10. Build Integration

```cmake
# src/parts/CMakeLists.txt: + digit.cpp ls47.cpp ls90.cpp
# src/view/CMakeLists.txt:  + digit_shape.cpp
# tests/CMakeLists.txt:     + machines/stopwatch_test.cpp parts/digit_test.cpp parts/ls47_test.cpp
#                             parts/ls90_test.cpp view/digit_shape_test.cpp
```

No new dependencies.

## 11. Dependency Diagram

```
bw_sim (fell) ← bw_parts (Ls90, Ls47, Digit, Lamp) ← bw_netlist (TTL-only warning) ← bw_board ← bw_view (digit_shape)
bw_devices (LedGlow anodes) ← bw_runner (LedConsumer lamp indexes, kMaxWatched) ← bw_session (lamps from bw_parts) ← bw_ui (digit drawing)
```

No new library edges: the Session already reaches `bw_parts` through `bw_machine`.

```
```

## 12. Testing

Catch2, tags as the module. The prescaler makes one display count every 10 ms; a Puzzle test needs 0.1–0.3 s of simulated time (debug ≈ 1–2 s wall). Only the seconds test runs past 1 s (`[slow]`).

| File | Checks |
|---|---|
| `tests/sim/simulator_test.cpp` | `fell` true only at the timestamp of a 1 → 0 change |
| `tests/parts/ls90_test.cpp` `[parts]` | catalogue row, pins (`VCC` pin 5, `GND` pin 10, NC); ÷2: `QA` toggles on `CKA` falling only; ÷5: `QB`–`QD` run 0..4 on `CKB` falling; with `QA` → `CKB` linked in the Bench: ten falling edges give 0..9, 0 and `QD` falls on 9 → 0; R0 both high clears asynchronously (one high: no clear); R9 sets 9 and wins over R0; from 5, 6, 7 the next `CKB` edge gives 2, 2, 0; delays `kLs90CkaToQA`, `kLs90CkbToQD`, `kLs90Clear`; R01 at X with R02 high → outputs X, then a clean clear recovers; X clock edge counts nothing |
| `tests/parts/ls47_test.cpp` `[parts]` | catalogue row, outputs `OpenCollector`; `Ls47Segments` for 0–15 against the table; `LT_N` low → all lit, `BI_N` low wins over `LT_N`; `RBI_N` low blanks 0 only; in a Bench with a pull-up per output: lit segment Net 0, dark 1 (released, pulled up); `A` at X with `B`–`D` 0 (0 or 1): `b`, `c` lit, `g` dark, `a`, `d`, `e`, `f` X; delay `kLs47` |
| `tests/parts/digit_test.cpp` `[parts]` | `FindPart("DIGIT")`: kind `Led`, package `DIGIT-0.56`, ten Mos input Pins in pad order, eight lamps (seven `{CA1, CA2}` segments, `DP`) all naming existing Pins; LED row has one Round lamp `A`/`K` |
| `tests/parts/catalogue_test.cpp` | every lamp's Pin names exist on its Part (all Parts); pinout check with one `VCC`/`GND` each and the `74LS90` exception |
| `tests/devices/led_glow_test.cpp` | second anode alone lights; both floating dark; existing cases unchanged |
| `tests/runner/led_consumer_test.cpp` | digit lamps sharing an anode Net: `nets()` distinct; one segment fed low lit, others dark |
| `tests/netlist/builder_test.cpp` | an unconnected `DIGIT` `DP` gives no warning; an unconnected TTL input still does |
| `tests/view/digit_shape_test.cpp` | `DigitFrameOf` on the LTS courtyard at 0°, 90°, 180°, 270°: `up` away from pad 1, `length` 19.56 mm; `LampOutline(SegmentA)` lies above `SegmentD` along `up`, `SegmentB` right of `SegmentF` along `right`; every outline inside the courtyard; fewer than 4 corners → nullopt |
| `tests/machines/stopwatch_test.cpp` `[machines]` | netlist loads without warnings; Puzzles have a title and a brief; `Display(loaded, sim)` reads the three digits as text (`"0.12"`, `'?'` for a non-digit shape, `' '` for blank) from the segment Nets (lit = 0, not floating); every reading taken mid-count (≥ 4 ms from a count edge: the first `GCLK` fall comes up to 0.5 µs after Start, plus ripple); fault-free: Stop and Reset first → `"0.00"`; Start at 2 ms, Stop at 125.4 ms → `"0.12"`; Start again 50 ms → `"0.17"`; `[slow]` Start, read at 1.055 s → `"1.05"`; per Puzzle (STOPWATCH.md, Puzzles), Symptom then Repair: stuck_at_zero — `"0.00"` 105 ms after Start, `RESET` 1, swap `U12` → `"0.10"` at 105 ms; nineteen — after Reset and Start the tenths only 0 and 1 over 400 ms, `U6.CKB` Net floating, mend → `"0.25"` at 255 ms; eights — `"0.80"` after Reset, swap `U9` → `"0.00"`; broken_bar — `DS1` reads `'?'` after Reset, `DS1.E` Net floating, mend → `"0.00"`; non_stop — Stop does not freeze, mend → frozen; dark_seconds — `DS1` blank while `DS2`/`DS3` count, `U5` not Powered, mend → `"0.00"` after Reset |
| `tests/session/session_test.cpp` `[session][slow]` | stopwatch with every instrument (8 Clips, 2 Hooks, probe, meter) attached, no presses (`Speed::Max` can't place them in time): `leds()` lists 24 lamps in order; `powerOn(1, 1_s)`, `waitIdle`; `DS1`'s `DP` lamp ≈ 1; nothing aborts (watch budget) |
| existing | `boards_test`, `electrical_test`, `board_scene_test`, `manuals_test` loops gain `"stopwatch"`; `drc_stopwatch`; `catalogue_test`, `kicad_netlist_test`, `session: counter LED` (now one lamp) |

UI check (release build): `bw_ui puzzles/eights.bwp --press SW3:50ms:100ms,SW1:200ms:300ms --screenshot /tmp/sw.png --after 1s` shows the three digits upright, `DS2` with its middle bar lit; the status line stays 1.00× with every instrument attached.

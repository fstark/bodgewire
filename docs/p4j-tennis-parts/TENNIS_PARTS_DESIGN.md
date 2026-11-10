# P4j Tennis Parts — Detailed Design

Implements [TENNIS_PARTS.md](TENNIS_PARTS.md): the 74LS157 and 74LS48 Parts, the reversed controller, two players' keys, and the tests of the paddle tennis. The Machine's netlist, Board and manuals are in [TENNIS_DESIGN.md](../machines/070-tennis/TENNIS_DESIGN.md).

Recipes followed: [ADD_PART.md](../recipes/ADD_PART.md) (74LS157, 74LS48), [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md) (a controller parameter, the UI section), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md). This design gives only what those recipes leave open.

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/parts/ls157.hpp, ls157.cpp           74LS157 Component
src/parts/ls48.hpp, ls48.cpp             74LS48 Component
datasheets/74LS157.md, 74LS48.md
tests/parts/ls157_test.cpp, ls48_test.cpp
tests/machines/tennis_test.cpp
netlists/tennis.bwn, manuals/tennis/, boards/tennis/ (Machine design)
```

Everything else is small changes to existing files (§6).

## 2. 74LS157

A Component of its own, not a gate package: its four units share `S` and `G_N` ([gate_package.cpp](../../src/parts/gate_package.cpp) takes only independent gates).

```cpp
// src/parts/ls157.hpp
// Quad 2-to-1 data selector. Pins per datasheet: 1 S, 2 1A, 3 1B, 4 1Y, 5 2A, 6 2B, 7 2Y, 8 GND,
// 9 3Y, 10 3B, 11 3A, 12 4Y, 13 4B, 14 4A, 15 G_N, 16 VCC. Y = A while S is low, B while S is
// high; G_N high forces every Y low.
class Ls157 final : public Component {
public:
    explicit Ls157(std::string reference);
    static constexpr PinIndex kS = 0, kGN = 14;

    void evaluate(EvalContext& ctx) override;
};
```

- Units `{a, b, y}`: `{1, 2, 3}`, `{4, 5, 6}`, `{10, 9, 8}`, `{13, 12, 11}`.
- `Y` = `G_N` ? 0 : (`S` ? `B` : `A`), with X folded as the 74LS47 does: when `S` or `G_N` is X, `Y` is worked out for each value they could have, and is that value where all agree, X otherwise (`S` at X with `A` = `B` gives `A`, 0 or 1; `G_N` at X with the selected data 0 gives 0). An X data input gives X when selected.
- Delay by the input that moved: `S` rose or fell → `kLs157Select`, else `G_N` rose or fell → `kLs157Strobe`, else `kLs157Data`. `src/parts/delays.hpp`, TI typicals (ps): `kLs157Data{9'000, 9'000}`, `kLs157Select{15'000, 18'000}`, `kLs157Strobe{13'000, 14'000}`. The PDF cannot be fetched: known typical values, said so in the commit.
- No `powerOn` (no state; the first evaluation drives every output).
- Catalogue: `PartInfo{"74LS157", "quad 2-to-1 multiplexer", "DIP-16", Chip, &CreateChip<Ls157, "74LS157">}` after `74LS107` (sorted as strings). Power at the corners.
- Datasheet `74LS157.md`: Function (four switches sharing one control: each `Y` copies its `A` input while `S` is low and its `B` input while `S` is high; `G_N` high turns every `Y` off (low), tie it low when unused), Pins (`S`, `G_N`, `1A`–`4A`, `1B`–`4B`, `1Y`–`4Y`, `VCC`, `GND`).

## 3. 74LS48

The 74LS47 with active-high outputs: same pinout, same function table, same glyphs (6 and 9 without tails), segment pins named `SA`–`SG` (no `_N`) and `PinKind::Output`.

- [ls47.hpp](../../src/parts/ls47.hpp) gains the X fold the 74LS47 does today in `evaluate`, so both decoders share it:

  ```cpp
  // Segments certainly lit and certainly dark (bit 0 = a … bit 6 = g) for the inputs read by
  // `ctx` at A, B, C, D, LT_N, RBI_N, BI_N (the 74LS47/48 pin indices): every value an X input
  // could have is tried.
  struct SegmentCertainty { std::uint8_t lit, dark; };
  SegmentCertainty ReadSegments(EvalContext& ctx);
  ```

  `Ls47::evaluate` drives `SX_N` low where lit, high where dark, X otherwise (unchanged behaviour); `Ls48::evaluate` drives `SX` high where lit, low where dark.
- `class Ls48 final : public Component` in `src/parts/ls48.{hpp,cpp}`, the pin indices of `Ls47` (`static constexpr PinIndex kSA = 12, …`).
- Delay `kLs48{50'000, 50'000}`: as `kLs47`, TI gives only 100 ns max.
- Catalogue: `PartInfo{"74LS48", "BCD to 7-segment decoder, active-high outputs", "DIP-16", Chip, &CreateChip<Ls48, "74LS48">}` after `74LS47`.
- Datasheet `74LS48.md`, in the shape of `74LS47.md`: a lit segment's output is high (pulled up inside the chip), for driving logic or a common-cathode display; `BI_N`, `LT_N`, `RBI_N` as the 74LS47.

## 4. Reversed controller

- `Paddle(std::string reference, Time shortest, Time longest, bool reversed = false)`; `pulseLength()` = `reversed_ ? longest_ − llround(knob × (longest_ − shortest_)) : shortest_ + …`. Nothing else in the Part changes.
- `CreatePaddle` in [catalogue.cpp](../../src/parts/catalogue.cpp) accepts `reversed`: `yes` → true, `no` → false, anything else "PADDLE: reversed must be yes or no". `min` and `max` keep their meaning (shortest and longest pulse).
- [PADDLE.md](../../datasheets/PADDLE.md): Function gains "A controller may be wired the other way round (the Machine's Service Manual says so): the longest pulse at the left stop."
- [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md): Parameters row mentions an optional parameter with a default.

## 5. Two players' keys

[side_panel.cpp](../../src/ui/side_panel.cpp) `DrawKnobs`:

```
kKnobKeyRate = 1.0                                   // turn per second (was 0.5)
keys: unless io.WantTextInput:
    one knob:   Up − Down turns knobs[0]
    two or more: Q − A turns knobs[0]; Up − Down turns knobs[1]
tooltip of knob i: "Turn the knob: drag, mouse wheel, or the <keys> keys" with
    <keys> = "Up and Down" (one knob, or i == 1), "Q and A" (i == 0 of several); others without keys
```

- Q and A are free: the Board view's keys are C, F, K, L, M, O, P, R, S, U, V, W, `+`, `-` and Escape.
- Knobs come in netlist order (`Session::knobs`), so the tennis netlist lists `J3` before `J4`.
- Not unit tested (ImGui input); checked by hand in the release check and by the user.

## 6. Other changes

| File | Change |
|---|---|
| `src/parts/catalogue.cpp` | rows and includes for `74LS157`, `74LS48`; `reversed` in `CreatePaddle` |
| `src/parts/CMakeLists.txt` | `ls157.cpp`, `ls48.cpp` (sorted) |
| `src/parts/delays.hpp` | `kLs157Data`, `kLs157Select`, `kLs157Strobe`, `kLs48` |
| `src/parts/ls47.{hpp,cpp}` | `ReadSegments` shared (§3) |
| `src/parts/paddle.{hpp,cpp}` | `reversed` (§4) |
| `src/ui/side_panel.cpp` | keys (§5) |
| `tests/CMakeLists.txt` | `parts/ls157_test.cpp`, `parts/ls48_test.cpp`, `machines/tennis_test.cpp` |
| `tests/parts/paddle_test.cpp` | `reversed=yes`: knob 0 → `max`, 1 → `min`, 0.5 half way; `reversed=maybe` refused with its message |
| `tests/machines/manuals_test.cpp` | `tennis` in the Machine list (phase 10) |
| `tests/board/boards_test.cpp`, `tests/machine/electrical_test.cpp`, `tests/view/board_scene_test.cpp` | `tennis` in the per-Machine loops (phase 11) |
| `tests/session/session_test.cpp` | `session: tennis with every instrument` (phase 12) |

## 7. Tests

### Parts

- `tests/parts/ls157_test.cpp` `[parts]`: catalogue row and pins; with `G_N` low each `Y` follows `A` while `S` is low and `B` while high (`GENERATE` over the four units); `G_N` high gives 0 whatever the data; delays `kLs157Data`, `kLs157Select`, `kLs157Strobe` on the edge each one covers; `S` at X: `Y` is `A` when `A` = `B`, X otherwise; `G_N` at X: 0 when the selected data is 0, X otherwise.
- `tests/parts/ls48_test.cpp` `[parts]`: catalogue row and pins (`SA`–`SG` are `Output`); `SX` high exactly where `Ls47Segments` lights, for 0–15; `BI_N` low blanks, `LT_N` low lights all, `RBI_N` low blanks a 0; an X input that changes nothing leaves outputs known; delay `kLs48`. The 74LS47 tests stay green after `ReadSegments` moves.

### The Machine (`tests/machines/tennis_test.cpp`)

Copies the shape of [ball_test.cpp](../../tests/machines/ball_test.cpp): `Watch` runs the TV in 1 ms steps from power-on and calls back on each completed frame; `Read` turns a frame into a picture.

- Geometry: one dot is 4 TV columns, one line one row. With no side walls, `Read` takes dot 0 as the first lit column of the top wall's second row (the first row is line 0, drawn early, see `ball: box and dashed net`) and the walls as the full-width row runs. Then `Dot(column)` and `Line(row)` place everything else.
- `Picture`: walls, net rows, the two paddles (lit cells on dots 6–7 and 88–89 inside the court: top line, height), the ball (lit cells inside the court that are not net, paddle or digit cells), and the four digit cells read as segment masks at fixed points (a: line 33, g: 45, d: 61, each at the cell's dot 1; f, b: line 39; e, c: line 53, at the cell's dots 0 and 3), turned into a number by the 74LS48 glyphs (`Ls47Segments`), blank tens as 0. A frame where the ball overlaps a digit cell is not read for digits.
- Knobs: `PaddleOf(loaded, "J3")->setKnob(k)` between steps (read at the next trigger, no wake). `KnobForTop(line)` = (224 − line) / 208, clamped, the inverse of TENNIS.md's ⌊224.5 − 208 k⌋.
- A test player (`Follow`): each frame, sets a knob so that the ball's line 4 will meet the paddle at a chosen paddle line, from the ball's top and vertical speed in the last two frames, two frames ahead (a knob takes effect at the next trigger, seen in the frame after). Checks use what the picture shows at the hit, not the aim.
- `Watch` gains a per-step hook that gets the `Simulator` (and the `LoadedCircuit`), called before each 1 ms step, for knobs and the button. Start: `button->setPressed(true)`, `sim.wake(button->id())` (as in `dice_test.cpp`), one step, then released and woken again.
- Netlist: `LoadNetlistFile(BW_NETLIST_DIR / "tennis.bwn")` until the Board exists, then `LoadNetlistMachine` (phase 11, ADD_MACHINE.md step 3).
- Power-on draws a random bit for every output and Net, so adding chips changes what a seed gives: seeds are searched by the test (from the Nets after a 1 µs run), never written down. From stage 6 on, every case presses Start first, since a power-on may be game over.
- Seeds (`FindSeed`): after `powerOn(seed)` and a 1 µs run, the rally cases take the first seed with `FAST` high and `SERVING` low; the left-at-10 seed: the test reads `TENS_L` and `SL0`–`SL3` (and from stage 6 `GAME_OVER`) from the Nets and takes the first seed from 1 up with the left score at 10 (and the game on). No picture is needed for that.

Cases (stage of the plan in brackets), each `[machines]`, `[slow]` when over ~1 s simulated:

| Case | Checks |
|---|---|
| `tennis: netlist loads without warnings` (4) | every stage |
| `tennis: court` (4) | locked; top wall lines 0–15, bottom 240–255, dashed net at dots 48–49; once the ball has been inside the court, nothing lit on dots 0–1 and 94–95; over 4 s the ball is seen at dots 2–3 and 92–93 (the goals are not drawn) |
| `tennis: paddles follow the knobs` (5) | knobs at 0, 0.5, 1 in turn every 200 ms (`J3` and `J4` set opposite): from 45 ms after each turn, each paddle seen 16 lines tall has its top on line 224, 120, 16 |
| `tennis: the paddles return the ball` (6) | the test player on both sides, aiming at the paddles' middle for 4 s (same seed rule): at least three turns at a paddle, and after the first, the ball never seen behind a paddle |
| `tennis: angle, …` (6) | one case per quarter (separate cases, so ctest runs them in parallel): aimed at paddle line 2, 6, 9 or 13 for 4 s from a seed with the ball fast and no serve under way; at each contact away from the walls (one frame on the paddle's column when fast, two when slow), the ball's line 4 at the last contact is worked out from the frame before, and the vertical speed from the 1st to the 2nd frame after it is −4, −2, +2 or +4 lines a frame for its quarter (unchanged if line 4 was off the paddle); at least one contact in the aimed quarter |
| `tennis: a miss scores` (7) | Start; left player follows, right paddle kept away from the ball: `SERVING` (polled each ms) lasts 29–32 frames; no ball drawn in a frame wholly inside it; then the ball leaves the right edge (dots 88–93) at 2 dots every other frame; the left score goes 0 → 1, the right stays 0 (Nets) |
| `tennis: rally speeds up` (7) | after a miss by the left player and its serve (which clears `FAST`), both players follow for up to 10 s: horizontal speed 1 dot a frame (2 dots every other frame) until the ball has turned left twice, then 2 dots every frame (`FAST` and `TURN_ODD` start random, and are only cleared by a serve) |
| `tennis: Start clears the scores` (7) | any seed: after Start, `SL0`–`SL3`, `SR0`–`SR3`, `TENS_L`, `TENS_R` are 0 (and `GAME_OVER` from stage 6) |
| `tennis: score digits` (8) | after Start both digits read 0, tens blank; after the right player misses, the left units read 1; a left-at-10 seed reads "10" for the left before anything is pressed |
| `tennis: game over` (9) | the game over seed; right paddle kept away: after the point the left reads 11, the paddles are gone, the ball turns at a side edge without disappearing and the scores stay; Start: both 0, the paddles back |

Simulated time per case, about: court 2 s, paddles 0.3 s, returns 4 s, each angle 2 s, a miss 4 s, speed-up 6 s, Start 0.1 s, digits 4 s, game over 6 s. Debug runs a TV Machine at about 0.03× real time with 26 chips; 58 chips will be slower, so a 6 s case may take 5–7 minutes (ctest's limit is 1500 s). Each slow case runs on its own (ctest runs them in parallel); the plan records the times, and shortens a case (fewer frames checked) if one passes 10 minutes.

### Speed

`bw run netlists/tennis.bwn --time 2s` in release measures headless speed at stages 2, 4 and 6 (060: 2.12×). The gate is the release `bw_ui` with every instrument attached at 1.00× (phase 12). If the headless speed falls under 1.3×, the plan stops and the user is told before the Board.

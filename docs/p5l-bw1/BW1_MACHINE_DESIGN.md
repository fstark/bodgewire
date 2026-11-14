# P5l BW-1 — Detailed Design

Implements [BW1_MACHINE.md](BW1_MACHINE.md): lamps with two cathodes, the `DIGITCC` and `SLIDE` Parts, and Machine 120 BW-1, whose netlist, Board, manuals and tests are in [BW1_DESIGN.md](../machines/120-bw1/BW1_DESIGN.md).

Recipes: [ADD_BOARD_DEVICE.md](../recipes/ADD_BOARD_DEVICE.md) (output device chain; switches), [ADD_PART.md](../recipes/ADD_PART.md) (catalogue row, datasheet, `PartPinNames`), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) (§2 netlist, §3 Board, §5 Session test and release check). Code follows [STYLE.md](../STYLE.md), [NAMING.md](../NAMING.md).

---

## 1. Lamps with two cathodes

A lamp is lit while one of its anodes is high and one of its cathodes is low, both driven (not floating). A common-cathode digit's lamps share two joined cathode pins as a common-anode digit's share two anodes.

- `src/parts/catalogue.hpp` `Lamp`: `std::string_view cathode` → `std::array<std::string_view, 2> cathodes` ("the second "" if one"). Comment: a lamp lights while one anode is high and one cathode low.
- `src/parts/catalogue.cpp`: `kLedLamps` `{"K", ""}`; `kDigitLamps` `{"A", ""}` … `{"DP", ""}`.
- `src/devices/led_glow.{hpp,cpp}`: `setCathode(Sample, std::size_t which = 0)`, `cathodes_` an array of two (both `Z` floating at reset); `lit()`: any anode `One` driven and any cathode `Zero` driven. Callers with one cathode unchanged.
- `src/runner/led_consumer.{hpp,cpp}`: `LedNets::cathode` → `std::array<std::optional<NetId>, 2> cathodes` ("at least one"); `nets_` adds each present cathode once; `sample` routes a Net to every anode and cathode slot it fills (a Net on both cathodes of one lamp sets both).
- `src/session/session.cpp` `makeLive()`: both cathodes through `ConnectedNet`; a lamp needs one connected anode and one connected cathode.
- `src/ui/chip_panel.cpp` (line 178): the hovered pin lights a lamp when it is one of its anodes or cathodes (`std::ranges::find` on both arrays).

Tests: `tests/devices/led_glow_test.cpp` `led glow: either cathode lights a two-cathode lamp` (cathode 1 floating, cathode 2 low: lit; both floating: dark); `tests/runner/led_consumer_test.cpp` a lamp with two cathode Nets; `tests/parts/digit_test.cpp` and `tests/parts/catalogue_test.cpp` (line 108) on `cathodes`. Every other LED and digit test passes unchanged (counter, chaser, dice, stopwatch, heartbeat, LED computer, the Session's LED cases).

## 2. `DIGITCC`

- `src/parts/digit.{hpp,cpp}`: `enum class DigitCommon : std::uint8_t { Anode, Cathode }`; `Digit(std::string reference, DigitCommon common)`; pins by pad as now, pads 3 and 8 named `CA1`, `CA2` or `CC1`, `CC2`. Header comment: the common-cathode pinout is the LTS-6780's, the same pads.
- `src/parts/catalogue.cpp`: `CreateDigit<DigitCommon>` (template on the common; "DIGIT takes no parameters", "DIGITCC takes no parameters"); `kDigitCcLamps`: `{SegmentA, {"A", ""}, {"CC1", "CC2"}}` … `{DecimalPoint, {"DP", ""}, {"CC1", "CC2"}}`; row `PartInfo{"DIGITCC", "red 0.56-inch 7-segment digit, common cathode", "DIGIT-0.56", Led, &CreateDigit<DigitCommon::Cathode>, kDigitCcLamps}` after `DIGIT` (sorted by name). The footprint (`kicad_netlist.cpp`, `DIGIT-0.56`), the Board drawing (`digit_shape`, by package) and the chip panel (`TopBottomRows`, by package) need nothing.
- `datasheets/DIGITCC.md`: as `DIGIT.md`, the segment pins anodes (lit while high, with the common pin low), `CC1`, `CC2` the common cathode, "usually switched to ground by a digit driver such as the 74145's outputs, one digit at a time".
- Part type names in a netlist: the parser takes the type as one word; `DIGITCC` is checked by the `digit:` test that builds `part DS1 DIGITCC`.

Tests (`tests/parts/digit_test.cpp`): `digit: common-cathode lamps` (eight lamps, anodes `A`…`DP`, cathodes `CC1`, `CC2`; pin names by pad: 3 `CC1`, 8 `CC2`); `digit: DIGITCC takes no parameters`. A Session's lamps need a Machine with a Board, so the Session path of `DIGITCC` is checked by the BW-1's Session case (phase 5).

## 3. `SLIDE`

The SST switch: a `Switch` whose position the player sets and leaves.

- `boards/footprints/SW_DIP_SPSTx01_Slide_9.78x4.72mm_W7.62mm_P2.54mm.kicad_mod`, copied unchanged from KiCad 10.0.7 (`Button_Switch_THT.pretty`; the README's rule): pads `1`, `2`, 7.62 mm apart.
- `src/board/kicad_netlist.cpp` `kFootprints`: `{"SLIDE-1", "Button_Switch_THT:SW_DIP_SPSTx01_Slide_9.78x4.72mm_W7.62mm_P2.54mm"}`.
- `src/parts/catalogue.cpp`: `PartInfo{"SLIDE", "slide switch", "SLIDE-1", Switch, &CreateSwitch<"SLIDE">}` (after `R`). `CreateSwitch`: `SLIDE` takes `legend=` only ("SLIDE takes legend=, not pc="); a legend as a `KEY`'s (1–4 characters). No PC key: a slide is moved by a click, so a one-character legend does not become its `pc=` as a key's does. The legend error names the Part ("SLIDE legend= …", not "KEY legend= …").
- `src/parts/switch.hpp` header comment: a push button, key or slide switch. No behaviour change: closed is `pressed()` (or the Fault); Faults `fault switch SW24 closed|open` and the swap work as for any switch.
- `datasheets/SLIDE.md`: two legs, joined while the knob is at ON; it stays where it is moved.
- `src/ui/board_view.hpp` `SwitchTag`: `bool key` → `SwitchLook look` (`enum class SwitchLook : std::uint8_t { Button, Key, Slide }`). `src/ui/board_view.cpp`:
  - drawing (line 462): `Slide`: the courtyard's rectangle a dark red DIP body (`kSlideBody`), a recessed slot (`kSlideSlot`) along its long side, the knob (`kSlideKnob`, off-white, a third of the slot) at the slot's `ON` end while pressed, else at the other end; the legend in `legendFont_` beside the body on the far side from pad 1 when it fits inside the courtyard, else above it (`ON` printed small at its end). Body colours in the file's constants with the button's.
  - click: a left click on a slide's body (not under the Swap tool) does not hold it: it sets the existing `latchToggle_` (as a right-click does), clears `held_` and `pressed_` (so the release gives no `BoardClick` to the tool). Held and latch stay as they are for `Button` and `Key`.
  - the keycap test for `DrawCourtyard` (line 525, `t.key`) becomes `look != Button`: a slide's reference goes small in the corner, as a key's, clear of the knob.
- `src/ui/main.cpp` (line 636): a set `slid` of references; `takeLatchToggle()` on a slide toggles it there (powered or not), on a button or key `latched` as now (powered only, cleared at power-off). `slid` is not cleared at power-off; `Session::setPressed` is ignored while unpowered, and the per-frame `setPressed` carries the position to the next power-on; `session.setPressed(reference, slid || scripted || …)`. The `SwitchTag`'s look from the Part's name (`KEY`, `SLIDE`, else button). `--press SW24:1ms:10s` holds it on as for any switch.

Tests: `tests/parts/switch_test.cpp` `switch: SLIDE takes legend= only` (the row, the legend, `pc=` refused); `tests/parts/catalogue_test.cpp` covers its row ; the package's footprint by `footprint: every catalogue package loads` (`tests/board/footprint_test.cpp`). The drawing and clicks are checked by screenshot on the BW-1 (phase 5).

## 4. Machine 120 BW-1

[BW1_DESIGN.md](../machines/120-bw1/BW1_DESIGN.md): netlist `netlists/bw1.bwn`, manuals, `boards/bw1/bw1.place`, the Board, `tests/machines/bw1_test.cpp`.

The digit reader of the tests (`Bw1Display` in `tests/machines/bw1_test.cpp`) uses the model's character table: `Bw1DigitChar(std::uint8_t segments)` moved from `bw1_model.cpp` to a free function in `bw1_model.hpp`, the model's `display()` calling it.

## 5. Docs

- [ADD_BOARD_DEVICE.md](../recipes/ADD_BOARD_DEVICE.md): lamps with two cathodes (common-cathode digits); the slide switch in "Input device" (a click moves it, kept across power).
- [ADD_MACHINE.md](../recipes/ADD_MACHINE.md): the BW-1's reference commits; a keypad placed by `next` rules; what this Machine found.
- [GLOSSARY.md](../GLOSSARY.md): nothing new expected (Strap came with P5k).
- [GAME_DESIGN.md](../GAME_DESIGN.md) ladder: row 120 BW-1. [ARCHITECTURE.md](../ARCHITECTURE.md): the P5l row done with links.

## 6. Release check

Shared code changes (lamps, Session, UI, catalogue): the full debug suite at the end. Release: `bw run netlists/bw1.bwn --time 2s` idle and with `--press SW1:1ms:2s` (key `0` held), both ratios noted; `bw_ui` on the Board with every instrument (32 Clips as BW1_DESIGN's Session case, 4 Hooks, probe, meter) at 1.00× and UI under ~1 ms; screenshots viewed: reset (`0200 xx` lit), a key held (cap down), SST on (knob at ON), the self-test's `PA55` after a Bodge Wire. The 110 and stopwatch screenshots unchanged (lamps).

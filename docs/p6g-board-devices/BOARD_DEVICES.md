# P6g — Board devices

Roadmap step P6g ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the Board's buttons, slide switch, digits and capacitors in the Board's real look (P6b, [UX_DESIGN.md](../p6b-ux-design/UX_DESIGN.md)), and who the PC keys go to: the bench or the Machine. Design: [BOARD_DEVICES_DESIGN.md](BOARD_DEVICES_DESIGN.md). Plan: [BOARD_DEVICES_PLAN.md](BOARD_DEVICES_PLAN.md). Recipe: [ADD_BOARD_DEVICE.md](../recipes/ADD_BOARD_DEVICE.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

P6b left the Board's devices for later: the Stopwatch's buttons are small grey discs, the BW-1's slide switch is a speck at Board zoom. Since then the user found, on every Machine with devices (screenshots of 2026-11-18):

- The pushbuttons are all the same grey disc with nothing printed by them: on the Stopwatch nothing tells Start from Stop from Reset.
- The BW-80's two capacitors (`C9`, `C11`, 47 pF) are bare pads: the real look draws no `C`.
- A seven-segment digit is a flat black body with no case.
- On the BW-1, pin 2 of each key sits under its cap 4.6 mm from the cap's centre, by the legend: zoomed out a click there sometimes misses the key (measured 2026-11-18: a press is refused only on the 2.2 mm pad, whatever the zoom; the pin's hover reaches 12 px).
- The BW-1 types from a PC keyboard (P5i) only after F2, which nothing shows since the shelf replaced the toolbar: the shelf's keyboard drawing is grayed on the BW-1.
- Tennis's knobs turn from Q/A and ↑/↓ at all times, out or away, which works but does not scale, and nothing on the knobs says so (a tooltip only).
- A floating keyboard types only while its own window has ImGui's focus: a click on the TV stops the typing.

## User decisions (2026-11-18)

| Question | Decision |
|---|---|
| A button's label | printed on the silkscreen beside the button, from the netlist (`legend=`, as the BW-1's keys and slide switch); not on the cap (a tactile switch's cap is unprinted and too small) and not text placed in KiCad (no reader for it; the manuals could not quote it) |
| The slide switch | drawn so it reads at Board zoom |
| The digits | the black face in a mid-grey plastic rim with a slight bevel, as a real 0.56" digit from above |
| Capacitors | a ceramic disc with its value printed (`47`), for every `C` |
| The BW-1's key pin | not fixed: zoomed in all is fine, and zoomed out a click on the cap's lower right presses; a fix only if trivial, and the one found (pick a capped part's pins on the pad only) changes the hover, not the press, so it may not answer it |
| Knob keys | printed on the knob's case under its reference |
| Where the keys go | one model, in P6g: the keys belong to the bench or to the Machine. A click on any of the Machine's objects (TV, keyboard, knob, the BW-1's keys) gives them to the Machine: every one of those objects shows the gold frame together, the keyboard types whatever has the focus among them, the knobs turn from their keys, the BW-1's keys press as a normal keyboard's. A click on the Board, a tool or an instrument gives them back to the bench (the shelf's shortcuts) |
| Knobs | behave the same: they turn from their keys only while the Machine has the keys, with the gold frame then |
| The BW-1's typing | turned on by the shelf's keyboard drawing (no longer grayed there), with the gold frame on the Board's keypad while on |

Decided while designing (2026-11-18), for the user to overrule:

- **Legends**: Dice `ROLL`, Stopwatch `START`, `STOP`, `RESET`, Tennis `START`, Typewriter `CLEAR`, Heartbeat and LED Computer `RESET`; capitals, as silkscreen. A BUTTON's or SLIDE's `legend=` takes 1 to 8 characters (a KEY's stays 1 to 4: it fills a cap); a one-character legend gives a KEY its PC key, never a BUTTON. Printed under the button's body, or above its reference when under would leave the Board (Tennis's `SW1` sits on the bottom edge), decided in Board millimetres so it does not move with the zoom. The User Manuals name the buttons by their legends (`` `START` (`SW1`) ``).
- **The button**: a black 6 × 3.5 mm body (the footprint's), a round plunger in its middle, light grey up, darker and smaller held; two metal legs at the pads' ends.
- **The slide switch**: the body as today; in the real look `ON` and its legend printed on the silkscreen beside it at the references' size (today they are inside the red body, a few pixels high), the knob larger and ridged.
- **The capacitor**: an orange-brown disc seen from above, upright between its pads (the footprint's 3 × 1.6 mm), its value in the EIA code: two digits under 100 pF (`47`), else two digits and a multiplier (`101` for 100 pF, `473` for 47 nF).
- **Esc does not give the keys back**: Esc is a key of the Machine (the Typewriter's ESC, the BW-1's ST is `pc=Escape`). F2 toggles: from the bench to the Machine and back.
- **The start**: the keys start on the bench, on every Machine (today's keyboards start so).
- **What takes keys**: the keyboard and the knobs only while out (a knob put away no longer turns from Q/A); the BW-1's keys always (they are on the Board). The TV takes none but is the Machine's: a click on it gives the keys to the Machine when something takes them. A Machine with nothing that takes keys (the Counter, the Test Pattern) never has them, and putting away the last thing that takes them gives the keys back to the bench. F2 with the keyboard away brings it out, as today.
- **A Board button with no PC key** (Tennis's `START`, the Typewriter's `CLEAR`) is a click on the Board: it gives the keys back to the bench, so after pressing `START` the player clicks the knob box or the TV again before the knobs turn from their keys.
- **While the Machine has the keys** every key event goes to it, none to the shelf's shortcuts, `Tab`, `L`, `F`, `Esc` or the zoom keys, which do nothing until the keys are back on the bench; the modifier keys still reach the UI, so a Shift-click works. Mouse clicks work everywhere as always. F2 does nothing while a text field (the scope's delay, the analyzer's pattern) is being edited.
- **The gold frame** (`(255, 196, 40)`, the shelf's focus gold) on the TV's case, the keyboard's case (today's typing edge), the knob box's case, a frame around the BW-1's keypad on the Board, and the shelf's keyboard drawing; the shelf's hint `Keys to the Machine · F2 or a click on the Board gives them back`.
- **`--keys machine`** (`bw_ui`): the keys start on the Machine, for screenshots of the gold frames; a usage error on a Machine with nothing that takes keys.
- **A Machine with both a keyboard and knobs**: none exists. The intended rule, written in the design for the day one comes: the keyboard takes the letters, the knobs the keys it has no use for. Not built.

## What is new

1. **Keys** — `src/view/machine_keys.{hpp,cpp}` (ImGui-free, unit tested): where the keys go, what moves them.
2. **Routing** — `main.cpp` sends every key event to the Machine while it has the keys: the keyboard (either kind), the Board's keys, the knobs' held keys; the keyboard windows type from that state, not ImGui's window focus.
3. **Gold frames** — the TV, keyboard, knob box, the BW-1's keypad and the shelf's keyboard drawing.
4. **Knob keys** printed on the knob box: two small caps under the reference (`Q` `A`, `▲` `▼`).
5. **Looks** — the button, its legend, the slide switch, the digit's rim, the capacitor (`src/view/board_marks` for the value's code, unit tested).
6. **Netlists** — `legend=` on the seven BUTTONs; the catalogue takes it.
7. **Docs** — User Manuals (the legends, the BW-1's typing), the BUTTON and KEY datasheets, GLOSSARY (**Typing**), ADD_BOARD_DEVICE, ADD_CONNECTOR_DEVICE, ARCHITECTURE.

The kernel, Parts' behaviour, the builder, devices and Puzzles are untouched.

## Tests

- **Machine keys** (`tests/view/machine_keys_test.cpp`): starts on the bench; a front click with takers moves to the Machine, without takers stays; a bench click moves back; F2 toggles; losing the last taker gives the keys back.
- **Board marks** (`tests/view/board_marks_test.cpp`): 47 pF `47`, 10 pF `10`, 5 pF `5`, 100 pF `101`, 330 pF `331`, 4.7 nF `472`, 100 nF `104`, 1 µF `105`.
- **Switch** (`tests/parts/switch_test.cpp`): `BUTTON legend=RESET` accepted with no PC key, `BUTTON legend=A` no PC key, 9 characters refused, `KEY legend=RESET` still refused, `SLIDE legend=SINGLE` accepted.
- The Machines whose netlists change keep their tests (ctest `-R "dice|stopwatch|tennis|typewriter|heartbeat|led.computer|manuals"`).
- No UI tests exist: the gold frames, clicks and key routing are checked by reading the code, by screenshots and by the user.
- **Screenshots** (release, viewed): the Stopwatch (three labelled buttons, digits in their rims), the Dice, Tennis (`START` above its reference, the knob keys printed), the BW-1 (the slide switch readable, the keypad), the BW-80 (`C9`, `C11` as discs printed `47`), the Knob Meter (its knob keys); with `--keys machine`: the BW-1 (its keypad framed), Tennis and the Typewriter (gold on each object).
- **Release:** the 110 with every instrument (1.00×, UI about 1 ms) and the BW-2001 with its TV out, the ratio unchanged. The full release suite passes.

## Later

- P6h: the keyboard pulled out from under the TV.
- A Machine with both a keyboard and knobs: the split above.
- The BW-1's key pin by the legend, if it still bothers: a press refused only on the pad, the hover 12 px around it; picking a capped part's pins on the pad only is one answer.
- Text placed on the silkscreen in KiCad (a Board's title, warnings): a reader for `gr_text`, when a Board wants one.

# P6g — Board devices — Design

Spec: [BOARD_DEVICES.md](BOARD_DEVICES.md). Plan: [BOARD_DEVICES_PLAN.md](BOARD_DEVICES_PLAN.md). Recipe: [ADD_BOARD_DEVICE.md](../recipes/ADD_BOARD_DEVICE.md) (Input device: the UI bullet; Output device: the UI row). No device, Session, kernel or Part behaviour change: the catalogue's `legend=` check widens for BUTTON, the rest is drawing and key routing.

## Files

| File | Change |
|---|---|
| `src/view/machine_keys.{hpp,cpp}` (new), `tests/view/machine_keys_test.cpp` (new) | where the keys go (below) |
| `src/view/board_marks.{hpp,cpp}` (new), `tests/view/board_marks_test.cpp` (new) | a capacitor's printed code (below) |
| `src/view/CMakeLists.txt`, `tests/CMakeLists.txt` | the new files |
| `src/parts/catalogue.cpp`, `tests/parts/switch_test.cpp` | BUTTON takes `legend=` (below) |
| `machines/{030-dice,040-stopwatch,070-tennis,090-typewriter,100-heartbeat,110-led-computer}/*.bwn` | `legend=` on each BUTTON (spec) |
| `src/view/bench_places.hpp`, `tests/view/bench_places_test.cpp` | `kKnobBox` taller (the printed keys) |
| `src/ui/board_view.{hpp,cpp}` | the button, its legend, the slide switch, the digit's rim, the keypad's gold frame (new `setKeypadGold`), a press on the Board reported (new `BoardPress`, `takePress`) |
| `src/ui/board_look.{hpp,cpp}` | `Kind::Capacitor`, `DrawCapacitor`; `SilkTextPx`, `kSilk` exported |
| `src/ui/knob_window.{hpp,cpp}` | new `KnobKeys`; keys from it, printed keys (a font), gold frame, a click reported |
| `src/ui/tv_window.{hpp,cpp}` | gold frame, a click reported |
| `src/ui/keyboard_window.{hpp,cpp}`, `src/ui/matrix_keyboard_window.{hpp,cpp}` | `capture.on` from the keys' state, not ImGui's focus; their F2 blocks (`focusNext_`) and `stop` handling go; a click reported; the frame in the shared gold |
| `src/ui/keyboard_capture.{hpp,cpp}`, `src/ui/matrix_capture.{hpp,cpp}`, `src/ui/board_keys.{hpp,cpp}` | F2 no longer handled inside (the routing owns it); `stop` goes |
| `src/ui/bench.hpp` | `kFocusGold` shared (today a copy in `shelf.cpp` and `kTypingEdge` twice) |
| `src/ui/shelf.{hpp,cpp}` | the keyboard drawing: live with Board keys (new `boardKeys`), its click reported, gold while the Machine has the keys; new `takeBenchClick`; the hint on `keyboardTyping`, `typing` gone |
| `src/ui/main.cpp` | the routing, `--keys machine` (below) |
| `machines/*/manuals/user.md` | Dice, Stopwatch, Tennis, Typewriter, Heartbeat, LED Computer: the legends; BW-1: its typing |
| `datasheets/BUTTON.md`, `datasheets/KEY.md` | `legend=` on BUTTON; Typing as the Machine's keys, not F2 alone |
| `docs/GLOSSARY.md`, `docs/recipes/ADD_BOARD_DEVICE.md`, `docs/recipes/ADD_CONNECTOR_DEVICE.md`, `docs/ARCHITECTURE.md` | below |

## Where the keys go (`src/view/machine_keys.hpp`)

ImGui-free, tested like `focus`.

```cpp
// Who the PC keys go to (P6g): the bench (the shelf's shortcuts, the Board's keys L, F, Tab,
// zoom, Esc) or the Machine (its keyboard, its Board keys, its knobs). Not the Focus (what a
// click on a pin does) and not ImGui's window focus.
enum class KeysTo : std::uint8_t { Bench, Machine };

class MachineKeys {
public:
    KeysTo to() const { return to_; }
    bool machine() const { return to_ == KeysTo::Machine; }
    // Each frame, before anything else: something takes keys now (the keyboard out, the knob box
    // out, Board keys with a PC key). None: the keys go back to the bench.
    void setTakers(bool takers);
    void front();   // a click on the TV, keyboard, knob box, a Board key, the shelf's keyboard
    void bench();   // a click on the Board off its keys, a tool or instrument, their windows
    void toggle();  // F2; the shelf's keyboard drawing on a Machine whose keys are on the Board

private:
    bool takers_ = false;
    KeysTo to_ = KeysTo::Bench;
};
```

`setTakers(t)`: `takers_ = t`, and `Bench` when `!t`. `front()`: `Machine` when `takers_`, else unchanged. `bench()`: `Bench`. `toggle()`: `Bench` → `front()`, `Machine` → `Bench`. Test cases `machine keys: …` `[view]`: starts on the bench; `front` with takers → Machine, without → bench; `bench` back; `toggle` both ways; takers lost while on the Machine → bench; `front` after takers come back → Machine.

## Routing (`src/ui/main.cpp`)

`MachineKeys keys;` after the captures. `boardKeys.keys` is filled as today (only without a keyboard: P5i). A helper `applyKeys()` sets, from `keys` and what is out: `keys.setTakers(keyboardOut || matrixOut || !boardKeys.keys.empty() || (shelf.knobOut && hasKnobs))`, then `capture.on = keys.machine() && keyboardOut`, `matrixCapture.on = keys.machine() && matrixOut`, `boardKeys.on = keys.machine() && !boardKeys.keys.empty()`, `knobsOn = keys.machine() && shelf.knobOut`. It runs at the top of each frame before polling, and again right after every change to `keys` (F2 in the poll, the clicks after the windows), so no event is read with stale `on` flags.

**Events.** In the poll loop:

1. Every event goes to `capture.handle`, `matrixCapture.handle`, `boardKeys.handle` and `knobKeys.handle` as today (each acts on key and text events only when `on`; `boardKeys` and `knobKeys` clear their held keys on `SDL_EVENT_WINDOW_FOCUS_LOST` whatever).
2. `SDL_EVENT_KEY_DOWN` F2, not a repeat, while last frame's `io.WantTextInput` is false: `keys.toggle()`; when that gives the Machine the keys and a keyboard window exists and is away, it comes out (`shelf.keyboardOut = true`, as F2 does today: the BW-2001 and BW-80 manuals' "or press F2" stays true); `applyKeys()`; the event taken.
3. While `keys.machine()`: `KEY_DOWN`, `TEXT_INPUT` and `TEXT_EDITING` are taken from ImGui, except the modifier keys (`SDLK_LSHIFT`, `RSHIFT`, `LCTRL`, `RCTRL`, `LALT`, `RALT`, `LGUI`, `RGUI`), which ImGui still sees so a Shift-click (`BoardClick::shift`) keeps working. `KEY_UP` always reaches ImGui, as today.
4. Else as today.

`KnobKeys` (new, `knob_window.hpp`): `std::set<SDL_Scancode> held_`; `handle(event, bool on)` adds on `KEY_DOWN` while `on`, erases on `KEY_UP`, clears on focus lost; `bool down(SDL_Scancode) const`; `clear()` (called when `knobsOn` goes false).

**Clicks.** Each case window reports a click with `InstrumentClicked()` (`src/ui/instrument_marks.cpp`: a press over the window, `AllowWhenBlockedByActiveItem` so a press on a knob or cap counts, the close corner left out so putting a window away does not give the keys) called right after `Begin`, as the instruments do. After the windows drew:

- `keys.front()` when the TV, keyboard, matrix keyboard or knob window reports a click, the Board reports a press on a switch with a PC key (`boardView.takePress()`, below), or the shelf reports its keyboard drawing clicked (bringing a keyboard out, or on Board keys `toggle()`).
- `keys.bench()` when the Board reports any other press, an instrument window reports a click (where `focus.clicked` is called today), or the shelf reports a tool or instrument drawing clicked (`Shelf::takeBenchClick()`).
- The manuals binder, the brief and the shelf's controls (power, run, speed, layer, fit) leave the keys where they are.

Then `applyKeys()`. A Board button with no PC key (Tennis's START, the Typewriter's CLEAR) is a press on the Board: the keys go back to the bench, and the player clicks the knob box or the TV again to play (spec).

**The shelf's keyboard drawing.** With a keyboard window, as today (out or away). On a Machine with Board keys and no keyboard (`Shelf::boardKeys`, new), it is no longer grayed: a click toggles, and it is lit while the Machine has the keys. `Shelf::keyboardTyping` becomes `keys.machine()` (the frame on the drawing); `Shelf::typing` goes. The hint shows on `keyboardTyping`: `Keys to the Machine · F2 gives them back`.

**Knob keys.** `KnobWindow::keys(Session&, const KnobKeys&, bool on)`: returns at once unless `on`; the turns as today from `down(SDL_SCANCODE_Q)`, `…A`, `…UP`, `…DOWN` (the first two knobs only, as today; a third has none). The `WantTextInput` check goes (while `on` no key reaches ImGui).

**`--keys machine`**: `Options::keysToMachine`; after the first `applyKeys()`, `keys.front()`. In `main()`, beside the `--tv` check, a Machine with no keyboard, knob or Board PC key gives `bw_ui: nothing on this Machine takes keys` and usage, exit `kExitUsage`. Usage line: `[--keys machine]`.

**A Machine with a keyboard and knobs** (none today): `knobKeys` would get only the scancodes the keyboard has no use for. Not built; a comment at the routing says so.

## Gold frames

`kFocusGold` (`IM_COL32(255, 196, 40, 255)`) moves to `bench.hpp`; `shelf.cpp`, `keyboard_window.cpp`, `matrix_keyboard_window.cpp` use it (`kTypingEdge`, `(255, 176, 40)`, goes). While `keys.machine()`:

- TV: `TvWindow::draw(bool& open, TvView& view, bool gold)` → `bool` clicked: a 2 px rect inside the case's edge, rounding 6.
- Keyboards: today's `if (capture.on)` frame, in `kFocusGold`.
- Knob box: `KnobWindow::draw(bool& open, Session&, bool gold)` → `bool` clicked: a 2 px rect inside the window's edge, rounding 6.
- The BW-1's keypad: `BoardView::setKeypadGold(bool)`: the box around every courtyard of a switch drawn `SwitchLook::Key`, grown 1.5 mm, a 2 px rounded rect in `kFocusGold`, drawn after the switches, both looks.
- The shelf's keyboard drawing: today's frame.

## The Board (`src/ui/board_view.cpp`)

**Presses.** `BoardView::takePress()` → `std::optional<BoardPress>`, `struct BoardPress { std::optional<std::size_t> component; }` (new): set at `ImGui::IsMouseClicked(Left)` over the panel (where `held_` is set today), with the switch body under the cursor (`buttonAt()`) or none. `main.cpp`: a press on a switch whose reference has a PC key (`session.switchKeys()`) → `keys.front()`; any other → `keys.bench()`.

**The button** (`SwitchLook::Button`, real look). The footprint `TACT-6x3.5` (`SW_PUSH_1P1T_6x3.5mm_H5.0_APEM_MJTP1250`): pads 1 and 2 6.5 mm apart, the body 6 × 3.5 mm between them (F.Fab 0.25..6.25 × ±1.75); its courtyard (-1.38..7.88 × ±2) holds the pads too, so the drawing comes from the pad centres, not the courtyard, and follows any rotation:

| | |
|---|---|
| body | along the pads' line from 0.25 mm past pad 1's centre to 0.25 mm short of pad 2's, 3.5 mm across, `(22, 22, 24)`, rounding 0.3 mm; a 1 px lighter edge `(60, 60, 64)` on its upper long side |
| legs | at each pad, a `(190, 190, 195)` rectangle 1.2 mm along the line × 0.8 mm across, from the pad's centre to the body's end |
| plunger | a disc at the midpoint, radius 1.1 mm, `(185, 185, 190)` with a `(235, 235, 240)` highlight up-left; held, radius 0.95 mm, `(110, 110, 115)`, no highlight |

The plain look keeps today's drawing (grey disc on a dark box), so `kButtonBody`, `kButtonCap`, `kButtonCapDown`, `kButtonRim`, `kButtonCapShare`, `kButtonCapDownScale` stay, for it; the real look's colours are new constants beside them.

**The legend** (BUTTON and SLIDE, real look only; KEY prints on its cap as today, the plain look prints none): `kSilk` and `SilkTextPx(map)` (`board_look.hpp`, exported: `map.pixels(1'400'000)` clamped to 22 px, as `DrawUseReferences`), none under 6 px; centred under the body, 0.4 mm below the courtyard's bottom. Where it goes is decided in board nm with the unclamped 1.4 mm text height, so it does not flip with zoom: when the box 0.4 mm + 1.4 mm under the courtyard crosses the Board's bounds (`scene_->bounds()`), the legend goes above the reference (one 1.4 mm line above the courtyard's top, where `DrawUseReferences` writes the reference: the legend one line higher). Tennis's `SW1` (courtyard bottom 167.1 mm, edge 168.31 mm) goes above.

**The slide switch** (`DrawSlide`): the body and slot as today in both looks; the knob 0.55 of the slot's width (was 0.45) with three dark ridge lines across it. In the real look `ON` and the legend leave the red body: `ON` in silkscreen right of the body, centred on the slot, the legend under the body as a button's. The plain look keeps them inside the body as today (`kSlideText` stays for it).

**The digit's rim** (both looks): the courtyard polygon filled `kDigitRim` `(105, 105, 110)`; the face, the courtyard's box inset by 6 % of its smaller side, `kDigitBody` as today, with a 1 px `(150, 150, 155)` line along its top and left and `(60, 60, 64)` along its bottom and right (the bevel). The segments are drawn on the face as today (`DigitFrameOf` unchanged: its margins keep the segments inside the face).

## The capacitor (`src/ui/board_look.cpp`, `src/view/board_marks.hpp`)

```cpp
// The code printed on a ceramic capacitor: picofarads under 100 as they are ("47", "10"), else
// two significant digits and a power of ten ("101" 100 pF, "473" 47 nF, "105" 1 µF).
std::string CapacitorCode(std::uint64_t picofarads);
```

Whole picofarads, as `ParseFarads` gives them (`src/netlist/units.hpp`: "4p7" → 5), so no fractional case. `KindOf`: `part == "C"` → `Kind::Capacitor`. `SilkBox`: the pads' box grown 1.0 mm. `DrawCapacitor`: a capsule (a line 1.6 mm wide with round ends) whose centre line runs from 0.55 mm inside one pad's centre to 0.55 mm inside the other's, along the pads' line (on the 2.5 mm pitch of `C_Disc_D3.0mm_W1.6mm_P2.50mm` it spans −0.25..2.75 mm: the footprint's 3 × 1.6 mm body), `(196, 110, 40)` with a 1 px darker edge `(140, 72, 25)` along its sides; the code centred on it in `(40, 25, 10)`, the text's height 0.9 of the body's width, not drawn under 5 px. The value: `ParseFarads(scene.value(k))` (bw_ui links bw_netlist through bw_machine), nothing printed when it does not parse.

## The knob box's keys (`src/ui/knob_window.cpp`)

`KnobWindow(ImFont* legends)` (new constructor, as `KeyboardWindow`'s; `main.cpp` passes the legends font). Under each of the first two knobs' references, two small caps 18 × 16 px at scale 1 (× `s`), 4 px apart, centred: `DrawKeyCap` (`src/ui/keycap.hpp`) with legend `Q` and `A` for the first of two knobs; for the other (or a single knob) the legends empty and a small filled triangle drawn on each cap, up and down, `(220, 220, 220)`. A third knob gets no caps. The reference moves up by the caps' height and `kKnobBox`'s height grows by it (`src/view/bench_places.hpp`; the `bench places` tests follow). The tooltip keeps its text, plus `· while the Machine has the keys`.

## Catalogue (`src/parts/catalogue.cpp`)

`CreateSwitch`: BUTTON takes `legend=` and `pc=`; KEY `legend=` and `pc=`; SLIDE `legend=`. The legend's length: 1 to 4 characters on a KEY (it fills a cap), 1 to 8 on a BUTTON or SLIDE (printed on the silkscreen). The default `pc=` from a one-character legend stays a KEY's only (`key && !pc && …`): `BUTTON legend=A` has no PC key. The errors: `BUTTON takes legend= and pc=, not x=`; `BUTTON legend= takes 1 to 8 printable characters, not '…'` (KEY keeps `1 to 4`). `tests/parts/switch_test.cpp`: the error texts at its lines 91 and 109 follow; new checks: `BUTTON legend=RESET` accepted, its `pcKey()` empty; `BUTTON legend=A` no PC key; a 9-character BUTTON legend refused; `KEY legend=RESET` refused; `SLIDE legend=SINGLE` now accepted (6), 9 refused.

## Netlists and manuals

The seven BUTTON lines get `legend=` (spec), the comments naming them kept. User Manuals keep references in backticks: the Stopwatch `` Press `START` (`SW1`) to start and `STOP` (`SW2`) to stop; `RESET` (`SW3`) resets the count to 0.00 `` and its last sentence `` press `STOP`, then `RESET` ``; the Dice `` Hold the `ROLL` button (`SW1`) ``; Tennis `` `START` (`SW1`) ``; the Typewriter, Heartbeat and LED Computer where they name the button. The BW-1's line 16: `On a PC keyboard, after a click on the keypad or the shelf's keyboard (or F2): …`. The BW-2001 and BW-80 lines (`click the keyboard's window (or press F2)`) stay true. `tests/machines/manuals_test.cpp` runs in the fence.

## Datasheets, recipes and glossary

- `datasheets/BUTTON.md`: `legend=` (1–8, printed beside it); PC keys through the Machine's keys. `datasheets/KEY.md`: "with Typing on (F2)" → while the Machine has the keys (a click on the keypad, the shelf's keyboard or F2).
- GLOSSARY **Typing**: the keys go to the bench or to the Machine (P6g): a click on the TV, keyboard, knob or a Board key gives them to the Machine, gold on each of its objects; a click on the Board, a tool or an instrument gives them back; F2 toggles; while the Machine has them every key goes to its keyboard, Board keys and knobs, none to the shelf's shortcuts.
- ADD_BOARD_DEVICE, Input device: BUTTON takes `legend=` (1–8, printed on the silkscreen under it); the UI bullet: the button's look, PC keys through the Machine's keys (`src/view/machine_keys`), not F2 alone. Output device, UI row: the digit's rim.
- ADD_CONNECTOR_DEVICE (its line on the capture mode, "on while its window is focused … F2 toggles"): the keyboard types while the Machine has the keys (`src/view/machine_keys`); a new device that takes keys adds itself to the takers in `main.cpp`'s `applyKeys`.
- ARCHITECTURE: the P6g row done, with the date and links.

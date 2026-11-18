# P6g — Board devices — Implementation Plan

Spec: [BOARD_DEVICES.md](BOARD_DEVICES.md). Design: [BOARD_DEVICES_DESIGN.md](BOARD_DEVICES_DESIGN.md).

## Phase 1 — Keys and marks, ImGui-free

- `src/view/machine_keys.{hpp,cpp}`, `tests/view/machine_keys_test.cpp` (design: where the keys go; spec: Tests).
- `src/view/board_marks.{hpp,cpp}`, `tests/view/board_marks_test.cpp` (design: the capacitor).
- `src/view/CMakeLists.txt`, `tests/CMakeLists.txt`.
- `src/parts/catalogue.cpp`, `tests/parts/switch_test.cpp`: BUTTON takes `legend=` (design: catalogue).

Fence: release build; `ctest --preset release -R "machine keys|board marks|switch"`; debug build, the same `-R`. Commit: `view, parts: where the keys go (bench or Machine), a capacitor's printed code; BUTTON takes legend=`.

## Phase 2 — The looks

- `machines/*/*.bwn`: the seven BUTTON legends; the six User Manuals (design: netlists and manuals; the BW-1's line waits for phase 3).
- `src/ui/board_look.{hpp,cpp}`: `Kind::Capacitor`, `DrawCapacitor`, `SilkTextPx` and `kSilk` exported.
- `src/ui/board_view.cpp`: the button, its legend, the slide switch, the digit's rim (design: the Board).
- `src/ui/knob_window.{hpp,cpp}` (the legends font), `src/ui/main.cpp` (passing it), `src/view/bench_places.hpp`, `tests/view/bench_places_test.cpp`: the printed keys, the box taller.
- `datasheets/BUTTON.md`: `legend=`.
- Screenshots viewed (spec: Tests): Stopwatch, Dice, Tennis, BW-1, BW-80, Knob Meter.

Fence: release build; `ctest --preset release -R "dice|stopwatch|tennis|typewriter|heartbeat|led.computer|manuals|bench places|switch"`; the screenshots. Commit: `ui: buttons as tactile switches with their legends on the silkscreen, a readable slide switch, digits in a grey rim, ceramic capacitors with their code, the knob keys printed`.

## Phase 3 — The keys go to the bench or the Machine

- `src/ui/bench.hpp`: `kFocusGold`.
- `src/ui/main.cpp`: `MachineKeys`, the event routing, F2, the clicks (design: routing).
- `src/ui/keyboard_capture.*`, `matrix_capture.*`, `board_keys.*`: F2 out; `keyboard_window.*`, `matrix_keyboard_window.*`: `on` from the routing, a click reported, the gold.
- `src/ui/knob_window.*`: `KnobKeys`, keys while on, gold, a click reported. `src/ui/tv_window.*`: gold, a click reported.
- `src/ui/board_view.*`: `takePress`, `setKeypadGold`.
- `src/ui/shelf.*`: the keyboard drawing with Board keys, `takeBenchClick`, the hint.
- `machines/120-bw1/manuals/user.md`: its typing line; `datasheets/BUTTON.md`, `datasheets/KEY.md`: Typing.
- Screenshots viewed: the BW-1 with its keys given to the Machine (`--keys machine`, design: routing), Tennis and the Typewriter the same; the speed gate (the 110 with every instrument, the BW-2001 with its TV out).

Fence: full release suite; debug build and `ctest --preset debug -R "machine keys|switch|manuals"`; the screenshots; the speed gate. Commit: `ui: the keys go to the bench or the Machine: a click on the TV, keyboard, knob or a Board key gives them, gold on each; the BW-1 types from the shelf's keyboard; knobs turn only then`.

## Phase 4 — Docs

- GLOSSARY, ADD_BOARD_DEVICE, ADD_CONNECTOR_DEVICE, ARCHITECTURE (design: datasheets, recipes and glossary); this plan cut down to the status table and notes.

Fence: links checked by reading. Commit: `docs: P6g done, Board devices`.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Machine keys, capacitor code, BUTTON legend | 2e2cd5b |
| 2 | The looks | d3b3681 |
| 3 | Keys to the bench or the Machine | |
| 4 | Glossary, recipe, ARCHITECTURE | |

## Implementation notes

- Docs review (before the docs commit), fixed: what takes keys follows what is out (`setTakers` each frame; F2 brings an away keyboard out, as today); every event still reaches the captures (focus lost clears held keys); the `on` flags set again right after each change of the keys; F2 ignored while a text field is edited; the modifier keys reach ImGui (Shift-click); clicks from `InstrumentClicked()` (a press on a knob counts, the close corner not); the hint on `keyboardTyping`; the button drawn from its pad centres (its courtyard holds the pads); the legend's place decided in Board millimetres; the capacitor's capsule 3 mm; `CapacitorCode` on whole picofarads; the knob box given the legends font; a BUTTON's one-character legend gives no PC key, SLIDE legends up to 8; the plain look keeps today's button; `led.computer` in the fences; the BUTTON and KEY datasheets and ADD_CONNECTOR_DEVICE listed; manual references in backticks.
- Phase 1: release and debug `-R "machine keys|board marks|switch"`. `CapacitorCode` rounds to two significant digits (999 pF → `102`).
- Phase 2: a switch's legend and `ON` keep a 6 px floor where the references drop out (the BW-1 at fit zoom draws its silkscreen under 6 px: `SST` and `ON` vanished on the first screenshot); not printed when the switch is under 16 px on screen. The knob box's keys are drawn as two small caps of its own (`DrawKeyCap`'s legends scale with the cap: unreadable at 16 px), so `KnobWindow` needs no font. The digit's bevel lit top and left, as designed. Screenshots viewed: Stopwatch (START, STOP, RESET; rims), Dice (ROLL), Tennis (START above SW1 at the Board's edge; the knob keys Q A and ▲ ▼), Knob Meter (▲ ▼), BW-1 (SST and ON readable), BW-80 (C9, C11 printed 47). `-R "dice|stopwatch|tennis|typewriter|heartbeat|led.computer|manuals|bench places|switch"`: 141 passed.
- Phase 3: the hint shortened to `Keys to the Machine · F2 gives them back` (the longer one was cut at the shelf's end). `KnobKeys` reads SDL scancodes, so the knobs follow the physical keys. A modifier key reaches ImGui even when a capture takes it (Shift-click). Screenshots viewed with `--keys machine`: the BW-1 (the keypad framed, the shelf's keyboard lit and framed), Tennis (knob box and TV framed; the shelf's keyboard stays grayed: no keyboard), the Typewriter (TV, keyboard, shelf); the Test Pattern refuses `--keys machine`. Clicks (a press on the TV, knob box, keyboard or a Board key giving the keys; the Board, a tool or an instrument taking them back; F2 both ways; typing) are checked by reading the code only: the agent cannot drive the mouse or keyboard. User check.
- Phase 3 checks: full release suite 1436 passed (83 s); debug `-R "machine keys|switch|manuals"` 38 passed. Speed gate: the 110 with every instrument 1.00×, UI 0.4 ms; the BW-2001 with its TV out 0.36×, UI 3.0 ms.

# P6g — Board devices — Implementation Plan

Spec: [BOARD_DEVICES.md](BOARD_DEVICES.md). Design: [BOARD_DEVICES_DESIGN.md](BOARD_DEVICES_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Machine keys, capacitor code, BUTTON legend | 2e2cd5b |
| 2 | The looks | d3b3681 |
| 3 | Keys to the bench or the Machine | 1725d07 |
| 4 | Glossary, recipes, ARCHITECTURE | this commit |

P6g implemented 2026-11-18, user check pending.

## Implementation notes

- Docs review (before the docs commit), fixed: what takes keys follows what is out (`setTakers` each frame; F2 brings an away keyboard out, as today); every event still reaches the captures (focus lost clears held keys); the `on` flags set again right after each change of the keys; F2 ignored while a text field is edited; the modifier keys reach ImGui (Shift-click); clicks from `InstrumentClicked()` (a press on a knob counts, the close corner not); the hint on `keyboardTyping`; the button drawn from its pad centres (its courtyard holds the pads); the legend's place decided in Board millimetres; the capacitor's capsule 3 mm; `CapacitorCode` on whole picofarads; the knob box given the legends font; a BUTTON's one-character legend gives no PC key, SLIDE legends up to 8; the plain look keeps today's button; `led.computer` in the fences; the BUTTON and KEY datasheets and ADD_CONNECTOR_DEVICE listed; manual references in backticks.
- Phase 1: release and debug `-R "machine keys|board marks|switch"`. `CapacitorCode` rounds to two significant digits (999 pF → `102`).
- Phase 2: a switch's legend and `ON` keep a 6 px floor where the references drop out (the BW-1 at fit zoom draws its silkscreen under 6 px: `SST` and `ON` vanished on the first screenshot); not printed when the switch is under 16 px on screen. The knob box's keys are drawn as two small caps of its own (`DrawKeyCap`'s legends scale with the cap: unreadable at 16 px), so `KnobWindow` needs no font. The digit's bevel lit top and left, as designed. Screenshots viewed: Stopwatch (START, STOP, RESET; rims), Dice (ROLL), Tennis (START above SW1 at the Board's edge; the knob keys Q A and ▲ ▼), Knob Meter (▲ ▼), BW-1 (SST and ON readable), BW-80 (C9, C11 printed 47). `-R "dice|stopwatch|tennis|typewriter|heartbeat|led.computer|manuals|bench places|switch"`: 141 passed.
- Phase 3: the hint shortened to `Keys to the Machine · F2 gives them back` (the longer one was cut at the shelf's end). `KnobKeys` reads SDL scancodes, so the knobs follow the physical keys. A modifier key reaches ImGui even when a capture takes it (Shift-click). Screenshots viewed with `--keys machine`: the BW-1 (the keypad framed, the shelf's keyboard lit and framed), Tennis (knob box and TV framed; the shelf's keyboard stays grayed: no keyboard), the Typewriter (TV, keyboard, shelf); the Test Pattern refuses `--keys machine`. Clicks (a press on the TV, knob box, keyboard or a Board key giving the keys; the Board, a tool or an instrument taking them back; F2 both ways; typing) are checked by reading the code only: the agent cannot drive the mouse or keyboard. User check.
- Phase 3 checks: full release suite 1436 passed (83 s); debug `-R "machine keys|switch|manuals"` 38 passed. Speed gate: the 110 with every instrument 1.00×, UI 0.4 ms; the BW-2001 with its TV out 0.36×, UI 3.0 ms.

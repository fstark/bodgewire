# P5i Switches on the Board — Implementation Plan

Design: [SWITCHES_DESIGN.md](SWITCHES_DESIGN.md)
Spec: [SWITCHES.md](SWITCHES.md)

Implemented 2026-11-14; user check pending.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Kernel: switched Pin, external drives, waking | `bce6852` |
| 2 | `Switch`, `SwitchNetwork`, `BUTTON` and `KEY`, builder, electrical, session | `7248627` |
| 3 | 74145 and the switch bench | `e1516b6` |
| 4 | `fault switch`: syntax, Machine, swap, dice | `8653f38` |
| 5 | `bw_ui`: keycaps on the Board, PC keys through Typing | `ce5d88b` |
| 6 | Release check, recipes, glossary, roadmap | this commit |

## Notes

- Review (before code), fixed in the docs: a press is read by the switch at its own wake, which then wakes the network (`EvalContext::wake`). Every existing `wake(button->id())` keeps working, presses stay deterministic on a scanned matrix, and no relaxed-atomics race can lose one. The switch legs stay never-driving TTL Outputs, one connected as `wireButtons` did, so every seed draws the same random bits: the 110's Blackout brief still holds (its test passed unchanged). The 74145 outputs are `Y0_N`–`Y9_N` (NAMING). The Swap tool swaps a switch instead of pressing it. Full suites at phases 4 and 5.
- A press reaches the Nets `kPressLatency` = 2 ps after the switch's wake (the P4d button: 1 ps).
- The `SwitchNetwork` cases are in `tests/parts/switch_test.cpp` (with the Part), the kernel's in `tests/sim/switched_test.cpp` with a test-only joining Component.
- Keycaps: a key's reference is drawn small in the cap's top-left corner, not centred over the legend (seen in the first screenshot). The MX pads show on the cap: left for the P5k look pass.
- Speed (release, headless, median of 3), noisy machine: `bus_load` 0.392× in the morning, 0.363× just before the kernel change, 0.374× after it, 0.378× at the end; 070 tennis 1.974×, 1.726×, 1.86×, 1.93×; 110 5.83×, 5.03×, 5.22×, 5.52×. Within the drift.
- `bw_ui`: 070 with 32 Clips, 4 Hooks, probe and meter, Start pressed: 1.00×, UI 1.3 ms, the same as the commit before P5i built side by side (1.3–1.4 ms). It is above the recipe's ~1 ms guide, and P5i did not move it. 110: 1.00×, UI 0.9 ms.
- Release check screenshots, viewed: the 090 keyboard window byte-identical to before the keycap move; the 030 dice with `SW1` held (cap down, the LEDs blurred by rolling); a scratch keypad Machine in the scratchpad (eight keys with legends `0`, `GO`, `AD`, `DATA`, `+`, `F`, `PC`, a button; `GO` and the button held). The scratch Board needs its `POWER` connector in a `row`: on a Board with no chips an `edge` rule has no outline to stand on.
- Not checkable headless, for the user: PC keys through F2 on Board keys (several held at once), the Swap tool on a switch body, left-click hold and right-click latch on a key.
- Full debug suite: 939 (phase 1), 946 (phase 2), 962 (phases 4 and 5); phase 6 changes docs only.

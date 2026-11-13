# P5d — ROM programs + first computer — Implementation Plan

Spec: [ROM_PROGRAMS.md](ROM_PROGRAMS.md). Design: [ROM_PROGRAMS_DESIGN.md](ROM_PROGRAMS_DESIGN.md). Machine: [LED_COMPUTER.md](../machines/110-led-computer/LED_COMPUTER.md), [LED_COMPUTER_DESIGN.md](../machines/110-led-computer/LED_COMPUTER_DESIGN.md).

Done 2026-11-13, user check pending.

Every phase ended with its fence: `cmake --build --preset debug` clean, `clang-format` on the C++ files touched, the tests the phase names green in debug (the full suite at phases 2 and 6), one commit. Only this step's own paths were committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Decode and assembler | `837dcee` |
| 2 | 74LS138, 74LS273, POR | `ba95150` |
| 3 | Program, netlist, manuals, fault-free test | `2e028aa` |
| 4 | Board (user routes) | `4c34956` |
| 5 | Session test, release check | this commit |
| 6 | Recipes, docs, full suite | this commit |

## Implementation notes

- Docs review (before the docs commit), fixed: `DurationFromText`, not `ParseDuration` (`bw_netlist` is above `bw_parts`); the phase 4 fence named no board-scene test (`view:`); `PartPinNames` also builds every Part and needs `time=` for `POR`; the bounce test reads `L0`–`L7` after each `LATCH_CLK` rise and only after the reset (the 74LS02's random power-on output can make one early rise); the POR's release at exactly `time`; the VCD window 50 µs; the service manual one bullet a reference with `J1`; the phase 1 fence takes Heartbeat; debug cost measured in phase 3; `asm` added to NAMING; the `.rom` header names the filename; `~` in the spec; `LATCH_CLK` 80 ns after `PHI0` falls; the 2114's data 120 ns after `CS_N`; POR's NC pins TTL; the Session check on `D3` at 300 ms.
- Written before the docs commit (drafted while the review ran): phase 1 code (decode, assembler, `bw asm`) and phase 2 Parts. POR simplified from a wake to one drive delayed by `time`.
- Phase 1: a label named `a` reads as the accumulator (`bne a`), as in ca65; the tests use other names. Fence 36 green.
- Phase 2: full debug suite 869 green in 544 s. The POR's release lands exactly at `time` (one drive with the hold as its delay).
- Phase 3: the program assembled first time as designed (`$F800`–`$F83D`, vectors at `$FFFA`). Release headless `bw run --time 2s`: **3.82×**, 11.5 M applied events a simulated second. VCD: `RES_N` rises at 20 ms, the first `LATCH_CLK` rise at 20.029 58 ms (80 ns after `PHI0` falls), `L0` at +17 ns; a 74LS02 power-on glitch on `LATCH_CLK` in the first 40 ns, as the review foresaw (the latch is held clear then). Steps measured 124.780 ms apart (20.030, 144.810, 269.590, 394.370 ms). Debug: the bounce case 27 s, the others under 6 s. The service manual first had addresses in backticks (`F800`), read as unknown references: written `$F800` in plain text.
- Phase 4: `bw place` gave 3.59 m on a 6 × 3 grid, the LED resistors in the two front rows of the grid. The user routed it and, before routing, moved the chip block as a whole, stood `R1`–`R8` upright (−90°) behind their LEDs, moved `R10`, `R11` and turned `SW1` 180°; so the committed Board is not what `led_computer.place` gives (`place_drc_led_computer` checks the rules' own Board). `check-board` ok; KiCad DRC 0 violations, 0 unconnected. The Machine tests now run on the routed Board; fence 58 green.
- Phase 5: `session: led computer with every instrument` green (4 s debug): 8 Clips on the data bus, Hooks on `LATCH_CLK` and `PHI0`, the probe on `ROM_N`, the meter on `D1`; `D3` lit at 300 ms, the others dark. Release `bw_ui netlists/led_computer.bwn` with the same instruments: 1.00×, UI 0.7 ms; screenshots viewed (the meter 0.20 V on the dark `D1` at 1 s; at 1.3 s `D5` lit and `D6` fading, the eleventh write).
- Phase 6: full debug suite 879 green in 493 s. Recipes (ADD_MACHINE: programs, `$` addresses in manuals, a `POR` on processor Machines; ADD_PART: a Part acting by itself, a required parameter and `PartPinNames`), ARCHITECTURE row, GAME_DESIGN ladder row 110.
- User check pending: `bw_ui netlists/led_computer.bwn` (the light, Reset with `SW1`, the instruments on the bus and strobes); `bw asm` on a program of your own.

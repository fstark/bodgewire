# P5g Memory Faults — Implementation Plan

Design: [MEMORY_FAULTS_DESIGN.md](MEMORY_FAULTS_DESIGN.md)
Spec: [MEMORY_FAULTS.md](MEMORY_FAULTS.md)

Implemented 2026-11-14; user check pending.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | `MemoryCells`: stuck bits in the 2114 and the ROMs | `5b11fde` |
| 2 | `fault cell`, `fault column`: syntax, builder, Machine, swap | `829d321` |
| 3 | Machine tests: 110 LED computer, 080 message display | `3cfad7b` |
| 4 | Release check, recipes, glossary, game design, roadmap | this commit |

## Notes

- Review (before code): the first design had the wrong symptom for `fault stuck U3.A0 0` on the 110. `sta dir` clears `light`, and `JSR`'s two bytes share `U3` word `$1FE`, so `RTS` lands in erased ROM, which runs to `$F800`: dark LEDs, not a wrong bounce. The stack-cell symptom is also exact, not "a crash": the open bus reads `$FF` (TTL inputs of `U7`), the 6502 runs it to `$E000`, and `D1` lights again and again. Both are now in the tests for seeds 1–3.
- Only `A0` of the 2114 shows with the 110's program: the other address lines alias words it never uses (noted for P5h).
- Speed: `bus_load` A/B with and without phase 1, in the same session: 0.382× before, 0.387× after (noise). The 110 is 5.7–5.9× headless, and `bw_ui` with the message stuck cell runs at 1.00×, UI 0.6 ms.
- Release check: the message display with `fault cell U15.O3 $29 0`. Every `E`'s top bar is missing its middle dot (TV frame zoomed and viewed). The check Puzzle file was deleted.
- The address-line test runs a step longer (`kPowerOnReset + 3 * kStep`): each restart through erased ROM adds a few ms to the step.
- Full debug suite at the phase 1, phase 2 and final fences (shared Parts and builder).

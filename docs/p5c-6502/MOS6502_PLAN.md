# P5c — 6502 Part — Implementation Plan

Spec: [MOS6502.md](MOS6502.md). Design: [MOS6502_DESIGN.md](MOS6502_DESIGN.md). Machine: [HEARTBEAT.md](../machines/100-heartbeat/HEARTBEAT.md), [HEARTBEAT_DESIGN.md](../machines/100-heartbeat/HEARTBEAT_DESIGN.md).

Done 2026-11-13, user check pending.

Every phase ended with its fence: `cmake --build --preset debug` clean, `clang-format` on the C++ files touched, the tests the phase names green in debug (the full suite at phases 3 and 6), one commit. Only this step's own paths were committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Test data | `d7aec85` |
| 2 | Instruction core | `9517870` |
| 3 | The 6502 Part | `0d5a860` |
| 4 | Heartbeat netlist, manuals, fault-free test | `6344de2` |
| 5 | Board | `8bbee8d` |
| 6 | Session, release check, docs | this commit |

## Implementation notes

- Docs review (before phase 1), fixed: `GND2` refused by the builder's power-pin check and missing from `addImplicitPower` (now `PowerPinRail`, phase 3, full-suite fence); LED resistors overlapping behind a row of LEDs (now in the grid) and `SW1` against the row (row from the left); `LA15`'s high time 65.536 ms; the reset fetch at the 6th fall; the subset written before each JSON is deleted; the taken-branch poll latched at the fetch; `NC1`–`NC3` MOS; how "wakes only on PHI0" is measured; the full-Harte debug time; Dormann's success line is a macro expansion; watched Nets 43 (one slot per Net); data Nets `DB0`–`DB7`; test names `"6502: …"`.
- Written before the docs commit (the core was drafted while the review ran): the subset and full set fetched (Harte `2f6980a`, Dormann `7954e2d`, success `$3469`; `external/6502/` 418 MB, the subset 4.8 MB); the core passes all 25,600 subset cases (17 s debug) and Dormann (3.7 s debug) at the first run; a mutation (ANE's constant) is caught. Phases 2 and 3 of the first plan (documented, then undocumented opcodes) became one: the table was written whole.
- Phase 2: every core case green; full Harte (2.56 M cases) green in 86 s debug with the runner reusing one array (17 s → 0.9 s for the subset), so it stays a registered ctest, not hidden; Dormann traps at `$3469` (success) in 3.7 s debug. Mutations caught: ANE's constant (subset), the taken-branch poll (IRQ case). Harte's PLP and RTI: B cleared and bit 5 set in the pulled P.
- Phase 3: first Part test run caught three test mistakes (an address sampled before the stalled cycle drove it, an X tie the Circuit refuses, power-on evaluations counted); the Part itself unchanged. Full debug suite: 838 tests green in 557 s.
- Phase 4: VCD over 12 µs as designed (`A0` every 2 µs at `PHI0`↓ + 130 ns, `LA0` 12 ns after through `U2`). Release headless `bw run --time 2s`: 7.08×, 9 M applied events a simulated second. The LED case first counted `LA15`'s settling from the 74LS244's random power-on output (20 ns) as an edge; it now skips the first µs. Debug: the four Heartbeat cases 2.3 s together.
- Phase 5: `bw place` (4.22 m) scattered the LED resistors over the chip grid; the user moved and rotated parts by hand before routing (LED resistors in a row behind their LEDs, the data-bus resistors together, `U1` turned), so the committed Board is no longer what `heartbeat.place` gives (`place_drc_heartbeat` still checks the rules' own Board). `check-board` ok, KiCad DRC 0 violations, 0 unconnected. Keeping a hand arrangement across a regeneration: logged in ARCHITECTURE, Board authoring.
- Phase 6: `session: heartbeat with every instrument` green (5 s debug). Release `bw_ui netlists/heartbeat.bwn` with the probe on `U1` `RW`, 8 Clips on `A0`–`A7`, Hooks on `PHI0` and `SYNC`, the meter on `D16` `A`: 1.00×, UI 0.7 ms; screenshot viewed (scope: `PHI0` at 1 MHz, `SYNC` every other cycle; meter 1.69 V on the half-lit `D16`). Seen, not fixed (shared UI code, older than P5c): the logic probe shows no level on a pin with no copper (`U1` `RW` and `SYNC` here, 080 `U1` `QA` too), while a scope Hook on the same pin works; noted in ADD_MACHINE.
- User check pending: `bw_ui netlists/heartbeat.bwn` (the LEDs, Reset with `SW1`, the instruments on the bus); `tests/data/6502/fetch.sh` on your machine if you want the full suites in your runs (about 420 MB in `external/6502/`, 86 s more in a debug suite).

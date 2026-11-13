# P5e — Instruments for buses — Implementation Plan

Spec: [BUS_INSTRUMENTS.md](BUS_INSTRUMENTS.md). Design: [BUS_INSTRUMENTS_DESIGN.md](BUS_INSTRUMENTS_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Probe on an unconnected pin | d0d01b0 |
| 2 | Analyzer engine: 32 × 64k, mask Triggers, bus patterns, stems; headless 110 | 168402f |
| 3 | Scope engine: 4 Hooks, delay; watch budget 96; Dispatcher heap merge | ceaf4a0 |
| 4 | Session: Channel Groups | 7c45e1b |
| 5 | UI: Board clicks and refusals, analyzer window, scope window, CLI | ddd0f7a |
| 6 | Release check, recipes, docs, full suite | this commit |

P5e implemented 2026-11-13, user check pending.

## Implementation notes

- Docs review (before the docs commit), fixed: the 110 latch case read the data at the cycle's last point, where the bus floats (now the last `PHI2`-high point) and the `FFFC` case needs `RW` = 1; existing tests broken by the new sizes listed (`session: clips`, the two 1 MHz Session Captures, `session: hooks`, the consumer tests); the Dispatcher's per-Sample scan of every slot and every Consumer's Nets, now a heap merge measured in phase 3; `NC2`/`GND2` tested in the Session, not `StemOf`; bit order by name index, not pin number; `clipGroup` with pins already clipped specified; `runner: 96 watched Nets` on tennis (`bus_load` has 88 distinct Nets); `board_view` and toolbar strings in phase 2; the cursor reason 1.5 s vs the banner's 2 s; drawing from runs instead of 32 per-Channel column caches; slow 1 MHz cases measured; ASCII `-`, `ParseDuration` refusing `0`, delay > timebase; the scope's `last_` initialiser; pattern box width; no UI tests in the phase 5 fence.
- Written while the review ran, before the docs commit: phase 1 to 4 code, most of phase 5 (Board clicks, refusals, CLI, the analyzer window). Committed by phase afterwards, file by file.
- Measured: release `bw_ui netlists/led_computer.bwn` with 32 Clips (`--group U1.A0:8000,U1.D0,U1.PHI0 --clip U1.RW:0,U1.SYNC,U1.RES_N,U1.IRQ_N,U1.NMI_N` at 10 MHz), 4 Hooks at 200 ns/div with a 1 µs delay, the probe on `U1` `RW` and the meter on `D1` `A`: **1.00×, UI 0.6 ms** (the heap merge went in before the measurement). Same on `netlists/heartbeat.bwn` (32 Clips, 4 Hooks with a delay, probe, meter): 1.00×, UI 0.6 ms. TSan: `[runner]` and non-slow `[session]` clean.
- Debug times: the headless 110 Triggers 0.4 s each; the existing 1 MHz cases (`crack stops H0` 0.8 s) stayed small; the full debug suite: 899 tests pass in 523 s.
- `StemOf` makes `PHI0`–`PHI2` a group `U1 PHI` (kept, tested). `clipGroup` returns the reason as text for the refusal.
- Group colours by group index (`GroupColour`), not by first Channel: three groups clipped in a row came out all magenta (first Channels 2, 18, 26).
- Scope window: bands 70 px, the window 640 × 440 at first use so 4 bands, the delay line and the Hook rows fit; the off-screen Trigger arrow sits bottom-left (`T -800 ns`).
- Screenshots viewed: the analyzer zoomed around the `8000` Trigger (view set by a temporary edit, reverted) reads `F80E F80F F810 8000 F811 F812 01FF 01FE …` on `U1 A` and `8D 00 80` (`STA $8000`) on `U1 D`; the scope with 4 bands and the delay; Board tags 1–32 in group colours.
- Navigation can only be checked by reading the code (no mouse): input once before drawing, the wheel owned by the canvas, the view clamped once.

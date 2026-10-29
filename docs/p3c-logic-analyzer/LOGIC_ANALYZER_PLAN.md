# Logic Analyzer — Implementation Plan (P3c)

Design: [LOGIC_ANALYZER_DESIGN.md](LOGIC_ANALYZER_DESIGN.md)
Spec: [LOGIC_ANALYZER.md](LOGIC_ANALYZER.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Capture engine `AnalyzerState` with hand-fed unit tests | done ca60a68 |
| 2 | Headless `LogicAnalyzer` on the Simulator; test pattern and no-leak Puzzle tests | done 1c488fc |
| 3 | `AnalyzerConsumer` through the Runner (+ `Rewatch` / `Consumer::seed`) | done 2ed759e |
| 4 | Session: Clips, settings, arm/disarm, rebuild | done f86e8fd |
| 5 | Board: `Clip (C)` tool, Clip tags, `Analyzer (A)` toggle, `--clip`/`--rate` | done 090f547 |
| 6 | `Logic Analyzer` window + screenshots | done 5d18ddc |
| 7 | Release check, user check, docs | done |

P3c DONE 2026-10-29 (user: "worked well", good enough; feedback below).

Implemented ca60a68..5d18ddc (2026-10-29): 374 tests pass in debug and release. Release `broken_trace` with 8 Clips at 100 MHz (`CLK` included), armed with a Trigger that never fires: 1.00×, `ui` 0.4 ms (P3b: 0.5 ms).

## Implementation notes (differences from the design)

- Runner consumer tests use 10 MHz: a 1 MHz Capture spans 4.1 ms, longer than the 2 ms runs.
- `adopt()` arms at the last slice end `now_` (commands land between slices), not at the Sample time.
- `configure()` watches/unwatches only the Channels whose Net changed (not all), so unchanged Clips send no `Rewatch`.
- Window: 4 table columns (label, trigger, `×`, lane); lanes are `InvisibleButton`s so dragging pans instead of moving the window; readout `dt =` instead of `Δ =` (not in the default font); a grey help line when not hovering.
- `Analyzer (A)` button and key only with a Machine.
- Screenshot `stuck_gate --clip U12.11,U12.12,U12.13 --rate 100kHz`: `4Y` flat high as expected; `4A` reads flat low at 100 kHz (the plan guessed "moving").

## Later (user check feedback)

- Hard to understand how the Analyzer works: the link between the `Clip` tool and the Analyzer window is not self-evident. → P3d (tool button in the window, tool opens the window).
- Poor UI feedback on refused actions, e.g. clipping a Trace instead of a pin. → P3d (placement rule: Clips on pads only, by design; refusal says why).
- 4096 points per Channel is not enough.
- Intermittent bugs when navigating the Capture (zoom/pan).
- General UI improvements needed (with P3b "Later").

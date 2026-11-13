# P5e — Instruments for buses — Implementation Plan

Spec: [BUS_INSTRUMENTS.md](BUS_INSTRUMENTS.md). Design: [BUS_INSTRUMENTS_DESIGN.md](BUS_INSTRUMENTS_DESIGN.md).

Every phase ends with its fence: `cmake --build --preset debug` clean (reconfigure first when files are added), `clang-format` on the C++ files touched, the tests the phase names green in debug, one commit of this step's own paths only. Instruments, runner, Session and UI are shared code: the full debug suite runs at phases 3 and 6. Runner changes also run under TSan (ADD_INSTRUMENT, Gotchas) in phase 3; phase 4's Session changes are UI-thread only.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Probe on an unconnected pin | |
| 2 | Analyzer engine: 32 × 64k, mask Triggers, bus patterns, stems; headless 110 | |
| 3 | Scope engine: 4 Hooks, delay; watch budget 96; consumers | |
| 4 | Session: Channel Groups | |
| 5 | UI: Board clicks and refusals, analyzer window, scope window, CLI | |
| 6 | Release check, recipes, docs, full suite | |

## Phase 1 — Probe on an unconnected pin

- `Machine::probeNet` fallback (design, Probe).
- `machine: probeNet on an unconnected pin` (the 100 Board, `U1` `RW` pad → `PinNet(U1, RW)`; a Trace-only point still nullopt as in `board_probe_test`); `session: probe on an unconnected pin` (lamps `HIGH` or `LOW` after power-on and 1 ms).
- Fence: `ctest --preset debug -j 12 -R "machine:|probe|session:|board_probe|heartbeat"`. Commit: `machine: the probe reads a pin with no copper`.

## Phase 2 — Analyzer engine

- `logic_analyzer.{hpp,cpp}` (design, Engine: analyzer); consumer masks to `ChannelMask` (it does not build otherwise); `board_view.{hpp,cpp}` (`kChannelColours` of 8 and `ChannelColour`: an array of 32 would leave tags 8–31 transparent), `analyzer_window.cpp` and `main.cpp` only as far as needed.
- Existing tests moved to the new sizes (design, Files): engine pre-Trigger counts; `analyzer_consumer` at 100 MHz; `session: clips`, `arm before power-on captures`, `a capture is not re-armed by a repair`.
- Engine tests (spec, Tests: Engine, Bus pattern, Name stems); existing cases moved to `PreTriggerPoints`.
- `tests/machines/logic_analyzer_test.cpp`: the two 110 cases (data at the last `PHI2`-high point of the cycle); debug time of these and of the existing 1 MHz cases measured and noted.
- Fence: `ctest --preset debug -j 12 -R "analyzer|led_computer|session:"`. Commit: `instruments: analyzer of 32 channels, 64k points, bus patterns`.

## Phase 3 — Scope engine, watch budget, consumers

- `oscilloscope.{hpp,cpp}` (design, Engine: scope); `scope_window.cpp` and `board_view.hpp` (`kHookColours` 4) only to build.
- `spsc_ring.hpp` raw storage, `kMaxWatched` 96, the Dispatcher's heap merge (design, Runner).
- `session: hooks` takes 4 Hooks and refuses the fifth.
- Tests: scope cases (spec, Tests: Scope); `runner: 96 watched Nets`; `analyzer_consumer` 32 Channels; `spsc_ring` unchanged and green.
- TSan build of `tests/runner` and `tests/session` (separate build dir), green.
- Release ratio of `bw_ui netlists/led_computer.bwn` with `--group U1.A0,U1.D0,U1.PHI0 --clip U1.RW,...` (32 Clips; the window code of phase 2 is enough) and 4 Hooks: noted. Below 1.00×: profile the devices thread before going on.
- Fence: full debug suite. Commit: `instruments: scope of 4 hooks with a delay; watch budget 96`.

## Phase 4 — Session: Channel Groups

- `session.{hpp,cpp}` (design, Session).
- Tests (spec, Tests: Session) on the 100; `session: led computer with every instrument` widened to 32 Clips and 4 Hooks (spec), `HEARTBEAT_DESIGN`/`LED_COMPUTER_DESIGN` budgets updated.
- Fence: `ctest --preset debug -j 12 -R "session:|analyzer|scope"`. Commit: `session: channel groups`.

## Phase 5 — UI

- `board_view` (`shift`, `refuse`, tag colours), `toolbar` tooltips, `main.cpp` clicks, refusals, `--group`, `--hook`, `--delay`.
- `analyzer_window` rewrite (design, Analyzer window); `scope_window` (design, Scope window).
- Release build; screenshots viewed: `bw_ui netlists/led_computer.bwn --group U1.A0:8000,U1.D0 --clip U1.RW:0 --rate 10MHz --screenshot /tmp/p5e_analyzer.png --after 1s` (groups in hex, Trigger line at the latch write), `--hook U1.PHI0,U1.RW,U6.4Y,U1.SYNC --timebase 200ns --delay 1us` (4 bands, delayed, Trigger arrow), the empty windows (hints). Navigation tried by reading the code paths once more (the agent cannot drive the mouse): input before drawing, wheel owned.
- Fence: `ctest --preset debug -j 12 -R "session:|analyzer|scope"` (there are no UI tests). Commit: `ui: channel groups, one-canvas analyzer, 4-hook scope with delay, refusals at the cursor`.

## Phase 6 — Release check, recipes, docs, full suite

- Release `bw_ui netlists/led_computer.bwn` with every instrument: 32 Clips (spec, Tests: Session with every instrument) at 10 MHz with the `8000` Trigger, 4 Hooks with a delay, the probe on `U1` `RW`, the meter on `D1` `A`: ratio 1.00× and UI ms noted; same on `heartbeat`. If below 1.00×, measure where (devices thread vs UI) and bring it to the user.
- Recipes: ADD_INSTRUMENT (budget, raw rings, Channel Groups, one canvas, the 110 headless Triggers, reference commits), ADD_BOARD_DEVICE (budget 96, rings no longer cost up front), ADD_MACHINE (the probe note removed). GLOSSARY: Channel Group.
- ARCHITECTURE: P5e row done with the date and links; P3c row's "Later" pointing to P5e.
- This plan cut to the status table and implementation notes, "user check pending".
- Fence: full debug suite. Commit: `docs: P5e implemented, user check pending`.

## Implementation notes

- Docs review (before the docs commit), fixed: the 110 latch case read the data at the cycle's last point, where the bus floats (now the last `PHI2`-high point) and the `FFFC` case needs `RW` = 1; existing tests broken by the new sizes listed (`session: clips`, the two 1 MHz Session Captures, `session: hooks`, the consumer tests); the Dispatcher's per-Sample scan of every slot and every Consumer's Nets, now a heap merge measured in phase 3; `NC2`/`GND2` tested in the Session, not `StemOf`; bit order by name index, not pin number; `clipGroup` with pins already clipped specified; `runner: 96 watched Nets` on tennis (`bus_load` has 88 distinct Nets); `board_view` and toolbar strings in phase 2; the cursor reason 1.5 s vs the banner's 2 s; drawing from runs instead of 32 per-Channel column caches; slow 1 MHz cases measured; ASCII `-`, `ParseDuration` refusing `0`, delay > timebase; the scope's `last_` initialiser; pattern box width; no UI tests in the phase 5 fence.
- Written while the review ran, before the docs commit: phase 1 to 4 code, most of phase 5 (Board clicks, refusals, CLI, the analyzer window). Committed by phase afterwards, file by file.

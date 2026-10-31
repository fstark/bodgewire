# Oscilloscope and Multimeter — Implementation Plan (P3d)

Design: [SCOPE_METER_DESIGN.md](SCOPE_METER_DESIGN.md)
Spec: [SCOPE_METER.md](SCOPE_METER.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Shared groundwork: voltages, rails, `islandAt`/`connected`, `AcceptsLead`/`LeadLabel` | done ecb3915 |
| 2 | Multimeter engine `MeterState` + headless `Multimeter` | done 53327ba |
| 3 | Oscilloscope engine `ScopeState` + headless `Oscilloscope`; LS161 delay test | done 1414e2b |
| 4 | `MeterConsumer` and `ScopeConsumer` through the Runner | done 7180f0c |
| 5 | Session: `PadLead`, Hooks, scope settings, Test Leads, volts, continuity | done 2bdcbe2 |
| 6 | UI groundwork: tool↔window pairing, refusal reasons, placement rule, Hook/Lead tags | done 6c06d28 |
| 7 | `Multimeter` window, Meter clicks, `--meter` | done 94dd63b |
| 8 | `Oscilloscope` window, `--hook`/`--timebase`, screenshot | done d3388bf |
| 9 | Release check, user check, docs | release done f0e57fd; user check pending |

Implemented ecb3915..f0e57fd (2026-10-31): 404 tests pass in debug and release (runner tests also under TSan). Release `broken_trace` with 8 Clips at 100 MHz (`CLK` included), Hooks on `U1.2`/`U1.14` at 5 ns/div and the meter on `U1.2`: 1.00×, `ui` 0.8–0.9 ms (same run without Hooks and meter: 0.7 ms; P3c measured 0.4 ms). Screenshots read: meter `1.85 V` on `CLK`; at 20 ns/div `QA` rises 0.65 div after the Trigger mark.

## Remaining

### 9.2 — User check

The user measures a gate delay with the Oscilloscope and finds the `broken_trace` crack by continuity with the power off. Record feedback under "Later", then mark P3d done in the ARCHITECTURE roadmap.

## Implementation notes (differences from the design)

- `ScopeState::clear(channel, now, hooked)`: the engine needs to know unhooked Channels to leave them empty in a Sweep.
- `ScopeState` history: a Sample at the same time as the last one replaces it (power-on X at 0, then the first value at 0).
- `ScopeConsumer` keeps the last sent `ScopeConfig` (UI side) instead of `watched_`; `ScopeConfig` has `operator==`.
- Test `scope: LS161 clock to QA` runs 1 ms + 1 µs slices: with exact 1 ms slices the last Sweep per slice always shows the same `QA` edge.
- Test `meter: window starts at the first Sample`: the change is at 350 ms (the plan's 300 ms did not match its formula).
- Session adds `hookNet()` and `meterNet()` (tests); `setMeterMode` resets `blackNext`.
- A successful Clip no longer opens the Analyzer window; picking the tool does (pairing).
- Lead tags are drawn down-left of the point, so they do not cover the Hook tags (up-left).
- Oscilloscope window: runs of equal pixel columns are drawn as one rect (`ui` 1.1 → 0.85 ms).
- Window positions: Multimeter top-right of the Board panel, Oscilloscope under it.

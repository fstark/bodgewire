# Recipe — Add an instrument

A measuring tool placed on the Board with a window (logic probe P2g, logic analyzer P3c, multimeter and scope P3d). Threads: sim → devices (Consumers) → UI ([ARCHITECTURE.md](../ARCHITECTURE.md)).

Reference commits (P3d, multimeter + scope): `ecb3915` placement + volts, `53327ba`/`1414e2b` engines, `7180f0c` consumers, `2bdcbe2` Session, `6c06d28` tool/window pairing, `94dd63b`/`d3388bf` windows.

## Layers, in phase order

| Layer | Where | What |
|---|---|---|
| Engine | `src/instruments/<x>.{hpp,cpp}` (bw_instruments) | pure state fed with `Sample`s and `advanceTo(Time)`; no threads, no sim; reading struct for display. Unit tests `tests/instruments/<x>_test.cpp` |
| Headless check | `tests/machines/<feature>_test.cpp` | engine on a real Machine via `Simulator` + `Recorder` (cf. `scope_meter_test.cpp`) |
| Consumer | `src/runner/<x>_consumer.{hpp,cpp}` (bw_runner) | implements `Consumer` (`src/runner/consumer.hpp`): `nets()`, `powerOn`, `sample`, `seed`, `advanceTo`, `powerOff`; retargeting via `touch(runner, net)` + counted watch; UI → devices config through an atomic generation; devices → UI through atomics/`TripleBuffer`. Tests `tests/runner/<x>_consumer_test.cpp` |
| Session | `src/session/session.{hpp,cpp}` | owns the placement (tip / lead / clip / hook: Layer + Point), resolves the Net with `machine_->probeNet(...)` in `touchLive()`, pushes config in `configureLive()`, adds the consumer in `makeLive()`; survives rebuilds after a Repair. Tests in `tests/session/session_test.cpp` |
| Placement rule | `src/view/board_scene.{hpp,cpp}` | `AcceptsLead`/`LeadLabel`: held tips take any copper, attached clips/hooks take pads only; refusal says why |
| Tool | `src/ui/toolbar.{hpp,cpp}`, `src/ui/board_view.{hpp,cpp}` | tool mode + key; `takePicked()` hands the pick to `main.cpp`; `refused(reason)` |
| Window | `src/ui/<x>_window.{hpp,cpp}`, `src/ui/CMakeLists.txt`, `src/ui/main.cpp` | ImGui window; the tool opens its window, the window has a button for its tool, focus never changes the tool |
| CLI | `src/ui/main.cpp` | `bw_ui` flag to place it at start (`--probe`, `--clip`, `--hook`, `--meter`) so screenshots can show it |

## Gotchas

- `kMaxWatched` = 48 watched Nets for devices + every instrument together.
- A Net watched again gets no new Sample until it changes: implement `Consumer::seed` or it reads X.
- `configure()` while Off must set the UI-side phase directly (no slices arrive).
- `Runner::powerOff` is queued: `waitIdle()` before checking; `waitIdle` hangs if Running without `pauseAt`.
- Short tests: high sample rates (10 MHz), not 1 MHz (4.1 ms per Capture).
- Threads: run the session/runner tests under TSan (separate build dir with `-fsanitize=thread`).
- Release gate: 1.00× with every instrument attached on the busiest Machine, UI under ~1 ms (`bw_ui --screenshot` prints both).
- ImGui default font lacks `–`/`—`: use `Displayable`.
- The agent can't see the live screen: use `bw_ui … --screenshot X.png --after 1s` (release build) and view the PNG.

# UI Shell — Specification (P2e)

Fifth milestone of P2 (see [ARCHITECTURE.md](../ARCHITECTURE.md), Stack, Performance, Run control, Roadmap). The first window: a Machine runs live on a worker thread and its TV picture is shown as it is drawn. No Board view, probing or Repairs yet (P2f). Builds on [TV.md](../p2c-tv/TV.md) and [FAULTS.md](../p2d-faults/FAULTS.md).

Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Decisions

- **Own executable `bw_ui`**; `bw` stays headless (tests, CI, scripts). Both load the same `.bwn` and `.bwp` files through the same library.
- **SDL3 + Dear ImGui, fetched by CMake** (FetchContent, pinned tags, like Catch2): no Homebrew step. ImGui draws through SDL3's renderer backend (`imgui_impl_sdlrenderer3`): no OpenGL or Metal code of our own.
- **Two threads.** The simulation thread only simulates. The UI thread owns the TV model, ImGui and drawing.
- **One lock-free SPSC ring per watched Net** carries Samples from simulation to UI, as decided in ARCHITECTURE.md Performance. When a ring is full the simulation waits; edges are never dropped. The headless path (Recorder + `trimRecordings`) is unchanged.
- **Real time by default.** Simulated time follows wall-clock time (1 simulated second per second). A "Max" speed runs as fast as the machine allows. When the simulation cannot keep up, it falls behind and the UI shows the real ratio; it never skips time.
- **Run control = power.** Power on (with a seed), power off, pause/resume. Power on always restarts from a fresh power-on state, as in `bw run`.
- **The UI does not leak Faults.** It shows the Machine's name, never the Puzzle's Faults (the `.bwp` file is the answer key).

## Scope

In: `bw_ui <file.bwn|file.bwp>`; window with the live TV picture, status and run controls; SPSC ring and threaded runner in the headless library, with tests; SDL3/ImGui build integration.

Out: Board view, probe by clicking, Repair actions (P2f); file-open dialog; sound; phosphor glow and persistence; settings persistence; Windows/Linux packaging (macOS only for now, nothing macOS-specific in the code).

## Behaviour

### S1 Launch

- `bw_ui FILE` loads a `.bwn` netlist or a `.bwp` Puzzle (with its Board and Faults), then powers on with seed 1. Load errors are printed to stderr as with `bw run`, and `bw_ui` exits with code 1 without opening a window.
- The Machine must have a VIDEO connector; otherwise error, exit 1.

### S2 Window

- One resizable window titled `Bodgewire — <machine name>`.
- **TV panel**: the TV's latest completed frame (whole sweep, 512 × 320 cells), scaled to fit the panel at a 4:3 aspect, linear filtering. Refreshed every UI frame; a new TV frame appears within one UI frame of being completed.
- **Status line**: simulated time, speed ratio (simulated / wall over the last second), TV `locked` / `no lock`, `paused` / `running` / `off`.
- **Controls**: Power (on/off toggle), Pause/Resume, Seed (integer field, used at the next power-on), Speed (`1×` / `Max`).

### S3 Simulation thread

- Runs the Simulator in slices of 1 ms simulated time; at `1×` it sleeps between slices so simulated time does not get ahead of wall time.
- Pause: stops at the end of the current slice. Power off: stops, the TV gets no signal (its inputs go to Z: black screen per TV.md). Power on: builds a fresh Simulator state for the same Circuit with the chosen seed.
- Commands from the UI (power, pause, speed, seed) take effect at the next slice boundary.

### S4 Determinism

- For the same Circuit and seed, the TV frames seen through the threaded runner are identical to the headless `TvInput` path, whatever the ring size, UI frame rate or speed setting.

### S5 Performance

- The test pattern runs at `1×` with the UI live, and the simulation thread spends < 25% of its wall time waiting on rings (ring sized so the UI's 60 Hz frame is never the bottleneck).
- `Max` on the test pattern reaches at least 80% of the headless `bw run --tv` ratio.

### S6 Checks

- Ring: single-threaded FIFO behaviour, wrap-around, full/empty; a two-thread stress test (millions of Samples, order and content preserved).
- Runner, headless (no SDL): test pattern, seeds 1–2, frame at 60 ms through the threaded runner == frame from the single-thread `TvInput` path; tiny ring (forces waits) gives the same frame; pause/resume and power-cycle give the frame of a fresh run with that seed.
- `bw_ui` itself: manual smoke test (the UI is not unit-tested).

## Done when

- S1–S6 implemented; tests pass in debug and release.
- `bw_ui netlists/test_pattern.bwn` shows the grid, locked within 0.5 s, at ≈ 1.0×; `bw_ui puzzles/stuck_gate.bwp` shows the white box; pause, power-cycle and seed change behave as in S2–S3 (confirmed by the user).

# Board Repair — Implementation Plan (P2h)

Design: [BOARD_REPAIR_DESIGN.md](BOARD_REPAIR_DESIGN.md)
Spec: [BOARD_REPAIR.md](BOARD_REPAIR.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Machine: `ParseRepair`, `FormatRepair`; CLI uses them | |
| 2 | View: `BoardScene::componentAt` | |
| 3 | Session: Machine + rebuildable Runner, Repairs, log, blobs, probe | |
| 4 | Puzzles repaired through a Session; rebuild time | |
| 5 | `bw_ui` on a Session (no visible change) | |
| 6 | `bw_ui`: tools, Swap, Mend, blobs, repair log, `--repair` | |
| 7 | Release, Done-when | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug`
Last phase also: `cmake --preset release && cmake --build --preset release && ctest --preset release`

All C++ is `clang-format` clean (never run it on CMakeLists.txt).

---

## Phase 1 — Machine: Repair parsing

(design 2, 3.1)

- New [src/machine/repair.hpp](../../src/machine/repair.hpp)/.cpp: `SwapRepair`, `MendRepair`, `RepairAction`, `ParseRepair`, `FormatRepair` as design 2. Parsing moves from `Repair()` in [src/cli/main.cpp](../../src/cli/main.cpp) (line 152): `swap:` + non-empty reference; `mend:` + `F.Cu|B.Cu` + two `ParseMillimetres` fields; else `"bad repair '<text>'"`. `FormatRepair`: `"swap " + reference`, `"mend " + FormatPoint(layer, at)`.
- CLI `Repair()`: `ParseRepair`, then `std::visit`: Swap → `machine.swap` or `"unknown reference '…'"`; Mend → `machine.mend`, else the existing `bw: no crack near …` line. Output unchanged.
- `repair.cpp` in [src/machine/CMakeLists.txt](../../src/machine/CMakeLists.txt).
- New `tests/machine/repair_test.cpp` (`[machine]`): `"repair: parse swap and mend"`, `"repair: parse errors"` (`cut:U1`, `swap:`, `mend:X.Cu:1:2`, `mend:F.Cu:1`, `mend:F.Cu:a:2`), `"repair: format"` (`swap U3`, `mend F.Cu 52.30 41.00`). Add to [tests/CMakeLists.txt](../../tests/CMakeLists.txt).

### Fence

- [ ] `ParseRepair`/`FormatRepair` exist; `bw run --repair` behaves as before (faults tests pass)
- [ ] All tests pass; full build clean
- [ ] Commit: `"board repair: phase 1 — repair parsing"`

---

## Phase 2 — View: Swap target

(design 2, 3.2)

- [src/view/board_scene.hpp](../../src/view/board_scene.hpp)/.cpp: `std::optional<std::size_t> componentAt(Point at) const`: first Component with a pad that `Covers(at)`, else first with `Contains(courtyards_[k], at)`, else nullopt.
- [tests/view/board_scene_test.cpp](../../tests/view/board_scene_test.cpp): `"board scene: componentAt"` on the test-pattern Board: U1 pin 14 pad → U1's index; U1's courtyard centre (mean of its courtyard points) → U1's index; a point outside the outline → nullopt.

### Fence

- [ ] `componentAt` exists and is tested
- [ ] All tests pass; full build clean
- [ ] Commit: `"board repair: phase 2 — componentAt"`

---

## Phase 3 — Session

(design 2, 4, 5.1, 5.2, 7)

### 3.1 — Module

- New `src/session/session.hpp/.cpp`: `SolderBlob`, `ProbeTip`, `RepairResult`, `Session` exactly as design 2 and 4 (`Live` struct, member order: `circuit`, `tv`, `probe`, `runner`).
- Constructor: keep `machine`, `drawn_ = machine->board()` if any, `live_ = MakeLive(std::move(circuit))`, `dirty_ = false`.
- `powerOn`, `powerOff`, `setSpeed`, `repair`, `touch`, `probedNet` (`live_->probe.touched()`), `lamps` (dark unless `powered_`) as design 5.1, 5.2. `touch` without a Machine: ignored.
- New `src/session/CMakeLists.txt` (design 7); top-level [CMakeLists.txt](../../CMakeLists.txt): `add_subdirectory(src/session)` after `src/machine`; `bw_tests` links `bw_session`.

### 3.2 — Tests

New `tests/session/session_test.cpp` (`[session]`), helper `RunTo(session, t)` (`runner().pauseAt`, `resume`, `waitIdle`) and `Along(nm)` (point on the `kH0Crack` Trace, as `"machine: probeNet across a crack"`; include `../machine/test_points.hpp`):

- `"session: repairs only with the power off"`: `broken_trace`; `powerOn(1)` → `repair(SwapRepair{"U3"})` == Refused, `log()` empty; `powerOff()` → Swap U3 Done, `MendRepair{Front, {0, 0}}` Done (no crack there); `log()` == `{"swap U3", "mend F.Cu 0.00 0.00"}`; `blobs().size() == 1`; Swap U99 → Unknown.
- `"session: drawn Board unchanged by a mend"`: Traces of `drawnBoard()` (count, each start/end) equal before and after mending `kH0Crack`; `machine()->board()` did change (one Trace fewer).
- `"session: probe across a mend"`: touch `Along(-1 mm)` → net A, `Along(+1 mm)` → net B, A ≠ B; `powerOff`, mend `kH0Crack`, `powerOn(1)`: touch each side again → same Net; `probeTip()` still the last touched point.
- `"session: netlist without a Board"`: a Session on `LoadNetlistFile(counter.bwn)` with no Machine: `repair` → Unknown, `touch` leaves `probeTip()` empty, `powerOn(1, 1_ms)` + `waitIdle` → `status().simTime >= 1_ms`.

Add to [tests/CMakeLists.txt](../../tests/CMakeLists.txt).

### Fence

- [ ] `Session` exists; rebuild on the first power-on after a Repair
- [ ] All tests pass; full build clean
- [ ] Commit: `"board repair: phase 3 — Session"`

---

## Phase 4 — Puzzles through a Session

(design 9)

- New `tests/machines/board_repair_test.cpp` (`[machines]`). Helper `Frame(Session&, seed)`: `powerOn(seed, 60_ms)`, `runner().waitIdle()`, `tv()->pictures().update()`, cells. Fault-free: Session on `LoadNetlistMachine(test_pattern.bwn)` + its `build()`.
  - `"board repair: dead counter"`: Swap U3 → frame == fault-free; a fresh Session, Swap U4 → ≠.
  - `"board repair: stuck gate"`: Swap U12 → ==; Swap U13 → ≠.
  - `"board repair: broken trace"`: Mend F.Cu `kH0Crack` → ==; Mend 5 mm away → ≠.
  - Each Repair after a first powered run (power on to 10 ms, power off, repair, `Frame`), so the rebuild path is the one tested.
- `"board repair: rebuild time"` (`[.perf]`, hidden): `broken_trace`, 20 × (power off, mend far away, power on, `waitIdle`); print mean ms; `CHECK(mean < 100)`. Run it in release in phase 7.
- Add to [tests/CMakeLists.txt](../../tests/CMakeLists.txt).

### Fence

- [ ] Each Puzzle repaired through a Session gives the fault-free frame; wrong Repairs do not
- [ ] All tests pass; full build clean
- [ ] Commit: `"board repair: phase 4 — Puzzles through a Session"`

---

## Phase 5 — `bw_ui` on a Session

(design 3.4) No visible change: the probe and TV work as in P2g.

- [src/ui/main.cpp](../../src/ui/main.cpp): `Load` returns `std::optional<Session>` (or nullopt on errors); `main` resolves `--probe` with `session.machine()->padPosition` and calls `session.touch(ProbeTip{at, Layer::Front})` before `SDL_Init` (unknown pin → exit 2 as now); `RunWindow(window, renderer, session, options)`:
  - `BoardView` from `BoardScene(*session.drawnBoard(), machine.pristineBoard(), machine.designCircuit())`;
  - power buttons → `session.powerOn(seed)` / `powerOff()`; Pause/Resume/status → `session.runner()`; speed → `session.setSpeed`;
  - TV: `session.tv()` each frame (it may be replaced by a rebuild);
  - probe clicks → `session.touch(onCopper ? ProbeTip{at, layer} : nullopt)`; `setProbeTip(session.probeTip() → at)`; lamps `session.lamps()`.
- [src/ui/CMakeLists.txt](../../src/ui/CMakeLists.txt): `bw_ui` links `bw_session`.
- Check: `--probe` screenshots of the three Puzzles as in P2g (agent views one).

### Fence

- [ ] `bw_ui` runs through a Session; P2g screenshots unchanged
- [ ] All tests pass; full build clean
- [ ] Commit: `"board repair: phase 5 — bw_ui on a Session"`

---

## Phase 6 — `bw_ui`: Repairs

(design 2, 3.3, 5.3–5.5)

- [src/ui/board_view.hpp](../../src/ui/board_view.hpp)/.cpp:
  - `Tool` enum, `tool_ = Tool::Probe`, `tool()`; keys `P`, `S`, `M` in `input()` beside `L`, `F`;
  - `BoardClick::component = scene_->componentAt(at)`;
  - `controls(bool powered)`: radio buttons Probe / Swap / Mend, then the existing Layer and Fit; when `powered`, a second line `power off to repair`;
  - `setBlobs(span)` (copy), drawn after vias as design 5.4 (`kSolder`, radius `max(3 px, 0.6 mm)`, inactive Layer at `kInactiveAlpha`).
- [src/ui/main.cpp](../../src/ui/main.cpp):
  - clicks by `boardView.tool()` as design 5.3;
  - `setBlobs(session.blobs())` each frame; repair log under the speed row: `Repairs` then one line per `session.log()` entry;
  - `Options::repair` (`--repair a,b`, split on commas), applied in `main` after `--probe` as design 5.5; usage string gains `[--repair swap:REF,mend:LAYER:X:Y]`.
- Check: build and test gates; `bw_ui puzzles/broken_trace.bwp --repair mend:F.Cu:137.59:99.17 --probe U13.2B --screenshot /tmp/bw-repair.png --after 2s`: agent views blob, log, grid, U13.2B lamps with PULSE; `--repair swap:U99` and `--repair cut:U1` → exit 2.

### Fence

- [ ] Tools, Swap, Mend, blobs, log and `--repair` work
- [ ] All tests pass; full build clean
- [ ] Commit: `"board repair: phase 6 — Repairs in bw_ui"`

---

## Phase 7 — Release, Done-when

- Release build, `ctest --preset release`; `build/release/tests/bw_tests "[.perf]"` → rebuild mean < 100 ms (S3).
- `bw_ui puzzles/broken_trace.bwp --probe U1.CLK --screenshot /tmp/bw-clk.png --after 3s`: still ≈ 1.00× (P2g S4).
- User check (spec Done-when): play the three Puzzles end to end without reading the `.bwp` files. Record tuning wishes in this plan.
- [ARCHITECTURE.md](../ARCHITECTURE.md) roadmap: P2h row done, P2 first playable done.

### Fence

- [ ] Rebuild < 100 ms; 1.00× kept
- [ ] Debug and release tests pass
- [ ] User confirms the Done-when
- [ ] Commit: `"board repair: phase 7 — release check"`

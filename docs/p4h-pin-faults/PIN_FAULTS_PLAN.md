# P4h Pin Faults — Implementation Plan

Design: [PIN_FAULTS_DESIGN.md](PIN_FAULTS_DESIGN.md)
Spec: [PIN_FAULTS.md](PIN_FAULTS.md)

Each phase ends with its fence: the checks listed, `cmake --build --preset debug` and `ctest --preset debug -j 12` green, `clang-format` on the C++ files touched, then one commit with the message given (plus the attribution line). Only this step's paths are committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Stuck inputs: loader and builder | done |
| 2 | Dry Joints: syntax, loader, electrical netlist, leg/pad, `resolder` | done |
| 3 | Resolder Repair: parse, Session, `bw run`, `bw_ui --repair` | done |
| 4 | UI: Resolder tool, leg/pad words, Probe hint | done |
| 5 | Puzzles: Solid net, Missing eight; Machine docs | |
| 6 | Release check, recipes, glossary, roadmap | |

## Phase 1 — Stuck inputs

Design §2 (stuck lines), §5.

- `machine.cpp`: `kindOf` lambda; the stuck check and its message.
- `builder.cpp`: `StuckPin` comment, the `Check`, stuck inputs on their own tied Net after the loop.
- Tests: `builder_test` stuck input; `machine_test` load errors (`U3.A` loads, `J1.1` message) and swap removes a stuck input; `pin_faults_test.cpp` (new, in `tests/CMakeLists.txt`) with the stuck-input case.

Fence: `[netlist]`, `[machine]` and `pin faults` cases pass.
Commit: `faults: phase 1 — stuck inputs`

## Phase 2 — Dry Joints

Design §1, §2 (joint lines), §3, §4.

- `puzzle.{hpp,cpp}`: `JointDecl`, `joints`, `fault joint`.
- `machine.{hpp,cpp}`: `dry_`, `chipSignalPin`, joint loading, `resolder`, `dryJoints`, `dryLegAt`, `probeNet`, `connected`, `build`.
- `electrical.{hpp,cpp}`: the `dry` parameter.
- Tests: `puzzle_test`, `machine_test` (joint errors, `resolder`, swap keeps the Joint), `electrical_test`, `pin_faults_test` (leg/pad, continuity, Machine-level resolder and swap).

Fence: `[machine]`, `[netlist]`, `pin faults` pass; `board_probe`, `continuity` and `shorts power` cases unchanged.
Commit: `faults: phase 2 — dry Joints, leg or pad`

## Phase 3 — Resolder Repair

Design §6.

- `repair.{hpp,cpp}`: `ResolderRepair`, parse, format.
- `session.cpp`: the visitor branch.
- `src/cli/main.cpp`, `src/ui/main.cpp`: `Repair()`, usage strings. (`std::visit` in `FormatRepair` and `Session::repair` fails the build without the new alternative; the CLI's `get_if` chain does not: its branch goes before the final `else`.) `bw_ui --meter` label gains "leg" (design §7).
- Tests: `repair_test`; `pin_faults_test` Session case (refused powered, logged, frame fault-free, swap does not fix).
- Check by hand: `./build/debug/src/cli/bw run puzzles/broken_trace.bwp --time 10ms --repair resolder:U13.2B` prints no error (a good Joint is accepted); `resolder:R1.1` fails with "cannot apply".

Fence: `[machine]`, `pin faults` pass.
Commit: `faults: phase 3 — Resolder repair`

## Phase 4 — UI

Design §7.

- `board_view.{hpp,cpp}`: `Tool::Resolder`, key `R`.
- `toolbar.cpp`: button, hints (Resolder, Probe).
- `main.cpp`: the click case.
- `board_scene.{hpp,cpp}`: `chip()`, `ContactSide`, `LeadLabel` with the layer; the caller.
- `side_panel.cpp`: the side word.
- Tests: `board_scene_test` (`ContactSide`, `LeadLabel`).
- Release screenshot of any Machine with the Probe on a chip pad, viewed: toolbar with Resolder, the hint, "· leg" in the Probe panel.

Fence: `[view]` pass; release builds; the screenshot viewed.
Commit: `ui: phase 4 — Resolder tool, leg or pad`

## Phase 5 — Puzzles

Design §8. ADD_PUZZLE.md (files, tests, docs).

- `puzzles/solid_net.bwp`, `puzzles/missing_eight.bwp`; `git rm puzzles/wide_net.bwp`.
- `ball_test.cpp` (`solid_net` replaces `wide_net`), `stopwatch_test.cpp` (`missing_eight`), `test_points.{hpp,cpp}` (`WideNetCrack` removed).
- `BOUNCING_BALL.md`, `BOUNCING_BALL_DESIGN.md`, `STOPWATCH.md`, `STOPWATCH_DESIGN.md` as design §8.
- `bw run puzzles/solid_net.bwp --time 0.5s --probe U15.13,U3.QC` (leg floating, `V2` pulsing).

Fence: `ball:` and `stopwatch:` cases pass; `grep -rn wide_net src tests puzzles docs/machines` is empty.
Commit: `puzzles: phase 5 — Solid net, Missing eight`

## Phase 6 — Release check, recipes, docs

- Release build; on an idle machine: `bw_ui puzzles/solid_net.bwp --screenshot … --after 1s` (solid net) and `bw_ui puzzles/missing_eight.bwp …`, both viewed; 1.00× noted; with `--probe U15.13` the panel shows "leg".
- `ADD_PUZZLE.md`, `GLOSSARY.md` as design §9.
- `ARCHITECTURE.md` P4h row: done with the date and links; "the User Manual says it once" becomes "the Probe hint says it" (staged from `HEAD` plus this change only).
- This plan cut down to the status table and notes ("user check pending").

Fence: full debug `ctest` green; release builds.
Commit: `docs: P4h implemented, user check pending`

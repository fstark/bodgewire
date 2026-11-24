# P8c — Repair Validation — Implementation Plan

Spec: [REPAIR_VALIDATION.md](REPAIR_VALIDATION.md). Design: [REPAIR_VALIDATION_DESIGN.md](REPAIR_VALIDATION_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | The verdict: `Wiring`, `Machine::completed`, the Session's copy, `NextPuzzle`, their tests; `bw run`'s line | |
| 2 | Every Puzzle's test checks the verdict; `repair validation:` cases | |
| 3 | The acknowledgement: `RepairAck`, the post-it's tick, the Thank-you Card, the next Puzzle's bench | |
| 4 | Docs, README shot, release check with screenshots, debug, web build, full release suite | |

## Phase 1 — The verdict

1. `Wiring`, `WiringOf` in `src/machine/electrical.*` (design §1).
2. `Machine::golden_` set in `LoadMachine`, `Machine::completed()` (§1).
3. `Session::completed()`, `puzzle()`, `powerOns()` (§2).
4. `NextPuzzle` in `src/machine/machine_list.*` (§4).
5. `bw run`'s line (§6).
6. Tests: `electrical: wiring …`, `machine: completed …`, `session: completed …`, `machine list: next puzzle`, and `repair validation: verdict time` (`[.perf]`) in the new `tests/machines/repair_validation_test.cpp` (added to `tests/CMakeLists.txt`); run the perf case once, its mean in the notes.
7. `clang-format` on the changed C++ files.

Fence: release `ctest -R "^(electrical|power|machine|machine list|session|repair validation): "`; debug `ctest -R "^(electrical|power|machine|machine list): |^session: (completed|the working)"`; `bw run …01-dark-led.bwp --repair mend:F.Cu:192.6:121.96` prints `bw: Puzzle completed`; the full release suite (shared code: `bw_machine`, `bw_session`). Commit: `machine: Repair Validation, the Machine's verdict against the Golden Machine's Wiring (P8c phase 1)`, its body naming the checks.

## Phase 2 — Every Puzzle's test

1. Each Puzzle's `TEST_CASE` in the 16 test files of design §7 (`shorts_power_test.cpp` and `board_repair_test.cpp` included): `CHECK_FALSE(….completed())` after the load, `CHECK(….completed())` after the right Repair; shared helpers called with wrong Repairs take the expected verdict (§7). A case without a Repair gets the one its Machine design names. Count: 114 Puzzles, each checked both ways (a `grep -c completed` per file against its Puzzle count).
2. `repair validation: no Puzzle completed as loaded`, `repair validation: every working Machine is completed` (§7).
3. A Puzzle completed as loaded, or not completed after its test's Repair: stop and report it to the user (a Puzzle bug, or a gap in the rule), not worked around.

Fence: release `ctest -R "^(repair validation|counter|chaser|dice|stopwatch|knob|faults|board repair|shorts power|ball|tennis|message|typewriter|led_computer|bw1|bw2001|bw80): "`; debug the small Machines only, `ctest -R "^(repair validation|counter|chaser|dice|stopwatch|knob|faults|board repair|shorts power): "` (tennis, ball, BW-1, typewriter and the large Machines cost about an hour of debug CPU; this phase changes tests only). Commit: `tests: every Puzzle not completed as loaded, completed after its Repair (P8c phase 2)`.

## Phase 3 — The acknowledgement

1. `src/view/repair_ack.*` and `tests/view/repair_ack_test.cpp` (§3), in `bw_view` and `tests/CMakeLists.txt`.
2. The card colours into `brief_window.hpp`, `launcher.cpp` using them; `DrawBrief`'s tick (§5).
3. `src/ui/thank_you_card.*` (§5), in `src/ui/CMakeLists.txt`.
4. `main.cpp`: `RunBench` with the list, `RepairAck`, the card, `BenchEnd::Next`; `main` opening the next Puzzle's bench (§5).
5. Screenshots viewed (design, Checks): the Dark LED mended at 4 s (card and tick) and 2 s (neither), wired across at 4 s, Too Fast swapped (no Next), the working Counter (no card), the BW-2001's Odd E swapped (speed and UI ms against unrepaired).
6. `clang-format` on the changed C++ files.

Fence: release and debug `ctest -R "^repair ack: "`; release build and the screenshots; debug build, debug `bw_ui` on the Dark LED mended `--after 4s`: no sanitizer report; the web build. Commit: `ui: the completed Puzzle acknowledged, a tick on the post-it and the Thank-you Card (P8c phase 3)`, its body naming the checks.

## Phase 4 — Docs and checks

1. GLOSSARY, ARCHITECTURE (Repair validation, the P8c row done), GAME_DESIGN, ADD_PUZZLE (§8).
2. README_SPEC §6 step `repaired`, `tools/readme_shots.py`'s shot taken alone (`python3 -I tools/readme_shots.py repaired`), the README regenerated, the new picture viewed.
3. Release check: the screenshots again, viewed.
4. Full release suite.
5. This plan cut down to the phase table and notes.

Fence: full release suite. Commit: `docs: P8c done, Repair Validation in the glossary, recipes and README (P8c phase 4)`.

## Implementation notes

- Review (2026-11-24), must-fixes, fixed in the docs before phase 1: `Wiring` keeps the Net names (the builder ties `VCC`/`GND` by name: a rail island cut from `J1` keeps its pins but becomes `VCC~2`, a false "completed" without names); the `led_computer` prefix; `shorts_power_test.cpp` (four Test Pattern Puzzles) in the file list and fences; helpers called with wrong Repairs take the expected verdict; the debug fences narrowed (no `session:` whole, no tennis/ball/BW-1/typewriter). Should-fixes taken: `completed()` reuses `rebuildBoard`'s islands and `golden_` is lazy (`WiringOf` as first written cost about two rebuilds, 110 ms on the BW-2001); the card waits for an open question; the Next path's `main` branch spelt out; the card's Next and Machines left to the user (the path after them is `--visit`'s).

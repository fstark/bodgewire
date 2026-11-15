# P5u BW-80 chips — Implementation Plan

Design: [BW80_CHIPS_DESIGN.md](BW80_CHIPS_DESIGN.md)
Spec: [BW80_CHIPS.md](BW80_CHIPS.md)

Implemented 2026-11-15, in the worktree `../bodgewire-p5u`, branch `p5u-bw80-chips` (another agent worked on `main`).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `832126a` |
| 1 | Kernel: Joined Pin, network Nets | `293af8b` |
| 2 | `ResistorNetwork`, `ParseOhms`, builder | `5a4186c` |
| 3 | Board: electrical netlist, Net panel, `R` datasheet, glossary | `0a3eda5` |
| 4 | `2332`, `74LS05`, `74LS10`, `74LS365`, `74LS373` | `fc82ed1` |
| 5 | Bench, its program and test | `443d5ec` |
| 5b | A key on a joined Net; `value=` on every resistor (the user, after the review) | `f9dac00` |
| 6 | Release check, recipes, roadmap | this commit |

## Notes

- Review (before code), fixed in the docs: the catalogue test and `CreateRom`'s `select=` messages assumed three selects; `ResistorNetwork::powerOn` resets its driven values (a Circuit is powered on again); `JR *`, not `JR $`; `.rom $0000, $1000`; `RST1` (a reference is letters then digits); fence regexes by test name (`net pins`, `net drivers`, `manual`, `gate package`, `joined`); the NOP timing on the model (the 2114 times its output from `CS_N`, so `access=` cannot show the margin: P5v); the RAM test ≈ 52 ms, not 25; the unconnected-end message; links stored both ways, `kFar` skipped, a turn to X a change; weak sources count as sources and the 1 ps float, noted; `FormatNetPin`'s series text; the test Board's own loader; eight resistors, not nine; the reachable builder errors noted for P5x.
- Phase 1–4 code was written while the review ran, then checked against its findings (`powerOn`, the select messages and `JR *` were already as the review asks).
- Baselines: taken side by side at phase 6 against a worktree of the commit before phase 1 (the release build here had started after the kernel edit).

- Phases 1–5 were written together and tested together (full debug suite 1280/1280 in 958 s), then committed one by one by staging each phase's files and its own version of the shared CMake lists; the intermediate commits were not built one by one.
- The user, after the review's note that a builder error is reachable by a repair and stops the Session (2026-11-15): no repair may stop the game. Phase 5b: a Net holds a Switched and a Joined Pin, each with its own view; every resistor needs `value=` (eight test strings gained one; every netlist in the repo had one). Full debug suite after it, with `external/` linked into the worktree: 1280/1280 in 813 s.
- Bench, against the design: it passed at once on seeds 1–3. `STATUS` is `$55` before 53 ms (seed 1); `kDone` 56 ms. A mutation to the 2114's 450 ns grade still passes (the model times the output from `CS_N`, as the review said): the NOP margin is P5v's. Debug: ≈ 21 s for the three-seed case.
- Release, headless, base (`e89ba4c` in a worktree) against P5u, alternated, three runs each: `bus_load` 0.330–0.387 vs 0.308–0.389, `bus_load_z80` 0.342–0.350 vs 0.337–0.351, 110 4.45–4.57 vs 4.54–4.57, BW-1 2.10–2.15 vs 2.18–2.21, BW-2001 0.204–0.227 vs 0.209–0.232: no change. The bench 0.46× (100 ms).
- Release-only tests: `bw2001 machine: stage 3, a scrolling screen without snow` fails (`snow->accesses` 1, expected 0) on P5u and on the base commit alike: an existing failure on `main`, not P5u's. The other release-only BW-2001 cases pass.
- `bw_ui` release, `netlists/bus_load_z80.bwn`: the page on the TV as before, 0.43×, UI 0.0 ms; screenshot viewed.

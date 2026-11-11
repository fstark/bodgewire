# P4n RAM and bus — Implementation Plan

Design: [RAM_BUS_DESIGN.md](RAM_BUS_DESIGN.md)
Spec: [RAM_BUS.md](RAM_BUS.md)

Done 2026-11-11, user check pending.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Bidirectional pins | `cad60d5` |
| 2 | Packages: DIP-18, DIP-20, KK-10 | `c547493` |
| 3 | 2114 RAM | `5356e5e` |
| 4 | 74LS244, grid packages | `a52b167` |
| 5 | KEYBOARD Part | `e0feb33` |
| 6 | Typing: Session, `bw run`, `bw_ui --type`, bench | `53e109f` |
| 7 | Keyboard capture in `bw_ui` | `605db6c` |
| 8 | Release check, recipes, docs | this commit |

## Implementation notes

- Puzzle-file `stuck`/`joint` on a 2114 pin needs a Board with one: first exercised by the 090 Puzzles (P4p); P4n tests the electrical side through the builder.
- Phase 1: 070 headless 1.57× before the change on a fresh machine; later 1.47–1.49× both with and without it (same session, alternated builds): no cost from the change.
- Phases 2–4 were checked by one full debug run (743 tests) before their three commits (a background run had overlapped a rebuild); each commit builds on its own. The `DescribeNet` case planned for phase 4 was added to its commit.
- `ParseTyping` refuses a start of `0ms` (`ParseDuration` needs ≥ 1 ps): scripts start at `1ms` or later.
- Release check: `bw run tests/data/ram_bench.bwn --press SW1:1ms:2ms --type J1:3ms:HELLO --press SW1:600ms:601ms --record B0,…,B6 --vcd …`: the bus ends at 0x48 (`H`); `--type J9:…` and `--type J1:1ms:{` refused. `bw_ui tests/data/ram_bench.bwn --type 'J1:100ms:HELLO\n'` shows `Sent: HELLO[CR]` at 1.00×; 070 with every instrument unchanged at 1.00×, UI 1.2 ms. Headless after the step: 070 1.48–1.53×, 080 2.31×. `bw place` on 070 and 080 gives identical files before and after the grid change.
- User check pending: Type in the Keyboard section of `bw_ui tests/data/ram_bench.bwn`, a few keys show under Sent and the banner says Typing; `U` does not open the User Manual while typing; Escape stops it and `U` works again.
- Docs review (before phase 1), fixed: the catalogue test's all-TTL rule exempts the 2114; the bench resets U4 before typing (random power-on count); the typing banner drawn without a Board; the user check uses `U` (no tool keys on a netlist-only Session); the placer check diffs two scratch placements; no test of a TTL bidirectional warning (no such Part); a stuck `IO` against a driving 74LS244 reads 0; a stuck bidirectional pin's own Net tied at Supply; waiting keys dropped at power-off by a producer-side marker, kept at power-on, no `--type` key in the first powered frame of `bw_ui`; the keyboard gap counted from the `STROBE` fall; control bytes of text events skipped; Z set explicitly at power-on; the release `bw run` records the bus.

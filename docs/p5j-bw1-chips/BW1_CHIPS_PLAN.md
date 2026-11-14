# P5j BW-1 chips — Implementation Plan

Design: [BW1_CHIPS_DESIGN.md](BW1_CHIPS_DESIGN.md)
Spec: [BW1_CHIPS.md](BW1_CHIPS.md)

Implemented 2026-11-14; user check pending.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | ROM and RAM blocks, memory names in cell Faults | `02e9cd2` |
| 2 | Kernel: pull-up Pins | `fea1afb` |
| 3 | 6102 and 74125 | `30cebbd` |
| 4 | 6530 | `144ee6f` |
| 5 | Bench and its program | `ac1a3d6` |
| 6 | Release check, recipes, glossary, roadmap | this commit |

## Notes

- Review (before code), fixed in the docs: the bench's unused gate inputs tied (no warnings); the IRQ period bound from the handler's cycle count (816–824 µs, accepted 810–830); the `PB7` release delay named (`kMos6530Irq` from the deciding edge); the constructor and `Pins()` stated; the builder's stuck pull-up test on the chip's own stuck Net (checked to fail without the Supply tie: 3 of 16 seeds power on driving `PA0` low); phase 4's fence includes `manual`.
- `CellDecl::memory` and `StuckCell::memory` are the **last** fields, so the existing aggregate initialisers keep compiling. The 6530's unknown-parameter error uses the ROMs' wording (`6530: unknown parameter 'freq'`).
- Phase 1's full suite: 966/967; the one failure was the `asm` check on the then-uncommitted `roms/bw1_bench.asm` (no `.rom` yet), outside the commit.
- Phases 2–5 were written before phase 1's suite finished and tested together: their part, kernel, manual, netlist and machine tests, then one full debug suite on the whole tree; each phase is still its own commit, staged by path.
- Bench: `$55` on port A at 22.6 ms; timer interrupts 817–820 µs apart (the reviewer's 816–824 µs). Headless release 2.25× (100 ms run; 1.40× over 33 ms with a VCD recorded).
- Ratios (release, headless): this morning's baselines 110 5.72×, `bus_load` 0.396×, 080 2.93×; after phase 5 they read about 5 % lower on all three, the 080 included (no 2114). Measured side by side with `f785ac8` built in a worktree, three runs each: 110 5.28–5.63 (base) vs 5.49–5.54 (new), `bus_load` 0.370–0.372 vs 0.369–0.373, 080 2.75–2.79 vs 2.60–2.80: machine drift, no change.
- `bw_ui` release, 110 First light: D1 lit as the brief says, 1.00×, UI 0.7 ms; screenshot viewed.
- Full debug suite: 966/967 at phase 1 (see above), 994/994 on the whole tree before phases 2–5 were committed; each of phases 3 and 4 was also built alone and its fence subset run (55/55, 142/142).
- Not checked: the 6102's chip-enable and output-off times and `DOUT` during a write, and the 74125's delays, are assumptions (datasheets not reached); the 6530's interrupt flag read gives 0 on `D6`–`D0` (assumed).

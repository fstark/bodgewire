# P5z2 Kernel: Parts — Specification

Roadmap row P5z2 ([ARCHITECTURE.md](../ARCHITECTURE.md), Roadmap). Design: [KERNEL_PARTS_DESIGN.md](KERNEL_PARTS_DESIGN.md). Plan: [KERNEL_PARTS_PLAN.md](KERNEL_PARTS_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md). Before: P5z ([KERNEL_BW2001.md](../p5z-kernel-bw2001/KERNEL_BW2001.md)), [OPTIMISATION_IDEAS.md](../p1d-kernel-perf/OPTIMISATION_IDEAS.md).

## Why

P5z took the BW-2001 from 0.21× to 0.36× and stopped at its 5 % bar. The BW-80 did not move (0.25×). The profile on P5z's result (`build/profile-lto`, BW-2001) puts Parts at about a third of the time:
- gate packages about 10 % (74LS00 4.9 %, 74LS08 3.0 %, others);
- 74LS93 4.6 % and 74LS107 2.9 %.

On the BW-80, gate packages are 40 % of all evaluations and the 74LS373 3 % (P5z spec, Why).

Three of the ideas listed after P5z are cheap and keep traces exact (the user, 2026-11-16: "do A–C"):
- **A:** gate packages evaluate only the units whose inputs moved;
- **B:** fast paths in the 74LS93 and 74LS107 for known values;
- **C:** quiet Pins on more Parts.

The rest are potential rows on the roadmap.

## Terms (set 2026-11-16)

- **Keeping an idea:** kept when its gain is worth the complexity it adds (the user, 2026-11-16; [OPTIMISATION_IDEAS.md](../p1d-kernel-perf/OPTIMISATION_IDEAS.md), Keeping an idea). This replaces P5z's 5 % bar. Each idea's gain, lines added and verdict go in the plan's idea log.
- **Traces exact:** `bench/trace_check.sh` identical to `f849080` on its eight runs, the same applied events (P5z spec, S1).
- **One kernel path; the Part API unchanged.** These are Part-local changes.
- **No netlist slower** beyond noise (±7 % on the small ones; a marked netlist is measured again, P5z plan notes).

## User decisions (2026-11-16)

| Question | Decision |
|---|---|
| What P5z2 does | ideas A, B and C of the list after P5z |
| The rest | potential rows on the roadmap: oscillator lane, event apply trimmed, multi-Pin `EvalContext`, Bus as one entity, two cores, generated code |
| When an idea is kept | when its gain is worth the complexity it adds; the 5 % bar is gone |

Not asked: which Parts C covers. The 6520 and 6522 already mark their bus Pins `sampled` (P5n), so they gain nothing; the 74LS373 is the one left.

## Behaviour

Nothing a player, a Puzzle or an instrument sees changes.

### A. Gate packages, unit by unit

A gate package (`GatePackage`, every `EvaluatePackage<units, delays>`) evaluates a unit only if one of its inputs changed at this timestamp, or at the power-on evaluation. A unit whose inputs did not move would drive what it already drives, or has pending, so skipping it is exact. Today a wake of a 74LS00 evaluates all four gates.

P5z tried the same skip on the 74LS244 (Z3.2) and it gained nothing: a 244 half's work is two reads and a drive, so the test cost what it saved. A gate unit computes `GateFunction` over two to eight inputs and its delay, and gate packages are 40 % of the BW-80's evaluations. If A comes out equal, it is dropped as Z3.2 was. The change test reads each input's Net once (value and `changedAt` together), not `read` and `readBefore` separately.

### B. 74LS93 and 74LS107 fast paths

- **74LS93:** the reset gate (`R01·R02`) is read without building a span for `GateFunction` (two reads and a compare when both are 0 or 1). The rest is unchanged.
- **74LS107:** when `J`, `K` and the state are all 0 or 1, the next state is the JK table directly (`NextKnown`). The X loop of `Next()` runs only otherwise.

Same results by construction. The 74LS90 has the same reset form (`r0`, `r9`) and gets the same change if the 93's shows a gain.

### C. Quiet D on the 74LS373

While `LE` reads a definite 0 (the latch closed), the eight `D` Pins are quiet (P5z Z2's mechanism). With the latch closed, `evaluate` reads `D` only at the falling `LE` edge (through `readBefore`, which reads the Net whether or not the chip was woken), or when `LE` is X (then it is loud). So a closed latch skips only evaluations that changed nothing. Woken by `LE`, the chip makes `D` loud again. The 74LS373 is the BW-80's character latch (`IC5`, the RAM data latch); the BW-2001 has none.

## Tests

- A: `tests/parts/gate_package_test.cpp`:
  - after power-on, every unit drives its value even though nothing changed;
  - as a regression guard (it passes before and after A, since re-driving a pending value is already a no-op): a pending change on one unit's output survives a change of another unit's input;
  - the existing cases pass unchanged.
- B: the existing 74LS93 and 74LS107 tests pass unchanged; X cases already cover the slow paths.
- C: `tests/parts/ls373_test.cpp`:
  - with `LE` low, D changes do not evaluate the chip (evaluation count against `LE` at X, as P5z's memory tests do);
  - D changed while closed, then `LE` rising, gives the new D after the latch delay;
  - D changed just before `LE` falls is held.
- The trace check after each idea, and the full debug suite at the end of each phase (gate packages and the 74LS373 are shared Parts).

## Results

Measured 2026-11-16 (`bench/measure.sh`, release, best of 3, and `/usr/bin/time -l` on the BW-80):

| Idea | BW-2001 | BW-80 | Verdict |
|---|---|---|---|
| before (P5z's end) | 0.368×, 509 instr., 95 cycles an event | 0.248×, 499 instr., 90 cycles | — |
| A, gate packages unit by unit | 0.373×, 503, 93 | 0.253×, 496, 88 | kept: small, steady, about twenty lines |
| B, 74LS93 and 74LS107 known-value paths | equal (503 instructions) | equal | dropped: LTO had already folded `GateFunction` into the 93, and the 107's loop was cheap for known values |
| C, quiet D on the 74LS373 | — (none on the board) | 1.3 % fewer evaluations, 495 instr., ratio and cycles equal | dropped: no time saved |

Traces identical throughout; no netlist slower. `bw_ui`: BW-2001 0.37× (UI 2.3 ms), BW-80 0.23× (UI 0.9 ms).

**Far below the estimates** (A's 8–15 % on the BW-80 came out at 1–2 %). The estimates took each Part's share of the profile as what an idea could remove. But:
- most gate package wakes come from the unit whose input moved, so there was little to skip;
- with LTO, what was left in the 93 and 107 was already small.

The cheap, exact Part-local ideas are now used up on both computers. What remains is the generic run loop and queue (about half the time) and the design-bending options, which are on the roadmap as potential rows.

## Done when

- A, B and C tried, each logged with its gain, lines added and verdict.
- Results here; ARCHITECTURE (Performance, row P5z2) and OPTIMISATION_IDEAS (History) updated; ADD_PART updated if Parts are written differently (A changes how a gate package is written).
- Full debug suite green; release screenshots of a BW-2001 and a BW-80 Puzzle viewed.

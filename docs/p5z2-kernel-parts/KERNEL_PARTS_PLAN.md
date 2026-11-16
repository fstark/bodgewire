# P5z2 Kernel: Parts — Implementation Plan

Spec: [KERNEL_PARTS.md](KERNEL_PARTS.md). Design: [KERNEL_PARTS_DESIGN.md](KERNEL_PARTS_DESIGN.md).

Implemented 2026-11-16; user check pending. The phases and fences are in git history (`127939c`); results in the spec (Results).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `127939c` |
| 1 | A: gate packages unit by unit | `fcb70a0` |
| 2 | B: 74LS93 and 74LS107 known-value paths | dropped |
| 3 | C: quiet D on the 74LS373 | dropped |
| 4 | Finish | this commit |

## Idea log

| # | Idea | bw2001 ratio | Instr./event | Cycles/event | Eval./event | bw80 | bw80 evaluations (1 s) | bus_load | Lines | Kept | Commit |
|---|------|--------------|--------------|--------------|-------------|------|------------------------|----------|-------|------|--------|
| 0 | P5z's end (`f010fb8`), measured again before A | 0.368× | 509 | 95 | 1.07 | 0.248× (499 instr., 90 cycles) | 180.2 M | 0.419× | — | — | `f010fb8` |
| A | gate packages evaluate only the units with an input changed now (`readChanged`), all at power-on | 0.372–0.373× | 503 | 93 | 1.07 | 0.252–0.255× (496 instr., 88–89 cycles) | 180.2 M | 0.417× | +15 in `gate_package`, +6 `readChanged` | yes: small (BW-2001 −2 % cycles, BW-80 −1…2 %) for a few plain lines | `fcb70a0` |
| B | 74LS93 reset read without `GateFunction`; 74LS107 `NextKnown` when `J`, `K`, state known | 0.376× | 503 | 92 | 1.07 | 0.254× | 180.2 M | 0.419× | +10 | no: instructions equal, the rest within noise | — |
| C | 74LS373 `D` quiet while `LE` is a definite 0 | 0.373× | 503 | 93 | 1.07 | 0.254–0.255× (495 instr., 88 cycles) | 177.8 M | 0.416× | +12 | no: 1.3 % fewer BW-80 evaluations, no time saved | — |

## Implementation notes

- Docs review (2026-11-16), fixed before the docs commit:
  - ADD_PART's Quiet Pins rule reworded for a closed latch, not only a deselect;
  - A's pending-drive test called a regression guard;
  - ADD_PART in phase 1;
  - the BW-80's evaluations and instructions measured for C;
  - P5z's Z3.2 cited, with what makes A different and what drops it;
  - the 74LS90 in the spec and Files;
  - `measure.sh` output paths;
  - `LE`/`OE_N` loud;
  - one Net read per input (`readChanged`), not `read` and `readBefore`.
- Phase 1, A (2026-11-16):
  - Far below the estimate (BW-80 8–15 %): most package wakes come from a unit whose own input moved, so there is little to skip. The BW-80's evaluation count is unchanged by construction (a skipped unit is inside one evaluation).
  - Kept under the new rule: steady over two runs, and about twenty plain lines.
  - Traces identical.
  - Full debug suite 1376/1376 in 622 s; release `bw2001|bw80|bw1|bus load|typewriter` 178/178.
- B: instructions per event unchanged (503). With LTO, `GateFunction(And)` over two inputs was already inlined and folded in the 93, and the 107's `Next()` loop visits one combination when its inputs are known. The 74LS90 variant was not tried.
- C: the BW-80's latch `IC5` closes and opens every character (`LE` from the load pulse), so `D` sleeps only between loads. The skipped evaluations were the cheapest kind (`LE` low, nothing driven).
- ADD_PART: the Quiet Pins rule names the 74LS244 (the review's note). The rule's rewording for a closed latch was not made, since C was dropped.
- Phase 4 (2026-11-16):
  - Trace check identical.
  - Screenshots viewed: `ghost_letter` (BW-2001, 0.37×, UI 2.3 ms) and `q_everywhere` (BW-80, 0.23×, UI 0.9 ms), pictures as before.
  - The full debug suite ran on A's code (1376/1376); nothing in `src` or `tests` changed since.

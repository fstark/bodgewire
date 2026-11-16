# P5y BW-80 Puzzles — Implementation Plan

Spec: [BW80_PUZZLES.md](BW80_PUZZLES.md). Design: [BW80_PUZZLES_DESIGN.md](BW80_PUZZLES_DESIGN.md). Machine: [BW80.md](../machines/140-bw80/BW80.md), [BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md).

Implemented 2026-11-16; user check pending.

Every phase ended with its fence: `cmake --build --preset debug` and `--preset release` clean, `clang-format` on the C++ files touched, `ctest --preset debug -j 12 -R "bw80|manuals|puzzle"` and `ctest --preset release -j 12 -R "bw80"` green, each new Puzzle's symptom seen with `bw run --tv` or `bw_ui --screenshot`, one commit of this step's own paths naming the tests run.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `3fffc75` |
| 1 | Rig | `49ad5f3` |
| 2 | Picture Puzzles (1–7) | `e5bac3b` |
| 3 | Keyboard and sync Puzzles (8–12) | `2915849` |
| 4 | Dead Puzzles (13–15) | `c1df945` |
| 5 | Finish | this commit |

## Implementation notes

- Candidates tried before the docs (headless, `bw run --tv`): the spec's "Found while trying candidates" lists what was dropped and why.
- Docs review (before the docs commit), fixed: LET is on K, not on `A13`'s row; the user manual and BW80.md had PRINT on P and LET on O (the ROM and the key table: O and K), fixed in phase 3; phase 1 is the rig alone (a test file with no case does not build); the typed Puzzles' screenshots need `--after 25s`; `BottomHalvesCrack` in phase 2; the TV's Nets by connector pin; `KBD_N`'s junction is a via; the Still starting check spelled out; seeds 2–3 for every Puzzle.
- Phase 1: the rig as designed; `Bw80TestRomDirectory` no longer removes the shared directory (two release cases run in parallel use it: the 450 ns case failed under `ctest -j` once the Puzzle cases joined). Release `bw80` 17/17, debug fence green.
- Phase 2: all seven as designed. The page a Puzzle's picture should show is built from the display file's codes through the fault's glyph set, but the cells past a row's end stay blank whatever the chain does to a space: no code is loaded there (the first page for Quotes for spaces drew them as quotes and failed); only Q everywhere fills them, with the HALT's glyph. The cursor under Quotes for spaces is an inverse `L`, as the mapping says (seen at 10×). Seeds 2 and 3 give the same pictures. Release 6–28 s a case; debug `every Puzzle builds` 4 s for seven. Release `bw80` 26/26; debug `bw80|manuals|puzzle` 38/38 in 122 s.
- Phase 3: all five as designed. No PRINT's edit line after `PRINT 2+2` NEW LINE reads `2+s2l` (the syntax error marker before the last 2, then the `L` cursor). `PinNet` on a resistor's pad gives nothing useful, so Eight keys checks the column at the connector reads high (its pull-up kept) instead of naming `R15`. The user manual's and BW80.md's keys corrected (PRINT on O, LET on K). Seeds 2 and 3 as seed 1. `VerticalStripesCrack` declared here with the other three (phase 4 uses it). Release `bw80` 31/31 in 35 s; debug `bw80|manuals|puzzle` 43/43 in 129 s.
- Phase 4: the three as designed but White screen's clue: the memory side of the bus (`MD3`) sits still on seed 1 once the CPU is stuck, so the clue is `D3` low and still at `IC1` while `CLK` runs (the brief and the spec say so now); a white frame still has about 900 ink points (the frame's edge lines), the case allows 1500. Still starting's pattern: over 2000 ink points, unlocked, every word of `IC3` with bit 3 clear, seeds 1–3. Release `bw80` 34/34 in 33 s.
- Phase 5: Machine docs (BW80.md Puzzles, BW80_DESIGN.md's Puzzle table, the crack positions and the first key after a report), ADD_PUZZLE (bridges on chips only, a Trace's ends and junction vias from the segments, probing both sides of a crack, a seed-dependent misbehaviour is out, a chain fault on a CPU-drawn picture against the predicted page, the first key after a report), the roadmap row. `bw_ui` screenshots of all fifteen viewed against their briefs (the typed ones with `--after 25s`, the keyboard ones `--after 12s`). Full debug suite 1359/1359 in 757 s; release `bw80` 34/34.
- User check (pending): `bw_ui puzzles/<file>.bwp` for one of each kind: `q_everywhere` (type a line and LIST), `eight_keys` against `same_eight_keys` (mend finds nothing on the second; probe `IC10` pin 12's leg while holding X), `rolling_screen`, `still_starting` (the power-on pattern for ever), `vertical_stripes` (probe `ROM_CS1` at `IC6` pin 7 and `IC2` pin 20).

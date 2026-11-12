# P4q Shift-register TV typewriter — Implementation Plan

Design: [SHIFT_TYPEWRITER_DESIGN.md](SHIFT_TYPEWRITER_DESIGN.md)
Spec: [SHIFT_TYPEWRITER.md](SHIFT_TYPEWRITER.md); Machine: [TYPEWRITER.md](../machines/090-typewriter/TYPEWRITER.md), [TYPEWRITER_DESIGN.md](../machines/090-typewriter/TYPEWRITER_DESIGN.md)

Done 2026-11-12, user check pending.

Every phase ended with its fence: `cmake --build --preset debug` clean (`-Werror`), `clang-format` on the C++ files touched, `ctest --preset debug -j 12` green (`[slow]` cases included unless the phase says otherwise), then one commit with the message given. Only this step's own paths are committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | `EvalContext::randomBit`, DIP-8 package | `1d5a1a6` |
| 2 | 2524 Part | `f9948c9` |
| 3 | 2518 Part | `411c3b8` |
| 4 | Netlist stage 1: video from the page | `d14e18c` |
| 5 | Netlist stage 2: keyboard, writes, cursor, Clear | `452af51` |
| 6 | Manuals | `bc291a0` |
| 7 | Board (user routing), per-Machine loops | `2eb59e7` |
| 8 | Session test, release check, recipes, docs | this commit |

## Implementation notes

- Docs review (before phase 1), fixed: the stale P4p Board moved out before the new netlist; the DIP-8 placer case with the 2524; MOS no-connect supply pins; the 2524's output moving only on `PHI1`; `ROW_CLK` = NAND(`QC`, `ACTIVE`) (a 2 ns hazard at dot 12 gone); the 2518 pin `REC`; the strobe margin (24.2 ms of 25 ms) and the one asynchronous key race stated; the blink screenshots 8 frames apart.
- Phases 1–3: full debug suite 775 then 790 tests green.
- Phase 4: release speed 0.67× with the page clocks decoded from `QA` (dots 11 and 13), P4p's final netlist 0.63× back to back on the same (loaded) machine. Decoding `PHI2_N` from `QC`, `QB_N` (dots 12–13) and `PHI1_N` from `DOT_END` (dot 15) gave 7% fewer evaluations, 0.73×: adopted, docs updated (cell 0, not 511, at the page's output after a frame). The stage-1 `.place` held only `J1`, `J2`, `U23` (the others came with stage 2). The phase-4 commit left `manuals: every Machine's manuals pass their checks` red (P4p's Service Manual names stage-2 parts), green again from phase 5.
- Phase 5: every case passed first time. VCD of `--press SW1:1ms:30ms --type 'J3:40ms:HI' --time 200ms`: shortest levels `PHI1_N` 247 ns, `PHI2_N` 498 ns, `ROW_CLK` 499 ns (no glitch); `WR` rises once a key, 1.06 ms after `H`'s strobe and 19 ms after `I`'s (waiting for its cell). Release headless, alternated with P4p's final netlist on a quieter machine: random page 0.97–0.98× (P4p 0.94–0.95×), after Clear 1.21× (P4p 1.25–1.29×: the row buffer clocks every character on a blank page too). `bw_ui` without Board or instruments 1.00×, UI 0.2 ms. Screenshots at 2 s and 2.157 s: `HELLO` with the block off and on.
- Phase 7: `bw place` gives a 4 × 10 grid, 7.24 m of wire (P4p's RAM version: 4 × 9, 7.00 m); the six 2524s and the 2518 annealed into a group at the left, beside the 2513; `place_drc_typewriter` passes. `bw export-kicad` wrote `boards/typewriter/typewriter.net`. Routed by the user: `bw check-board` prints `ok`; KiCad DRC 0 unconnected pads, one silkscreen warning (`U23`'s reference against the back edge, as on P4p). The tests build from the Board; the typewriter joins the per-Machine loops.
- Phase 8: `session: typewriter with every instrument` green. Release `bw_ui netlists/typewriter.bwn --press SW1:300ms:400ms --type 'J3:500ms:HELLO, WORLD\nTV TYPEWRITER' --after 4500ms` on the routed Board: 1.00×, UI 1.2 ms with 8 clips, 2 hooks, the probe and the meter (0.9 ms without); the screenshot shows the two lines, the block after `TV TYPEWRITER`, the keyboard window. Recipes: ADD_PART (2524, 2518, DIP-8 references; chips without `GND`; dynamic storage), ADD_MACHINE (090 references; clocks from the dot counter; the keyboard strobe).
- After the user's first check (2026-11-12): a `PHI2_N` crack after `U16` made the page flicker between pairs of characters (the first model went random every 10 ms). Changed with the user: a fade once to the level of the last clock phase, then held; `EvalContext::randomBit` removed. Checked with the 2524, typewriter, catalogue, manuals and simulator cases only (the change reaches nothing else).
- User check pending: Clear, type a few lines in `bw_ui netlists/typewriter.bwn`, Return, a full line wrapping, the blinking block; then probe a page clock (`U15` pin 6 or 8) and watch the page go when it is cut (Cut tool) and come back random after Mend.

# P4q Shift-register TV typewriter — Implementation Plan

Design: [SHIFT_TYPEWRITER_DESIGN.md](SHIFT_TYPEWRITER_DESIGN.md)
Spec: [SHIFT_TYPEWRITER.md](SHIFT_TYPEWRITER.md); Machine: [TYPEWRITER.md](../machines/090-typewriter/TYPEWRITER.md), [TYPEWRITER_DESIGN.md](../machines/090-typewriter/TYPEWRITER_DESIGN.md)

Every phase ends with its fence: `cmake --build --preset debug` clean (`-Werror`), `clang-format` on the C++ files touched, `ctest --preset debug -j 12` green (`[slow]` cases included unless the phase says otherwise), then one commit with the message given. Only this step's own paths are committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | `EvalContext::randomBit`, DIP-8 package | `1d5a1a6` |
| 2 | 2524 Part | `f9948c9` |
| 3 | 2518 Part | `411c3b8` |
| 4 | Netlist stage 1: video from the page | `d14e18c` |
| 5 | Netlist stage 2: keyboard, writes, cursor, Clear | |
| 6 | Manuals | |
| 7 | Board (user routing), per-Machine loops | |
| 8 | Session test, release check, recipes, docs | |

## Phase 1 — `randomBit`, DIP-8

- `EvalContext::randomBit` and its test (design §2).
- DIP-8 footprint, `kFootprints`, `IsGridPackage`, the footprint and footprint-table tests (design §3; the placer case comes with the 2524).
- Fence. Commit `sim, board: random bit in evaluate, DIP-8 package`.

## Phase 2 — 2524

- `src/parts/shift_register_2524.{hpp,cpp}`, delays, catalogue row, `datasheets/2524.md` (design §4); `catalogue_test.cpp` `kMosSignals` and `kNoGround` (design §6); `placer: DIP-8 sits in the chip grid` (design §3).
- `tests/parts/shift_register_2524_test.cpp` (spec, Tests): a bench of one chip with a `Stimulus` per input, clock pairs as the Machine makes them (`PHI1` low 250 ns, then `PHI2` low 250 ns, 1.5 µs a pair). The 512-pair round trip runs 0.8 ms; the decay cases 9 ms and 11 ms of stopped clocks.
- Fence. Commit `parts: 2524 dynamic recirculating shift register`.

## Phase 3 — 2518

- `src/parts/shift_register_2518.{hpp,cpp}`, delay, catalogue row, `datasheets/2518.md` (design §5), `kMosSignals`, `kNoGround`.
- `tests/parts/shift_register_2518_test.cpp` (spec, Tests).
- Fence. Commit `parts: 2518 hex 32-bit shift register`.

## Phase 4 — Netlist stage 1

- The P4p Board files out of the way and the new `typewriter.place` (design §9), before the netlist changes.
- `netlists/typewriter.bwn` stage 1 (design §6), header comment per ADD_MACHINE step 2.
- `tests/machines/typewriter_test.cpp` reworked (design §7), the stage-1 cases only (the stage-2 cases of P4p removed in this phase, back in phase 5).
- Release speed, `bw run netlists/typewriter.bwn --time 2s`, recorded below with P4p's stage-1 figure (1.09×). Far below (under 0.8×), stop and tell the user.
- A first look: `build/release/src/ui/bw_ui netlists/typewriter.bwn --screenshot /tmp/typewriter.png --after 1s`, viewed: a page of random characters, 32 × 16, locked.
- Fence. Commit `typewriter: netlist stage 1, video from the shift-register page`.

## Phase 5 — Netlist stage 2

- The final netlist (TYPEWRITER_DESIGN.md § Netlist word for word); the stage-2 cases; the picture case accepting the cursor (design §7).
- `bw run netlists/typewriter.bwn --press SW1:1ms:30ms --type 'J3:40ms:HI' --time 200ms --record PHI1_N,PHI2_N,ROW_CLK,WRITE,GO,WR,KEY --vcd /tmp/typewriter.vcd`: by eye, `PHI1_N`/`PHI2_N` in bursts of 32 on top lines, `ROW_CLK` 32 a text line, no glitch on any of them; `WRITE` high at one `PHI2_N` rise per key; `WR` steps once a key.
- Speed, random page and after `--press SW1:1ms:30ms`, recorded with P4p's (0.95× / 1.31×); a screenshot after `--press SW1:300ms:400ms --type 'J3:500ms:HELLO'` at `--after 2s` and `2157ms` (8 frames apart: one blink state each), viewed: `HELLO` top left, the block after it, on and off.
- Fence. Commit `typewriter: netlist stage 2, keyboard, writes and cursor on the page`.

## Phase 6 — Manuals

- `manuals/typewriter/user.md`, `service.md` rewritten (TYPEWRITER_DESIGN.md § Manuals).
- Fence: manuals cases green. Commit `typewriter: manuals for the shift-register page`.

## Phase 7 — Board

- `boards/typewriter/typewriter.place` (TYPEWRITER_DESIGN.md § Board); the old files handled as design §9; `build/release/src/cli/bw place netlists/typewriter.bwn boards/typewriter/typewriter.place -o boards/typewriter/typewriter.kicad_pcb`; grid and wire length recorded; reconfigure so `place_drc_typewriter` appears and passes; `bw export-kicad` writes `boards/typewriter/typewriter.net` for the user.
- **Stop for the user**: route `boards/typewriter/typewriter.kicad_pro` in KiCad (KICAD_GUIDE.md steps 7–8: F.Cu/B.Cu only, no zones, DRC 0 errors). No routing constraints (no Puzzles yet). Placement is not theirs to change.
- After: `bw check-board netlists/typewriter.bwn boards/typewriter/typewriter.kicad_pcb` prints `ok`; `drc_typewriter` passes; `typewriter` in the per-Machine loops and a `typewriter matches its netlist` case (ADD_MACHINE step 3); the tests build from the Board (design §9).
- Fence: everything green. Commit `typewriter: Board` (only `typewriter.place`, `.kicad_pro`, `.kicad_pcb` and the test changes; `typewriter.place` is first committed with phase 4).

## Phase 8 — Session test, release check, docs

- `session: typewriter with every instrument` (design §8).
- Release check (design §11): ratios and UI time recorded, the PNG viewed.
- Recipes: ADD_PART, ADD_MACHINE (design §10).
- ARCHITECTURE row P4q done with the date and links; GAME_DESIGN's 090 row (it says "video RAM") checked against what was built; this plan cut to the status table and implementation notes, "user check pending".
- Fence: everything green. Commit `docs: P4q implemented, user check pending`.

## Implementation notes

- Docs review (before phase 1), fixed: the stale P4p Board moved out before the new netlist; the DIP-8 placer case with the 2524; MOS no-connect supply pins; the 2524's output moving only on `PHI1`; `ROW_CLK` = NAND(`QC`, `ACTIVE`) (a 2 ns hazard at dot 12 gone); the 2518 pin `REC`; the strobe margin (24.2 ms of 25 ms) and the one asynchronous key race stated; the blink screenshots 8 frames apart.
- Phases 1–3: full debug suite 775 then 790 tests green.
- Phase 4: release speed 0.67× with the page clocks decoded from `QA` (dots 11 and 13), P4p's final netlist 0.63× back to back on the same (loaded) machine. Decoding `PHI2_N` from `QC`, `QB_N` (dots 12–13) and `PHI1_N` from `DOT_END` (dot 15) gave 7% fewer evaluations, 0.73×: adopted, docs updated (cell 0, not 511, at the page's output after a frame). The stage-1 `.place` held only `J1`, `J2`, `U23` (the others came with stage 2). The phase-4 commit left `manuals: every Machine's manuals pass their checks` red (P4p's Service Manual names stage-2 parts), green again from phase 5.
- Phase 5: every case passed first time. VCD of `--press SW1:1ms:30ms --type 'J3:40ms:HI' --time 200ms`: shortest levels `PHI1_N` 247 ns, `PHI2_N` 498 ns, `ROW_CLK` 499 ns (no glitch); `WR` rises once a key, 1.06 ms after `H`'s strobe and 19 ms after `I`'s (waiting for its cell). Release headless, alternated with P4p's final netlist on a quieter machine: random page 0.97–0.98× (P4p 0.94–0.95×), after Clear 1.21× (P4p 1.25–1.29×: the row buffer clocks every character on a blank page too). `bw_ui` without Board or instruments 1.00×, UI 0.2 ms. Screenshots at 2 s and 2.157 s: `HELLO` with the block off and on.

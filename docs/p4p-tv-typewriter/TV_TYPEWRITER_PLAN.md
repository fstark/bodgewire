# P4p TV Typewriter — Implementation Plan

Design: [TV_TYPEWRITER_DESIGN.md](TV_TYPEWRITER_DESIGN.md)
Spec: [TV_TYPEWRITER.md](TV_TYPEWRITER.md); Machine: [TYPEWRITER.md](../machines/090-typewriter/TYPEWRITER.md), [TYPEWRITER_DESIGN.md](../machines/090-typewriter/TYPEWRITER_DESIGN.md)

Every phase ends with its fence: `cmake --build --preset debug` clean (`-Werror`), `clang-format` on the C++ files touched, `ctest --preset debug -j 12` green (`[slow]` cases included unless the phase says otherwise), then one commit with the message given. Only this step's own paths are committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Netlist stage 1: video from the RAMs | |
| 2 | Netlist stage 2: keyboard, writes, cursor counter, Clear | |
| 3 | Netlist stage 3: the cursor on the screen | |
| 4 | Manuals | |
| 5 | Board (user routing), per-Machine loops | |
| 6 | Session test, release check, recipes, docs | |

## Phase 1 — Video from the RAMs

- `netlists/typewriter.bwn` stage 1 (design §2), header comment per ADD_MACHINE step 2.
- `tests/machines/typewriter_test.cpp` with the helpers and the stage-1 cases (design §3); `tests/CMakeLists.txt`.
- Release speed on an idle machine: `build/release/src/cli/bw run netlists/typewriter.bwn --time 2s`, recorded below (expected about 1.0×, the random page; below 1× accepted by the user, spec). Far below (under 0.8×), stop and tell the user.
- A first look: `build/release/src/ui/bw_ui netlists/typewriter.bwn --screenshot /tmp/typewriter.png --after 1s` (no Board: the TV alone), view the PNG: a page of random characters, 32 × 16, locked.
- Fence as above. Commit `typewriter: netlist stage 1, video from the RAMs`.

## Phase 2 — Keyboard, writes, cursor counter, Clear

- Stage 2 of the netlist (design §2); the stage-2 cases (design §3).
- `bw run netlists/typewriter.bwn --press SW1:1ms:30ms --type 'J3:40ms:HI' --time 300ms --record WE_N,WEK_N,WR,KEY,LINE_END --vcd /tmp/typewriter.vcd`: by eye in the VCD, `WEK_N` low twice a writing line, no glitch on it.
- Speed again, random page and after `--press SW1:1ms:30ms` (typed page expected about 1.3×), recorded.
- Fence as above. Commit `typewriter: netlist stage 2, keyboard and cursor counter`.

## Phase 3 — The cursor on the screen

- Stage 3, the final netlist (TYPEWRITER_DESIGN.md § Netlist word for word); the stage-3 cases and the stage-1 picture case taking the cursor (design §3).
- Speed recorded; a screenshot after `--press SW1:300ms:400ms --type 'J3:500ms:HELLO'`, viewed: `HELLO` top left and the block after it (take two, `--after 2s` and `2200ms`, to see it on and off).
- Fence as above. Commit `typewriter: netlist stage 3, blinking cursor`.

## Phase 4 — Manuals

- `manuals/typewriter/user.md`, `service.md` (TYPEWRITER_DESIGN.md § Manuals); `typewriter` in `tests/machines/manuals_test.cpp`.
- Fence: manuals cases green. Commit `typewriter: manuals`.

## Phase 5 — Board

- `boards/typewriter/typewriter.place` (TYPEWRITER_DESIGN.md § Board), then `build/release/src/cli/bw place netlists/typewriter.bwn boards/typewriter/typewriter.place -o boards/typewriter/typewriter.kicad_pcb`; record the grid and wire length; reconfigure so `place_drc_typewriter` appears and passes; export the `.net` if the user wants it (`bw export-kicad`).
- **Stop for the user**: route `boards/typewriter/typewriter.kicad_pro` in KiCad (KICAD_GUIDE.md steps 7–8: F.Cu/B.Cu only, no zones, DRC 0 errors). No routing constraints (no Puzzles yet). Placement is not theirs to change.
- After: `bw check-board netlists/typewriter.bwn boards/typewriter/typewriter.kicad_pcb` prints `ok` (an unrouted connection left by Freerouting: ADD_MACHINE step 3); `drc_typewriter` passes; `typewriter` in the per-Machine loops and a `typewriter matches its netlist` case (ADD_MACHINE step 3); the tests build from the Board (design §5).
- Fence: everything green. Commit `typewriter: Board` (only `typewriter.place`, `.kicad_pro`, `.kicad_pcb` and the test changes).

## Phase 6 — Session test, release check, docs

- `session: typewriter with every instrument` (design §4).
- Release check (design §7): ratios and UI time recorded, the PNG viewed.
- Recipes: ADD_MACHINE (design §6).
- ARCHITECTURE row P4p done with the date and links; GAME_DESIGN's 090 row checked against what was built; this plan cut to the status table and implementation notes, "user check pending".
- Fence: everything green. Commit `docs: P4p implemented, user check pending`.

## Implementation notes

- Docs review (before phase 1), on the design netlist run in a scratch copy: it loads without warnings, keys, Return, Backspace and the line wrap land as designed, the cursor block is one cell, the text starts at TV column 34. Fixed: the speed (1.0× headless on a full page; the 1.3× gate and the 1.00× target dropped with the user's agreement, spec); `U36` counts on `V8` and `LINE_END` is re-timed in `U21` (both were clean only through the simulator's inertial delays); `--vcd` in phase 2; `--after 4500ms`; `LitOutside` one dot wider for a cursor in column 31; `LockedFrames` on a running simulator; the Session test's key checked as accepted only; stage-1 ties named; debug times stated; the 2 ns write-end margin noted.

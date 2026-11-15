# P5s Z80 Part — Implementation Plan

Design: [Z80_DESIGN.md](Z80_DESIGN.md)
Spec: [Z80.md](Z80.md)

Implemented 2026-11-15 on branch `p5s-z80` (a worktree), while P5q ran on `main`; rebased on `main` at P5q's `54d13c3` before the finish.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `2ea0360` |
| 1 | Test data: converter, fetch script, committed subset | `281db54` |
| 2 | Core: every opcode, the runner, the Harte subset | `7bea0d5` |
| 3 | Core: interrupts, HALT, reset; hand cases; full Harte, ZEXDOC, ZEXALL in release | `061cd10` |
| 4 | `Z80` Part, datasheet, catalogue | `f8b9940` |
| 5 | Bus load with the Z80: ROM, netlist, test, ratios | `ccec7a5` |
| 6 | Release check, recipes, roadmap | this commit |

## Notes

- Review (before commit of the docs), fixed in them: `step()` and the INT sampling made one rule (inputs at the cycle's last rising edge, `step()` there or at the later data edge), internal T-states as their own cycles, bus request and reset in the Part only; `RFSH_N` not tri-state, reset floats only A and D with the controls high; power pins `Ttl`; `NMI_N` read at every rising edge; RAM read with `Ram2114::word`; the pulse rule per pin; file names with `_`; Harte's sampling written in; hand cases for the prefix chains and `ED` holes Harte lacks; the bus-load tail invariant and `--check`; the wake test in the 6502's shape; the CMake work in its phases; `python3` for the scripts.
- The core, the runner and the data were written while the review ran (phases 1–2 code), then checked against its findings. Release: the Harte subset passes (0.5 s); the full Harte set passes (1,604,000 cases, 4.7 s); ZEXDOC and ZEXALL pass, 46,734,977,142 T-states each, 44 s and 46 s. Fixes found by the data: Q cleared by a `DD`/`FD` prefix (`dd 37`), WZ = PC + 1 on a repeating `INIR`/`OTIR` family (`ed b2`).
- Phase 2: the core was written whole (interrupts, HALT and `reset()` included); phase 3 adds their hand cases. Debug: the Harte subset 2.7 s (the estimate of 60–90 s was far off: the runner reuses its RAM and the core is small per cycle).
- Phase 3 mutations of the core, each failing a hand case: the EI shadow ignored; IM 0 counting PC; NMI on its level (the first try survived: the case gave NMI once, not at every cycle as the Part does; fixed); no P/V loss; R's bit 7 counted; a halted fetch counting PC; `DD ED` keeping the `DD` (the first try survived: `ED 4B` loads BC whatever the prefix; now `ED 6B`); an `ED` hole running `NEG`; an interrupt taken after a prefix; IM 2 clearing the vector's bit 0.
- Release, phase 3: `Z80 core: Harte full` 4.7 s; `ZEXDOC` and `ZEXALL` "Tests complete" with every test OK, 46,734,977,142 T-states each, 44 s and 46 s.
- Phase 4: `kZ80Float` 90 ns and `kZ80Busak` 100 ns are the Z80A's usual published maxima, not read from a scan of UM0080 here (said in `delays.hpp`). The reset-to-first-fetch count was not checked against UM0080's Reset figure either: the model starts T1 of the fetch at `$0000` at the second rising edge after `RESET_N` goes high (the first sees it high). Both are open for a check against the manual.
- Phase 4, the bench: a write ends at the first of its strobes to rise (`WR_N` before `MREQ_N`, `IORQ_N` before `WR_N` by the data sheet's delays), so `BenchMemory` stores at `WR_N`'s rise and records an `OUT` at `IORQ_N`'s. "Reads before the edge" probes the timing on the same circuit as it measures (a Stimulus on the data, its step far away): a tied bus draws other power-on bits, so other timing. Debug: `ctest -R "Z80|catalogue|manuals|footprint"` 80 tests, 4.3 s with `-j 12`.
- Phase 5, the program against the design's first sketch: the handler is on the ZX80's balanced pattern (`DEC C`, `JP NZ`; `POP`, `DEC B`, `RET Z`, `SET 3,C`, or `POP`, `RET Z`, `JR`), 54 T-states on both paths, not 42; so a line has 35 Fetches after `EI` (one halted Fetch on a display line, not four) and A = `$5D`. Border rows are runs of `HALT`s in RAM around the page (a `HALT`'s pushed PC is the next row), so one `JP (HL)` serves both. The design was updated to the program as built. The page came up on the first run, 3.7 µs right of the stub's (71 T-states from HSYNC to the first character, the stub's 59).
- Phase 5, the TV lock: with the 100 ms cap no locked frame came. The Z80's first VSYNC comes after the POR and the copy (6.2 ms), and the TV (free-running at 21 ms, accepting a vertical sync only 16–21 ms into its frame) falls in step after about 6 frames; the stub's first VSYNC is at once. The Z80 netlist's cap is 250 ms rather than a POR tuned to the TV.
- Phase 5, debug: `bus load: the TV shows the page` 80 s (both netlists, both seeds), the copy case 1.3 s. Release, `bw run --time 2s`, alternated, 3 runs each: `bus_load` 0.383–0.389× (239 M applied events, 228 M evaluations), `bus_load_z80` 0.428–0.431× (236 M applied events, 221 M evaluations): the real CPU is no dearer than the stub, slightly cheaper (its border lines are halted fetches where the stub played ROM and RAM cycles). `bw_ui netlists/bus_load_z80.bwn --after 1s`: 0.43×, UI 0.0 ms, the page on the TV (screenshot viewed).
- Phase 6: rebased on `main` at P5q's `54d13c3` with no conflict. Full debug suite: 1,218 of 1,219 pass (770 s); the one failure, `simulator: a strong 0 beats a pull-up pin's 1; its own 0 is strong`, comes from P5q's `3bed4a9` (a port's low now loses to an output's high, the test still expecting X) and touches nothing of P5s's; left to the P5q session. Release on the rebased tree, alternated, 3 runs each: `bus_load` 0.387–0.390×, `bus_load_z80` 0.432–0.436×; `bw_ui netlists/bus_load_z80.bwn --after 1s` 0.43×, UI 0.0 ms, the page (screenshot viewed).

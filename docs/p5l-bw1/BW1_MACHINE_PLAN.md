# P5l BW-1 — Implementation Plan

Spec: [BW1_MACHINE.md](BW1_MACHINE.md). Design: [BW1_MACHINE_DESIGN.md](BW1_MACHINE_DESIGN.md). Machine: [BW1.md](../machines/120-bw1/BW1.md), [BW1_DESIGN.md](../machines/120-bw1/BW1_DESIGN.md).

Implemented 2026-11-15; user check pending.

Every phase ended with its fence: `cmake --build --preset debug` clean, `clang-format` on the C++ files touched, the tests the phase named green in debug, one commit of this step's own paths.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Lamps with two cathodes, `DIGITCC` | `e88bc09` |
| 2 | `SLIDE` | `e88bc09` (with phase 1) |
| 3 | Netlist, manuals, fault-free tests | `232d7fe` |
| 4 | Board (user routes) | `e42142a` (writer fix `e8dfffa`) |
| 5 | Session test, release check | `eea0505` |
| 6 | Recipes, docs, full suite | this commit |

## Implementation notes

- Docs review (before the docs commit), fixed: `DIGIT-CC` is not a part name (`IsPartName`: letters and digits): `DIGITCC`; a Bodge Wire to an unconnected pad joins no Net (`ElectricalNetlist` builds from connections): the strap pin on a one-pin Net `STRAP`, and the netlist-level strap test rewrites that line; the SST case's second step shows RAM's byte at `$0204`; the strap test times counted from power-on (POR 20 ms); `--press` needs a unit and a start after 0 (`1ms`); the slide's click reuses `latchToggle_`, clears `pressed_`, and the keycap test; no default PC key for a one-character slide legend, the legend error names the Part; the Session Clips named; `manuals` in the early fences; the footprint test named; debug costs; 16 grid chips; three `NMI_N` Drivers; `X1` in the manual's order. The Session case for `DIGITCC` alone dropped: a Session's lamps need a Board.
- Drafted while the review ran: phases 1–3 code and the Board placement.
- Phases 1 and 2: one commit, as `catalogue.cpp` carries both; fence 278 green. A one-pin Net (`STRAP`) gives no builder warning.
- Phase 3: every case passed first time but the SST one, whose expectation was wrong (fixed in the docs review). Release headless `bw run --time 2s`: **2.55×** idle, **2.53×** with key `0` held (`--press SW1:1ms:2s`). Debug: reset 4 s, RS 9 s, typing cases 18–31 s, the strap case 83 s. Manuals: pin names (`RS0`, `CS1`) and the undriven `A13`–`A15` are not backticked (unknown references).
- Phase 4: `bw place` gave 8.74 m on an 8 × 3 grid. The user laid the Board out after the KIM's own (chips turned, `X1` where the KIM's crystal is; the 74LS244 and POR, which the KIM lacks, placed freely) and routed it, so the committed Board is not what `bw1.place` gives (`place_drc_bw1` checks the rules' own Board). Freerouting left `SEG_C` open between `DS2` and `DS3`: three B.Cu segments added under the digits at y 162.5. KiCad DRC 0 errors, 0 unconnected (48 silkscreen warnings); `check-board` ok. The user saw stray magenta crosses: KiCad 10 footprint points, which KiCad keeps in Board coordinates but `bw place` wrote relative (fixed in the writer, separate commit; the 21 in this Board moved back to their parts' centres). The Machine tests run on the Board's Circuit; the Bodge Wire case (U24 pad 4 to `U2` pad 2) shows `888888` at 300 ms. Fence 149 green.
- Phase 5: `session: bw1 with every instrument` green: 48 lamps, not 42 (built from the Board, each unconnected decimal point has its pad's own Net), 19 + 32 + 4 + 1 + 1 = 57 watched Nets. Release `bw_ui netlists/bw1.bwn` with 32 Clips (bus Trigger `1F1F`), 4 Hooks, probe on `ROW0`, meter on `PA0`: **1.00×, UI 1.0 ms** (0.8–0.9 ms without instruments). Screenshots viewed: the keypad's caps, `0` held (cap down, `2000` shown), SST at ON, the self-test after `--repair wire:U24.4:U2.2` with GO held (`PA5513`). The digits first drew nearly dark: each is lit a sixth of the time and the Board drew brightness linearly. Digit segments now go through a cube-root perception curve (`Perceived` in `board_view.cpp`); round LEDs stay linear, so the Heartbeat's half-lit address LEDs still read as half (a curve on them made every LED look fully lit). The stopwatch's steady digits are unchanged. Left for P5n: the reference and part name drawn over the digit's glyph; the slide switch is small at Board zoom.
- After phase 5: the first marker fix in this Board was lost to a save from KiCad, which still had the Board open; redone with KiCad closed (`5fac110`). The same stray points moved back on the other Boards placed by `bw place` (tennis, message, typewriter, heartbeat, led_computer: 155 points, `7c7baa6`); the hand-placed Boards were right.
- Phase 6: full debug suite 1035/1035 in 548 s; the Board tests again after the marker fixes (115 green). Recipes: ADD_BOARD_DEVICE (two-cathode lamps, `DIGITCC`, `SLIDE`, the perception curve, digits turned 90°, the budget of multiplexed digits), ADD_MACHINE (BW-1 commits, the user may lay out the Board, keypad by `next`, a one-pin Net for a player's wire pad, `--press` syntax, footprint points, closing KiCad before an agent edit). GAME_DESIGN ladder row 120; ARCHITECTURE row P5l.
- User check pending: `bw_ui netlists/bw1.bwn`: type a program (`AD 0200 DA A9 + 42 + 85 + 10 + 00 AD 0200 GO`, then `AD 0010`), ST on a `4C 00 02` loop, SST at ON with GO, RS; the self-test by a Bodge Wire from `U24` pad 4 to any `PA0` pad, then RS; F2 and the PC keys.

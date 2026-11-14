# P5l BW-1 — Implementation Plan

Spec: [BW1_MACHINE.md](BW1_MACHINE.md). Design: [BW1_MACHINE_DESIGN.md](BW1_MACHINE_DESIGN.md). Machine: [BW1.md](../machines/120-bw1/BW1.md), [BW1_DESIGN.md](../machines/120-bw1/BW1_DESIGN.md).

Every phase ends with its fence: `cmake --build --preset debug` clean, `clang-format` on the C++ files touched, the tests the phase names green in debug, one commit of this step's own paths (the user edits docs in parallel).

## Phase 1 — Lamps with two cathodes, `DIGITCC`

Design §1, §2. ADD_BOARD_DEVICE output chain; ADD_PART (catalogue row, datasheet).

Fence: `ctest --preset debug -j 12 -R "led|digit|catalogue|session|counter|chaser|dice|stopwatch|heartbeat|manuals"` green (every Machine with lamps; `led` covers "led computer", `manuals` the datasheet check).
Commit: `parts: DIGITCC, a common-cathode digit; lamps with two cathodes`

## Phase 2 — `SLIDE`

Design §3. Footprint copied, `kFootprints`, catalogue row, datasheet, `SwitchLook`, the drawing and the click in the UI.

Fence: `-R "switch|catalogue|kicad|footprint|session|board|manuals"` green; `bw_ui` builds (release, so phase 5's screenshots can follow).
Commit: `parts: SLIDE, a slide switch moved by a click`

## Phase 3 — Netlist, manuals, fault-free tests

BW1_DESIGN Netlist, Manuals, Behaviour checked (all but the Bodge Wire case). `Bw1DigitChar` moved into `bw1_model.hpp`. `manuals_test.cpp` list. Release headless ratio idle and with a key held (`bw run … --time 2s`, `--press SW1:1ms:2s`), noted; debug time of each case measured, `[slow]` set from it.

Fence: `-R "bw1|manuals|asm"` green.
Commit: `bw1: netlist, manuals and fault-free tests`

## Phase 4 — Board (the user routes)

ADD_MACHINE §3. `boards/bw1/bw1.place`, `bw place`; fix the rules until it places; then stop: the user routes `boards/bw1/bw1.kicad_pro` (constraints in BW1_DESIGN, Board). After: `check-board` ok, the per-Machine loops, the tests on the Board's Circuit, the Bodge Wire case.

Fence: `-R "bw1|board|electrical|drc|place_drc|view"` green.
Commit: `bw1: Board, placed by bw place and routed by the user`

## Phase 5 — Session test, release check

BW1_DESIGN Per-Machine checks; BW1_MACHINE_DESIGN §6: the Session case, release ratios and screenshots (reset, a key held, SST on, the self-test after a Bodge Wire), the 110 and stopwatch screenshots unchanged.

Fence: `-R "session"` green; screenshots viewed.
Commit: `bw1: Session test and release check`

## Phase 6 — Recipes, docs, full suite

BW1_MACHINE_DESIGN §5. Full debug suite (shared code: lamps, Session, UI). Plan cut to the table and notes.

Fence: full `ctest --preset debug -j 12` green.
Commit: `docs: P5l implemented, user check pending`

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Lamps with two cathodes, `DIGITCC` | `e88bc09` |
| 2 | `SLIDE` | `e88bc09` (with phase 1) |
| 3 | Netlist, manuals, fault-free tests | `232d7fe` |
| 4 | Board (user routes) | `e42142a` (writer fix `e8dfffa`) |
| 5 | Session test, release check | this commit |
| 6 | Recipes, docs, full suite | |

## Implementation notes

- Docs review (before the docs commit), fixed: `DIGIT-CC` is not a part name (`IsPartName`: letters and digits): `DIGITCC`; a Bodge Wire to an unconnected pad joins no Net (`ElectricalNetlist` builds from connections): the strap pin on a one-pin Net `STRAP`, and the netlist-level strap test rewrites that line; the SST case's second step shows RAM's byte at `$0204`; the strap test times counted from power-on (POR 20 ms); `--press` needs a unit and a start after 0 (`1ms`); the slide's click reuses `latchToggle_`, clears `pressed_`, and the keycap test; no default PC key for a one-character slide legend, the legend error names the Part; the Session Clips named; `manuals` in the early fences; the footprint test named; debug costs; 16 grid chips; three `NMI_N` Drivers; `X1` in the manual's order. The Session case for `DIGITCC` alone dropped: a Session's lamps need a Board.
- Drafted while the review ran: phases 1–3 code and the Board placement.
- Phases 1 and 2: one commit, as `catalogue.cpp` carries both; fence 278 green. A one-pin Net (`STRAP`) gives no builder warning.
- Phase 3: every case passed first time but the SST one, whose expectation was wrong (fixed in the docs review). Release headless `bw run --time 2s`: **2.55×** idle, **2.53×** with key `0` held (`--press SW1:1ms:2s`). Debug: reset 4 s, RS 9 s, typing cases 18–31 s, the strap case 83 s. Manuals: pin names (`RS0`, `CS1`) and the undriven `A13`–`A15` are not backticked (unknown references).
- Phase 4: `bw place` gave 8.74 m on an 8 × 3 grid. The user laid the Board out after the KIM's own (chips turned, `X1` where the KIM's crystal is; the 74LS244 and POR, which the KIM lacks, placed freely) and routed it, so the committed Board is not what `bw1.place` gives (`place_drc_bw1` checks the rules' own Board). Freerouting left `SEG_C` open between `DS2` and `DS3`: three B.Cu segments added under the digits at y 162.5. KiCad DRC 0 errors, 0 unconnected (48 silkscreen warnings); `check-board` ok. The user saw stray magenta crosses: KiCad 10 footprint points, which KiCad keeps in Board coordinates but `bw place` wrote relative (fixed in the writer, separate commit; the 21 in this Board moved back to their parts' centres). The Machine tests run on the Board's Circuit; the Bodge Wire case (U24 pad 4 to `U2` pad 2) shows `888888` at 300 ms. Fence 149 green.
- Phase 5: `session: bw1 with every instrument` green: 48 lamps, not 42 (built from the Board, each unconnected decimal point has its pad's own Net), 19 + 32 + 4 + 1 + 1 = 57 watched Nets. Release `bw_ui netlists/bw1.bwn` with 32 Clips (bus Trigger `1F1F`), 4 Hooks, probe on `ROW0`, meter on `PA0`: **1.00×, UI 1.0 ms** (0.8–0.9 ms without instruments). Screenshots viewed: the keypad's caps, `0` held (cap down, `2000` shown), SST at ON, the self-test after `--repair wire:U24.4:U2.2` with GO held (`PA5513`). The digits first drew nearly dark: each is lit a sixth of the time and the Board drew brightness linearly. Digit segments now go through a cube-root perception curve (`Perceived` in `board_view.cpp`); round LEDs stay linear, so the Heartbeat's half-lit address LEDs still read as half (a curve on them made every LED look fully lit). The stopwatch's steady digits are unchanged. Left for P5n: the reference and part name drawn over the digit's glyph; the slide switch is small at Board zoom.

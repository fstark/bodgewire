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
| 1 | Lamps with two cathodes, `DIGITCC` | |
| 2 | `SLIDE` | |
| 3 | Netlist, manuals, fault-free tests | |
| 4 | Board (user routes) | |
| 5 | Session test, release check | |
| 6 | Recipes, docs, full suite | |

## Implementation notes

- Docs review (before the docs commit), fixed: `DIGIT-CC` is not a part name (`IsPartName`: letters and digits): `DIGITCC`; a Bodge Wire to an unconnected pad joins no Net (`ElectricalNetlist` builds from connections): the strap pin on a one-pin Net `STRAP`, and the netlist-level strap test rewrites that line; the SST case's second step shows RAM's byte at `$0204`; the strap test times counted from power-on (POR 20 ms); `--press` needs a unit and a start after 0 (`1ms`); the slide's click reuses `latchToggle_`, clears `pressed_`, and the keycap test; no default PC key for a one-character slide legend, the legend error names the Part; the Session Clips named; `manuals` in the early fences; the footprint test named; debug costs; 16 grid chips; three `NMI_N` Drivers; `X1` in the manual's order. The Session case for `DIGITCC` alone dropped: a Session's lamps need a Board.
- Drafted while the review ran: phases 1–3 code and the Board placement.

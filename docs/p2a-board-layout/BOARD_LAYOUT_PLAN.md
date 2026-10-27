# Board Layout via KiCad — Implementation Plan (P2a)

Design: [BOARD_LAYOUT_DESIGN.md](BOARD_LAYOUT_DESIGN.md)
Spec: [BOARD_LAYOUT.md](BOARD_LAYOUT.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | `LoadedCircuit` parts and connections | |
| 2 | `bw_board` module: Board model, S-expression reader | |
| 3 | KiCad netlist writer, `bw export-kicad` | |
| 4 | `.kicad_pcb` reader | |
| 5 | Copper islands | |
| 6 | Board check, `bw check-board` | |
| 7 | KiCad guide | |
| 8 | User layouts, integration tests, decision (needs the user) | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug --output-on-failure`

All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md); run `clang-format -i` on changed C++ files (never on CMakeLists.txt).

---

## Phase 1 — `LoadedCircuit` parts and connections

Design §3.1. `src/netlist/builder.hpp`: add `LoadedPart`, `Connection`, fields `parts`, `connections`. `src/netlist/builder.cpp`, `Builder::build()`: after the error check, fill `parts` from `netlist_.parts` (netlist order; `value` = resistor `value` parameter or the Part name) and `connections` from `pinNet_` (already ordered by reference, pin).

Tests in `tests/netlist/builder_test.cpp`: `"builder: parts in netlist order with values"`, `"builder: connections include implicit power and resistor pins"`.

Fence: tests pass; commit `"board: phase 1 — LoadedCircuit parts and connections"`.

## Phase 2 — `bw_board` module, Board model, S-expression reader

Design §1, §2 (`board.hpp`), §4 (`SExpr`), §7. Create `src/board/CMakeLists.txt` (only `sexpr.cpp` for now), `board.hpp`, `sexpr.hpp/.cpp`; root `CMakeLists.txt` `add_subdirectory(src/board)`; link `bw_board` into `bw_tests`; `.gitignore` entries.

`ParseSExpr`: tokens `(`, `)`, quoted strings (`\"`, `\\`, `\n` escapes), atoms up to whitespace or parenthesis; line of each node = line of its first token; exactly one top-level list; errors "unbalanced parentheses", "unterminated string", "expected one top-level list".

Tests `tests/board/sexpr_test.cpp`.

Fence: tests pass; commit `"board: phase 2 — module, Board model, S-expression reader"`.

## Phase 3 — KiCad netlist writer, `bw export-kicad`

Design §2 (`kicad_netlist.hpp`), §5.6, §3.2. `src/board/kicad_netlist.hpp/.cpp`; CLI subcommand `export-kicad <file.bwn> -o <file.net>` (netlist errors → exit 1, file not written). Link `bw_board` into `bw`.

Tests `tests/board/kicad_netlist_test.cpp`: footprint table; netlist with 74LS00 + R + OSC: R value, OSC NoConnect pins absent, VCC/GND nodes present, nets sorted, codes from 1, same UUIDs on two runs, output parses with `ParseSExpr`.

Fence: tests pass; `bw export-kicad netlists/test_pattern.bwn -o /tmp/tp.net` writes a file; commit `"board: phase 3 — KiCad netlist export"`.

## Phase 4 — `.kicad_pcb` reader

Design §5.1–5.3. `src/board/kicad_board.hpp/.cpp`: `ParseMillimetres`, `ReadKicadBoard`, `ReadKicadBoardFile`.

Tests `tests/board/kicad_board_test.cpp`: millimetre parsing (`"12.7"`, `"-0.000001"`, `"3"`); footprint at 0/90/180° with pad positions; pad shapes; roundrect corner radius; segment, arc, via; outline from `gr_rect` and `gr_line`; errors with lines: zone, In1.Cu layer, smd pad, copper `gr_line`, version 20240108, no outline.

Fence: tests pass; commit `"board: phase 4 — kicad_pcb reader"`.

## Phase 5 — Copper islands

Design §4 (`CopperShape`), §5.4. `src/board/copper.hpp/.cpp`: `FindIslands`.

Tests `tests/board/copper_test.cpp` (Boards built in code): Trace to pad joins; via joins Layers; Trace on the other Layer without a via joins only through through-hole pads; T-junction; arc; near miss stays separate; touching a pad edge joins; rotated rectangle pad; oval pad along its long axis.

Fence: tests pass; commit `"board: phase 5 — copper islands"`.

## Phase 6 — Board check, `bw check-board`

Design §5.5, §3.2. `src/board/board_check.hpp/.cpp`: `CheckBoard`. CLI `check-board <file.bwn> <file.kicad_pcb>`: prints Board read errors (`Format`) or problems, else `ok`.

Tests `tests/board/board_check_test.cpp`: ok, open, short, copper on unconnected pad, missing Component, extra Component, wrong footprint, missing pad.

Fence: tests pass; commit `"board: phase 6 — board check"`.

## Phase 7 — KiCad guide

Spec S5. `docs/p2a-board-layout/KICAD_GUIDE.md`. Commit `"docs: P2a KiCad guide"`.

## Phase 8 — User layouts, integration tests, decision

Needs the user. Layouts of `boards/counter` and `boards/test_pattern` following the guide; then `tests/board/boards_test.cpp` (tag `[board]`, `BW_BOARD_DIR`): both pass `CheckBoard`; fixture `tests/board/fixtures/counter_open.kicad_pcb` (one track removed) reports one open. Decision in [ARCHITECTURE.md](../ARCHITECTURE.md), Board authoring.

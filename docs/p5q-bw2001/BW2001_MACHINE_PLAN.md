# P5q BW-2001 — Implementation Plan

Spec: [BW2001_MACHINE.md](BW2001_MACHINE.md). Design: [BW2001_MACHINE_DESIGN.md](BW2001_MACHINE_DESIGN.md). Machine: [BW2001.md](../machines/130-bw2001/BW2001.md), [BW2001_DESIGN.md](../machines/130-bw2001/BW2001_DESIGN.md).

Every phase ends with its fence: `cmake --build --preset debug` clean, `clang-format` on the C++ files touched, the tests the phase names green in debug, one commit of this step's own paths with the message given. Machine and BW-2001 cases are `[slow]`; their release-only cases run in the release build at the phase that adds them.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Port drive strength | |
| 2 | `74LS139`, `MC3446`, passive connectors | |
| 3 | `MONITOR`, the TV's separate drives | |
| 4 | Switch contacts, `MATRIXKB`, Session, `--type` | |
| 5 | The keyboard window, capture, paste | |
| 6 | `grid pet` in `bw place` | |
| 7 | The editor's retrace wait | |
| 8 | Netlist stage 1: video | |
| 9 | Netlist stage 2: computer | |
| 10 | Netlist stage 3: edges, BASIC 1, manuals | |
| 11 | Board (user routes) | |
| 12 | Board tests, Session test, release check | |
| 13 | Recipes, docs, full suite | |

## Phases

### 1. Port drive strength (design §1)

`DriveStrength::Port`; `DriveOf` for `PullUp`; datasheets 6520, 6522, 6530; `logic_test` cases.

Fence: `ctest --preset debug -R "logic|resolve|6520|6522|6530|bw1|bw2001|bench|switch"`; the P5n bench's power-on note checked (VIA `PB5` against `X2` before `RES_N`: now 1, not X; nothing asserts on it). Commit `kernel: an NMOS port's low output loses to an output's high (P5q)`.

### 2. `74LS139`, `MC3446`, passive connectors (design §2–§4)

Parts, delays, catalogue rows, datasheets (`74LS139`, `MC3446`, `IEEE488`, `USERPORT`, `CASSETTE`), footprints (`PinHeader_2x12`, `1x06`; the `1x07` and `1x20` come in phases 3 and 4), `PartPinNames` needs nothing (no parameters).

Fence: `-R "139|3446|connector|catalogue|footprint|manual"`. Commit `parts: 74LS139, MC3446, IEEE-488, user port and cassette connectors (P5q)`.

### 3. `MONITOR`, the TV's separate drives (design §5)

`TvSync`, the drive setters, `TvConsumer`/`TvInput` constructors (`powerOn` keeps the mode), `tests/runner/tv_consumer_test.cpp` (new), the Session and `bw run --tv` finding `MONITOR`; `HDR-1x07` footprint.

Fence: `-R "tv|session|connector|catalogue|footprint|pattern|message|typewriter|ball|tennis"`. Commit `tv: separate drives and the MONITOR connector (P5q)`.

### 4. Switch contacts, `MATRIXKB`, Session, `--type` (design §6.1–§6.4)

`SwitchContact` (no behaviour change: every switch test and switch Machine as before), `MatrixKeyboard`, `Bw2001KeyFor`, the builder, `MatrixKeyboardOf`, Session, CLI; `HDR-1x20` footprint.

Fence: `-R "switch|matrix|keyboard|session|bw1|dice|catalogue|footprint|manual|bw2001 netlist"` (the P5p minimal netlist's 74 keys run on the contacts too). Commit `parts: the BW-2001 keyboard behind MATRIXKB, keys on switch contacts (P5q)`.

### 5. The keyboard window, capture, paste (design §6.5)

Layout (tested headless), window, capture generalised, paste. Checked by release screenshots on a small netlist with a `MATRIXKB` (the phase 4 test netlist) before the Machine exists, and again in phase 12; iterate until the caps read as the PET 2001's.

Fence: `-R "layout|keyboard|capture"`; screenshots viewed. Commit `ui: the BW-2001 keyboard window, PC typing by character, paste (P5q)`.

### 6. `grid pet` in `bw place` (design §7)

Rule, placer, writer angles, tests.

Fence: `-R "place|placer|board|drc|footprint"` (every Machine's `place_drc_*` unchanged). Commit `place: grid pet, chips at their PET locations (P5q)`.

### 7. The editor's retrace wait (design §8)

`bw2001.inc`, `bw2001_edit.asm` (and any print path in `bw2001_basic.asm`/`_kernel.asm` that stores to the screen outside the editor), the `.rom` files, the reset's VIA `PCR` = `$0C`, the model's `PB5`, its VIA reads not flagged, `snowAccesses()`.

Fence: `-R "bw2001|asm|basic"` (model cases and the minimal netlist's every-build cases); release: the minimal netlist's whole conformance set. Commit `bw2001: the editor writes the screen only in the retrace (P5q)`.

### 8. Netlist stage 1: video (Machine design, Netlist, stage 1)

`netlists/bw2001.bwn` with sheet 3, `X1`, `J7`, `J8`, the ties; `bw2001_screen`; stage 1 cases; `bw2001` in `manuals_test` only from phase 10 (no manuals yet; the manuals test lists Machines explicitly).

Machine test cases are named `bw2001 machine: stage N …` so the fences match. Fence: `-R "bw2001 machine"`; the debug ratio measured and recorded (a case that would run past about 10 minutes in debug becomes release-only); release ratios recorded in the notes (headless `--time 2s`, `bw_ui netlists/bw2001.bwn` without a Board: the TV and the ratio). Commit `bw2001: the video stage of the PET 2001 netlist (P5q)`.

### 9. Netlist stage 2: computer

Sheets 1 and 2 without their connectors; the ties of stage 1 removed; our ROMs; stage 2 cases; stage 1 cases kept (a power-on picture before the reset code clears the screen: the screen-reader case moves to a seed and time where the video RAM is still random, or reads the video RAM against the picture at any time).

Fence: as phase 8; ratios. Commit `bw2001: the computer stage, our firmware on the TV (P5q)`.

### 10. Netlist stage 3: edges, BASIC 1, manuals

`J1`, `J2`, `J3`, `J5`, `J6`, the MC3446s; stage 3 cases; the BASIC 1 and killer-poke cases (release, skipped without `external/roms/pet`); the Machine harness target and one P5o session on the Machine (release); manuals; `bw2001` in `manuals_test`.

Fence: `-R "bw2001|manuals"`; release: the release-only Machine cases; ratios; `bw_ui` screenshots of the banner and the snow (BASIC 1 by a local netlist copy). Commit `bw2001: the PET's edges, the keyboard connector, manuals (P5q)`.

### 11. Board

`boards/bw2001/bw2001.place`, `bw place … -o boards/bw2001/bw2001.kicad_pcb`, `bw export-kicad netlists/bw2001.bwn -o /tmp/bw2001.net` for reference. **Stop for the user**: route it in KiCad (two layers, no zones, DRC 0 errors), the layout constraints from the Machine design (chips at the PET grid; connectors on their edges; any crack constraints come with P5r). The agent then runs `check-board` (adds a missing connection if Freerouting left one, ADD_MACHINE §3).

Commit (after the user's routing) `bw2001: Board, the PET 2001's grid, routed by the user (P5q)`.

### 12. Board tests, Session test, release check

The Machine cases built from the Board; the per-Machine Board loops (`boards_test`, `electrical_test`, `board_scene_test`); `session: bw2001 with every instrument`; release check (design §11), screenshots viewed.

Fence: `-R "bw2001|board|electrical|scene|session|drc"`. Commit `bw2001: Board tests, Session case, release check (P5q)`.

### 13. Recipes, docs, full suite

Design §10; the plan cut down to this table and the notes; the ARCHITECTURE row done with the date and links; the kernel speed row. Full debug suite.

Commit `docs: P5q done (P5q)`.

## Implementation notes

- Before the docs (2026-11-15): the three schematic sheets transcribed by three agents in parallel ([schematic/](../machines/130-bw2001/schematic/README.md)); joined with stand-ins (two 74LS138 halves for the 74LS139, no MC3446s or edge connectors, a gate-made composite sync for today's TV), they boot BASIC 1 to its banner on the TV in the right place, release 0.24× headless. The 8 MHz clock and its gated copy were 28 % of all Net changes, the 1 MHz phase clocks another 15 %.
- Docs review (before the docs commit), fixed: the TV mode enum is `TvSync` (`TvInput` is the headless plug class) and the consumer's `powerOn` keeps the mode; `MONITOR`'s grounds plain `Input`s on `GND` and its unused pin `NoConnect` (a `Power` pin `GND1` breaks the builder), `MATRIXKB`'s too; datasheets for the three passive connectors; the retrace wait covers screen reads as well (`SEL8` has no R/W gating) and its timing budget (about 22 cycles after the `PB5` read); the model's VIA `ORB` reads and `PCR` write not flagged as unmodelled, and it counts screen accesses in the display window; the harness's Machine target holds keys by row and column; `DS1` gets an `edge` rule; stage 1 ties `RW_E` and holds `SEL8` low through `UE5`, and ties the shared chips' sheet-1 inputs; `DriveOf(PullUp, X)` is `Port` and the existing pull-up case is rewritten; our reset writes VIA `PCR` = `$0C` (the first character set, as BASIC 1), else the TV shows the lower-case half; `Ls139` a class as `Ls138`; passive connector and `MATRIXKB` pins `sampled`; the 2 × 12 header's pads keep each PET letter opposite its number; `MatrixKeyFor` (the harness has `Bw2001KeyFor`); `bw_ui --type` accepts a `MATRIXKB`; held matrix keys released at power-off; chained `next` resistors and the `angle` field new; `R27` added, `R42` dropped, sheet 2's `R12` and the `VERT_DRIVE` pull-down left out; the stuck-Fault strength wording; phase 4's fence runs the P5p netlist; debug timings measured in phase 8, long cases release-only, cases named `bw2001 machine: stage N`.

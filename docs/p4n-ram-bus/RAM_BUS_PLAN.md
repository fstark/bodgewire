# P4n RAM and bus — Implementation Plan

Design: [RAM_BUS_DESIGN.md](RAM_BUS_DESIGN.md)
Spec: [RAM_BUS.md](RAM_BUS.md)

Every phase ends with its fence: `cmake --build --preset debug` clean (`-Werror`), `clang-format` on the C++ files touched, `ctest --preset debug -j 12` green (the `[slow]` cases included), then one commit with the message given. Only this step's own paths are committed.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Bidirectional pins | |
| 2 | Packages: DIP-18, DIP-20, KK-10 | |
| 3 | 2114 RAM | |
| 4 | 74LS244, grid packages | |
| 5 | KEYBOARD Part | |
| 6 | Typing: Session, `bw run`, `bw_ui --type`, bench | |
| 7 | Keyboard capture in `bw_ui` | |
| 8 | Release check, recipes, docs | |

## Phase 1 — Bidirectional pins

- `PinKind::Bidirectional`, `IsDriver`, `IsReader`, `DriveOf` (design §2.1); `buildTables` (§2.2); `powerOn` Z start and `readBitBefore` (§2.3); builder own-Net and warning rules (§2.4). Grep every `PinKind::` and `IsDriver` use and check each against §10 item 2.
- Tests: design §11.1 rows that need no new Part (`logic_test`, `circuit_test`, `simulator_test` with a small test Component in the test file; the builder cases that need a 2114 move to phase 3).
- Before and after, release: `bw run netlists/tennis.bwn --time 2s`; record both ratios here.
- Fence as above. Commit `sim: bidirectional pins`.

## Phase 2 — Packages

- Copy the three footprints (design §5) from KiCad 10 unchanged; `kFootprints` rows; `footprint_test` and `kicad_netlist_test` cases. Nothing in the catalogue uses them yet.
- Fence as above. Commit `board: DIP-18, DIP-20 and KK-10 packages`.

## Phase 3 — 2114 RAM

- `src/parts/ram2114.{hpp,cpp}` (design §3), delays, catalogue row, `datasheets/2114.md`, `tests/parts/ram2114_test.cpp`; `kMosSignals` in `catalogue_test` (design §10 item 3).
- The builder and net_pins cases of §11.1 that use a 2114 (stuck `IO`, unconnected `IO4`, dry `IO1`, Drivers list): the 74LS244 does not exist yet, so the second Driver in these cases is a 74LS161 output or a Stimulus; the `DescribeNet` case gains the 74LS244 in phase 4.
- Fence as above. Commit `parts: 2114 static RAM` (Intel's maximums, the only figures given, from known values).

## Phase 4 — 74LS244, grid packages

- `src/parts/ls244.{hpp,cpp}` (design §4), delays, catalogue row, `datasheets/74LS244.md`, `tests/parts/ls244_test.cpp`; `IsGridPackage` (§5) and the `placer_test` case.
- Check: `bw place` on the 070 and 080 rules, run to scratch files before the `IsGridPackage` change and after it (same rules, same seed), gives identical files (the committed Boards are routed, so they are not the reference).
- Fence as above. Commit `parts: 74LS244; DIP-18 and DIP-20 in the placer grid` (TI typicals from known values, the PDF not fetched).

## Phase 5 — KEYBOARD Part

- `src/parts/keyboard.{hpp,cpp}` (design §6.1), `ConnectorPins`, `ConnectorPowersDevice`, `CreateKeyboard`, catalogue row, `KeyboardOf`, `datasheets/KEYBOARD.md`; `tests/parts/keyboard_test.cpp`, `manual_test` size check.
- Fence as above. Commit `parts: ASCII keyboard connector`.

## Phase 6 — Typing

- `src/machine/typing.{hpp,cpp}` and `typing_test.cpp` (§6.3); Session `keyboards`, `type`, `typed` (§6.2); `bw run --type` with input edges; `bw_ui --type`.
- `tests/data/ram_bench.bwn` (§9), `tests/netlist/ram_bench_test.cpp`, the `session: typing` case (§11.3).
- Check by hand: `build/debug/src/cli/bw run tests/data/ram_bench.bwn --type J1:1ms:HELLO --time 1s`; the two usage errors.
- Fence as above. Commit `keyboard: typing through the Session, bw run and bw_ui; RAM bench`.

## Phase 7 — Keyboard capture

- `src/ui/keyboard_capture.{hpp,cpp}` (§7), the event loop, `DrawKeyboard`, the toolbar banner.
- Release build; `bw_ui tests/data/ram_bench.bwn --type J1:100ms:HELLO --screenshot /tmp/bench.png --after 1s`, view the PNG (Keyboard section, `Sent: HELLO`). Also a screenshot of 070 with every instrument to see nothing moved.
- Fence as above. Commit `ui: typing on the Machine's keyboard`.

## Phase 8 — Release check, docs

- Release check (design §11.4) with the screenshots viewed; speeds recorded here.
- Recipes ADD_PART, ADD_CONNECTOR_DEVICE; GLOSSARY; ARCHITECTURE Pins paragraph (design §8).
- ARCHITECTURE row P4n done with the date and links; this plan cut to the status table and implementation notes, "user check pending".
- Fence: everything green. Commit `docs: P4n implemented, user check pending`.

## Implementation notes

- Puzzle-file `stuck`/`joint` on a 2114 pin needs a Board with one: first exercised by the 090 Puzzles (P4p); P4n tests the electrical side through the builder.
- User check: Type in the Keyboard section of `bw_ui tests/data/ram_bench.bwn`, a few keys show under Sent and the banner says Typing; `U` does not open the User Manual while typing; Escape stops it and `U` works again.
- Docs review (before phase 1), fixed: the catalogue test's all-TTL rule exempts the 2114; the bench resets U4 before typing (random power-on count); the typing banner drawn without a Board; the user check uses `U` (no tool keys on a netlist-only Session); the placer check diffs two scratch placements; no test of a TTL bidirectional warning (no such Part); a stuck `IO` against a driving 74LS244 reads 0; a stuck bidirectional pin's own Net tied at Supply; waiting keys dropped at power-off by a producer-side marker, kept at power-on, no `--type` key in the first powered frame of `bw_ui`; the keyboard gap counted from the `STROBE` fall; control bytes of text events skipped; Z set explicitly at power-on; the release `bw run` records the bus.

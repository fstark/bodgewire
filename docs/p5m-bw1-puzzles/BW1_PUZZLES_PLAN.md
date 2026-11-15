# P5m BW-1 Puzzles — Implementation Plan

Spec: [BW1_PUZZLES.md](BW1_PUZZLES.md). Design: [BW1_PUZZLES_DESIGN.md](BW1_PUZZLES_DESIGN.md). Machine: [BW1.md](../machines/120-bw1/BW1.md), [BW1_DESIGN.md](../machines/120-bw1/BW1_DESIGN.md).

Every phase ends with its fence: `cmake --build --preset debug` clean, `clang-format` on the C++ files touched, `ctest --preset debug -j 12 -R "bw1|manuals|puzzle"` green, each new Puzzle's screenshot viewed against its brief, one commit of this step's own paths whose message names the tests run. A Puzzle changed or dropped when run is noted below with why.

## Phases

1. **Manuals and rig.** The service manual's Self-test section, the user manual's line (design §1); `bw1_rig.{hpp,cpp}`, `Bw1` reading Nets by pin, the empty `bw1_puzzles_test.cpp` with the title-and-brief case (design §2). The P5l cases unchanged and green.
   Commit: `bw1: self-test in the service manual, test rig for Puzzles`
2. **Display Puzzles**: 2 `odd_seven`, 3 `missing_bar`, 4 `half_segment`, 5 `bright_mess`.
   Commit: `bw1: display Puzzles`
3. **Keypad Puzzles**: 6 `dead_da`, 7 `dead_row`, 8 `deaf_keypad`.
   Commit: `bw1: keypad Puzzles`
4. **Main RAM Puzzles**: 1 `bad_byte`, 11 `dead_machine`, 12 `wrong_suspect`, 13 `half_byte`, 14 `two_bits`, 15 `aliased_page`.
   Commit: `bw1: RAM Puzzles`
5. **RIOT and ROM Puzzles**: 9 `self_test_fails`, 10 `no_stop`, 16 `rom_test_fails`.
   Commit: `bw1: RIOT and ROM Puzzles`
6. **Instrument Puzzles**: 17 `wrong_block`, 18 `held_reset`, 19 `sst_kills`.
   Commit: `bw1: instrument Puzzles`
7. **Finish.** BW1.md and BW1_DESIGN.md Puzzle lists (design §4); ADD_PUZZLE fixed with what the step found; the full debug suite; ARCHITECTURE row P5m done with date and links; this plan cut to the table and notes.
   Commit: `docs: P5m implemented, user check pending`

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Manuals and rig | |
| 2 | Display Puzzles | |
| 3 | Keypad Puzzles | |
| 4 | Main RAM Puzzles | |
| 5 | RIOT and ROM Puzzles | |
| 6 | Instrument Puzzles | |
| 7 | Finish | |

## Implementation notes

- Docs review (before the docs commit), fixed: Dead machine's code `110008` (`$00` at `$0000` reads right), Wrong suspect's `100008`; Aliased page fails at `$0000` (`100020`); Wrong block stuck 1 cannot reach the self-test either (`$1800` decodes as `K7`); ROM test fails in the fill after `.checksum`, stuck 0; No stop stuck 1 (`NMIV` low byte `$00`); Odd seven not on bit 7 (`PA7` unconnected); `Machine` not copyable; crack pads from the routing order (`PA3`, `ROW2`, `BUF_N`); a Bright mess reader; repair checks short of `PA55` (debug time); Two bits on `U1`'s bus.

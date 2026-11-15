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
| 1 | Manuals and rig | `3ee64bd` |
| 2 | Display Puzzles | `1f4b472` |
| 3 | Keypad Puzzles | `17da469` |
| 4 | Main RAM Puzzles | `b1c73bc` |
| 5 | RIOT and ROM Puzzles | `e530d3a` |
| 6 | Instrument Puzzles | |
| 7 | Finish | |

## Implementation notes

- Docs review (before the docs commit), fixed: Dead machine's code `110008` (`$00` at `$0000` reads right), Wrong suspect's `100008`; Aliased page fails at `$0000` (`100020`); Wrong block stuck 1 cannot reach the self-test either (`$1800` decodes as `K7`); ROM test fails in the fill after `.checksum`, stuck 0; No stop stuck 1 (`NMIV` low byte `$00`); Odd seven not on bit 7 (`PA7` unconnected); `Machine` not copyable; crack pads from the routing order (`PA3`, `ROW2`, `BUF_N`); a Bright mess reader; repair checks short of `PA55` (debug time); Two bits on `U1`'s bus.
- Phase 1: a service manual allowed only `## Components`; the check now takes any sections after it (`CheckServiceManual`, shared code: the full suite at the end). Codes in the manual are plain text, not backticked (`PA55` would read as an unknown reference).
- Phase 2: all four passed first time; `bright_mess` reads `DS3` as `0x7F` (lit with every digit). Screenshots: Odd seven's 7s and Half a segment's open tops show; Missing bar's middle bar is hidden under the digit's reference (the P5n known issue), so the screenshot cannot show it.
- Phase 3: the monitor starts in address mode, so with DA dead the typed bytes shift into the address (`456 DA 77` shows `5677`): the brief says so. Deaf keypad uses `+` (`SW19`): the self-test shows `PA5512` with no key held (118 s in debug, the only case run to `PA55`).
- Phase 4: every fault tried monitor and strapped on seeds 1–3 first. Half a byte: the crack `U14.1`–`U13.4` cuts `U13` alone (`U15.6`–`U13.1` cut both buffers: `1100FE`); unstrapped the monitor wanders into the self-test's keypad loop and shows `PA55` on every seed; strapped `11000E` (bit 0 happens to match the bus charge). Aliased page is not dead: it starts on `1D27 46` (every seed) and the keys work, but bit 5 of the address comes from the stack (`AD 0300` shows `0320`). Wrong suspect: a few frozen characters that change with the seed (`18F` on seed 1), the brief says "a meaningless pattern". Two bits (`U1` `D2`–`D3`) is dark with the self-test too: moved to phase 6.
- Phase 5: `NMIV` bits 1, 4 and 6 stuck at 1 still stop correctly (they land on instructions that work); bit 0 (`$1C01`) lands mid-instruction and shows a wrong address, which changes with what ran before (`00207B`, `3F24FF`): the brief says "a meaningless address", and the keys still work after it. Self-test codes: `379504` at 75 ms, `5800FF` at 100 ms. `no_stop` is the slowest case after `deaf_keypad` (137 s in debug, seeds 1–3).
- Phase 6: Wrong block with `A10` stuck 0 (stuck 1 is dark too, and its clue weaker: `K7` selected for both ROMs): the 6502 never runs, so the clue is not a moving `A10` but a still one: `U4`'s leg reads `A10` high while `U4` selects `K6`. SST kills it: one digit stays lit, which one depends on timing (the test counts lit digits); back at OFF the monitor shows `1C0085`, stopped in its own SAVE. Two bits (`U1` `D2`–`D3` bridged): dark or one frozen digit by seed, strapped too.

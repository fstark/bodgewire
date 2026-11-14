# P5k BW-1 monitor — Implementation Plan

Design: [BW1_MONITOR_DESIGN.md](BW1_MONITOR_DESIGN.md)
Spec: [BW1_MONITOR.md](BW1_MONITOR.md)

Each phase ends with a fence: its checks, then one commit (staged by path; the user edits docs in parallel). Tests in the debug build unless said.

## Phase 1 — `.checksum`

Design §1: the directive, the second pass 2, the header comment; the cases in `tests/asm/asm6502_test.cpp`.

Fence: `cmake --build --preset debug`; `ctest --preset debug -j 12 -R "asm"` (every `asm_*` ctest and the `[asm]` cases).
Commit: `asm: .checksum, a byte making the ROM sum to 0`.

## Phase 2 — The model

Design §4: `tests/machines/bw1_model.{hpp,cpp}` and `bw1_model_test.cpp` on a program assembled in the test. `bw1_002.rom` and `bw1_003.rom` do not exist yet: the constructor reads them, so this phase commits a first `roms/bw1_002.asm` (`RST` and the vectors only, `.checksum`) and `roms/bw1_003.asm` (`jmp *` at `$1800`, `.checksum`) with their `.rom`s (the stub's `RST` is `jmp *`: `COLD` and `SELFTEST` do not exist yet); phases 3 and 4 fill them. Each `.rom` from `build/debug/src/cli/bw asm roms/bw1_00x.asm -o roms/bw1_00x.rom`.

Fence: build; `ctest --preset debug -j 12 -R "bw1 model|asm"`.
Commit: `tests: phase 2 — a KIM model around the 6502 core for the BW-1's ROMs`.

## Phase 3 — The monitor

Design §2 and §5.1: `roms/bw1_002.asm` written out, `bw asm` to its `.rom`, `tests/machines/bw1_monitor_test.cpp`. Measure a scan (cycles between two `SBD` writes of digit 1) and adjust `DELAY` if far from 3.5 ms; record it in the notes.

Fence: build; `ctest --preset debug -j 12 -R "bw1|asm"`.
Commit: `roms: phase 3 — BW-1 monitor in the 6530-002 at the KIM's addresses`.

## Phase 4 — The self-test

Design §3 and §5.2: `roms/bw1_003.asm`, its `.rom`, `tests/machines/bw1_selftest_test.cpp`. The display test's times in the cases come from the program: measure when each phase starts and set the checks inside them, with margins; record them in the notes.

Fence: build; `ctest --preset debug -j 12 -R "bw1|asm"`.
Commit: `roms: phase 4 — BW-1 self-test in the 6530-003, using no RAM`.

## Phase 5 — Finish

Design §6, §7: recipe, glossary, the release check (release build, the `bw1` and `asm` tests, a `bw_ui` screenshot of the 110 viewed), the full debug suite, the ARCHITECTURE row done with the date and links; this plan cut to the table and notes ("user check pending").

Fence: `ctest --preset debug -j 12` (full, about 15 min); release `ctest --preset release -R "bw1|asm"`.
Commit: `docs: P5k implemented, user check pending`.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | `.checksum` | `4387229` |
| 2 | The model | this commit |
| 3 | The monitor | |
| 4 | The self-test | |
| 5 | Finish | |

## Notes

- Review (before code), fixed in the docs: the ROM sums' `Z` after `inx` (a `tay` before the branch); the `3795F0` stuck values; the SST case's third display (`$0206` holds RAM); the KIM entry-point program written out; test names beginning `bw1 …` for the `-R` filters; the timer flagged, not refused, and not checked in the bad-RAM cases (random code); `SEG_*` constant names, numbered local labels, `FAILn` by `jmp`; the scan about 3.5 ms; the 003's `PA` pins left unconnected in P5l.

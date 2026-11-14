# P5k BW-1 monitor — Implementation Plan

Design: [BW1_MONITOR_DESIGN.md](BW1_MONITOR_DESIGN.md)
Spec: [BW1_MONITOR.md](BW1_MONITOR.md)

Implemented 2026-11-14; user check pending.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | `.checksum` | `4387229` |
| 2 | The model | `b826d4f` |
| 3 | The monitor | `0530d01` |
| 4 | The self-test | `ed63922` |
| 5 | Release check, recipe, glossary, roadmap | this commit |

## Notes

- Review (before code), fixed in the docs: the ROM sums' `Z` after `inx` (a `tay` before the branch); the `3795F0` stuck values; the SST case's third display (`$0206` holds RAM); the KIM entry-point program written out; test names beginning `bw1 …` for the `-R` filters; the timer flagged, not refused, and not checked in the bad-RAM cases (random code); `SEG_*` constant names, numbered local labels, `FAILn` by `jmp`; the scan about 3.5 ms; the 003's `PA` pins left unconnected in P5l.
- Monitor: 306 bytes of code and tables; every entry point on its KIM address (`.org`, checked by a test). A scan by cycle count: about 552 cycles a digit (`CONVX` 545 with its `jsr`, `DELAY` 100), 3.4 ms with `AK`: `DELAY` kept.
- Self-test: 885 bytes (`$1800`–`$1B74`, `$1BF0` free for the ROM-sum case). By cycle count: tests 1–5 about 75 ms (main RAM two passes about 47 ms, RIOT RAMs 4 ms, ROM sums 23 ms); every segment from about 75 to 865 ms; each digit alone 0.33 s, digit 1 from 0.86 to 1.19 s; `PASS` from about 2.85 s. The cases check at 2–7 ms, 300 ms, 1.0 s and 3.2 s.
- Full debug suite: 1016/1016.
- Release: the `bw1` and `asm` tests pass in the release build (36/36); `bw_ui` of the 110 at 1.00×, UI 0.3 ms, screenshot viewed (unchanged).
- Sources: the KIM-1 User Manual's scan (Figs. 3.3, 3.5, 3.6, 3.13; Appendix I's symbol table and listing pages 16, 17, 26–28). Its listing pages 18–25 (`AK`, `SCAND`, `CONVD`) are missing from the scan: those routines' behaviour follows the KIM books, and `$1F40` (`KEYIN` in the books) is not in the symbol table.

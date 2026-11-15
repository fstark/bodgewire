# P5p BW-2001 firmware — Implementation Plan

Spec: [BW2001_FIRMWARE.md](BW2001_FIRMWARE.md)
Design: [BW2001_FIRMWARE_DESIGN.md](BW2001_FIRMWARE_DESIGN.md)

Implemented 2026-11-15.

Every phase ended with its fence: the debug build clean, `clang-format` on the C++ files touched, the `.rom` files regenerated with the debug `bw asm` and committed with their `.asm`, the phase's tests green, and one commit naming them.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `2b125f6` |
| 1 | `.include`, `.checksum BASE, SIZE`, `offset=` | `54b0246` |
| 2 | The model and the harness | `7a7ece0` |
| 3 | Kernel reset and the editor, on a BASIC stub | `279370b` |
| 4 | Tiny BASIC: the conformance set on the model | `ca72077` |
| 5 | The monitor | `506c2c9` |
| 6 | The self-test | `92e6e15` |
| 7 | The minimal netlist | `8c0dada` |
| 8 | Release check, recipes, roadmap | this commit |

## Notes

- Review (before code), fixed in the docs: `offset=` reads the file at 64K (`ParseRomContents` refuses words past its size); the monitor runs below the saved SP instead of resetting `S`, and does `cli` after `BRK`; the model takes its ROMs in `Options::roms`, so phase 2 needs no firmware; the self-test's RAM passes rewrite page 0 before comparing it, and a page-0 failure is `ZERO PAGE BAD` on its own row; `.checksum` with one value has its own error (the old expectation changes); `.include` with a bare file name resolves against `.`; `OUT OF MEMORY` also checks a replaced line keeping its old text; the editor cuts at 64 before trimming, clears `LINKS[0]` and moves `SROW` on a scroll, and keeps `sei` short; `INPUT` answers past 5 digits are `REDO`; the real expression stack figure; keys 30 ms held and 30 ms released; `LINES` read before RETURN; the timings from P5n's bench; the netlist test renamed `bw2001_netlist_test.cpp`; CMake and `-R bw2001` names; the release `bw` rebuilt before use.
- Phase 1: the full debug suite gave 1121/1122. The one failure was the `roms/*.asm` glob meeting phase 3's sources, written while the suite ran; it passed with them moved aside. `Rom::word()` was added for the `offset=` test.
- Phase 2: the model's probe first read port B in its main loop, which clears the retrace flag before the IRQ is taken (the P5n bench note). Keys are read only in the interrupt, as the editor does.
- Phase 3: the editor is 1.3 KB with `KEYTAB`. The model runs about 4× real time in debug, so the editor cases take about 3 s.
- Phase 4: BASIC is 3.9 KB, `$C000`–`$C75F` in UH1 and `$C800`–`$CF65` in UH5 (154 bytes spare); UH2 and UH6 stay empty. All 31 sessions passed on the model with no change to P5o, in 31 s of debug time. The deepest 6502 stack use is `$01A6` (90 bytes, session `15_depth`), against the design's estimate of 160–200; the conformance case checks it stays above `$0120`. The editor cases were rewritten for BASIC (`LIST` shows the line handed over) in place of the stub's echo.
- Phase 5: the monitor is 0.6 KB in UH4. It prints `C*` or `B*` on a fresh row, so it reads the editor's cursor column: `CCOL` moved into `roms/bw2001.inc`.
- Phase 6: the self-test is 1.4 KB in UH7 (to `$FE10`), its pre-RAM code written out per row, since it may use neither the stack nor any RAM but `$00`–`$01`. A failure keeps its three values in screen cells beyond its row's digits and its row in `S`. Every row reaches the `KEY` row within the cases' 1.2 s window. In the key echo, `putch` first kept `Y` in the key index's byte, which the test caught.
- Phase 7: `tests/data/bw2001_firmware.bwn` is the bench with our ROMs (`offset=` for UH5 and UH7), SW1–SW73 on the matrix and SW74 on DIAG. Debug: power-on to `READY` 17 s, `27_cls` 124 s, DIAG to the `KEY` row in parallel. Release: all 31 sessions pass on the real Parts in 175 s for 186 s simulated, 1.04–1.07× while typing; idle at `READY` (`bw run … --time 2s`, median of 3) 1.07×. `ctest --preset release -R "bw2001|asm|basic_"` 51/51.
- Phase 8: full debug suite 1151/1151 in 658 s, the release-only netlist case skipped. Release: `ctest --preset release -R "bw2001|asm|basic_"` 51/51 (phase 7). No screenshot: there is no picture before P5q's video chain. Recipe ADD_MACHINE §2: firmware over several chips, the shared `.inc`, `offset=`, local label scope, one harness for a model and a netlist, release-only cases; and a gotcha: a 6520 port read clears its control line's flags.

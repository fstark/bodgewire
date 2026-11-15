# P5p BW-2001 firmware — Implementation Plan

Spec: [BW2001_FIRMWARE.md](BW2001_FIRMWARE.md)
Design: [BW2001_FIRMWARE_DESIGN.md](BW2001_FIRMWARE_DESIGN.md)

**Each phase's fence** is:

- `cmake --build --preset debug` clean;
- `clang-format` on the C++ files touched;
- the phase's tests green: `ctest --preset debug -j 12 -R "<filter>"`;
- the `.rom` files regenerated with the debug `build/debug/src/cli/bw asm` just built (a release `bw` from before phase 1 lacks `.include`), and committed with their `.asm`;
- new source files added to `bw_tests` in `tests/CMakeLists.txt`;
- one commit, whose message names the tests run.

The full debug suite runs at phase 1 (the assembler and a Part are shared code) and at phase 8.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `2b125f6` |
| 1 | `.include`, `.checksum BASE, SIZE`, `offset=` | `54b0246` |
| 2 | The model and the harness | `7a7ece0` |
| 3 | Kernel reset and the editor, on a BASIC stub | `279370b` |
| 4 | Tiny BASIC: the conformance set on the model | this commit |
| 5 | The monitor | |
| 6 | The self-test | |
| 7 | The minimal netlist | |
| 8 | Release check, recipes, roadmap | |

## Phases

### 1. Tools (design §1, §2)

- `.include` and windowed `.checksum` in `asm6502.{hpp,cpp}`; `offset=` in `CreateRom`.
- Tests: `asm: .include`, `asm: .checksum BASE, SIZE`, and the `rom_test` offset cases.
- The `.checksum 5` expectation in the existing test changes to `.checksum takes nothing or BASE, SIZE`.
- Docs: the 2316B, 2716 and 2513 datasheets, and GLOSSARY.
- Fence: the **full debug suite** (the assembler and `CreateRom` are shared).
- Commit: `asm, rom: .include, windowed .checksum, offset= on the ROM Parts (P5p)`.

### 2. The model and the harness (design §7, §8)

- `bw2001_model.{hpp,cpp}`, `bw2001_harness.{hpp,cpp}`, and the model case of `bw2001_model_test.cpp`: a program assembled in the test and passed in `Options::roms`, since no firmware `.rom` exists yet.
- `roms/bw2001.inc`.
- The harness's session parser, tested on a session file against a fake target that reproduces a screen.
- Fence: `-R "bw2001|asm"`.
- Commit: `bw2001: the PET model and the session harness (P5p)`.

### 3. Kernel reset and the editor (design §4.1, §4.2, §5)

- `bw2001_kernel.asm` (reset, IRQ, NMI `rti`, entry `JMP`s; `MONITOR`, `MONBRK` and `SELFTEST` jump to stubs);
- `bw2001_edit.asm` in full;
- `bw2001_basic.asm` as a stub: `COLD` prints `READY` and loops on `EDLINE`, printing each line back after `=`; `NUMOUT` for the banner.
- Tests:
  - power-on banner rows and bytes free;
  - the editor list of the spec, as far as it needs no BASIC: echo, CRSR, DEL and INST, wrapping, the 64 limit, scroll, HOME and CLR, STOP at `READY`;
  - the ROM-sum and entry-point cases;
  - `KEYTAB` against `Bw2001KeyFor`.
- Fence: `-R "bw2001|asm"`.
- Commit: `bw2001: kernel reset and the full-screen editor (P5p)`.

### 4. Tiny BASIC (design §6)

- `bw2001_basic.asm` in full, replacing the stub.
- Tests:
  - the conformance case (all 31 sessions);
  - USR;
  - OUT OF MEMORY;
  - the editor cases that need BASIC: editing a listed line; `INPUT` after its prompt; a 30-line listing scrolling with its links.
- A difference from the reference interpreter is settled as the spec says: the ROM fixed, or P5o's spec, interpreter and sessions fixed in this commit, with `basic_` run too.
- Notes: the size of BASIC, the model's debug time for the set.
- Fence: `-R "bw2001|asm|basic_"`.
- Commit: `bw2001: Tiny BASIC, the conformance set on the model (P5p)`.

### 5. The monitor (design §4.3)

- Replace the stubs. Tests: the spec's monitor list.
- Fence: `-R "bw2001|asm"`.
- Commit: `bw2001: the TIM-like monitor (P5p)`.

### 6. The self-test (design §4.4)

- Replace the stub. Tests: the spec's self-test list.
- Notes: the timings by cycle count.
- Fence: `-R "bw2001|asm"`.
- Commit: `bw2001: the self-test (P5p)`.

### 7. The minimal netlist (design §9, §10)

- `tests/data/bw2001_firmware.bwn`, `tests/netlist/bw2001_netlist_test.cpp`.
- The every-build cases in debug.
- The release-only conformance case in release: `cmake --build --preset release` first, then `ctest --preset release -R bw2001`.
- The headless ratio (`bw run … --time 2s`, median of 3, on an idle machine).
- Fence: `-R "bw2001"` in debug and in release.
- Commit: `bw2001: our firmware on a minimal netlist of the real Parts (P5p)`.

### 8. Finish

- Full debug suite.
- Release: `ctest --preset release -R "bw2001|asm|basic_"`. No screenshot (no picture before P5q).
- Recipes: ADD_MACHINE §2 (design §11).
- Roadmap row done with the date and links.
- This plan cut down to the table and notes.
- Commit: `docs: P5p implemented`.

## Notes
- Review (before code), fixed in the docs: `offset=` reads the file at 64K (`ParseRomContents` refuses words past its size); the monitor runs below the saved SP instead of resetting `S`, and does `cli` after `BRK`; the model takes its ROMs in `Options::roms`, so phase 2 needs no firmware; the self-test's RAM passes rewrite page 0 before comparing it, and a page-0 failure is `ZERO PAGE BAD` on its own row; `.checksum` with one value has its own error (the old expectation changes); `.include` with a bare file name resolves against `.`; `OUT OF MEMORY` also checks a replaced line keeping its old text; the editor cuts at 64 before trimming, clears `LINKS[0]` and moves `SROW` on a scroll, and keeps `sei` short; `INPUT` answers past 5 digits are `REDO`; the real expression stack figure; keys 30 ms held and 30 ms released; `LINES` read before RETURN; the timings from P5n's bench; the netlist test renamed `bw2001_netlist_test.cpp`; CMake and `-R bw2001` names; the release `bw` rebuilt before use.
- Phase 1: the full debug suite gave 1121/1122. The one failure was the `roms/*.asm` glob meeting phase 3's sources, written while the suite ran; it passed with them moved aside. `Rom::word()` was added for the `offset=` test.
- Phase 2: the model's probe first read port B in its main loop, which clears the retrace flag before the IRQ is taken (the P5n bench note). Keys are read only in the interrupt, as the editor does.
- Phase 3: the editor is 1.3 KB with `KEYTAB`. The model runs about 4× real time in debug, so the editor cases take about 3 s.
- Phase 4: BASIC is 3.9 KB, `$C000`–`$C75F` in UH1 and `$C800`–`$CF65` in UH5 (154 bytes spare); UH2 and UH6 stay empty. All 31 sessions passed on the model with no change to P5o, in 31 s of debug time. The deepest 6502 stack use is `$01A6` (90 bytes, session `15_depth`), against the design's estimate of 160–200; the conformance case checks it stays above `$0120`. The editor cases were rewritten for BASIC (`LIST` shows the line handed over) in place of the stub's echo.


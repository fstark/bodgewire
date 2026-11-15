# P5n BW-2001 chips — Implementation Plan

Design: [BW2001_CHIPS_DESIGN.md](BW2001_CHIPS_DESIGN.md)
Spec: [BW2001_CHIPS.md](BW2001_CHIPS.md)

Implemented 2026-11-15.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `cf0564a` |
| 1 | Binary contents, 2316B, `ExternalRom` | `77fa639` |
| 2 | TTL: 74LS154, 74177, 74100, 7417, 74LS21, 74LS145 | `ca0624a` |
| — | Character ROM 901447-08 shipped (user, 2026-11-15) | `2b19976` |
| 3 | 6520 | `78dc33d` |
| 4 | 6522 core and Part | `2230bf5` |
| 5 | Bench, its program, BASIC 1 boot | `463bcd9` |
| 6 | Release check, recipes, roadmap | this commit |

## Notes

- Review (before code), fixed in the docs: `kDone` from 36 pages, not 9 (300 ms; `bw run --time 400ms`); the 6522's power-on bytes 16; the 74100 in `kOffCornerPower`; only `catalogue_test` builds every ROM; the external-ROM header beside its users in `tests/netlist/` (no include path for `tests/`); the 6522 samples its inputs at the rise only; `CB1` and `CB2` direction per SR mode; `PB7` under T1 by `ACR` bit 7 alone, low from the T1 write in both modes; undriven pull-up pins driven `One`; the retrace Net named `RETRACE` (the 6502 has a `SYNC` pin); PIA2's and the 2114s' wiring spelled out; the T1 IRQ checked on the bench; one bench run a seed, stuck cells on seed 1; fence regexes by test name (`[parts]` is a tag); the `CB2` pulse end in the 6520's rise; pin ranges ascending in the datasheet files; the power-on contention before `RES_N` noted; the retrace window 55 ms.
- Phase 1 and 2 code was written while the review ran, then checked against its findings (74100 already in `kOffCornerPower`, `external_rom.hpp` already in `tests/netlist/`).
- Baselines before phase 1 (release, headless, median of 3): 110 5.48×, `bus_load` 0.397×, BW-1 3.04×.
- Phases 1 and 2 were tested together: one full debug suite on the tree with both, 1080/1080 in 645 s. Each was then committed alone by staging its own version of the shared files (catalogue, delays, CMake, catalogue test), and that snapshot was built in a scratch tree and its fence subset run there.
- Test helpers: `tests/parts/chip_bench.hpp` (one chip, a Stimulus on every input) for the TTL Parts; `tests/parts/bus_bench.hpp` (the 6530 test's scripted 1 MHz bus, made general) for the 6520 and 6522.
- 6520 mutations, each failing a test: a port read keeping the flags; a DDR read clearing them; the CB2 pulse never ending; C1 on either edge; no CB2 write strobe.
- 6522 core mutations, each failing a test: T1 decrementing in its write cycle; T1 not reloading; `IER` bit 7 read as 0; mode 1 at the φ2 rate; independent C2 modes cleared; PB7 needing `DDRB`; T2 reloading; reset clearing T1; no PA latching. The shift register starts from the cycle after the `SR` access (as T1 from the cycle after its write).
- Bench, against the design: the keys are read in the retrace interrupt, as the PET does, not in the main loop (reading PIA1's port B clears the retrace flag, so a main-loop read could take the interrupt away before the CPU saw it); key checks therefore allow 20 ms. The T1 IRQ check is by level at each PB7 turn, skipping turns within 300 µs of a retrace. `kDone` measured: `$55` at 349.5 ms (37 cycles a byte over 35 pages), so 400 ms, not the review's 300.
- BASIC 1 boots to `*** COMMODORE BASIC ***`, ` 7167 BYTES FREE`, `READY.` on the bench with no change to any chip.
- Debug times: the program case (3 seeds) 82 s, the stuck cells 39 s, BASIC 1 34 s; release 2.4 s for all three. Headless release ratio of the bench 1.31× (450 ms).
- Release (headless, median of 3): the bench 1.25× (450 ms), the bench booting BASIC 1 1.09× (600 ms). Against `cf0564a` built in a worktree, side by side, three runs each: `bus_load` 0.336–0.372 (base) vs 0.364–0.369 (new), BW-1 2.65–2.82 vs 2.68–2.77, 110 5.60–5.67 vs 5.63–5.78: no change (the ROM's run-time select mask costs nothing measurable). Against the morning's baselines the same build read 0.389, 2.92, 6.06: machine drift.
- `bw_ui` release, `netlists/message.bwn` (2716 and 2513): the poem as before, 1.00×, UI 1.0 ms; screenshot viewed.
- Full debug suite at the end: 1117/1117 in 616 s.

# P5v — BW-80 video spike

Roadmap step P5v ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), after P5u's chips ([BW80_CHIPS.md](../p5u-bw80-chips/BW80_CHIPS.md)). It wires the whole Sinclair ZX80 as Sinclair designed it, keyboard matrix aside, and runs a picture on it the ZX80's way. The display file runs as forced NOPs through the data bus resistors. The character set comes out of the ROM, addressed by the latched code and the line counter while the CPU refreshes. `HALT` ends each row, the interrupt from `A6` starts each HSYNC, and `IN`/`OUT` make the VSYNC. A test ROM of ours shows a page of every character first. Then the ZX80's own ROM brings up its `K` cursor screen. The speed ratio is measured.

## Why

P5a and P5s ran a ZX80-shaped display on stand-in chips (74LS244s in place of the resistors, a 74LS161 for HSYNC). Sinclair's circuit has no such margins: the NOP is forced through 1K resistors while the RAM still drives the memory side, and the 74LS165 loads from the CPU side of those resistors. Two RC differentiators cut its load and clock pulses. A 2-flip-flop chain clocked by `M1` makes HSYNC, and a NAND latch set by `OUT` and reset by a keyboard read makes VSYNC. Only the real ROM's display code can show whether all of it holds together in the model, so this step runs it.

## Decided

With the roadmap row (2026-11-15): first a test ROM, then the real ZX80 ROM as the faithfulness check (its `K` cursor screen); the ratio measured; a failure goes to the user.

Asked at the start of the step (the user, 2026-11-16):

- **RC shapers as a capacitor Part**: a new `C` Part with `value=` (farads: `47p`). Between a driven Net and a Net with a pull resistor, it repeats each edge of the driven side on the pulled side, then lets go after one time constant (the pull's ohms × its farads). The pull then brings the Net back. C11 (47p) with R25 (470) makes the 74LS165's load pulse from the CPU clock. C9 (47p) with R2 (470) makes its 6.5 MHz shift clock. Both stay at their designators, so a wrong value or a cut resistor changes the pulse. Offered instead: a fixed-width stand-in (`EDGE width=`), or gates and delays.
- **The ZX80's own ROM ships with the BW-80**: `roms/zx80_ic2.bin`, the user's 4K dump (SHA1 `b6769a3197c77009e0933e038c15b43cf4c98c7a`), committed in git. The user notes that the ZX80 ROM may be distributed with emulators. So P5w (our own ROM) is skipped and P5x is short (the user, 2026-11-16). The commented listing (Geoff Wearmouth's assembly listing of the ZX80 ROM, k1.spdns.de/Vintage/Sinclair/80/Sinclair ZX80/ROMs/) is the reference for its code. It stays outside git, in `external/roms/zx80/zx80_rom_source.txt`, because its annotations are not Sinclair's.
- **The test ROM's character set is the ZX80's**: its bytes at `$0E00`–`$0FFF`, taken from `roms/zx80_ic2.bin` when the test ROM is assembled.
- **The whole ZX80, no keys**: `netlists/bw80.bwn` has every chip of Searle's schematic at its designator (IC1–IC21), the data bus resistors, the NOP gates in the schematic's order, and IC10 with its pull-ups R13–R17 but no key matrix. Not included: the cassette's and modulator's analog parts, and the edge connector. It opens in `bw_ui` with a TV, and P5x grows it into the Machine.

Decided in this step's design (not the user's):

- **Memory timing from the address**: the `2114` and the ROMs (`2716`, `2513`, `2316B`, `2332`) time their outputs from the later of the address change and the select, as their data sheets do: the data comes at the address's time plus the access time, or the select's time plus the select time, whichever is later. Today they time from whichever input changed last, so a chip selected after its address settled answers at the select time alone (P5u's open point: `access=` could not show the NOP margin).
- **The RAM's grade**: IC3 and IC4 at `access=300ns`. IC4 holds bit 6, which decides the NOP. As a 450 ns 2114, its data comes about 560 ns into the fetch, and the NOP chain (74LS04, 74LS10, 74LS32 and two 74LS05s, about 65 ns) puts the NOP on the bus at about 625 ns. The model's Z80 reads the data at T3's rise, 615 ns, with no setup time, so it misses by about 10 ns. Searle's drawing marks IC4 as a 2114-1, a fast grade. A test shows a 450 ns IC4 failing. IC3's bits only reach the data latch, which stays open until after T3's rise, so its grade is not tested.
- **Reset**: `RST1 POR time=1ms` with R21, as the P5u bench. The ZX80's R21 and C10 hold the reset for about 0.2 s, an RC the new `C` Part does not model (it couples edges; it does not charge). P5x decides whether that matters on the Board.
- **The halted Z80 reads the HALT itself**: while halted, the model put the byte after the `HALT` on the address bus (P5s). The ZX80's load gate is not stopped by `HALT_N`, so a short row would repeat the next row's first character to the line's end, where Searle says it stays blank. So the halted fetches now show the `HALT`'s own address and read `$76` again; PC stays past the `HALT`, which the interrupt pushes (the ZX80 ROM's `MASK-INT` takes the next row from it). MAME's Z80 does the same. Found by this step's review: the spike's finding on the Z80 Part.
- **`.incbin`** in `bw asm`: `.incbin "file", OFFSET, LENGTH` copies bytes of a binary file into the ROM. The test ROM uses it for the character set.

## New

1. **`C` (capacitor)**: a passive Part, wired by the builder as resistors are: its Nets decide what it does (Design §2).
2. **Memory timing**: the `2114` and the ROM family time from address and select (Design §3).
3. **`.incbin`** in `bw asm` (Design §4).
4. **`netlists/bw80.bwn`**: the ZX80 (Design §5).
5. **`roms/zx80_ic2.bin`**, its note `roms/zx80_ic2.md`, and the test ROM `roms/bw80_test.asm` → `.rom`: our display driver to the ZX80's rules, showing a page of every character, normal and inverse (Design §6).
6. **Tests and measures** (Design §7).

## Tests

- `C`: edges repeated and released after R·C; no pull, both rails, or a pull on both sides: it passes nothing; the builder's errors (`value=` missing or bad, an end unconnected, both ends on one Net); the Board keeps it between two islands; the Net panel lists it.
- Memory timing: a 2114 and a 2332 selected after their address settled answer at address + access when that is later, and at select + select time when that is later.
- `.incbin`: bytes copied; errors (no file, offset or length outside it).
- `bw80` netlist: loads without warnings.
- Test ROM, seeds 1–2: the TV locks (vertical sync, at least 90 % of lines synced), and the page is on the screen: every code's glyph from the ROM, inverse rows inverted, in two frames. With IC4 at 450 ns, the page is not shown (the NOP is missed).
- Z80: halted fetches read the `HALT`'s own address; the interrupt pushes the address after it.
- The ZX80 ROM, seed 1: the TV locks and shows the `K` cursor screen: one inverse `K` cell, the rest of the picture area paper (the 24 rows of 32 cells around it; the margins, where the TV also draws the sync periods, are not checked).
- Release: `bw run netlists/bw80.bwn --time 2s`, the ratio (and the test ROM's); `bw_ui` screenshots of both, viewed.

Design: [BW80_VIDEO_DESIGN.md](BW80_VIDEO_DESIGN.md). Plan: [BW80_VIDEO_PLAN.md](BW80_VIDEO_PLAN.md).

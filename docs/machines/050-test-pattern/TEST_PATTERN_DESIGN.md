# 050 — Test-pattern Generator — Machine Design

How the Machine in [TEST_PATTERN.md](TEST_PATTERN.md) is built: netlist, Board, Puzzles, manuals. It uses only code that exists (Parts from P1b, the TV from P2c, Faults from P2d and P3f).

## Files

Machine name `test_pattern`:

```
netlists/test_pattern.bwn                       design netlist
boards/test_pattern/test_pattern.kicad_pcb      Board (KiCad 10)
manuals/test_pattern/user.md, service.md        User and Service Manuals
puzzles/broken_trace.bwp, dead_counter.bwp, stuck_gate.bwp, dead_rail.bwp,
        line_bridge.bwp, unpowered_chip.bwp, sync_bridge.bwp, zips.bwp
```

## Netlist

`netlists/test_pattern.bwn` (TEST_PATTERN.md, Circuit). References: `U1`–`U5` 74LS161 counters, `U6` 74LS20, `U7` `U8` 74LS30, `U9` 74LS04, `U10` 74LS86, `U11` 74LS08, `U12` 74LS32, `U13` 74LS02, `U14` 74LS74, `X1` `OSC freq=1MHz`, `J1` `POWER`, `J2` `VIDEO` (the Session adds the TV by itself).

Decodes, as wired:

| Net | Gate | Low (or high) for |
|---|---|---|
| `LINE_END_N` | `U6` gate 1: NAND(`HC`, `H4`, `H5`, +5 V) | dot 63 |
| `HSYNC_N` | `U6` gate 2: NAND(`H5`, `H4`, `H3_N`, `H2`) | dots 52–55 |
| `FRAME_END_N` | `U7`: NAND(`V8`, `V5`, `V4`, `V2`, `V1`, `V0`, `LINE_END`, +5 V) | line 311 at its line end |
| `VSYNC_N` | `U8`: NAND(`V8`, `V5`, `V4`, `V2`, `V3_N`, +5 V ×3) | lines 308–311 |
| `HBLANK` | `U11` gate 1: AND(`H5`, `H4`) | high for dots 48–63 |
| `VBLANK` | `U11` gate 2: AND(`V8`, `V5`) | high for lines 288–311 |
| `GRIDH` | `U13` gate 2: NOR(`H21` = `H2` OR `H1`, `H0`) | high when dot mod 8 = 0 |
| `GRIDV` | `U13` gate 3: NOR(`V32` = `V3` OR `V2`, `V10` = `V1` OR `V0`) | high when line mod 16 = 0 |

`U4` `QC`, `QD` (`V6`, `V7`) drive nothing: the frame-end decode does not need them, since line 311 is reached before any value with them set. Unused gate inputs are tied to `GND`; the netlist loads without warnings.

## Board

Exported with `bw export-kicad`, placed by hand and routed with Freerouting in KiCad 10 (P2a, `8d89c52`; connectors added in P2b, `129fd25`). Chips in two rows, counters along the front (`U1` `U9` `U3` `U2` `U4` `U5`), decoders and gates behind; `J1` at the left, `J2` at the back.

- `bw check-board` clean; `drc_test_pattern` passes.
- Cracks (placed by pads and position since P4e):
  - `H0`: `U1.QA U13.2B 79%`.
  - `VCC` at `U6`: `U6.VCC U10.VCC 24%`, cutting `U6` pin 14 off.
  - `LINE_END`: `U9.6 U3.7 50%`, on the 17 mm F.Cu stretch from `U9` pin 6. `LINE_END` runs `U9` pin 6 → `U3` pin 7 → `U3` pin 10 → `U4` pin 7 → `U5` pin 7 on F.Cu, and `U9` pin 6 → `U7` pin 11 on B.Cu; this crack cuts the whole F.Cu chain off. A crack between `U3` pin 10 and `U4` pin 7 instead changes nothing visible: `U4` already gets the line end through `U3.RCO` on its `ENT`.

## Puzzles

`machine test_pattern` in each file (TEST_PATTERN.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `broken_trace.bwp` | `fault crack U1.QA U13.2B 79%` | `H0` |
| `dead_counter.bwp` | `fault stuck U3.QA 0` | `V0` |
| `stuck_gate.bwp` | `fault stuck U12.4Y 1` | `GRID` |
| `dead_rail.bwp` | `fault bridge U1.7 U1.8` | `VCC` to `GND` |
| `line_bridge.bwp` | `fault bridge U9.6 U9.7` | `LINE_END` to `GND` |
| `unpowered_chip.bwp` | `fault crack U6.VCC U10.VCC 24%` | `VCC` at `U6` |
| `sync_bridge.bwp` | `fault bridge U10.1 U10.2` | `HSYNC_N` to `VSYNC_N` |
| `zips.bwp` | `fault crack U9.6 U3.7 50%` | `LINE_END` at `U3` |

Titles and briefs:

- Missing grid lines: "The vertical lines of the test grid are gone; the horizontal ones are fine."
- Rolling picture: "The picture will not hold still: the grid repeats in short bands down the screen and rolls."
- White screen: "The whole picture area is white; no grid at all."
- Nothing works: "The screen is a blank white glow, nothing else. It worked yesterday."
- No horizontal lines: "Only the vertical grid lines show, from top to bottom; the horizontal lines are gone."
- Picture breaks up: "The grid breaks into short slanted dashes all over the screen and will not hold still."
- Torn picture: "The grid is there, but broken into slanted dashes that drift; the picture never holds still."
- Zips: "The grid has turned into ragged stripes, like zips, running down the whole screen; there are no horizontal lines."

Tests: `tests/machines/test_pattern_faults_test.cpp` (titles and briefs of all eight; dead counter, stuck gate, broken trace, zips: picture differs, probe lamps, right Repair restores the fault-free picture, wrong Repair does not) and `tests/machines/shorts_power_test.cpp` (the four P3f Puzzles fixed by their Repair; swapping `U9` does not fix the line bridge; a wire to +5 V powers `U6`; the Dead Rail limits current and reads 0 V). Crack helpers `H0Crack()` and `ZipsCrack()` in `tests/machine/test_points.{hpp,cpp}`.

## Electrical Nets on a damaged Board

- Missing grid lines: `U13` pin 5 alone on its island, no Driver: floating, reads high. `GRIDH` = NOR(`H21`, 1) is always low.
- Picture breaks up: `U6` pin 14 on a `VCC~2` island with no supply pin: `U6` is not Powered, its Pins get private Nets. `LINE_END_N` and `HSYNC_N` hold only inputs: floating, read high. `U2` is never reloaded (the line runs 256 dots) and there is no horizontal sync.
- Zips: `U3` pins 7 and 10, `U4` pin 7 and `U5` pin 7 share an island without a Driver: floating, read high. `U3` counts on every clock; `U4` on every `U3` carry, `U5` on every `U4` carry. `U7` still sees the real `LINE_END`, so `FRAME_END_N` only goes low if line value 311 meets dot 63, which happens at most once after power-on; then the counters run 0–511 every 512 dots. The grid decode follows `V0`–`V3` along each line, which draws the zips.
- Bridges join their two pads' islands in every build: a signal on `GND` (line bridge) is a Tie and wins; two outputs (sync bridge) are a Contention, 0 wins; `VCC` on `GND` (dead rail) is a Dead Rail.

## Manuals

Existing, unchanged:

- User: What it does (power on `J1`, TV on `J2`; a steady grid of 6 by 18 lines, locked within half a second).
- Service: Components, one bullet per reference with its Nets, `X1` to `J2`.

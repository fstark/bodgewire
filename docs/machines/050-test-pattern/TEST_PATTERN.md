# 050 — Test-pattern Generator

Fifth rung of the tutorial ladder. A board that drives a TV: a crystal and TTL counters make the sync and a white grid on black. It was the first playable Machine (P1c circuit, P2 Board and Faults, P3f power Faults); it needs no new code.

## Lesson

A picture is made by counting: a horizontal counter steps once per dot, a vertical counter once per line, and gates decode their values into sync, blanking and the grid. Every fault shows as a picture, and the shape of the picture points at the counter bit or decoder that is wrong: missing lines, bands, rolling, tearing. The signals are too fast and too many to read one by one, so the logic analyzer and the scope earn their place here.

## Circuit

PAL-like and progressive, one dot per clock:

| | Value |
|---|---|
| Dot clock | `X1` 1 MHz `OSC` → `CLK` (1 µs per dot) |
| Line | 64 dots = 64 µs (15.625 kHz) |
| Frame | 312 lines = 19.968 ms (≈ 50.08 Hz) |
| Active picture | dots 0–47, lines 0–287 |
| Horizontal sync | dots 52–55 |
| Vertical sync | lines 308–311 |
| Composite sync | horizontal XOR vertical, active low |

- Horizontal counter `H0`–`H5`: `U1` 74LS161 counts freely (`H0`–`H3`, carry `HC`); `U2` 74LS161 (`H4`, `H5`) counts on `HC`. `U6` gate 1 decodes dot 63 into `LINE_END_N`, which reloads `U2` with 0: the line is 64 dots.
- Vertical counter `V0`–`V8`: `U3`, `U4`, `U5` 74LS161, enabled by `LINE_END` (`U9` inverts `LINE_END_N`), so they count once per line. `U7` 74LS30 decodes line 311 at the line end into `FRAME_END_N`, which reloads them with 0: the frame is 312 lines.
- Sync: `U6` gate 2 makes `HSYNC_N` for dots 52–55 (with `H3_N` from `U9`); `U8` 74LS30 makes `VSYNC_N` for lines 308–311 (with `V3_N` from `U9`). `U10` 74LS86 mixes them: `SYNC` = `HSYNC_N` XOR `VSYNC_N`, inverted into `SYNC_N_D`.
- Picture: `U11` gates make `HBLANK` (dot ≥ 48) and `VBLANK` (line ≥ 288); `U13` NORs them into `ACTIVE`. `U12` ORs and `U13` NORs make `GRIDH` (every 8th dot) and `GRIDV` (every 16th line); `U12` ORs them into `GRID`; `U11` gates it with `ACTIVE` into `VIDEO_D`.
- Output: `U14` 74LS74 registers `SYNC_N_D` and `VIDEO_D` on the clock into `SYNC_N` and `VIDEO`, glitch-free and one dot late, to `J2` (the TV). `J1` power connector.
- No reset: from any power-on state the counters fall into step within 512 lines.

## When it works

The TV shows a steady white grid on black in the top-left part of the screen: 6 vertical lines and 18 horizontal ones. It locks within half a second and never rolls.

## Puzzles

| Puzzle | Fault | Symptom | Path to the fix |
|---|---|---|---|
| Missing grid lines | Broken Trace on `H0` between `U1` pin 14 and `U13` pin 5 | the vertical grid lines are gone; the horizontal ones are fine | `H0` pulses at `U1` pin 14 but floats (reads high) at `U13` pin 5, so `GRIDH` stays low: mend the Trace |
| Rolling picture | `U3` output `QA` (`V0`) stuck at 0 | the grid repeats in short bands down the screen and rolls | line 311 is never decoded: `V1`–`V3` count but `V0` stays low, and `FRAME_END_N` never goes low: swap `U3` |
| White screen | `U12` output `4Y` (`GRID`) stuck at 1 | the whole picture area is white | `GRIDH` and `GRIDV` pulse, `GRID` stays high: swap `U12` |
| Nothing works | Solder Bridge between `U1` pin 7 (`ENP`, on +5 V) and pin 8 (`GND`) | a blank white glow on the TV, nothing else; the current-limit LED is lit | `VCC` and `GND` are one Dead Rail: the meter reads 0 V everywhere, no chip is Powered. The bridge is on the solder side of `U1`: cut it |
| No horizontal lines | Solder Bridge between `U9` pin 6 (`LINE_END`) and pin 7 (`GND`) | only the vertical grid lines show, top to bottom | `LINE_END_N` pulses at `U9` pin 5, but `LINE_END` stays low, so the vertical counter never counts. It looks like a dead inverter, but swapping `U9` does not help: cut the bridge |
| Picture breaks up | Broken Trace on `VCC` that cuts `U6` pin 14 off | the grid breaks into short slanted dashes all over the screen and will not hold still | `U6`'s inputs pulse, its outputs `LINE_END_N` and `HSYNC_N` float; the meter reads 0 V at `U6` pin 14: mend the Trace (or wire pin 14 to +5 V) |
| Torn picture | Solder Bridge between `U10` pin 1 (`HSYNC_N`) and pin 2 (`VSYNC_N`) | the grid is there but broken into drifting slanted dashes; the picture never holds still | `HSYNC_N` and `VSYNC_N` are identical (0 wins), so the XOR never sees them differ and `SYNC` stays low: no sync. Cut the bridge |
| Zips | Broken Trace on `LINE_END` between `U9` pin 6 and `U3` pin 7 (cuts `U3` pins 7 and 10, `U4` pin 7, `U5` pin 7 off) | ragged stripes like zips run down the whole screen; no horizontal lines | `LINE_END` pulses at `U9` pin 6 and `U7` pin 11, but floats (reads high) at `U3`'s `ENP` and `ENT`, so `V0` counts on every dot: mend the Trace |

Netlist, Board, Puzzle files and manuals: [TEST_PATTERN_DESIGN.md](TEST_PATTERN_DESIGN.md). History: the circuit was specified in P1c ([TEST_PATTERN.md](../../p1c-test-pattern/TEST_PATTERN.md)), the Board laid out in P2a, the first three Puzzles authored in P2d, the four power and bridge Puzzles in P3f.

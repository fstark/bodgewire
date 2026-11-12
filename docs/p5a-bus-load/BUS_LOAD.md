# P5a Bus load spike — Specification

Roadmap row P5a ([ARCHITECTURE.md](../ARCHITECTURE.md), Roadmap). Row letters below follow the roadmap after P5a, which inserted P5b (kernel) and moved the later rows one letter on. Design: [BUS_LOAD_DESIGN.md](BUS_LOAD_DESIGN.md). Plan: [BUS_LOAD_PLAN.md](BUS_LOAD_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

P1d left the kernel at ~5× real time on the test pattern and deferred further work "until a BW-80-like load is measured" ([OPTIMISATION_IDEAS.md](../p1d-kernel-perf/OPTIMISATION_IDEAS.md), Next steps 1). P5 keeps buses as one Net per line (decided 2026-11-12), so a ZX80-class Machine puts 16 address, 8 data and the control lines on the event queue every machine cycle. Before any CPU work, this step builds that load with a stub CPU, measures it, and decides whether the kernel needs work and when.

Today (2026-11-12, release, M5 Max, `bw run --time 2s`): test pattern 5.5×, 090 typewriter 0.99× (75 M applied events in 2 s: ≈ 37 M applied events per wall second).

## What is new

- **`Z80STUB`**, a catalogue Part on the real Z80 DIP-40 pinout. It does not execute code: it plays a fixed script of Z80 machine cycles shaped like a ZX80 showing its display, with Z80 pin timing at 3.25 MHz (S1). P5j's real Z80 Part takes its place on the same pins.
- **DIP-40 package** (KiCad footprint, `kFootprints` row). P5c's 6502 needs it too.
- **`netlists/bus_load.bwn`**: a ZX80-shaped circuit around the stub, existing Parts only (S2). No Board, no manuals, no Puzzles: a benchmark netlist, not a Machine. It runs in `bw run` and in `bw_ui` (netlist-only Session), and its TV shows a page of text.
- **`roms/bus_load.rom`**: the 2716 contents (code-like bytes and an 8 × 8 character set), written by `roms/make_bus_load_rom.py` from `roms/2513.rom`.
- **Measurements and a decision** (S3, S4), recorded here and in OPTIMISATION_IDEAS.

## User decisions (2026-11-12)

| Question | Decision |
|---|---|
| The stub's bus traffic | a scripted ZX80 frame: display lines of M1 fetches from the display file with refresh, the NEWLINE HALT, an interrupt acknowledge, a few ROM and stack cycles; border lines of ROM fetches, RAM reads and writes; vertical sync from `IN`/`OUT` port `FE` |
| Chips around it | existing catalogue Parts only; two 74LS244s stand in for the resistor-isolated data bus (pass, force NOP), a third for writes. P5k models the real ZX80 chips |
| Threshold | ≥ 2× real time headless (release) means no kernel work. 1–2×: kernel work (idea 1, specialised evaluation) planned inside P5j, before BW-80. < 1×: kernel work becomes its own step before the 6502 Part |
| Afterwards | kept as a benchmark: `Z80STUB` and `netlists/bus_load.bwn` stay; P5j swaps the real Z80 in and measures again |

## Behaviour

### S1 The stub Z80

- Pins: the Z80's 40, datasheet names (`A0`–`A15`, `D0`–`D7`, `CLK`, `M1_N`, `MREQ_N`, `IORQ_N`, `RD_N`, `WR_N`, `RFSH_N`, `HALT_N`, `BUSAK_N`, `INT_N`, `NMI_N`, `WAIT_N`, `BUSRQ_N`, `RESET_N`, `VCC`, `GND`). `D0`–`D7` bidirectional. The inputs other than `CLK` are read by nobody (stated in its Datasheet).
- Evaluated on both `CLK` edges; drives its Pins with the Z80A datasheet's maximum delays (the only ones published).
- Plays one frame of 312 lines, 207 T-states a line (63.7 µs, 19.9 ms a frame, as the ZX80), then starts again. Cycle kinds with Z80 timing at the Pins: opcode fetch (M1, 4 T, refresh address `I:R` in T3–T4, `R` counting), memory read and write (3 T), `IN` and `OUT` (4 T), interrupt acknowledge (6 T), one internal T-state. Names in code are spelled out (`InterruptAcknowledge`).
- Frame (lines 0–311): lines 0–3 vertical sync (an `IN` from port `FE` opens it, an `OUT` closes it at the end of line 3, no interrupt); lines 4–51 and 244–311 border lines; lines 52–243 display lines, 24 rows of 8.
- A display line: 32 M1 fetches from the display file (`0xC000` + row × 33 + column), the NEWLINE (`0x76`, a HALT) fetch, 4 HALT fetches (`HALT_N` low), an interrupt acknowledge at T-state 148, one internal T-state, two stack writes, 10 ROM fetches and two stack reads.
- A border line: 10 × (fetch, RAM read; fetch, RAM write) and 2 fetches, then the same interrupt acknowledge and ROM routine. The writes walk the display file, writing the page (S2), so the picture is whole from the second frame whatever RAM held at power-on.
- `Z80Stub::Page()` gives the display file the writes store, for the tests.

### S2 The circuit

ZX80-shaped, about 25 chips: 6.5 MHz oscillator divided by 2 for the CPU clock; 2716 ROM; 2 × 2114 RAM (1K); an address decode on `A14`; the CPU data bus behind 74LS244s (read pass, NOP forced when an M1 fetch has `A15` high, `D6` low and `HALT_N` high, write pass); the character code and the display flag latched at the end of each M1 (2 × 74LS175); during refresh 74LS157s give the ROM the character code and the line counter (74LS93) in place of `A0`–`A8`; a 74LS165 loads the glyph line at the end of refresh (a 74LS74 marks the end of the refresh strobe) and shifts it at 6.5 MHz; `D7` inverts a character, applied from the end of its refresh; HSYNC from the interrupt acknowledge, ≈ 4.4 µs (74LS161); VSYNC from `IN`/`OUT` `FE`; composite sync and video to the `VIDEO` connector.

The TV shows 24 rows of 32 characters, black on white: a title on row 0, the 64 characters on the rows below, every other pair of rows inverted.

### S3 Measurements

All in the release build, on this machine, recorded in the plan's notes and summarised here under Results:

1. Headless: `bw run netlists/bus_load.bwn --time 2s`, best of 3: ratio, `applied_events`, `evaluations`; per simulated µs. Test pattern and typewriter run back to back as references.
2. Instructions: `/usr/bin/time -l` instructions retired, per applied event, against the test pattern's ~760 (OPTIMISATION_IDEAS, How to measure).
3. Where the time goes: `build/profile` sampled with `sample`, shares by function (kernel run loop, queue, Net resolution, each Part type, the stub).
4. Where the events are: applied events per Net group (address, data CPU side, data memory side, control, video chain, clocks), from a VCD of 20 ms or a counting test-only build.
5. `bw_ui netlists/bus_load.bwn --screenshot … --after 2s`: speed ratio and UI ms, TV open. Screenshots viewed.
6. Recording cost: headless with `--record A0,…,A7`, what 8 analyzer Channels on the bus would add (`bw_ui` clips need a Board).

### S4 Decision

From S3.1 against the threshold above. The decision, with its numbers and the projected BW-80 (real Z80 Part instead of the stub, about the same chip count), goes into ARCHITECTURE (Performance, roadmap rows P5a and the row that carries kernel work) and OPTIMISATION_IDEAS (Target, Next steps).

## Tests

- `parts: Z80STUB …`: M1 fetch pin order and delays, refresh address and `R` counting, read and write cycles (data driven, `WR_N`), `IN`/`OUT` and interrupt acknowledge strobes, 207 T-states a line, 312 lines a frame, the display file written by one frame's writes equals `Page()`.
- `bus load: …`: the netlist loads without warnings; the TV locks (it free-runs through the 4 lines of vertical sync, which carry no HSYNC as on the ZX80, and takes about 20 lines to fall back in step: ≈ 311 lines a frame, ≥ 90 % synced); from the second frame the picture is the page drawn through the ROM's glyphs, in two frames.
- Catalogue, footprint and datasheet tests cover `Z80STUB` and DIP-40 as for any Part.

## Results

Measured 2026-11-12, release, M5 Max ([BUS_LOAD_PLAN.md](BUS_LOAD_PLAN.md), notes, for the runs).

| | bus_load | test pattern | typewriter |
|---|---|---|---|
| Ratio, `bw run --time 2s`, best of 3 alternated | **0.22×** (0.20–0.22) | 5.4× | 0.93× |
| Applied events per simulated µs | 120 | 6 | 37 |
| Evaluations per simulated µs | 129 | 12 | 59 |
| Instructions per applied event | 644 | 768 | — |

- The per-event cost is the P1d kernel's; the load is 20× the test pattern's events per µs. The estimate in OPTIMISATION_IDEAS (100–200 M events/s for real time) holds: 120 M. At ≈ 26 M applied events per wall second the kernel is 4.6× short of real time and 9× short of the 2× threshold.
- Net changes per simulated µs (VCD, 5 ms of a running frame): clocks (`DOT`, `CLK`, `CLK_N`) 26; CPU data bus 18; memory data bus 17; CPU control 13; address 9; ROM address mux 9; decoding, video and sync 8.5; character latch 1.5. Total ≈ 102: the buses and their control are two thirds.
- Profile (`sample`, 6,700 samples): event queue `pop` 27 %, `push` 11 %; `runUntil`'s own code 25 %; Parts ≈ 30 % (ROM 7 %, gate packages and `GateFunction` 9 %, 74LS157 4 %, 74LS74 3 %, 74LS244 2 %, the stub 2 %, 2114 1 %); `driveChanged` and `markNetDirty` 5 %. P1d's heap held 8–15 events; this Circuit keeps many more pending (address and data bits with 100–450 ns delays), and the queue becomes the largest single cost.
- Recording 8 address Nets (`--record A0,…,A7`): no measurable cost (0.21× both ways).
- `bw_ui netlists/bus_load.bwn`: 0.21×, UI 0.0 ms; the page on the TV (screenshot viewed).

## Decision (2026-11-12)

0.22× is below 1×: **kernel work becomes its own step, P5b, before the 6502** (the user's threshold). Target: ≥ 2× on `netlists/bus_load.bwn` headless, about 10× from today. A real Z80 Part replacing the stub adds its own evaluation cost, so the target keeps its margin. First levers, from the profile: the event queue (a bucket queue or timing wheel, rejected in P1d for a small heap, to be measured again on this load), the run loop's own code, then specialised evaluation (OPTIMISATION_IDEAS idea 1). The stub and the netlist stay as the benchmark. P5b's terms were then set with the user (same day, ARCHITECTURE row P5b): the design must not bend for speed, so 2× is the aim, not a gate; ideas are tried one at a time under a trace check, and the user hears of each.

## Done when

- The tests above pass in debug; the full debug suite green at the end.
- S3 measured, S4 decided and written into ARCHITECTURE and OPTIMISATION_IDEAS.
- Release screenshots viewed: the page on the TV.

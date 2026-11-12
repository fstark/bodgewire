# P4q — Shift-register TV typewriter

Roadmap step P4q ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the 090 TV typewriter with its page in MOS shift registers, as in the 1973 original, instead of P4p's two 2114 RAMs. Memory without addresses: a cell is read or written only when it comes round. The Machine is specified in [TYPEWRITER.md](../machines/090-typewriter/TYPEWRITER.md). No Puzzles: P4r.

## The original

Don Lancaster's TV Typewriter (Radio-Electronics, September 1973, pp. 43–52, and the 24-page construction reprint) kept its page of 16 × 32 characters in six Signetics 2524 512-bit recirculating shift registers, one per bit, and the row being drawn in a Signetics 2518 hex 32-bit shift register, the "line register": "On line No. 1, our line register is connected to the memory and it samples the next 32 characters… For the next twelve scans, the memory is idle, but the line register brings the same characters back over and over again." The memory went round once a frame (512 clock pairs). A key was entered by switching the memory "from recirculate to update just this one character position" when the cursor's position came round; Clear wrote the space code into every cell for as long as it was held. The datasheets are in the 1972 Signetics MOS data book (bitsavers, `components/signetics/_dataBooks/1972_Signetics_MOS.pdf`): 2518 pp. 81–85, 2524 pp. 93–97.

## Decided with the user (2026-11-12)

- **The 2524 and the 2518**, the original's chips, with their datasheet pinouts.
- **The 2524 is dynamic: it loses its bits when its clocks stop.** If either clock has not pulsed for more than 10 ms, the bits go random (again every 10 ms while it stays stopped). The datasheet's guaranteed minimum clock rate is about 1 kHz at room temperature, its typical curve 100 times lower; 10 ms sits between, above the design's longest pause (4.5 ms). A lost clock shows as a lost page.
- **The 2524's two MOS clocks driven by TTL gates.** The original's 7406 and transistors shifting the clocks to +5/−12 V are left out, as the 2513's negative supplies are: the clocks are active-low pulses from gates, and the negative supply pins (2524 `VDD`, 2518 `VGG`) are not connected.
- **Ordinary outputs.** Both chips have "bare drain" outputs that only pull high, with a pull-down resistor to a negative supply on the board. They are modelled as ordinary outputs, no resistors; the datasheets say so.
- **A key waits for its cell with `KEYBOARD strobe=25ms`.** A key is written when its cell comes round, up to one frame (19.7 ms) after its strobe; the 25 ms strobe keeps its code on the keyboard's lines until then and paces the next key (at most 40 a second; the original about 30). No latch chips.
- **The RAM netlist is replaced** in place; P4p's version stays in git history (up to `1d24ed2`) and in its docs.
- **The P4p Board files are overwritten** by the new placement (`temp-freerouting.dsn` and `.history` removed, `.kicad_prl` kept).

## Decided while designing (2026-11-12), for the user to overrule

- **The page shifts on the first line of each text row** (lines 0, 16, … 240), 32 times, during characters 0–31: 512 a frame. That line shows glyph line 0, blank in every character, so the row buffer taking the row on it never shows. (The original used its line 1, blank too, with 12-line rows.)
- **Clocks from the dot counter.** `PHI1` (output clock) pulses on dot 11 and `PHI2` (input clock) on dot 13 of the character; the row buffer's clock is low on dots 12–15 and rises as the next character starts. Within a character each edge comes from one counter bit, and the gating signals change only while that bit holds the gate off, so none glitches.
- **The cursor is kept** (P4p's counter, comparator and blinking block), not the original's phase-shifted 512 counter: the comparator tells when the cursor's cell is at the page's output, on its row's top line. Return and the other control codes also wait for that moment, so every key takes the same path; a flip-flop clocked by `PHI2` steps the cursor and clears the key right after its cell.
- **Clear** holds the page's `WRITE` high with spaces on the write bus (P4p's second 74LS244) for as long as it is pressed: the page is blank after one frame.
- **Chips renumbered** in signal order (`U1`–`U40`): P4p had no routed Board and no Puzzles to keep.
- **DIP-8** joins the chip grid of `bw place` (a 2524 takes a DIP-16 slot).
- **The 2524's output moves only on `PHI1`** (a written bit is not seen until it comes round), as the real chip.
- **2524 timing**: the datasheet's maximum clock-to-output, 100 ns (no typical given). **2518**: the typical, 300 ns.
- **Step directory** `docs/p4q-shift-typewriter/`, Parts `ShiftRegister2524`, `ShiftRegister2518`.

## New code

1. **A random bit in `evaluate`.** `EvalContext::randomBit()`, as `PowerOnContext`'s: the 2524's decay draws from the Simulator's seeded generator.
2. **DIP-8 package.** The KiCad footprint vendored in `boards/footprints/`; a grid package in `bw place`.
3. **2524 512-bit dynamic recirculating shift register.** `part U1 2524`, DIP-8 (`PHI2` `OUT` `READ` `VDD` `WRITE` `IN` `PHI1` `VCC`). On the rise of `PHI2` the cell at the output is written from `IN` if `WRITE` is high, else kept (`OUT` does not change: the written bit comes out 512 clocks later); on the rise of `PHI1` the next cell comes to `OUT` (100 ns), or 0 while `READ` is low. MOS inputs, random contents at power-on, the 10 ms decay. Datasheet `2524.md`.
4. **2518 hex 32-bit static shift register.** `part U1 2518`, DIP-16 (`IN4` `IN5` `IN6` `REC` `VGG` `CLK` `OUT6` `OUT5` `OUT4` `OUT3` `OUT2` `OUT1` `IN1` `IN2` `IN3` `VCC`). On the rise of `CLK` each of the six registers puts the bit at its output back in (`REC` high, the datasheet's "Recirculate") or takes its `IN` (`REC` low), and the next bit comes out 300 ns later. MOS inputs, random contents at power-on. Datasheet `2518.md`.

Neither has a `GND` pin (P-MOS between `VCC` and a negative supply): the catalogue test allows that for these two. The 2114 and the 74LS244 stay in the catalogue for the CPU Machines.

## Machine

The 090 netlist redone as in [TYPEWRITER_DESIGN.md](../machines/090-typewriter/TYPEWRITER_DESIGN.md): the video side kept, the memory side the page, the row buffer and their clocks; grown in two tested stages (the video from the page; then keyboard, writes, cursor and Clear). Manuals rewritten; Board placed by `bw place` and routed by the user.

## Tests

- `EvalContext::randomBit`: a Component drawing in `evaluate` gives the same bits for the same seed, others for another.
- DIP-8: footprint pads and courtyard; KiCad footprint table; placed in the grid by `bw place`.
- 2524: catalogue row and pins (MOS, no `GND`); recirculates (512 clock pairs bring the contents back); a write replaces the cell at the output and only it; `READ` low gives 0; the output delay after `PHI1`; `PHI2` alone never moves the ring or `OUT`, and a write shows at `OUT` only 512 shifts later; `PHI1` alone shifts without writing; X on `WRITE` or `IN` read as a random bit; random contents at power-on, seeds differ; clocks stopped 9 ms keep the bits, 11 ms lose them.
- 2518: catalogue row and pins; 32 clocks bring the six registers round; `REC` low loads the inputs, each register its own; the output delay; random contents at power-on.
- `tests/machines/typewriter_test.cpp` (P4p's cases, read from the page instead of the RAMs): the netlist loads without warnings; on the TV, the page shown is the page's power-on contents; Clear leaves 0x20 in every cell and the cursor at (0, 0); keys land at the cursor; Return, other control keys; the line and page wrap; 33 keys queued at once all land; the typed page with a blinking cursor.
- The typewriter in the per-Machine checks: Board against netlist, KiCad DRC, Board scene, manuals; the Session test with every instrument and a key typed.
- Release build: headless speed (random and typed page), compared with P4p's (0.95× and 1.31×); `bw_ui` with every instrument, its ratio and UI time; a screenshot of a typed page with the keyboard window.

Design: [SHIFT_TYPEWRITER_DESIGN.md](SHIFT_TYPEWRITER_DESIGN.md). Plan: [SHIFT_TYPEWRITER_PLAN.md](SHIFT_TYPEWRITER_PLAN.md).

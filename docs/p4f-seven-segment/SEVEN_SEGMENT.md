# P4f — Seven-Segment Digits

Roadmap step P4f ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the code that the 040 stopwatch needs, and the tests of that Machine. The Machine itself is specified in [STOPWATCH.md](../machines/040-stopwatch/STOPWATCH.md).

Decided with the user (2026-11-08): three digits, 0.00–9.99 s; the 74LS90 as decade counter; separate Start, Stop and Reset buttons.

## New code

1. **74LS90 decade counter Part.** `part U1 74LS90`; DIP-14 with the datasheet pinout, `VCC` on pin 5 and `GND` on pin 10 (the first catalogue chip whose power Pins are not at the corners: the catalogue's pinout check learns the exception). Two sections: a ÷2 (`QA`, clocked by `CKA`) and a ÷5 (`QB`–`QD`, clocked by `CKB`), both on the falling edge; wired `QA` → `CKB` outside, it counts 0–9 in BCD. Asynchronous resets: `R01` and `R02` both high clear it to 0; `R91` and `R92` both high set it to 9, and win over the clear. Power-on: random state, illegal ÷5 values (5, 6, 7) included; they recover as the real chip's flip-flops do (5 and 6 go to 2, 7 to 0). Datasheet typical delays. Datasheet `74LS90.md`.
2. **74LS47 BCD-to-7-segment decoder Part.** `part U8 74LS47`; DIP-16, inputs `A`–`D`, `LT_N`, `RBI_N`, `BI_N`; outputs `SA_N`–`SG_N`, open collector, low = segment lit. The datasheet's glyphs for 0–15, 6 and 9 without tails, 15 blank. Lamp test (`LT_N` low) lights every segment; `BI_N` low blanks; `RBI_N` low blanks a 0. Pin 4 is `BI`/`RBO` on the real chip: modelled as the blanking input only (the ripple-blanking output, for chaining leading-zero blanking, is left out). An input at X makes X exactly the segments that the possible values disagree on. Datasheet `74LS47.md`.
3. **7-segment digit Part.** `part DS1 DIGIT`: a red 0.56-inch common-anode digit, 10 pins in the standard pinout (`E`, `D`, `CA1`, `C`, `DP`, `B`, `A`, `CA2`, `F`, `G`); KiCad footprint `Display_7Segment:7SegmentLED_LTS6760_LTS6780` (LTS-6960HR pinout). Electrically eight LEDs: each segment is lit while either anode pin reads a driven 1 and its own pin a driven 0, exactly like the LED (P4a): same 20 ms windows, half-lit when switching fast, dark when floating. Its two anode pins are joined inside the real part; the model lights a segment from either, but does not join their Nets. Datasheet `DIGIT.md`.
4. **LEDs generalised to lamps.** A catalogue Part lists its lamps (anode Pins, cathode Pin, shape): the LED has one round lamp, the digit seven segments and a decimal point. The Session watches and reports every lamp of every such Part, so later digit Parts (common cathode, other sizes) are a catalogue row.
5. **Digits drawn on the Board.** A dark digit body over the courtyard, with the eight segment shapes (slanted bars, decimal point) drawn lit or dim like the LED, upright along the courtyard's long side and standing on its pad 1 row, whatever the footprint's rotation. Unpowered: every segment dim.
6. **Falling-edge clocks.** `EvalContext::fell(pin)` beside `rose`, for the 74LS90.
7. **Watch budget.** `kMaxWatched` 32 → 48. The stopwatch's digits watch 25 distinct Nets (21 segment pins, three anode Nets, `GND` under `DS1`'s decimal point); with every instrument attached the worst case is 37, plus one while a lead moves. Lamps on unconnected Pins (the unused decimal points) are not watched. Every ring is allocated up front, about 1 MB each: 48 MB per Session.
8. **Unconnected-input warning for TTL only.** The netlist builder's "unconnected: floats high" warning is only true of TTL inputs; LED and digit pins (MOS-family loads) left unconnected are just dark, so they no longer warn (the unused decimal points).

Nothing else is new: the stopwatch's 74LS74, 74LS00, oscillator, buttons, resistors and its Faults (Broken Traces, stuck outputs, an unpowered chip) already exist.

## Machine

The stopwatch is built as in [STOPWATCH_DESIGN.md](../machines/040-stopwatch/STOPWATCH_DESIGN.md): netlist, Board (laid out by the user in KiCad), manuals, six Puzzles.

## Tests

- Unit tests for the 74LS90 (÷2 and ÷5 on falling edges, BCD count with the outside link, reset and set-9 priority, recovery from 5–7, delays, X), the 74LS47 (every glyph 0–15, lamp test, blanking, `RBI_N`, X per segment, open-collector outputs) and the digit (pins, lamps).
- `LedGlow` with two anodes; `LedConsumer` with digit lamps; the digit segment shapes upright for each footprint rotation.
- `tests/machines/stopwatch_test.cpp`: fault-free (Reset shows 0.00; a Start–Stop of 123.4 ms shows 0.12, readings taken mid-count; Start continues after Stop; a run past one second shows the seconds); each Puzzle shows its Symptom and is fixed by its Repair (STOPWATCH.md, Puzzles).
- The stopwatch joins the per-Machine checks: Board against netlist, KiCad DRC, Board scene, manuals; a Session test with every instrument attached (watch budget).
- Release build: 1.00× with every instrument attached; a screenshot of the digits counting after `--press`.

Design: [SEVEN_SEGMENT_DESIGN.md](SEVEN_SEGMENT_DESIGN.md). Plan: [SEVEN_SEGMENT_PLAN.md](SEVEN_SEGMENT_PLAN.md).

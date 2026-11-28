# 045 — Knob Meter

Between the stopwatch and the test pattern on the tutorial ladder. A paddle controller is plugged into the Board; two 7-segment digits show how far its knob is turned, from 00 at the left stop to 99 at the right, ten times a second. Its code comes with roadmap step P4i ([PADDLE.md](../../../docs/p4i-paddle/PADDLE.md)).

## Lesson

A knob's position reaches the logic as a time: the controller answers each trigger with one pulse, as long as the knob is turned (a pot and a 555 one-shot, logic-only). Measuring that time is counting: a known clock is let through a gate only while the pulse lasts, and the count is the position. The count is only meaningful at the end of the pulse, so a latch takes it then and holds it for the digits while the counter is cleared and counts again. Three stages, three ways to fail: the trigger and the controller (nothing measured), the counter and its clear (wrong or wandering numbers), the latch and the decoders (wrong shapes). The controller sits off the Board, but it runs on the Board's power through its Connector, like a chip.

## Circuit

- `X1` 1 MHz `OSC` → `CLK`.
- Timebase, five 74LS90 decade counters, each wired as a ÷10 (`QA` → `CKB`), counting on the falling edge at `CKA`; `R01`, `R02`, `R91`, `R92` at 0 V (never cleared):
  - `U1` → `U2` → `U3` → `U4`: `F100K`, `F10K`, `F1K`, `F100` (their `QD`; `F500K`, `F50K`, `F5K`, `F500` are the `QA` → `CKB` links). `F10K` is the count clock: high for 20 µs every 100 µs (a 74LS90's `QD` is high for counts 8 and 9 only).
  - `U5` steps every 10 ms through 0–9, a measuring cycle of 100 ms. `U5` `QB` (`TB1`) and `QC` (`TB2`) are both high in steps 6 and 7: they clear the counters through their `R01` and `R02`. `U5` `QD` falls at the step from 9 to 0: it is `TRIG_N`, the controller's trigger.
- `J2` paddle controller connector (`PADDLE`): `TRIG_N` in, `OUT` (`PULSE`) out, `VCC` and `GND` from the Board. Each fall of `TRIG_N` starts one high pulse on `OUT`: 30 µs with the knob at its left stop, 9.93 ms at its right stop, in proportion between. A fall during a pulse is ignored. The controller works only while its `VCC` and `GND` pins are on +5 V and 0 V.
- `U6` 74LS00:
  - gate 1: `GCLK` = NAND(`F10K`, `PULSE`): falls at each rise of `F10K` while the pulse lasts, high otherwise;
  - gate 2: `LATCH` = NAND(`PULSE`, `PULSE`): rises when the pulse ends;
  - gates 3 and 4 unused, inputs at 0 V.
- Counters, two 74LS90 as ÷10, `R91`/`R92` at 0 V, `R01` on `TB1`, `R02` on `TB2`: `U8` units (`GCLK` → `C0`–`C3`), `U7` tens (`C3` → `C4`–`C7`). The count is the number of `F10K` rises during the pulse: 0 at the left stop, 99 at the right (a count of 100 would show 00).
- Latches, two 74LS175 quad D flip-flops on `LATCH` (rising edge), `CLR_N` at +5 V: `U9` holds the tens (`C4`–`C7` → `L4`–`L7`), `U10` the units (`C0`–`C3` → `L0`–`L3`). `Q_N` outputs unused.
- Decoders, 74LS47, `LT_N`, `RBI_N`, `BI_N` at +5 V: `U11` for the tens (`L4`–`L7`), `U12` for the units (`L0`–`L3`).
- Digits `DS1` (tens) and `DS2` (units), left to right: red 0.56-inch common-anode digits, the two anode pins on one Net `DSn_CA` fed through `R1` (`DS1`) and `R2` (`DS2`), 330 Ω; each segment pin on its decoder output (`DSn_A` … `DSn_G`); decimal points unconnected.
- `J1` power connector.

One measuring cycle, from the step 9 → 0 of `U5`:

| Time | What happens |
|---|---|
| 0 | `TRIG_N` falls; the pulse starts about 0.1 µs later, just after a fall of `F10K` |
| 80 µs, then every 100 µs | `F10K` rises; while the pulse lasts, the units counter counts |
| end of the pulse (≤ 9.93 ms) | `LATCH` rises: `U9` and `U10` take the count; the digits change |
| 60–80 ms | `TB1` and `TB2` both high: the counters are cleared |
| 100 ms | the next cycle |

A pulse of `t` gives the count ⌊(`t` + 20 µs + 0.2 µs) / 100 µs⌋ (the 0.2 µs: the pulse starts about 0.2 µs after the `F10K` fall, through three 74LS90 ripples and the controller's 0.1 µs): with the knob at a fraction `k` of its travel, the display reads ⌊99 `k` + 0.502⌋, that is about the knob's percentage. Half way lands exactly on a count boundary without the 0.2 µs and reads 50 thanks to it.

## When it works

- The display follows the knob: 00 at the left stop, 99 at the right stop, 50 half way, changing ten times a second. A still knob gives a steady number.
- At power-on the counters, latches and timebase hold random values: for up to 0.2 s the display may show any number or odd shapes, then the reading.
- The 74LS47 draws 6 and 9 without their tails.

## Puzzles

| Puzzle | Fault | Symptom | Path to the fix |
|---|---|---|---|
| Dead knob | Broken Trace on `VCC` that cuts `J2` pin 3 off | the display shows the same thing from power-on, whatever the knob does | `TRIG_N` pulses at `J2` pin 1, but `PULSE` floats (reads high) at `J2` pin 2 and `U6`, `LATCH` stays low; `J2`'s `VCC` pin has no +5 V (the controller runs on the Board's power): mend the Trace |
| Runaway | `U5` output `QC` (`TB2`) stuck at 0 | the number keeps going up by itself, faster the further the knob is turned; at the left stop it stands still | the pulse and `LATCH` look right, but the counters are never cleared: `TB1` pulses at `U7`/`U8` `R01`, `TB2` is always low at `R02` and at `U5` pin 8: swap `U5` |
| Single figures | Broken Trace on `C3` between `U8` pin 11 (`QD`) and `U7` pin 14 (`CKA`) | the left digit always shows 0; turning the knob up, the right digit goes round 0 to 9 again and again | `C3` falls at `U8` pin 11 and `U10` pin 13 at each carry, but floats (reads high) at `U7` pin 14: mend the Trace |
| Odd tens | dry Joint on `U11` pin 7 (`A`, `L4`) | the left digit never shows an even number: the left stop reads 10, a quarter turn (25) reads 35 | turn the knob until the true tens are odd (10 % to 19 %): `L4` goes high on `U11` pin 7's pad and at `U9` pin 2, and low again below 10 %, but the leg reads high whatever the pad says (floating): resolder `U11` pin 7 |
| Four to seven | Solder Bridge between `U12` pins 2 (`C`, `L2`) and 3 (`LT_N`, +5 V) | the right digit only shows 4, 5, 6 or 7, or an odd shape where 8 and 9 should be; the left digit is right | `L2` is always high at `U12` pin 2 and `U10` pin 10 while `U8` `QC` (`C2`) counts; continuity from `L2` to +5 V; the blob between pins 2 and 3 on the solder side: cut it |
| Ones and zeros | Two Faults: a dry Joint on `U7` pin 1 (`CKB`, `C4`), and `U11` input `C` (`L6`) stuck at 0 inside the chip | the left digit only shows 0 or 1 (35 % reads 15, 80 % reads 19); once resoldered, right up to 39, then the tens 4 too low (47 % reads 07, 80 % reads 39) | the tens' divide-by-5 never counts: `C4` toggles on `U7` pin 1's pad, the leg floats: resolder it. That kept the tens below 2, hiding the second Fault: `L6` now goes high on `U11` pin 2, leg and pad, but the digit ignores it: swap `U11`. A masking double, close to Single figures but not the same |
| Eight and up | Two Faults: a dry Joint on `U12` pin 6 (`D`, `L3`), and `U10` output `4Q` (`L3`) stuck at 1 | the right digit shows 8 or 9 for 0 or 1, the shapes of 10–14 for 2–6, nothing for 7; the same after either Repair alone | `U12` pin 6's leg floats while its pad reads `L3`: resolder it; nothing changes, since `L3` is high at `U10` pin 15 while `C3` pulses at pin 13: swap `U10`. A same-symptom double, the Machine's one |

Netlist, Board, Puzzle files and manuals: [KNOB_METER_DESIGN.md](KNOB_METER_DESIGN.md).

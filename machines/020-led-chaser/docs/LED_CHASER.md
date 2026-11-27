# 020 — LED Chaser

Second rung of the tutorial ladder. One lit LED runs along a row of eight, about 15 steps a second. Its code comes with roadmap step P4b ([SHIFT_REGISTER.md](../../../docs/p4b-shift-register/SHIFT_REGISTER.md)).

## Lesson

A shift register passes a value from stage to stage on each clock. A fault in one stage shows on the LEDs from that stage on; a fault in the feedback stops new values from entering. Compare stages side by side with the analyzer.

## Circuit

- `X1` 1 MHz `OSC` → net `CLK` → clock pins of `U1`–`U4`.
- `U1`–`U4`: four 74LS161 chained as in the counter (bits `C0`–`C14`, carries `CARRY1`–`CARRY3`). The top bit, `U4.QD`, is net `STEP`: about 15.3 Hz.
  - All `ENP` and `LOAD_N` high, data inputs at 0 V, `CLR_N` on `RESET_N` pulled up by `R1`.
- `U5` 74LS164 8-bit shift register, clocked by `STEP`, cleared by `RESET_N`. Outputs `S1`–`S8` (`QA`–`QH`).
  - Serial input `A` on net `SER`; `B` tied high.
- `U6` 74LS30 8-input NAND: inputs `S1`–`S7`, the eighth tied high; output `SER`.
  - `SER` is low only while `S1`–`S7` are all high: a single low bit is fed in each time the previous one reaches `S8`. Any power-on state settles into one low bit within 8 steps.
- `D1`–`D8` red LEDs, wired as in the counter: +5 V → `R2`…`R9` → `Dn` anode (net `LED_An`) → cathode on `Sn`. An LED is lit while its stage is low.
- `J1` power connector.

## When it works

- `STEP` rises every 65.5 ms. On each rise the low bit moves one stage on: `D1`, `D2`, … `D8`, then `D1` again.
- Exactly one LED is lit at a time; a full run takes 0.52 s.
- Just after power-on, a few LEDs may be lit together for up to half a second.

## Puzzles

| Puzzle | Fault | Symptom | Path to the fix |
|---|---|---|---|
| Gap | Broken Trace on `LED_A5` between `R6` and `D5` | the light skips `D5` | `D5` anode floats while `S5` still goes low in turn: mend the Trace |
| Stuck light | `U5.QD` (`S4`) stuck at 0 | `D4` lit for good, the others dark | `STEP` pulses; `S4` never goes high; `SER` stays high: swap `U5` |
| Dark chaser | Broken Trace on `SER` between `U6` pin 8 and `U5` pin 1 | the light runs off the end and never comes back | `U6` pin 8 is low, `U5` pin 1 floats (reads high): mend the Trace |
| Twins | Solder Bridge between `U5` pins 3 and 4 (`S1`, `S2`) | `D1` and `D2` light together, for two steps | `S1` and `S2` identical on the analyzer; the bridge is seen when zoomed in on the solder side: cut it |
| Half row | Broken Trace on `VCC` between `R5` pin 1 and `R6` pin 1 | the light runs `D1`–`D4`, then four dark steps; `D5`–`D8` never light | misleading: it looks like a four-stage row, but `U5.QE`–`U5.QH` still go low in turn; `D5`–`D8` pin 2 and both ends of `R6`–`R9` float while `R5` pin 1 reads H: mend the `VCC` Trace |

Netlist, Board, Puzzle files and manuals: [LED_CHASER_DESIGN.md](LED_CHASER_DESIGN.md).

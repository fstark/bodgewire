# LED Chaser — Service Manual

## How it works

**Clock.** The crystal oscillator `X1` makes a 1 MHz square wave on its pin 8 (`OUT`), net `CLK`. It goes to the clock pins of the four counters.

**Reset.** `R1` pulls `RESET_N` up to +5 V, so the counters and the shift register are never cleared. Nothing pulls it low on this board.

**Divider.** `U1` to `U4` are four 4-bit counters chained into one 16-bit counter, bits `C0` (`U1` pin 14) to `C14` (`U4` pin 12), carries `CARRY1` to `CARRY3`. The top bit, `U4` pin 11 (`QD`), is net `STEP`: it rises once every 65 536 clocks, every 65.5 ms (about 15.3 Hz).

**Shift register.** `U5` shifts on each rise of `STEP`: every stage takes the value of the one before it, `S1` (`U5` pin 3) to `S8` (`U5` pin 13), and `S1` takes the serial input `SER` on `U5` pin 1. Pin 2 (`B`) is tied high.

**Feedback.** `U6` is an 8-input NAND on `S1` to `S7`, its eighth input tied high. Its output, `SER` on `U6` pin 8, is low only while `S1` to `S7` are all high: each time the low bit reaches `S8`, a new low bit goes into `S1` on the next step. Whatever the stages hold at power-on, after 8 steps exactly one of them is low.

**LEDs.** `R2` to `R9` feed +5 V into the anodes of `D1` to `D8` (nets `LED_A1` to `LED_A8`); each cathode sinks into its stage, `D1` into `S1` up to `D8` into `S8`. An LED lights while its stage is low.

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `CLK`
- `J1` power connector: +5 V and 0 V for the whole board
- `R1` pull-up: holds `RESET_N` high so the counters and `U5` run
- `R2` LED resistor: limits the current through `D1`
- `R3` LED resistor: limits the current through `D2`
- `R4` LED resistor: limits the current through `D3`
- `R5` LED resistor: limits the current through `D4`
- `R6` LED resistor: limits the current through `D5`
- `R7` LED resistor: limits the current through `D6`
- `R8` LED resistor: limits the current through `D7`
- `R9` LED resistor: limits the current through `D8`
- `U1` divider bits 0–3: counts on every clock, carry `CARRY1` to `U2`
- `U2` divider bits 4–7: counts when `U1` carries, carry `CARRY2` to `U3`
- `U3` divider bits 8–11: counts when `U2` carries, carry `CARRY3` to `U4`
- `U4` divider bits 12–15: counts when `U3` carries; its top bit is `STEP`
- `U5` shift register: moves `S1`–`S8` one place on each rise of `STEP`
- `U6` feedback: 8-input NAND, feeds a new low bit into `SER` when `S1`–`S7` are all high
- `D1` LED: lit while `S1` is low
- `D2` LED: lit while `S2` is low
- `D3` LED: lit while `S3` is low
- `D4` LED: lit while `S4` is low
- `D5` LED: lit while `S5` is low
- `D6` LED: lit while `S6` is low
- `D7` LED: lit while `S7` is low
- `D8` LED: lit while `S8` is low

## Troubleshooting

Follow the signal from the step clock to the LEDs; the fault is where it stops.

- Check `U5` pin 8 (`CLK`, `STEP`): a rise every 65.5 ms. If not, walk the divider: `X1` pin 8, then the carries on pin 15 of `U1`, `U2` and `U3`.
- Check `U5` pin 9 (`CLR_N`): it must be high. If low, the stages are held at 0.
- Check `SER` at `U5` pin 1 and at `U6` pin 8: the two must be the same, low for one step out of eight. If `U6` pin 8 never goes low, one of `S1`–`S7` never goes high.
- Compare each stage with the next on the analyzer, `S1` (`U5` pin 3) against `S2` (`U5` pin 4) and so on: each must be the one before it, one step later. A stage that breaks the pattern, or two stages that are always the same, point at `U5` or at its pins.
- An LED that stays dark while its stage goes low: check its anode. It must be at +5 V through its resistor; if it floats, the Trace from the resistor is broken.

# Stopwatch

## What it does

Welcome to the Stopwatch. It times things to a hundredth of a second, like the watch a coach holds at the track.

Three red digits show the time. `DS1` shows the seconds, `DS2` the tenths, `DS3` the hundredths. The decimal point after the seconds is always lit, so the display reads 0.00 to 9.99. After 9.99 it starts again from 0.00.

The board has three buttons: START, STOP and RESET.

## Using it

- START (`SW1`) starts the count. Pressed while the watch is already running, it does nothing.
- STOP (`SW2`) freezes the count. START then goes on from the same time.
- RESET (`SW3`) sets the display back to 0.00. If the watch was running, it counts on from 0.00 when you let go.

While it runs, the hundredths change too fast to read: that digit looks like a blur of bars. The tenths flicker. The seconds are steady.

When the board is switched on, the chips wake up in a random state. The display may show any number, or odd shapes, and it may already be counting. Press STOP, then RESET, and it reads 0.00.

The digits draw a 6 without its top bar and a 9 without its bottom bar. That is how this display chip draws them.

## How it works

**The goal.** The watch must add one to the hundredths 100 times a second. The board has a clock that ticks a million times a second. So it slows the clock down 10 000 times, and counts what is left.

**The clock.** `X1` ticks one million times a second, on `CLK`.

**The buttons.** Each button works like a doorbell push. A resistor holds its wire high. Pressing the button connects the wire to 0 V, so it goes low.

- START pulls `START_N` low. `R1` holds it high.
- STOP pulls `STOP_N` low. `R2` holds it high.
- RESET pulls `RST_N` low. `R3` holds it high.

**Running or stopped.** `U11` remembers whether the watch runs. Think of a light with two switches: one only turns it on, the other only turns it off. START sets `U11`'s output `RUN` high. STOP sets it low. It stays that way after you let go.

**The clock gate.** `U12` lets the clock through only while `RUN` is high. Its output is `GCLK`. While the watch runs, `GCLK` ticks a million times a second. While it is stopped, `GCLK` stays high and nothing counts.

**One chip counts from 0 to 9.** A 74LS90 holds a number from 0 to 9. It shows it on four outputs, `QA` to `QD`. It adds one each time its input goes from high to low. After 9 it goes back to 0. At that moment its last output, `QD`, goes from high to low. That is the signal for the next chip.

Inside, the 74LS90 is two smaller counters: one counts to 2, the other to 5. A wire outside the chip, from `QA` to `CKB`, joins them into one that counts to 10. Read the datasheet: this chip takes +5 V on pin 5, not on pin 14 like most.

**Slowing the clock down.** `U1`–`U4` are four of these counters in a chain. Each one passes on one tick for every ten it gets. Like gears, each step turns ten times slower than the one before.

- `U1` gets `GCLK`, a million ticks a second. It passes on 100 000 a second, on `F100K`.
- `U2` passes on 10 000 a second, on `F10K`.
- `U3` passes on 1 000 a second, on `F1K`.
- `U4` passes on 100 a second, on `F100`.

**The time.** Three more counters hold the time. They work like a car's odometer: each is one wheel, and a wheel turns only when the one before it rolls over.

- `U7` holds the hundredths, on `H0`–`H3`. It counts the ticks on `F100`.
- When `U7` goes from 9 to 0, it moves `U6` on by one, through `H3`. `U6` holds the tenths, on `T0`–`T3`.
- When `U6` goes from 9 to 0, it moves `U5` on, through `T3`. `U5` holds the seconds, on `S0`–`S3`.
- `U5`'s carry goes nowhere. So after 9.99 comes 0.00.

**Reset.** `U12` also turns the low `RST_N` into a high `RESET`. `RESET` goes to every counter, `U1`–`U7`. While it is high, they all hold 0.

**From a number to a digit.** A counter gives a number in binary, on four wires. The digit needs to know which of its seven bars to light. A 74LS47 decoder translates one into the other. There is one per digit: `U8` for the seconds, `U9` for the tenths, `U10` for the hundredths.

**The digits.** Each digit is seven LED bars and a decimal point. Their tops, the anodes, all get +5 V through one resistor: `R4` for `DS1`, `R5` for `DS2`, `R6` for `DS3`. The bottom of each bar goes to its decoder. A bar lights while the decoder pulls its pin low. When a bar must stay dark, the decoder lets go of its pin: it drives it neither high nor low. `DS1`'s decimal point goes straight to 0 V, so it is always lit.

**Pins with nothing to do.** These chips have features this board does not use. Their pins are tied so they stay out of the way:

| Chip | Pin | What it can do | Here |
|---|---|---|---|
| 74LS90 | R91, R92 | set the count to 9 | tied low: never sets |
| 74LS47 | `LT_N` | light every bar, to test the digit | tied high: never tests |
| 74LS47 | `BI_N`, `RBI_N` | blank the digit, or blank a leading 0 | tied high: never blanks |
| 74LS74 | `1D`, `1CLK` | store a bit at a clock edge | tied low: `U11` uses only its set and clear inputs |
| 74LS74 | second flip-flop | a second memory | inputs tied, outputs unused |
| 74LS00 | gates 3 and 4 | two more gates | inputs tied low, outputs unused |

The Service Manual lists what the probe shows on a working board.

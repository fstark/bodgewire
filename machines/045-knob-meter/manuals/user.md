# Knob Meter

## What it does

Welcome to the Knob Meter. A paddle controller is plugged into `J2`: a small box with a knob, on a cable. The two digits show how far the knob is turned. They read 00 at the left stop and 99 at the right stop. In between, the reading is about the knob's percentage: half way reads 50.

The reading is renewed ten times a second, so it follows the knob as you turn it. A still knob gives a steady number.

## Using it

The knob sits in its own box on the bench, labelled `J2`. There are three ways to turn it:

- drag it up or down with the mouse: up turns it right, towards 99;
- roll the mouse wheel over it: each notch moves the reading by about one;
- hold the Up or Down key: going from one stop to the other takes about a second.

Just after power-on, the digits may show any number, or odd shapes. After a fifth of a second they show the knob.

## How it works

**The goal.** Logic chips cannot read a knob directly. They only know high and low. So the controller turns the knob's position into a time, and the board measures that time by counting.

**The controller.** Think of someone holding a door open for as long as you ask. The board asks; the controller holds a wire high for a while, then lets it go. The further the knob is turned, the longer it holds. At the left stop the pulse lasts 30 µs. At the right stop it lasts almost 10 ms. The board asks on `TRIG_N`, `J2` pin 1. The pulse comes back on `PULSE`, `J2` pin 2. The controller runs on the board's +5 V, through `J2` pin 3.

**The clock.** `X1` ticks one million times a second. Four counter chips, `U1`–`U4`, each divide by ten, like the Counter's odometer wheels. `U2`'s output, `F10K`, ticks 10 000 times a second: once every tenth of a millisecond. It is the count clock. `U4`'s output, `F100`, ticks 100 times a second.

**The measuring cycle.** `U5` steps from 0 to 9 on `F100`, one step every 10 ms. One round is a tenth of a second: one measurement. It plays three roles:

- When it steps from 9 back to 0, its output `U5.QD` falls. That is `TRIG_N`: the controller starts a pulse.
- In steps 6 and 7, its outputs `U5.QB` and `U5.QC` are both high. Those wires, `TB1` and `TB2`, clear the counters to 0, ready for the next round.
- The rest of the time it just waits.

**Counting the pulse.** It is like counting "one, two, three…" while the door stays open. The gate `U6` lets the count clock through only while `PULSE` is high. What comes out is `GCLK`, on `U6.1Y`. The units counter `U8` counts it from 0 to 9. When it goes round from 9 to 0, it tells the tens counter `U7` on `C3`. A pulse of almost 10 ms lets about 99 ticks through. A pulse of 30 µs lets none through.

**Holding the reading.** The count is right only once the pulse has ended. So a second gate of `U6` watches for that moment. When `PULSE` falls, its output `LATCH` rises. At that instant the two latches copy the count: `U9` takes the tens, `U10` the units. They hold it for the whole tenth of a second. Meanwhile the counters are cleared and count again. So the digits never show the count running up.

**The digits.** Each latch feeds a decoder: `U11` for the tens digit `DS1`, `U12` for the units digit `DS2`. A decoder turns a number into the seven segments of its shape. It lights a segment by pulling its pin low. These decoders draw 6 and 9 without their tails.

**Pins with nothing to do.** The chips have features this board does not use. Their pins are tied so they stay out of the way:

| Pin | What it can do | Here |
|---|---|---|
| R01 and R02 of `U1`–`U5` | clear the counter to 0 | tied low: the timebase never stops |
| R91 and R92 of every counter | set the counter to 9 | tied low: never used |
| `CLR_N` of `U9`, `U10` | clear the latch | tied high: never clears |
| `LT_N`, `BI_N`, `RBI_N` of `U11`, `U12` | light every segment, darken them all, darken a 0 | tied high: the digits always show the number |
| `Q_N` outputs of `U9`, `U10` | the inverse of each held bit | not connected |
| gates 3 and 4 of `U6` | two spare gates | inputs tied low |

The Service Manual lists what the probe shows on a working board.

# Dice

## What it does

Welcome to the Dice. Seven red LEDs, `D1`–`D7`, sit in the pattern of a die face. Hold the button and the dice rolls. Let go and it stops on a face, from 1 to 6.

Behind the roll is a counter. It counts from 1 to 6 a million times a second, far too fast for the eye. When you let go, it stops wherever it is. That is your throw.

## Using it

- `ROLL` (`SW1`): hold it to roll. All seven LEDs glow while you hold it, some brighter than others. Let go, and the LEDs show one face until the next roll.
- Every face is as likely as any other. Nobody can let go at the right millionth of a second.
- When the board is switched on, the LEDs may show any pattern: a face, all seven, or none. The first roll sorts it out.

## How it works

**The goal.** The board picks a number from 1 to 6. It shows it as the spots of a die. The player's hand decides when to stop, so nobody can predict the number.

**The clock.** `X1` ticks one million times a second, on `CLK`.

**The button is a gate on the clock.** Think of a tap on a pipe. `U4`'s first gate is the tap.

- While `ROLL` is up, `R9` holds `BTN_N` high. The gate's output, `ROLL_CLK`, stays low. No ticks get through.
- While `ROLL` is held, `SW1` pulls `BTN_N` to 0 V. The gate now lets the ticks through, on `ROLL_CLK`, to `U1`.

**The counter runs from 1 to 6.** `U1` is a 74LS161, a chip that holds a number and adds one at each tick. Its outputs `QA`, `QB` and `QC` show the number as three wires, `N0`, `N1` and `N2`. They are worth 1, 2 and 4: the number is the sum of the wires that are high.

On its own, the chip would count on to 15. This board stops it at 6. At 6, `SIX_N` goes low. That tells `U1` to load a new number at the next tick instead of adding one. The number it loads is 1, set by its `A`–`D` pins. So the count goes 1, 2, 3, 4, 5, 6, 1, 2… and starts again every six millionths of a second.

**The decoder turns the number into spots.** Look at a die. Its spots come in four groups. Each group answers one question about the number:

| Group | LEDs | Lit for | Its wire | Made by |
|---|---|---|---|---|
| centre spot | `D4` | 1, 3, 5: the odd numbers | `ODD_N` | `U2` turns `N0` upside down |
| one diagonal | `D1`, `D7` | 2 and more | `TWO_N` | `U4` gives low as soon as `N1` or `N2` is high |
| other diagonal | `D2`, `D6` | 4 and more | `FOUR_N` | `U2` turns `N2` upside down |
| middle row | `D3`, `D5` | 6 only | `SIX_N` | `U3` gives low only while `N1` and `N2` are both high |

Add the groups up and you get every face. A 3 is the centre and the diagonal of `D1` and `D7`. A 6 is both diagonals and the middle row.

**The LEDs.** Each LED gets +5 V through its own resistor, `R2`–`R8`. Its other end goes to its group's wire. An LED is lit while that wire is low.

While you roll, the groups switch on and off a million times a second. The eye sees them all glowing at once. The middle row is lit only one tick in six, so it looks dimmest. `D1` and `D7` are lit five ticks in six, so they look brightest.

**Pins with nothing to do.** The board does not use every feature of its chips. Those pins are tied so they stay out of the way:

| Pin | What it can do | Here |
|---|---|---|
| `U1.ENP`, `U1.ENT` | pause the count | tied high: never pauses |
| `U1.CLR_N` | reset the count to 0 | held high on `RESET_N` by `R1`: never resets |
| `U1.QD`, `U1.RCO` | show the 8s place, warn a next chip | not connected |
| the spare gates of `U2`, `U3`, `U4` | other logic | inputs tied to 0 V |

The Service Manual lists what the probe shows on a working board.

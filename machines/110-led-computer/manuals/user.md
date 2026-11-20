# LED computer

## What it does

Welcome to the LED computer, the first real computer on the bench. A row of eight LEDs sits at the front. One light runs along it from right to left, one LED about every eighth of a second. It bounces off `D8` at the left end and runs back to `D1`. Then it starts again, for ever.

- The processor is `U1`, a 6502, ticking a million times a second.
- The program sits in `U2`, a 2K memory chip that keeps its contents with the power off.
- The program's notes go in the RAM, `U3` and `U4`.
- The display is the row of LEDs, driven by `U7`.

## Using it

RESET (`SW1`) restarts the computer. While you hold it, the LEDs go dark. When you let go, the light starts again at `D1`.

To read the program, click the window on the EPROM `U2`: the binder opens at Contents, the program as 6502 code from $F800, or in hex.

## How it works

**The goal.** The light must move by one LED about eight times a second. Nothing on the board knows how to do that by itself. A program does it, one small instruction at a time. The board's job is to let the processor read that program, keep a few notes, and show a byte on the LEDs.

**The clock.** `X1` ticks one million times a second, on `PHI0`. Each tick is one step of the processor: it reads or writes one byte.

**The reset.** At power-on, `U8` holds `RES_N` low for 20 thousandths of a second. That gives the power time to settle. While `RES_N` is low, the processor waits and the LEDs stay dark. When it goes high, the processor starts its program from the beginning. RESET (`SW1`) pulls the same wire low by hand.

**The processor.** `U1` works like a cook following a recipe. It reads one instruction, does it, then reads the next. To read or write, it puts a number on the 16 address lines, `A0`–`A15`. That number says where. The byte itself travels on the 8 data lines, `DB0`–`DB7`. The `RW` line says which way: high to read, low to write. `SYNC` goes high each time the processor reads a new instruction.

**The address decoder.** Every chip sits on the same address and data lines. Only one may answer at a time. `U5` picks it, like the house numbers on a street. It looks at the top three address lines, `A13`–`A15`, and pulls one select line low:

| Addresses | Select | Chip |
|---|---|---|
| $0000–$1FFF | `RAM_N` | the RAM, `U3` and `U4` |
| $8000–$9FFF | `LATCH_N` | the LED latch, `U7`: written only |
| $E000–$FFFF | `ROM_N` | the program, `U2` |
| anything else | none | nothing answers |

The decoder only looks at the top lines. So a chip answers at more than one address. The RAM holds 1024 bytes, at $0000–$03FF. It answers again at $0400, $0800, and so on up to $1FFF. The program is 2048 bytes, at $F800–$FFFF. It answers again from $E000.

**The program.** `U2` holds the program. It drives the data lines whenever `ROM_N` is low. In words, the program does this:

1. At the start, light `D1` and choose "moving left".
2. Write the light to the LEDs.
3. Wait about an eighth of a second, by counting down a long time.
4. Move the light one LED left or right.
5. At `D8` or `D1`, turn round.
6. Go back to step 2.

The waiting takes almost all the time. So the processor reads the program on almost every tick, and `ROM_N` is low nearly all the time.

**The RAM.** The program keeps two notes: which LED is lit, and which way it moves. They live at $0000 and $0001. The processor also keeps a return address in the RAM each time it goes off to wait. A byte has 8 bits, but each RAM chip holds only 4. So `U3` keeps the low half of every byte, on `DB0`–`DB3`. `U4` keeps the high half, on `DB4`–`DB7`. Both answer together when `RAM_N` is low.

**The write strobes.** A RAM must be told when to store a byte. `U6` makes that signal. During a write, the processor puts the byte on the data lines in the second half of the tick. `U6` pulls `WR_N` low just for that half. That is the write strobe.

**The LED latch.** The LEDs must keep showing the light while the processor does other things. `U7` holds the byte, like a note pinned on a board. When the processor writes to $8000, `U6` makes one more pulse, `LATCH_CLK`. At the end of that write, `LATCH_CLK` rises. `U7` copies the data lines onto its outputs, `L0`–`L7`, and keeps them there. `RES_N` clears it, so the LEDs are dark during a reset.

**The LEDs.** `D1` shows `L0`, the lowest bit. `D8` shows `L7`, the highest. Each LED is lit while its bit is 1. The light moves when the program writes a new byte, once every eighth of a second.

**Pins with nothing to do.** Some features are not used here. Their pins are tied so they stay out of the way:

| Pin | What it can do | Here |
|---|---|---|
| `U1.IRQ_N`, `U1.NMI_N` | interrupt the program | held high by `R10` and `R11`: never used |
| `U1.RDY` | pause the processor | tied high: never pauses |
| `U2.OE_N` | switch the program's outputs off | tied low: `ROM_N` alone decides |
| `U1.A11`, `U1.A12` | address lines | not connected: no chip needs them |

The Service Manual lists what the probe and the logic analyzer show on a working board.

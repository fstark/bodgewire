# Heartbeat

## What it does

Welcome to the Heartbeat, the first board on the bench with a processor. Its processor is `U1`, a 6502. The board has no memory and no program. It still runs.

Sixteen LEDs sit in a row at the front, `D16` on the left and `D1` on the right. Together they show the address the processor is reading, as a binary number. The left LED blinks about eight times a second. The next one blinks twice as fast. The one after that flickers, and the rest glow.

That blinking is a heartbeat. It tells you the processor is alive: it has power, a clock, and an address bus that counts. Repairers check this first on any dead computer.

## Using it

- RESET (`SW1`): press it to restart the processor. While you hold it, the LEDs freeze. When you let go, the processor starts again at the same address every time, and the count carries on from there.
- When the board is switched on, the processor starts at a random address. It runs without a reset anyway.

## How it works

**The goal.** The board shows that a processor works, with nothing else around it. The processor gets a clock and a harmless instruction to run, over and over. If it is healthy, its address counts up for ever. The LEDs show that count.

**A processor is a chip like the others.** It has a clock coming in, like the counters on other boards. It has an address going out, on sixteen pins, `A0`–`A15`. At each tick of the clock, it puts an address on those pins and reads a byte back on its eight data pins. The byte tells it what to do next. Everything else on a computer board is there to answer those reads.

**The clock.** `X1` ticks one million times a second, on `PHI0`. Each tick is one read.

**The data bus always says "do nothing".** On a real computer, memory answers each read. Here, eight resistors answer instead. `R17`–`R24` hold the data lines `DB0`–`DB7` at a fixed pattern: some pulled up to +5 V, some down to 0 V. That pattern is the 6502's NOP instruction, "no operation". So every byte the processor reads, wherever it reads it, says "do nothing, go on to the next one".

**So the address counts.** Think of a reader turning pages of a book in which every page says "turn the page". The processor fetches a NOP, does nothing, and fetches the next one. Each NOP takes two ticks, so the address goes up by one every two millionths of a second. After the last address, it wraps round to the first and starts again.

**The address lines are a binary counter.** `A0` is the fastest line: it changes every time the address goes up. Each line above changes half as often as the one below. `A15`, the slowest, is high for half the round and low for the other half.

**The buffers and LEDs.** A processor pin is too weak to light an LED. `U2` and `U3` are buffers: each copies eight address lines and gives them the strength to drive an LED. `U2` copies `A0`–`A7`, `U3` copies `A8`–`A15`. Each LED is lit while its address line is high. `D16` shows `A15`, so it blinks about eight times a second. The low lines change so fast the eye sees their LEDs glow at half brightness.

**Reset.** `R27` holds `RES_N` high. RESET pulls it to 0 V, and the processor stops. When you let go, it reads its starting address from two fixed places, as every 6502 does. Here those reads return the NOP pattern too. So it always starts at the same address, $EAEA, and counts on from there.

**Pins with nothing to do.** The processor and the buffers have features this board does not use:

| Pin | What it can do | Here |
|---|---|---|
| `U1.IRQ_N`, `U1.NMI_N` | interrupt the program | held high by `R25` and `R26`: never interrupts |
| `U1.RDY` | pause the processor | tied to +5 V: never pauses |
| `U1.SO` | set a flag from outside | tied to +5 V: never used |
| `U1.RW` | tell memory whether this is a read or a write | not connected: there is nothing to write to |
| `U1.SYNC` | mark the fetch of each instruction | not connected, but worth probing |
| `U1.PHI1`, `U1.PHI2` | give the clock to other chips | not connected |
| `U2.1OE_N`, `U2.2OE_N`, `U3.1OE_N`, `U3.2OE_N` | switch the buffers off | tied to 0 V: always on |

The Service Manual lists what the probe shows on a working board.

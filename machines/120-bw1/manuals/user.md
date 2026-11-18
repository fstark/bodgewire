# BW-1

## What it does

Welcome to the BW-1, the first computer on the bench you can program yourself. It is built like the KIM-1 of 1976. You type a program in with the keypad, run it, stop it and step through it.

- The processor is `U1`, a 6502, ticking a million times a second.
- The main memory holds 1024 bytes, in eight chips, `U5`–`U12`.
- The program that runs the keypad and the display is the monitor. It lives in `U2`.
- A self-test lives in `U3`. It checks the board when the monitor cannot run.
- The display is six red digits, `DS1`–`DS6`. Four show an address, two show the byte stored there.

At power-on the digits show 0200 and a byte. The byte is whatever the memory held when the power came on.

## Using it

**The keypad.** It sits at the front right, as on the KIM-1:

| GO | ST | RS | SST |
|---|---|---|---|
| AD | DA | PC | + |
| C | D | E | F |
| 8 | 9 | A | B |
| 4 | 5 | 6 | 7 |
| 0 | 1 | 2 | 3 |

SST is a slide switch, not a key. Each key acts once per press. Let go of a key before you press the next.

**Choosing an address.** Press AD, then type hex digits. Each digit comes in from the right and pushes the others left. AD 0 3 0 0 shows 0300 and the byte there.

**Changing a byte.** Press DA, then type hex digits. They go into the byte at the address shown, and into memory at once. DA A 9 stores A9.

**The next address.** + moves to the next address. It keeps the mode: after DA, you can type the next byte straight away.

**Typing a program.** Programs usually start at 0200. Press AD 0 2 0 0. Press DA and type the first byte. Press + and type the next byte. Go on until the program is in.

**Running it.** Show the program's first address, for example AD 0 2 0 0, then press GO. While a program runs, the display goes dark: only the monitor lights it.

**Stopping it.** A program stops by itself at a BRK instruction, the byte 00. The display then shows the address two past the BRK. ST stops a program wherever it is, and shows the address of the next instruction. PC goes back to that address after you have looked elsewhere. GO carries on from there.

**One step at a time.** Slide SST to ON. Each GO now runs one instruction of your program, then stops and shows the next address. Slide it back to OFF to run at full speed.

**Starting again.** RS resets the computer. The digits show 0200 again. The memory keeps what you typed.

**From the PC keyboard.** Click the keypad or the shelf's keyboard first, or press F2. Then the keys 0–9 and A–F are the hex keys. Enter is GO, + is +, / is AD, * is DA, P is PC. Escape is ST and Backspace is RS.

**The self-test.** Run it when the BW-1 will not start, or behaves oddly. It checks the memory, the two programs and the display. It uses no memory itself, so it runs even when the monitor cannot.

- To start it, run a Bodge Wire from `U24` pin 4 to `U2` pin 2, then press RS or turn the power on. `U24` pin 4 is a bare pad, `STRAP`; `U2` pin 2 is on `PA0`. Remove the wire to get the monitor back.
- A working monitor also starts it: AD 1 8 0 0 GO. SST must be at OFF.
- While it runs, the first digit shows the test. 1 is the main memory, $0000–$03FF. 2 is the memory in `U2`, $17C0–$17FF. 3 is the memory in `U3`, $1780–$17BF. 4 is the program in `U2`. 5 is the program in `U3`.
- Then every segment of every digit lights: 888888. Then each digit lights alone.
- Last comes the keypad test. The digits show PA55, and the code of the key you hold: 0–F for the hex keys, 10 for AD, 11 DA, 12 +, 13 GO, 14 PC. With no key held, nothing shows after PA55.

**When the self-test finds a fault**, it stops and keeps the result on the digits:

- The first digit is the test that failed.
- The next three digits are the last three hex digits of the address.
- The last two digits are the bad bits. Each 1 is a bit that came back different from what was written.
- For a program, the three digits are its first address, and the last two are its sum. A good program sums to 00.

Some examples. 123408 means address $0234, bit 3. 27D501 means address $17D5, bit 0. 4C0001 means the program in `U2` sums to 01.

Each main memory chip holds one bit of every byte. `U5` holds bit 7, `U6` bit 6, and so on down to `U12`, bit 0. One bad bit at every address points at one chip. Several bits at once point at something they share. `U13` passes bits 0 to 3, `U14` bits 4 to 7.

## How it works

**The goal.** A computer must read a program, keep notes, and talk to you. On the BW-1 that means a processor, a few memory chips, and one display that also reads the keys. The tricky part is that every chip shares the same wires. The board's job is to decide who talks, and when.

**The clock.** `X1` ticks one million times a second, on `PHI0`. The processor sends the tick back out, as `PHI2`. In each tick, it reads or writes one byte. The second half of the tick, while `PHI2` is high, is when the byte is on the wires. `U2`, `U3` and the memory writes all follow `PHI2`.

**The reset.** At power-on, `U25` holds `RES_N` low for 20 thousandths of a second. That gives the power time to settle. Then the processor starts the monitor from the beginning. RS pulls the same wire low by hand.

**The processor.** `U1` works like a cook following a recipe. It reads one instruction, does it, then reads the next. It puts a number on the address lines, `A0`–`A12`, to say where. The byte travels on the data lines, `D0`–`D7`. `RW` says which way: high to read, low to write. `SYNC` goes high each time it reads a new instruction. The 6502 has three more address lines, A13 to A15, but they go nowhere on this board.

**The address decoder.** Only one chip may answer at a time. `U4` picks it, like the house numbers on a street. It looks at `A10`–`A12` and pulls one of four select lines low:

| Addresses | Select | What answers |
|---|---|---|
| $0000–$03FF | `K0` | the main memory, `U5`–`U12` |
| $1700–$173F | `K5` | `U3`'s ports: not used |
| $1740–$177F | `K5` | `U2`'s ports: the display and the keypad |
| $1780–$17BF | `K5` | `U3`'s memory, 64 bytes |
| $17C0–$17FF | `K5` | `U2`'s memory, 64 bytes |
| $1800–$1BFF | `K6` | `U3`'s program: the self-test |
| $1C00–$1FFF | `K7` | `U2`'s program: the monitor |
| anything else | none | nothing answers |

`K5` wakes both `U2` and `U3`. Each one then looks at a few low address lines to see which part of it is meant. The decoder ignores A13 to A15. So the whole map repeats every 8K. When the 6502 looks for its start address near $FFFC, it gets the monitor's, at $1FFC.

**The main memory.** Each memory chip holds 1024 bits, one for each address. Eight of them side by side make 1024 bytes, like eight pages of the same ledger. `U5` keeps bit 7 of every byte, `U12` bit 0.

- Writing: the byte goes straight into the chips, on `D0`–`D7`. `U15` pulls `RAM_RW` low only during the second half of a write tick, when the byte is ready.
- Reading: the chips' outputs, `DOUT0`–`DOUT7`, pass through two gates, `U13` and `U14`. `U15` opens them, on `BUF_N`, only while the processor reads from $0000–$03FF. The rest of the time they stay off the data lines.

**The two 6530s.** A 6530 is four things in one chip. It holds a 1K program, 64 bytes of memory, two 8-bit ports and a timer. `U2` holds the monitor. Its memory keeps the addresses the processor jumps to on ST and BRK. Its ports drive the display and read the keypad. `U3` holds the self-test. Its ports are not wired.

**The display.** Six digits, but only one is lit at a time. The monitor switches from one to the next every half thousandth of a second. That is far too fast for the eye: it sees all six at once, like the frames of a film.

- `U2` sends a digit's segments on `PA0`–`PA6`. `U17` boosts them onto `SEG_A`–`SEG_G`. These seven wires go to every digit.
- `U2` sends the digit's number on `PB1`–`PB4`. `U24` turns that number into one low line, `DIG1` to `DIG6`. Only the digit whose line is low lights up.
- A full round of six digits takes about three and a half thousandths of a second. That is about 290 rounds a second.

**The keypad.** It shares the same wires. After each round of digits, the monitor turns `PA0`–`PA6` round to listen. Then it picks the key rows one by one, through `U24` again: `ROW0`, `ROW1`, `ROW2`. Each key joins one row to one column. So a held key pulls its column low while its row is picked. The monitor knows the key from the row and the column.

| Row | Keys, from `PA6` down to `PA0` |
|---|---|
| `ROW0` | 0 1 2 3 4 5 6 |
| `ROW1` | 7 8 9 A B C D |
| `ROW2` | E F AD DA + GO PC |

ST and RS are not in the grid. ST pulls `NMI_N` low: that tells the processor to stop and jump into the monitor. RS pulls `RES_N` low.

**Single step.** `U15` watches each instruction read. It pulls `SST_N` low when the instruction comes from outside the monitor's program, that is, when `K7` is high. At ON, SST joins `SST_N` to `NMI_N`. So the processor stops after each instruction of your program, but never inside the monitor.

**The monitor in words.** At reset, it first checks the self-test strap. It selects `U24` pin 4 and reads `PA0`. If the strap pulls it low, it jumps to the self-test at once, before using any memory. Otherwise it sets up its notes in memory and shows 0200. Then it loops:

1. Read the address and the byte there. Show them, one digit at a time.
2. Scan the keypad.
3. Wait until every key is up, then until one is down. Check it twice.
4. Do what the key says: change the address, change the byte, step on, or GO.
5. Go back to step 1.

GO puts back the processor's notes and jumps to the address shown. ST or a BRK saves the notes and comes back into the loop.

**Pins with nothing to do.** Some features are not used here:

| Pin | What it can do | Here |
|---|---|---|
| `U1.IRQ_N` | interrupt the program | held high by `R7`: never used |
| `U1.RDY`, `U1.SO` | pause the processor, set a flag | tied high |
| A13 to A15 | reach more memory | not wired |
| `U2.PA7`, `U2.PB0`, `U2.PB5`, `U2.PB7` | a teletype and the timer's interrupt | not wired |
| `U3`'s ports | anything | not wired |
| `U4`'s outputs 1–4, 8 and 9 | select more chips | not wired |
| the digits' decimal points | a dot | not wired |

The Service Manual lists what the probe shows on a working board.

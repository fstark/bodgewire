# 110 — LED computer

The first computer: a 6502 running a program from ROM, with RAM for its variables and stack, an address decoder, and an output latch driving 8 LEDs. The program bounces one light from end to end. Its code came with roadmap step P5d ([ROM_PROGRAMS.md](../../p5d-rom-programs/ROM_PROGRAMS.md)).

## Lesson

A computer is a processor plus chips that each answer at their own addresses. The decoder turns the top address lines into one select per chip; the program, the RAM and the output port all share the same 16 address and 8 data lines. When the light misbehaves, the question is which of them did not answer, or answered when it should not have. Reading a write strobe on the scope (a short pulse at the end of a write cycle) and the data bus at that instant is the habit every later computer needs.

## Circuit

- `X1` 1 MHz `OSC` → `PHI0` of `U1`, the 6502.
- `U8` power-on reset holds `RES_N` low for 20 ms after power-on, so the processor starts at its reset vector; `SW1` (Reset) pulls the same line low; `R9` pulls it up. `IRQ_N` and `NMI_N` pulled up by `R10`, `R11`; `RDY` and `SO` on `VCC`.
- `U5` 74LS138 decodes `A15`–`A13` into eight 8K blocks: block 0 (`$0000`–`$1FFF`) selects the RAM (`RAM_N`), block 4 (`$8000`–`$9FFF`) the LEDs (`LATCH_N`), block 7 (`$E000`–`$FFFF`) the ROM (`ROM_N`). Partial decoding: each chip answers at several addresses in its block (mirrors), as on cheap real boards.
- `U2` 2716 EPROM (2K) holds the program, at `$F800`–`$FFFF` and its mirrors; enabled by `ROM_N`.
- `U3`, `U4` 2114 static RAMs (1K × 4 each): `U3` the low half of each byte (`DB0`–`DB3`), `U4` the high half (`DB4`–`DB7`); selected by `RAM_N`, written while `WR_N` is low.
- `U6` 74LS02 makes the write strobes: `WR` high in the second half of a write cycle (`PHI1` low and `RW` low), `WR_N` its opposite for the RAMs, and `LATCH_CLK`, which rises at the end of a write to the LED block.
- `U7` 74LS273 latches the data bus on `LATCH_CLK`'s rise and drives `L0`–`L7`; `RES_N` clears it, so the LEDs are dark during reset.
- `D1`–`D8` LEDs, `D1` on `L0` … `D8` on `L7`, each lit while its bit is 1: anode on `Ln`, cathode through `R1`–`R8` (330 Ω) to `GND`. They sit in a row at the front, `D8` on the left.
- `J1` power connector.

## The program

`roms/led_computer.asm`, assembled by `bw asm` into `roms/led_computer.rom`:

1. Reset: the stack pointer to `$FF`, decimal mode off, the light at bit 0 (`light`, zero page `$00`), moving left (`dir`, `$01`, = 0).
2. Loop: write `light` to the LEDs (`STA $8000`); call `delay` (about 125 ms: two nested countdowns in X and Y); move `light` one bit left or right by `dir`; at bit 7 or bit 0, flip `dir`.
3. The IRQ and NMI vectors point at an `RTI` (neither line is ever pulled low).

So every step needs the ROM (the code), the RAM (the two variables, and the return address `JSR` pushes on the stack at `$01FE`–`$01FF`), the decoder (three selects), the strobe gates and the latch.

## When it works

- At power-on the LEDs stay dark for 20 ms, then `D1` lights and the light moves left one LED about every 125 ms, bounces off `D8`, comes back to `D1`, and so on: a full round trip in 1.75 s.
- `PHI0` 1 MHz; `SYNC` high on each opcode fetch; `RW` low only on writes: two stack writes per step (the `JSR`), one to `$8000`, and the writes to `light` and `dir`.
- `LATCH_CLK` pulses once a step: low from about 60 ns after `PHI0` rises in the `STA $8000` write cycle, rising about 80 ns after `PHI0` falls at its end.
- `ROM_N` low on almost every cycle (the delay loop runs from ROM); `RAM_N` low only on the few stack and zero-page cycles of each step; `LATCH_N` low only in the `STA $8000` write cycle.
- Holding `SW1`: the LEDs go dark; on release the light starts again at `D1`.

## Puzzles

Seven Puzzles (P5h), each a different link. Four of them leave the LEDs dark: alike from outside, the instruments tell them apart.

- Lights out: `LATCH_CLK` cracked at `U7`: dark LEDs with the program running. `LATCH_CLK` pulses at `U6`, not at `U7`.
- Sleeper: the power-on reset `U8` stuck low: a dead computer that the Reset button cannot wake. `RES_N` low, the clock fine.
- Full house: `RAM_N` bridged to `VCC` at `U5`: the RAM never answers, the variables read `$FF`, every LED lit. A new `U5` does not help.
- Three lights: `LATCH_N` bridged to `WR_N` at `U6`: every write also loads the latch, the LEDs show the stack (`$13`, the low byte of `JSR`'s return address). A new `U6` does not help.
- First light: a stuck cell in the stack (`U4`, bit 7 of `$1FF`): the first `RTS` goes astray and the program starts again, so `D1` lights again and again.
- Lightless: `U3` ignores its `A0` (an internal address line): `light` and `dir` share a cell, `sta dir` clears the light, the LEDs stay dark. Of the 2114's address lines only `A0` shows with this program; the others alias words it never uses.
- Blackout: `U2` reads `CE_N` stuck high: the program is never read and the CPU wanders. Mostly dark; on some power-ons a random pattern freezes on the LEDs.

Files, Fault lines and Nets: [LED_COMPUTER_DESIGN.md](LED_COMPUTER_DESIGN.md#puzzles).

Netlist, program, Board and manuals: [LED_COMPUTER_DESIGN.md](LED_COMPUTER_DESIGN.md).

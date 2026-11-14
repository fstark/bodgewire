# 100 — Heartbeat

First Machine with a processor: a 6502 on a board with no memory at all, every byte it reads a NOP. It is the "free-run" test a technician does first on a dead 6502 computer: if the CPU, its clock and its address bus work, the address counts up for ever and the top address lines blink. Its code came with roadmap step P5c ([MOS6502.md](../../p5c-6502/MOS6502.md)).

## Lesson

A processor is a chip like the others: clock in, address out. Before suspecting a program or a memory, check that the CPU free-runs: power, clock, reset, and an address bus that counts. Reading the address lines as a binary number (and seeing each one at half the rate of the one below) is the habit every later computer needs.

## Circuit

- `X1` 1 MHz `OSC` → `PHI0` of `U1`.
- `U1` 6502. Its data bus (pins `D0`–`D7`, Nets `DB0`–`DB7`) is held at `$EA` (NOP) by eight resistors `R17`–`R24`, to `VCC` for a 1 bit and to `GND` for a 0: nothing else drives it, so every opcode and operand read is `$EA`. Each NOP takes two cycles (the fetch, then a dummy read of the next address), so the address advances by one every 2 µs and wraps after 131 ms.
- `IRQ_N` and `NMI_N` pulled up by `R25`, `R26` (3.3 kΩ, as the data sheet asks); `RDY` and `SO` on `VCC`.
- `RES_N` pulled up by `R27`; `SW1` (Reset) pulls it to `GND`. On release the CPU reads its reset vector, `$EAEA`, and starts there.
- `U2`, `U3` 74LS244, always enabled: `A0`–`A7` and `A8`–`A15` buffered to `LA0`–`LA15` (a 6502 output drives one TTL load, not an LED).
- `D1`–`D16` LEDs, `D1` on `A0` … `D16` on `A15`, each lit while its line is high: anode on `LAn`, cathode through `R1`–`R16` (330 Ω) to `GND`. They sit in a row at the front, `D16` on the left, so the row reads as the address in binary.
- `J1` power connector.

## When it works

- `PHI0` 1 MHz; `PHI2` follows it, `PHI1` is its opposite.
- `SYNC` high every other cycle (each fetch); `RW` always high (nothing is written).
- `A0` changes every 2 µs (the address goes PC, PC + 1, PC + 1, PC + 2, …), each line above at half the rate of the one below.
- `D16` (`A15`) blinks about 7.6 times a second, `D15` about 15, `D14` flickers; `D1`–`D13` glow at half brightness.
- Pressing and releasing `SW1`: the CPU runs its reset sequence and fetches from `$EAEA` (the LEDs jump to `1110 1010 1110 1010`, too fast to see, then count on).
- Power-on starts the CPU at a random address, so no two power-ons begin the same; it still free-runs without a reset.

## Puzzles

Three Puzzles (P5h), each a different link of the free-run test:

- Flatline: the clock cracked between `X1` and `U1`: every LED frozen. The clock is at `X1`'s pad, not at `U1`'s.
- Fifteen: `A14` cracked between `U1` and its buffer `U3`: `D15` always lit. The analyzer shows `A14` counting at `U1`, not at `U3`.
- Flutter: `U1`'s `D2` stuck at 1: every opcode is `$EE` (INC absolute), the address sits at `$EEEE` half the time, `RW` pulses although nothing should be written. The analyzer reads `$EE` on the data bus; a resistor is not to blame; swap `U1`.

A data line cracked shows nothing on this Machine: the floating line keeps the bit it had. Files, Fault lines and Nets: [HEARTBEAT_DESIGN.md](HEARTBEAT_DESIGN.md#puzzles).

Netlist, Board and manuals: [HEARTBEAT_DESIGN.md](HEARTBEAT_DESIGN.md).

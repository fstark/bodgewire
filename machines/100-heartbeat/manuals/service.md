# Heartbeat — Service Manual

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `PHI0` of `U1`
- `J1` power connector: +5 V and 0 V for the whole board
- `U1` processor: a 6502; every byte it reads on `DB0`–`DB7` is a NOP, so it reads one address after the next for ever, on `A0`–`A15`
- `U2` address buffer: copies `A0`–`A7` to `LA0`–`LA7`, always on
- `U3` address buffer: copies `A8`–`A15` to `LA8`–`LA15`, always on
- `SW1` RESET button: pulls `RES_N` low while pressed
- `D1` LED: lit while `A0` is high (`LA0`)
- `D2` LED: lit while `A1` is high (`LA1`)
- `D3` LED: lit while `A2` is high (`LA2`)
- `D4` LED: lit while `A3` is high (`LA3`)
- `D5` LED: lit while `A4` is high (`LA4`)
- `D6` LED: lit while `A5` is high (`LA5`)
- `D7` LED: lit while `A6` is high (`LA6`)
- `D8` LED: lit while `A7` is high (`LA7`)
- `D9` LED: lit while `A8` is high (`LA8`)
- `D10` LED: lit while `A9` is high (`LA9`)
- `D11` LED: lit while `A10` is high (`LA10`)
- `D12` LED: lit while `A11` is high (`LA11`)
- `D13` LED: lit while `A12` is high (`LA12`)
- `D14` LED: lit while `A13` is high (`LA13`)
- `D15` LED: lit while `A14` is high (`LA14`)
- `D16` LED: lit while `A15` is high (`LA15`)
- `R1` LED resistor: limits the current through `D1`
- `R2` LED resistor: limits the current through `D2`
- `R3` LED resistor: limits the current through `D3`
- `R4` LED resistor: limits the current through `D4`
- `R5` LED resistor: limits the current through `D5`
- `R6` LED resistor: limits the current through `D6`
- `R7` LED resistor: limits the current through `D7`
- `R8` LED resistor: limits the current through `D8`
- `R9` LED resistor: limits the current through `D9`
- `R10` LED resistor: limits the current through `D10`
- `R11` LED resistor: limits the current through `D11`
- `R12` LED resistor: limits the current through `D12`
- `R13` LED resistor: limits the current through `D13`
- `R14` LED resistor: limits the current through `D14`
- `R15` LED resistor: limits the current through `D15`
- `R16` LED resistor: limits the current through `D16`
- `R17` data pull-down: holds `DB0` low
- `R18` data pull-up: holds `DB1` high
- `R19` data pull-down: holds `DB2` low
- `R20` data pull-up: holds `DB3` high
- `R21` data pull-down: holds `DB4` low
- `R22` data pull-up: holds `DB5` high
- `R23` data pull-up: holds `DB6` high
- `R24` data pull-up: holds `DB7` high
- `R25` pull-up: holds `IRQ_N` high: no interrupts
- `R26` pull-up: holds `NMI_N` high: no interrupts
- `R27` pull-up: holds `RES_N` high while `SW1` is released

## Test points

What the logic probe shows on a working board. **P** is the pulse lamp: it stays lit while the signal keeps changing. Click a pin below with the probe in hand to probe it.

| Probe | Net | A working board shows |
|---|---|---|
| `U1.PHI0` | `PHI0` | P lit: the clock, a million ticks a second |
| `U1.RES_N` | `RES_N` | H, steady. RESET held: L, and every address line stops |
| `U1.D0`, `U1.D2`, `U1.D4` | `DB0`, `DB2`, `DB4` | L, steady |
| `U1.D1`, `U1.D3`, `U1.D5`, `U1.D6`, `U1.D7` | `DB1`, `DB3`, `DB5`, `DB6`, `DB7` | H, steady |
| `U1.IRQ_N`, `U1.NMI_N` | `IRQ_N`, `NMI_N` | H, steady |
| `U1.SYNC` | | P lit: high at every other tick, as each NOP is fetched |
| `U1.RW` | | H, steady: the processor only reads |
| `U1.A0`, `U2.1Y1` | `A0`, `LA0` | P lit: far too fast for the eye |
| `U1.A15`, `U3.2Y4`, `D16.A` | `A15`, `LA15` | H for about a sixteenth of a second, then L for as long. P stays lit: the changes come faster than its lamp goes out |
| Logic analyzer: shift-click `U1.A0` to clip the whole address bus; clip `U1.SYNC`, triggered on its rising edge, and `U1.RW` | `A0`–`A15` | sample every 100 ns, 2 µs a division. The bus lane counts up in hex, one step every 2 µs: 139F, 13A0, 13A1… The numbers differ from one run to the next. `SYNC` is high one microsecond in two. `RW` stays high |

## Troubleshooting

**Every LED is frozen.** The processor is not counting.

1. Probe `U1.PHI0`. It must show P. If it does not, probe `X1.OUT`, the clock's own output.
2. Probe `U1.RES_N`. It must read H while RESET is up.
3. Probe `U1.RDY`. It must read H.
4. Probe the data pins, `U1.D0` to `U1.D7`. Each must read as the table above says: L on `U1.D0`, `U1.D2` and `U1.D4`, H on the others.
5. Probe `U1.SYNC` and `U1.RW`. `SYNC` must show P, and `RW` must read H. If the clock, the reset and the data pins are right and these are not, suspect the processor itself.

**The LEDs change, but not as a count.** Some glow when they should blink, or `U1.RW` shows P.

1. Probe the data pins, `U1.D0` to `U1.D7`, as above. One wrong bit makes the processor run a different instruction.
2. Hook the logic analyzer on the address bus, as in Test points. A working bus goes up by one every 2 µs.

**One LED is wrong: always lit, always dark, or out of step.**

1. Find its address line in the Components list. Probe that line on `U1`. It must change half as often as the line below it.
2. Probe the same line at the buffer's input, then at its output. Each must read the same as at `U1`.
3. If the buffer's output is right, probe the LED's `A` pin, then its `K` pin. `K` must read L.

# LED computer — Service Manual

## Components

- `X1` clock: 1 MHz crystal oscillator, drives `PHI0` of `U1`
- `J1` power connector: +5 V and 0 V for the whole board
- `U1` processor: a 6502 running the program in `U2`; reads and writes on `A0`–`A15` and `DB0`–`DB7`, `RW` low while it writes
- `U2` program ROM: a 2716 at $F800–$FFFF (and again at each 2K from $E000), driving `DB0`–`DB7` while `ROM_N` is low
- `U3` RAM: the low half of each byte (`DB0`–`DB3`) at $0000–$03FF (and again up to $1FFF); answers while `RAM_N` is low, stores while `WR_N` is low
- `U4` RAM: the high half of each byte (`DB4`–`DB7`), as `U3`
- `U5` address decoder: from `A13`–`A15`, pulls one select low: `RAM_N` low for $0000–$1FFF, `LATCH_N` for $8000–$9FFF, `ROM_N` for $E000–$FFFF
- `U6` write strobes: `WR` high in the second half of a write, `WR_N` its opposite; `LATCH_WR` high while writing to the LEDs, `LATCH_CLK` its opposite, rising at the end of that write
- `U7` LED latch: copies `DB0`–`DB7` onto `L0`–`L7` at each rise of `LATCH_CLK`; `RES_N` low clears it
- `U8` power-on reset: holds `RES_N` low for 20 thousandths of a second after power-on, then lets go
- `SW1` Reset button: pulls `RES_N` low while pressed
- `D1` LED: lit while `L0` is high
- `D2` LED: lit while `L1` is high
- `D3` LED: lit while `L2` is high
- `D4` LED: lit while `L3` is high
- `D5` LED: lit while `L4` is high
- `D6` LED: lit while `L5` is high
- `D7` LED: lit while `L6` is high
- `D8` LED: lit while `L7` is high
- `R1` resistor: limits the current of `D1`
- `R2` resistor: limits the current of `D2`
- `R3` resistor: limits the current of `D3`
- `R4` resistor: limits the current of `D4`
- `R5` resistor: limits the current of `D5`
- `R6` resistor: limits the current of `D6`
- `R7` resistor: limits the current of `D7`
- `R8` resistor: limits the current of `D8`
- `R9` resistor: pulls `RES_N` up once the power-on reset and `SW1` let go
- `R10` resistor: holds `IRQ_N` high
- `R11` resistor: holds `NMI_N` high

## Test points

What the logic probe shows on a working board. **P** is the pulse lamp: it stays lit for a tenth of a second after each change. "Flickers" means P is lit most of the time, with a short dark gap about eight times a second: the signal changes only when the light moves. "Held" means RESET held down.

| Probe | Net | A working board shows |
|---|---|---|
| `X1.OUT`, `U1.PHI0` | `PHI0` | P lit |
| `U1.PHI1` | `PHI1` | P lit |
| `U8.RST_N`, `U1.RES_N`, `U7.CLR_N`, `SW1` pin 1 | `RES_N` | H, steady. Held: L, steady |
| `U1.SYNC` | `SYNC` | P lit. Held: L, steady |
| `U1.A0`, `U1.D0`, `U1.D7` | `A0`, `DB0`, `DB7` | P lit |
| `U1.A15`, `U1.RW` | `A15`, `RW` | H, and P flickers |
| `U5.Y7_N`, `U2.CE_N` | `ROM_N` | L, and P flickers |
| `U5.Y0_N`, `U3.CS_N` | `RAM_N` | H, and P flickers |
| `U5.Y4_N`, `U6.3A` | `LATCH_N` | H, and P flickers |
| `U6.2Y`, `U3.WE_N` | `WR_N` | H, and P flickers |
| `U6.3Y` | `LATCH_WR` | L, and P flickers |
| `U6.4Y`, `U7.CLK` | `LATCH_CLK` | H, and P flickers |
| `U7.1Q` to `U7.8Q` | `L0` to `L7` | H on the lit LED's bit, L on the others. P flashes as the light passes |
| `D1` pin 2 to `D8` pin 2 | `L0` to `L7` | the same as the latch output on the same wire |
| `U1.IRQ_N`, `U1.NMI_N`, `U1.RDY` | `IRQ_N`, `NMI_N`, `VCC` | H, steady |

The logic analyzer sees what the probe cannot: each tick of the processor. Clip a group by shift-clicking one of its pins: `U1.A0` clips the address lines `U1 A`, `U1.D0` the data lines `U1 D`. A group's trigger is an address in hex.

| Logic analyzer | A working board shows |
|---|---|
| The program starting. Clip the groups `U1 A` and `U1 D`, and `U1.SYNC`, `U5.Y7_N` and `U5.Y0_N`. Trigger on `U1 A` = F800. Sample every 100 ns; 1 µs a division. Arm, then press RESET | Just before the trigger, address FFFD reads F8: the start of the program. Then the addresses count F800, F801, F802… and the data reads A2, FF, 9A, D8, A9, 01, 85: the program's first bytes. `SYNC` is high on each new instruction. `ROM_N` stays low and `RAM_N` high |
| A RAM write. Clip the groups `U1 A` and `U1 D`, and `U1.RW`, `U5.Y0_N` and `U6.2Y`. Trigger on `U6.2Y` falling. Sample every 20 ns; 200 ns a division. Arm, then press RESET | The first write: address 0000, data 01, the lit LED's note. `RW` and `RAM_N` are low for the whole tick. `WR_N` is low only for its second half, while the data lines hold the byte |
| The LED write. Clip the groups `U1 A` and `U1 D`, and `U1.RW`, `U1.PHI0` and `U6.4Y`. Trigger on `U1 A` = 8000. Sample every 20 ns; 200 ns a division | One write about every eighth of a second, to address 8000. `RW` is low for the tick. In its second half, `PHI0` is high, the data lines hold the light (01 for `D1`, 02 for `D2`…) and `LATCH_CLK` is low. `LATCH_CLK` rises at the end of the tick: the LEDs take the byte then |

## Troubleshooting

**The LEDs never light.**

1. Probe `U1.PHI0`. It must show P.
2. Probe `U1.RES_N`. It must read H. If it stays L, probe `U8.RST_N` and `SW1` pin 1: it is the same wire.
3. Probe `U1.SYNC`. It must show P. If it does not, the processor is stopped.
4. Probe `U5.Y4_N`, then `U6.4Y`, then `U7.CLK`. Each must flicker. The first one that stays still is where the write to the LEDs is lost.
5. Set up the LED write on the logic analyzer. The data lines must hold one lit bit when `LATCH_CLK` rises.
6. Probe `U7.1Q` to `U7.8Q`. One of them must read H.

**The processor runs, but the light never moves, or shows the wrong LEDs.**

1. Set up the LED write on the logic analyzer. The data must be a single bit, and it must move at each write.
2. If it does not, the program loses its notes. Set up the RAM write. Its address must be 0000 and its data 01.
3. Probe `U5.Y0_N` and `U3.CS_N`, `U4.CS_N`. They must flicker together. Probe `U6.2Y`, `U3.WE_N` and `U4.WE_N` the same way.
4. Probe `U5.Y4_N`. It must stay H except during the LED write. A select low too often loads the latch with the wrong byte.

**The light starts at `D1` again and again.**

1. Probe `U1.RES_N`. It must stay H. If it flashes L, check `SW1` and `U8`.
2. Set up the program starting on the logic analyzer. Arm it once the light has started moving. A working board never triggers: the program runs from F800 only after a reset. If it triggers, the processor has lost its way back from the waiting. Its way back is kept in the RAM, at 01FE and 01FF: check the RAM chips, `U3` and `U4`.

**One LED never lights, or never goes out.**

1. Probe its latch output, from `U7.1Q` for `D1` to `U7.8Q` for `D8`. It must read H only while that LED should be lit.
2. Probe the LED's pin 2. It must read the same.
3. If the latch output is wrong, check the matching data line at `U7`: `U7.1D` for `D1` to `U7.8D` for `D8`. It must show P.

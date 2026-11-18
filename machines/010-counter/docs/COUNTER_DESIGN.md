# 010 — Counter — Machine Design

How the Machine in [COUNTER.md](COUNTER.md) is built: netlist, Board, Puzzles, manuals. The code it needs came with P4a ([COUNTER_LED.md](../../../docs/p4a-counter-led/COUNTER_LED.md)).

## Files

```
counter.bwn                  design netlist
board/counter.kicad_pcb      Board (KiCad 10, laid out by hand)
manuals/user.md, service.md  User and Service Manuals
puzzles/NN-*.bwp             3 Puzzles, easiest first
```

## Netlist

[machines/010-counter/counter.bwn](../counter.bwn) (COUNTER.md, Circuit):

```
part X1 OSC freq=1MHz
part J1 POWER
part R1 R value=4k7
part R2 R value=330
part U1 74LS161
...
part U5 74LS161
part D1 LED

net CLK      X1.OUT U1.CLK U2.CLK U3.CLK U4.CLK U5.CLK
net RESET_N  R1.2 U1.CLR_N U2.CLR_N U3.CLR_N U4.CLR_N U5.CLR_N
net VCC      R1.1 R2.1 U1.ENT U1.ENP U1.LOAD_N ... U5.ENP U5.LOAD_N
net GND      U1.A U1.B U1.C U1.D ... U5.D
net C0       U1.QA          ...  net C19 U5.QD D1.K
net CARRY1   U1.RCO U2.ENT  ...  net CARRY4 U4.RCO U5.ENT
net LED_A    R2.2 D1.A
```

## Board

`bw export-kicad machines/010-counter/counter.bwn`, then placed and routed by hand in KiCad ([KICAD_GUIDE.md](../../../docs/p2a-board-layout/KICAD_GUIDE.md)): 140 segments, no vias. `bw check-board` is clean and `drc_counter` passes.

## Puzzles

`machine counter` in each file (COUNTER.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `dark_led.bwp` | `fault crack U5.QD D1.1 51%` | the straight `C19` segment into `D1` pad 1 |
| `frozen_led.bwp` | `fault stuck U3.RCO 0` | `CARRY3` |
| `too_fast.bwp` | `fault stuck U1.RCO 1` | `CARRY1` |

Crack placement (P4e) skips copper too close to a pad, via, corner or junction: a 1.2 mm stub at a 1.6 mm pad is never cracked.

## Manuals

Written to STYLE's Manuals rules (P6i, the pilot):

- User: What it does (a welcome: `D1` blinks about once a second; five counters divide 1 MHz by 1 048 576); How it works (the goal, the clock, one chip counting 0–15, the five chips as an odometer with `CARRY1`–`CARRY4`, the LED, the unused pins in a table).
- Service: Components (every reference); Test points (a table of probe readings on a working board, checked with `bw run --probe` at 1 s, 1.5 s and 2 s); Troubleshooting (the LED never changes, the count stops, the LED blinks too fast: ordered checks, never naming a fault).

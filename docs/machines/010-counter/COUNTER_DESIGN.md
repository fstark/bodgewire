# 010 — Counter — Machine Design

How the Machine in [COUNTER.md](COUNTER.md) is built: netlist, Board, Puzzles, manuals. The code it needs came with P4a ([COUNTER_LED.md](../../p4a-counter-led/COUNTER_LED.md)).

## Files

```
netlists/counter.bwn                 design netlist
boards/counter/counter.kicad_pcb     Board (KiCad 10, laid out by hand)
manuals/counter/user.md, service.md  User and Service Manuals
puzzles/dark_led.bwp, frozen_led.bwp, too_fast.bwp
```

## Netlist

[netlists/counter.bwn](../../../netlists/counter.bwn) (COUNTER.md, Circuit):

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

`bw export-kicad netlists/counter.bwn`, then placed and routed by hand in KiCad ([KICAD_GUIDE.md](../../p2a-board-layout/KICAD_GUIDE.md)): 140 segments, no vias. `bw check-board` is clean and `drc_counter` passes.

## Puzzles

`machine counter` in each file (COUNTER.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `dark_led.bwp` | `fault crack U5.QD D1.1 51%` | the straight `C19` segment into `D1` pad 1 |
| `frozen_led.bwp` | `fault stuck U3.RCO 0` | `CARRY3` |
| `too_fast.bwp` | `fault stuck U1.RCO 1` | `CARRY1` |

Crack placement (P4e) skips copper too close to a pad, via, corner or junction: a 1.2 mm stub at a 1.6 mm pad is never cracked.

## Manuals

- User: What it does (one paragraph: power `J1`, `D1` blinks about once a second).
- Service: Components (every reference).

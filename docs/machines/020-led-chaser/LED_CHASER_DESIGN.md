# 020 — LED Chaser — Machine Design

How the Machine in [LED_CHASER.md](LED_CHASER.md) is built: netlist, Board, Puzzles, manuals. The code it needs comes with P4b ([SHIFT_REGISTER.md](../../p4b-shift-register/SHIFT_REGISTER.md)).

## Files

Machine name `chaser`:

```
netlists/chaser.bwn                  design netlist
boards/chaser/chaser.kicad_pcb       Board (KiCad 10, laid out by hand)
manuals/chaser/user.md, service.md   User and Service Manuals
puzzles/gap.bwp, stuck_light.bwp, dark_chaser.bwp, twins.bwp
```

## Netlist

`netlists/chaser.bwn` (LED_CHASER.md, Circuit):

```
part X1 OSC freq=1MHz
part J1 POWER
part R1 R value=4k7
part R2 R value=330
...
part R9 R value=330
part U1 74LS161
...
part U4 74LS161
part U5 74LS164
part U6 74LS30
part D1 LED
...
part D8 LED

net CLK      X1.OUT U1.CLK U2.CLK U3.CLK U4.CLK
net RESET_N  R1.2 U1.CLR_N U2.CLR_N U3.CLR_N U4.CLR_N U5.CLR_N
net VCC      R1.1 R2.1 R3.1 R4.1 R5.1 R6.1 R7.1 R8.1 R9.1
net VCC      U1.ENT U1.ENP U1.LOAD_N ... U4.ENP U4.LOAD_N U5.B U6.H
net GND      U1.A U1.B U1.C U1.D ... U4.D

net C0       U1.QA          ...  net C14 U4.QC
net CARRY1   U1.RCO U2.ENT  ...  net CARRY3 U3.RCO U4.ENT
net STEP     U4.QD U5.CLK

net SER      U6.Y U5.A
net S1       U5.QA U6.A D1.K
...
net S7       U5.QG U6.G D7.K
net S8       U5.QH D8.K

net LED_A1   R2.2 D1.A
...
net LED_A8   R9.2 D8.A
```

`U4.RCO` is left unconnected.

## Board

`bw export-kicad netlists/chaser.bwn`, then placed and routed by hand in KiCad ([KICAD_GUIDE.md](../../p2a-board-layout/KICAD_GUIDE.md)):

- `D1`–`D8` in one row, left to right, with `R2`–`R9` beside them; `U5` and `U6` next to the row; `U1`–`U4`, `X1`, `J1`, `R1` as on the counter.
- `bw check-board` clean; `drc_chaser` passes (picked up by the `boards/*/*.kicad_pcb` glob).
- The `LED_A5` and `SER` Traces each need one straight segment long enough to crack (both pieces clear of the pads).

## Puzzles

`machine chaser` in each file (LED_CHASER.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `gap.bwp` | `fault crack F.Cu 73.46 101.05` | midpoint of the straight `LED_A5` segment between `R6` and `D5` |
| `stuck_light.bwp` | `fault stuck U5.QD 0` | `S4` |
| `dark_chaser.bwp` | `fault crack B.Cu 70.17 73.69` | midpoint of the long straight `SER` segment between `U6` pin 8 and `U5` pin 1 |
| `twins.bwp` | `fault bridge U5.3 U5.4` | `S1`–`S2`, solder side |

## Manuals

- User: What it does (one paragraph: power `J1`, one light running along eight LEDs).
- Service: Components (every reference).

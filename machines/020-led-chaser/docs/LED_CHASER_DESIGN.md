# 020 — LED Chaser — Machine Design

How the Machine in [LED_CHASER.md](LED_CHASER.md) is built: netlist, Board, Puzzles, manuals. The code it needs comes with P4b ([SHIFT_REGISTER.md](../../../docs/p4b-shift-register/SHIFT_REGISTER.md)).

## Files

Machine name `chaser`:

```
chaser.bwn                   design netlist
board/chaser.kicad_pcb       Board (KiCad 10, laid out by hand)
manuals/user.md, service.md  User and Service Manuals
puzzles/NN-*.bwp             5 Puzzles, easiest first
```

## Netlist

`machines/020-led-chaser/chaser.bwn` (LED_CHASER.md, Circuit):

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

`bw export-kicad machines/020-led-chaser/chaser.bwn`, then placed and routed by hand in KiCad ([KICAD_GUIDE.md](../../../docs/KICAD_GUIDE.md)):

- `D1`–`D8` in one row, left to right, with `R2`–`R9` beside them; `U5` and `U6` next to the row; `U1`–`U4`, `X1`, `J1`, `R1` as on the counter.
- `bw check-board` clean; `drc_chaser` passes (picked up by the `machines/*/board/*.kicad_pcb` glob).
- The `LED_A5` and `SER` Traces each need one straight segment long enough to crack; crack placement (P4e) skips short stubs at the pads.

## Puzzles

`machine chaser` in each file (LED_CHASER.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `gap.bwp` | `fault crack R6.2 D5.2 51%` | the straight `LED_A5` segment between `R6` and `D5` |
| `stuck_light.bwp` | `fault stuck U5.QD 0` | `S4` |
| `dark_chaser.bwp` | `fault crack U6.8 U5.1 50%` | the long straight `SER` segment between `U6` pin 8 and `U5` pin 1 |
| `twins.bwp` | `fault bridge U5.3 U5.4` | `S1`–`S2`, solder side |
| `half_row.bwp` | `fault crack R5.1 R6.1 50%` | the `VCC` run along the resistors' pin 1 row, between `R5` and `R6`: `R6`–`R9` sit on its far end, so all four lose +5 V |

## Manuals

Written to STYLE's Manuals rules, as the Counter's (P6k):

- User: What it does (a welcome: one light runs along `D1`–`D8` about 15 steps a second; a few LEDs may light together for the first half second); How it works (the goal, the clock, the four counters as an odometer making `STEP`, `U5` as a line passing buckets along `S1`–`S8`, `U6` feeding a new low into `SER` and cleaning up after power-on, the LEDs, the unused pins in a table). No Using it: nothing to operate.
- Service: Components (every reference); Test points (a table of probe readings on a working board, checked with `bw run --probe` at 1 s, 1.03 s, 1.2 s, 1.45 s, 1.5 s and 2 s, and the `S1`–`S8` low walking checked every 33 ms from 1 s to 1.6 s; a logic analyzer row checked with `bw_ui --clip U4.QD,U6.Y:f,U5.QA,…,U5.QH --sample 20us --view 100ms:+400ms --after 3s`, the staircase viewed); Troubleshooting (no LED lights or all stay lit, the light never comes back, one LED wrong, two together or a jump: ordered checks, never naming a fault).

# 030 — Dice — Machine Design

How the Machine in [DICE.md](DICE.md) is built: netlist, Board, Puzzles, manuals. The code it needs comes with P4d ([PUSH_BUTTON.md](../../../docs/p4d-push-button/PUSH_BUTTON.md)).

## Files

Machine name `dice`:

```
dice.bwn                     design netlist
board/dice.kicad_pcb         Board (KiCad 10, laid out by hand)
manuals/user.md, service.md  User and Service Manuals
puzzles/NN-*.bwp             4 Puzzles, easiest first
```

## Netlist

`machines/030-dice/dice.bwn` (DICE.md, Circuit). Unused gate inputs go to `GND`, so the netlist loads without warnings.

```
part X1 OSC freq=1MHz
part J1 POWER
part SW1 BUTTON
part R1 R value=4k7
part R2 R value=330
...
part R8 R value=330
part R9 R value=4k7
part U1 74LS161
part U2 74LS04
part U3 74LS00
part U4 74LS02
part D1 LED
...
part D7 LED

net CLK      X1.OUT U4.1A
net BTN_N    R9.2 SW1.1 U4.1B
net ROLL_CLK U4.1Y U1.CLK
net RESET_N  R1.2 U1.CLR_N
net VCC      R1.1 R2.1 R3.1 R4.1 R5.1 R6.1 R7.1 R8.1 R9.1 U1.ENP U1.ENT U1.A
net GND      SW1.2 U1.B U1.C U1.D
net GND      U2.3A U2.4A U2.5A U2.6A U3.2A U3.2B U3.3A U3.3B U3.4A U3.4B U4.3A U4.3B U4.4A U4.4B

net N0       U1.QA U2.1A
net N1       U1.QB U3.1A U4.2A
net N2       U1.QC U2.2A U3.1B U4.2B

net ODD_N    U2.1Y D4.K
net FOUR_N   U2.2Y D2.K D6.K
net TWO_N    U4.2Y D1.K D7.K
net SIX_N    U3.1Y U1.LOAD_N D3.K D5.K

net LED_A1   R2.2 D1.A
...
net LED_A7   R8.2 D7.A
```

`U1.QD` and `U1.RCO` are left unconnected. `SW1` pad 1 is the signal end, pad 2 the `GND` end.

## Board

`bw export-kicad machines/030-dice/dice.bwn`, then placed and routed by hand in KiCad ([KICAD_GUIDE.md](../../../docs/KICAD_GUIDE.md)):

- `D1`–`D7` as a die face: `D1` top left, `D2` top right, `D3` `D4` `D5` across the middle, `D6` bottom left, `D7` bottom right; `R2`–`R8` around them.
- `SW1` at the front edge, below the face, with `R9` next to it; `U1`–`U4`, `X1`, `J1`, `R1` behind the face.
- `bw check-board` clean; `drc_dice` passes (picked up by the `machines/*/board/*.kicad_pcb` glob).
- Cracks (user layout, 167 segments, 2 vias; placed by pads and position since P4e):
  - `BTN_N`: `SW1.1 R9.2 48%`, on the 9.4 mm F.Cu segment from `SW1` pad 1 to `R9` pad 2.
  - `N2`: `U2.3 U4.6 52%`, on the 6.4 mm B.Cu segment between `U2` pin 3 and `U4` pin 6. `N2` runs `U3` pin 2 → `U2` pin 3 → `U4` pin 6 → `U1` pin 12 in one chain, so no crack isolates `U2` alone: this one cuts `U2` and `U3` off `U1`.
  - Test only: GND `SW1.2 J1.2 7%` cuts `SW1` pad 2 off the rail.

## Puzzles

`machine dice` in each file (DICE.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `dead_button.bwp` | `fault crack SW1.1 R9.2 48%` | `BTN_N` from `SW1` pad 1 |
| `loaded_dice.bwp` | `fault crack U2.3 U4.6 52%` | `N2` between `U2` and `U4` |
| `seven.bwp` | `fault stuck U3.1Y 1` | `SIX_N` |
| `odd_dice.bwp` | `fault stuck U1.QA 1` | `N0` |

Briefs:

- Dead button: "Pressing the button does nothing: the dice always shows the same face."
- Loaded dice: "The dice only ever shows 3 or 6."
- Seven: "The dice never rolls a 6, and sometimes shows no spots at all."
- Odd dice: "The dice only rolls odd numbers, or lights every LED."

## Electrical Nets on a damaged Board

With the `dead_button` crack, `SW1` pad 1 is alone on its island. That island is named `BTN_N` (SW1's Connection comes before U4's), and the rest of the Net becomes `BTN_N~2`. `SW1` joins its own island to `GND` while pressed (a switch since P5i, [SWITCHES.md](../../../docs/p5i-switches/SWITCHES.md)): pressing it pulls that island low, which the probe shows, while `U4` pin 3 stays high through `R9`.

With the `loaded_dice` crack, `U2` pin 3 and `U3` pin 2 share an island without a Driver: floating TTL inputs read high. `FOUR_N` is low for good and `SIX_N` follows `NOT N1`, so `U1` loads 1 whenever it reaches 2: the count alternates 1, 2, shown as a 3 (`D2` `D4` `D6`) and a 6.

## Manuals

Written to STYLE's Manuals rules (P6k, the Counter's as the model):

- User: What it does (a welcome: seven LEDs as a die face; a counter running 1 to 6 a million times a second, stopped by the player's hand); Using it (`ROLL`: hold to roll, the LEDs glow at different brightness; every face equally likely; any pattern at power-on until the first roll); How it works (the goal, the clock, the button as a tap on the clock through `U4`, the counter 1 to 6 reloaded by `SIX_N`, the decoder as four spot groups in a table, the LEDs and their brightness while rolling, the unused pins in a table).
- Service: Components (every reference, gate wording without NAND/NOR); Test points (probe readings idle and with `ROLL` held, checked with `bw run --probe` idle at 300 ms and 1 s, held with `--press SW1:100ms:600ms` at 300 ms and 500 ms, released at 1 s with two release times giving faces 2 and 5; a logic analyzer row on `ROLL_CLK`, `N0`–`N2` and `SIX_N` triggered on `SIX_N` falling, 100 ns samples, 2 µs a division, checked with `bw_ui --press SW1:100ms:100s --clip U4.1Y,U1.QA,U1.QB,U1.QC,U3.1Y:f --sample 100ns --view 2us` and viewed); Troubleshooting (rolling does nothing, faces missing or wrong, one spot or pair never lights: ordered checks, never naming a fault).

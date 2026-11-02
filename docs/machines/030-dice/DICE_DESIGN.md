# 030 — Dice — Machine Design

How the Machine in [DICE.md](DICE.md) is built: netlist, Board, Puzzles, manuals. The code it needs comes with P4d ([PUSH_BUTTON.md](../../p4d-push-button/PUSH_BUTTON.md)).

## Files

Machine name `dice`:

```
netlists/dice.bwn                    design netlist
boards/dice/dice.kicad_pcb           Board (KiCad 10, laid out by hand)
manuals/dice/user.md, service.md     User and Service Manuals
puzzles/dead_button.bwp, loaded_dice.bwp, seven.bwp, odd_dice.bwp
```

## Netlist

`netlists/dice.bwn` (DICE.md, Circuit). Unused gate inputs go to `GND`, so the netlist loads without warnings.

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

`bw export-kicad netlists/dice.bwn`, then placed and routed by hand in KiCad ([KICAD_GUIDE.md](../../p2a-board-layout/KICAD_GUIDE.md)):

- `D1`–`D7` as a die face: `D1` top left, `D2` top right, `D3` `D4` `D5` across the middle, `D6` bottom left, `D7` bottom right; `R2`–`R8` around them.
- `SW1` at the front edge, below the face, with `R9` next to it; `U1`–`U4`, `X1`, `J1`, `R1` behind the face.
- `bw check-board` clean; `drc_dice` passes (picked up by the `boards/*/*.kicad_pcb` glob).
- Crack points (user layout, 167 segments, 2 vias):
  - `BTN_N`: F.Cu 100.03 87.00, the 9.4 mm segment from `SW1` pad 1 to `R9` pad 2.
  - `N2`: B.Cu 58.86 71.17, the 6.4 mm segment between `U2` pin 3 and `U4` pin 6. `N2` runs `U3` pin 2 → `U2` pin 3 → `U4` pin 6 → `U1` pin 12 in one chain, so no crack isolates `U2` alone: this one cuts `U2` and `U3` off `U1`.
  - Test only: GND F.Cu 109.23 89.77 cuts `SW1` pad 2 off the rail.

## Puzzles

`machine dice` in each file (DICE.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `dead_button.bwp` | `fault crack F.Cu 100.03 87.00` | `BTN_N` from `SW1` pad 1 |
| `loaded_dice.bwp` | `fault crack B.Cu 58.86 71.17` | `N2` between `U2` and `U4` |
| `seven.bwp` | `fault stuck U3.1Y 1` | `SIX_N` |
| `odd_dice.bwp` | `fault stuck U1.QA 1` | `N0` |

Briefs:

- Dead button: "Pressing the button does nothing: the dice always shows the same face."
- Loaded dice: "The dice only ever shows 3 or 6."
- Seven: "The dice never rolls a 6, and sometimes shows no spots at all."
- Odd dice: "The dice only rolls odd numbers, or lights every LED."

## Electrical Nets on a damaged Board

With the `dead_button` crack, `SW1` pad 1 is alone on its island. That island is named `BTN_N` (SW1's Connection comes before U4's), and the rest of the Net becomes `BTN_N~2`. `SW1` is still between a rail and a signal, so it stays in the build: pressing it pulls its own island low, which the probe shows, while `U4` pin 3 stays high through `R9` ([PUSH_BUTTON_DESIGN.md](../../p4d-push-button/PUSH_BUTTON_DESIGN.md) §5.2).

With the `loaded_dice` crack, `U2` pin 3 and `U3` pin 2 share an island without a Driver: floating TTL inputs read high. `FOUR_N` is low for good and `SIX_N` follows `NOT N1`, so `U1` loads 1 whenever it reaches 2: the count alternates 1, 2, shown as a 3 (`D2` `D4` `D6`) and a 6.

## Manuals

- User: What it does (one paragraph: power `J1`; hold `SW1` and the LEDs flicker; let go and they show a face from 1 to 6; before the first roll they may show any pattern).
- Service: Components (every reference):
  - `X1` clock: 1 MHz crystal oscillator, drives `CLK`
  - `J1` power connector: +5 V and 0 V for the whole board
  - `SW1` roll button: pulls `BTN_N` low while pressed
  - `R1` pull-up: holds `RESET_N` high so `U1` runs
  - `R9` pull-up: holds `BTN_N` high while `SW1` is released
  - `R2`–`R8` LED resistors: limit the current through `D1`–`D7` (one line each)
  - `U1` counter: counts 1 to 6 on `ROLL_CLK`, back to 1 after 6 (`SIX_N` loads 1)
  - `U2` decoder: inverts `N0` into `ODD_N` and `N2` into `FOUR_N`
  - `U3` decoder: NAND of `N1` and `N2` into `SIX_N`
  - `U4` clock gate and decoder: `ROLL_CLK` from `CLK` while `BTN_N` is low; NOR of `N1` and `N2` into `TWO_N`
  - `D1`–`D7` LEDs: lit while their decoder output is low (one line each, naming the Net)

# 100 — Heartbeat — Machine Design

How the Machine in [HEARTBEAT.md](HEARTBEAT.md) is built: netlist, Board, manuals. The code it needs (the `6502` Part) comes with P5c ([MOS6502.md](../../../docs/p5c-6502/MOS6502.md), [design](../../../docs/p5c-6502/MOS6502_DESIGN.md)). Recipe: [ADD_MACHINE.md](../../../docs/recipes/ADD_MACHINE.md).

## Files

Machine name `heartbeat`:

```
heartbeat.bwn                          design netlist
board/heartbeat.place                  panel parts and the CPU (bw place rules)
board/heartbeat.kicad_pcb, .kicad_pro  Board (placed by bw place, routed by the user)
manuals/user.md, service.md
tests/machines/heartbeat_test.cpp
puzzles/NN-*.bwp                       3 Puzzles, easiest first
```

## Netlist

`machines/100-heartbeat/heartbeat.bwn`, with the header comment of ADD_MACHINE step 2. Three chips and `X1` (four Parts of kind Chip), `J1`, `SW1`, 16 LEDs, 27 resistors. Unused outputs unconnected: `U1` `PHI1`, `PHI2`, `SYNC`, `RW` (a probe still reads them on their pads); `NC1`–`NC3` no-connect.

```
part X1  OSC freq=1MHz
part J1  POWER
part U1  6502
part U2  74LS244     # A0-A7 to the LEDs
part U3  74LS244     # A8-A15 to the LEDs
part SW1 BUTTON      # Reset
part D1  LED         # A0
...
part D16 LED         # A15
part R1  R value=330 # D1's cathode
...
part R16 R value=330
part R17 R value=4k7 # D0 = 0
part R18 R value=4k7 # D1 = 1
part R19 R value=4k7 # D2 = 0
part R20 R value=4k7 # D3 = 1
part R21 R value=4k7 # D4 = 0
part R22 R value=4k7 # D5 = 1
part R23 R value=4k7 # D6 = 1
part R24 R value=4k7 # D7 = 1
part R25 R value=3k3 # IRQ_N
part R26 R value=3k3 # NMI_N
part R27 R value=4k7 # RES_N

net PHI0     X1.OUT U1.PHI0
net VCC      U1.RDY U1.SO
net GND      U1.GND2      # optional: power pins join their rail by themselves

# Data bus held at $EA (1110 1010): every byte the CPU reads is a NOP.
net DB0      U1.D0 R17.2
net DB1      U1.D1 R18.2
...
net DB7      U1.D7 R24.2
net VCC      R18.1 R20.1 R22.1 R23.1 R24.1
net GND      R17.1 R19.1 R21.1

# Interrupts off, reset by SW1.
net IRQ_N    U1.IRQ_N R25.2
net NMI_N    U1.NMI_N R26.2
net RES_N    U1.RES_N R27.2 SW1.1
net VCC      R25.1 R26.1 R27.1
net GND      SW1.2

# Address bus, buffered (U2: 1A1-1A4 = A0-A3, 2A1-2A4 = A4-A7; U3 the same for A8-A15).
net GND      U2.1OE_N U2.2OE_N U3.1OE_N U3.2OE_N
net A0       U1.A0 U2.1A1
net LA0      U2.1Y1 D1.A
net LK0      D1.K R1.2
...
net A15      U1.A15 U3.2A4
net LA15     U3.2Y4 D16.A
net LK15     D16.K R16.2
net GND      R1.1 R2.1 … R16.1
```

The data Nets are `DB0`–`DB7`, not `D0`–`D7` like the pins: the LEDs are `D1`–`D16`, and the manual check skips a backticked name that is also a Net (`src/manual/manual.cpp`), so `` `D1` `` must mean only the LED.

## Behaviour checked (fault-free test)

`tests/machines/heartbeat_test.cpp` (`[machines]`, copy `chaser_test.cpp`; Board-built once the Board exists, ADD_MACHINE step 3):

- `heartbeat: netlist loads without warnings`.
- `heartbeat: the address counts` — seeds 1–3, 200 µs after 10 µs of settling, `A0`–`A15`, `SYNC` and `RW` recorded: at each `PHI0` fall + 500 ns the address read off the Nets goes PC, PC + 1, PC + 1, PC + 2, … (mod 65536), `SYNC` high on the first of each pair, `RW` high throughout.
- `heartbeat: the top LED blinks` `[slow]` — seed 1, 300 ms: `LA15` high for 65.536 ms (± 10 µs) in each full period of 131.072 ms (A15 is high from the dummy read at `$8000` to the fetch at `$FFFF`: 1 + 2 × 32767 + 1 cycles).
- `heartbeat: reset starts at EAEA` — `SW1` held 5 µs then released (`SwitchOf`, as `dice_test.cpp`): the cycle ending at the first `PHI0` fall that sees `RES_N` high is the sequence's T1; at the 6th fall after it the CPU drives the fetch at `$EAEA` with `SYNC` high; the cycles between: PC, `$01xx` three times, `$FFFC`, `$FFFD`, all reads.

Debug cost: the slow case ≈ 300 ms simulated with 16 address Nets switching: a few seconds.

## Board

`machines/100-heartbeat/board/heartbeat.place` (ADD_MACHINE step 3; grammar in [BOARD_PLACEMENT.md](../../../docs/p4k-board-placement/BOARD_PLACEMENT.md#what-is-new)):

```
# 100 Heartbeat: panel parts and the CPU (machines/100-heartbeat/docs/HEARTBEAT_DESIGN.md, Board)
edge J1 left middle                    # power
row front left D16 D15 D14 D13 D12 D11 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1   # A15 on the left
edge SW1 front right                   # Reset
edge U1 back middle                    # the 6502: DIP-40, no slot in the chip grid
```

`U2`, `U3`, `X1` and all 27 resistors fill the chip grid by annealing: the LED resistors land along the front, near their LEDs; the data-bus resistors near `U1`'s data pins. Each LED's resistor is not put behind it with `next`: an axial resistor's courtyard (9.72 mm) is wider than the row's LED pitch (8.43 mm), so neighbours would overlap. The row starts at the left so it cannot meet `SW1` at the right. A draft `bw place` comes before the user routes; if a rule fails, the `.place` changes and the change is noted in the plan. The user routes it (F.Cu/B.Cu, DRC 0 errors). The two crack Puzzles cut two-pad Nets (`PHI0`, `A14`), so a new route keeps them.

Layout constraints for the user: none beyond the placed parts; route the `DB0`–`DB7` resistors short (they are the "bus" a player will probe).

## Puzzles

`machine heartbeat` in each file (HEARTBEAT.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `flatline.bwp` | `fault crack X1.OUT U1.PHI0 50%` | `PHI0` |
| `fifteen.bwp` | `fault crack U1.A14 U3.2A3 50%` | `A14` into its buffer |
| `flutter.bwp` | `fault stuck U1.D2 1` | `DB2` at the CPU |

Titles and briefs:

- Flatline: "The lights never move: a few are lit, the others dark, and they stay that way. Reset changes nothing."
- Fifteen: "The second light from the left, D15, is always lit: it never blinks. The others look as they should."
- Flutter: "The lights stop counting: they all glow at once, D1, D5, D9 and D13 dim and the others bright, the same whatever happens. Reset changes nothing."

### Electrical Nets on a damaged Board

- Flatline: `U1` `PHI0` alone on its Net (MOS, X): the CPU never sees an edge, `PHI1`/`PHI2` and the address stay where power-on left them. `X1`'s pad still oscillates.
- Fifteen: `U3` `2A3` alone on its Net: a floating TTL input reads high, so `LA14` is 1. `U1` `A14` counts as usual.
- Flutter: the CPU forces `DB2` to 1 and reads it (a stuck Bidirectional pin): every opcode and operand is `$EE`, INC absolute, 6 cycles: the fetch, `$EE` `$EE`, a read of `$EEEE` and two writes to it. The address is `$EEEE` in half the cycles (the LEDs `1110` in each group of four, the dim ones at a quarter), `RW` low twice an instruction.

Not used: a crack or dry Joint on a data line. The floating line keeps its bus charge, which is the `$EA` bit it already had: no symptom. Bridges between neighbouring data pins of `U1` pit two pull resistors (random) or give `$EB` (a two-byte NOP, the count twice as fast: too subtle).

The test (`tests/machines/heartbeat_test.cpp`) checks each symptom on `U1`'s pins and the LED anodes, then the repair: the address counts again.

## Manuals

Written to STYLE's Manuals rules, the Counter's as the model:

- User: What it does (a welcome: the first processor, no memory and no program; the 16 LEDs show the address in binary, `D16` blinking about eight times a second; the free-run as the first check on a dead computer); Using it (RESET `SW1`: the LEDs freeze while held, the count restarts from the same address; power-on starts anywhere); How it works (the goal, a processor as a chip with clock in and address out, the clock, `R17`–`R24` holding the data bus at NOP, the address counting as pages that say "turn the page", the address lines as a binary counter, the buffers and LEDs, reset from $EAEA, the unused pins of `U1`–`U3` in a table).
- Service: Components (every reference, netlist order); Test points (the clock, reset, data pins, `SYNC`, `RW`, `A0` and `A15` through the buffer to `D16`, and a logic analyzer setup: the address bus grouped by shift-clicking `U1.A0`, `SYNC` triggering on its rise, `RW`, 100 ns samples at 2 µs a division, the bus lane counting up one step every 2 µs). The probe readings checked with `bw run --probe` at 50, 100, 150 and 200 ms and at 1, 1.04 and 1.08 s (`A15` read high, then low, then low), RESET held with `--press SW1:100ms:400ms` read at 300 ms; the analyzer with `bw_ui --group U1.A0 --clip U1.SYNC:r,U1.RW --sample 100ns --view 2us`, screenshot viewed. Troubleshooting (every LED frozen; the LEDs change but not as a count; one LED wrong: ordered checks, never naming a fault).

## Per-Machine checks

ADD_MACHINE: `manuals_test.cpp` list; once the Board is routed, `boards_test.cpp`, `electrical_test.cpp`, `board_scene_test.cpp` loops; `drc_heartbeat` and `place_drc_heartbeat` appear by themselves.

Session (`tests/session/session_test.cpp`, `session: heartbeat with every instrument` `[session][slow]`): 16 lamps `D1`–`D16`; 8 Clips on `U1` `A0`–`A7`, Hooks on `U1` `PHI0` and `SYNC`, the probe on `U1` `RW`, the meter lead on `D16` `A` (`LA15`, already watched for the LED). Watched Nets, one slot per distinct Net: 32 for the LEDs + 8 Clips + 2 Hooks + 1 probe = 43 ≤ `kMaxWatched` (96 since P5e). After 300 ms: `D1` brightness 0.4–0.6 (`A0` at 250 kHz).

Release: `bw_ui machines/100-heartbeat/heartbeat.bwn` (then the Board) with every instrument, 1.00×, UI under 1 ms, screenshot viewed.

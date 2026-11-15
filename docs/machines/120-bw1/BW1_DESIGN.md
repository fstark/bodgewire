# 120 — BW-1 — Machine Design

How the Machine in [BW1.md](BW1.md) is built: netlist, Board, manuals, tests. The code it needs comes with P5j (chips), P5k (ROMs) and P5l (`DIGITCC`, `SLIDE`: [BW1_MACHINE_DESIGN.md](../../p5l-bw1/BW1_MACHINE_DESIGN.md)). Recipe: [ADD_MACHINE.md](../../recipes/ADD_MACHINE.md).

## Files

Machine name `bw1`:

```
netlists/bw1.bwn                               design netlist
roms/bw1_002.asm, .rom; roms/bw1_003.asm, .rom the two 6530 masks (P5k, unchanged)
boards/bw1/bw1.place                           panel parts (bw place rules)
boards/bw1/bw1.kicad_pcb, bw1.kicad_pro        Board (placed by bw place, routed by the user)
manuals/bw1/user.md, service.md
tests/machines/bw1_test.cpp, bw1_puzzles_test.cpp, bw1_rig.{hpp,cpp}
puzzles/*.bwp                                  19 Puzzles (P5m)
```

## Memory map

| `A12`–`A10` | Addresses | Select | Chip |
|---|---|---|---|
| 0 | `$0000`–`$03FF` | `K0` (`U4` `Y0_N`) | `U5`–`U12`, through `U13`, `U14` |
| 5 | `$1400`–`$17FF` | `K5` (`Y5_N`), the 6530s' `CS1` | `U3` I/O `$1700`, RAM `$1780`; `U2` I/O `$1740`, RAM `$17C0` (each by its `A9`–`A6`) |
| 6 | `$1800`–`$1BFF` | `K6` (`Y6_N`), `U3` `RS0` | the 003's ROM |
| 7 | `$1C00`–`$1FFF` | `K7` (`Y7_N`), `U2` `RS0` | the 002's ROM |
| 1–4 | | `Y1_N`–`Y4_N` unconnected | nothing: a read floats the bus |

`A13`–`A15` go nowhere (their copper ends at `U1`): every block is mirrored at each 8K, the vectors at `$FFFA` read `$1FFA`.

## Netlist

`netlists/bw1.bwn`, header comment as ADD_MACHINE step 2 (Machine 120, BW1.md, the circuit). The bench's wiring (`tests/data/bw1_bench.bwn`) for the processor, decoder, RAM and the 002, with the 003, the display, the keypad, ST, RS and SST added. 19 chips and `X1`, `J1`, 6 digits, 24 switches, 7 resistors.

```
part X1  OSC freq=1MHz
part J1  POWER
part U1  6502
part U2  6530 mask=002 contents=../roms/bw1_002.rom   # monitor; display and keypad ports
part U3  6530 mask=003 contents=../roms/bw1_003.rom   # self-test; ports unconnected
part U4  74145       # K0-K7
part U5  6102        # D7
…
part U12 6102        # D0
part U13 74125       # D3-D0 onto the bus
part U14 74125       # D7-D4 onto the bus
part U15 74LS00      # RAM_RW, BUF_N, SST_N
part U16 74LS04      # RW_N, K0_N
part U17 74LS244     # segment driver
part U24 74145       # digits and keypad rows
part U25 POR time=20ms
part DS1 DIGITCC    # leftmost
…
part DS6 DIGITCC
part SW1  KEY legend=0
…                    # SW2-SW16: legend=1 … legend=F (pc= defaults to the legend)
part SW17 KEY legend=AD pc=/
part SW18 KEY legend=DA pc=*
part SW19 KEY legend=+
part SW20 KEY legend=GO pc=Enter
part SW21 KEY legend=PC pc=P
part SW22 KEY legend=ST pc=Escape
part SW23 KEY legend=RS pc=Backspace
part SW24 SLIDE legend=SST
part R1  R value=3k3 # K0
part R2  R value=3k3 # K5
part R3  R value=3k3 # K6
part R4  R value=3k3 # K7
part R5  R value=3k3 # RES_N
part R6  R value=3k3 # NMI_N
part R7  R value=3k3 # IRQ_N

# Clock, reset, interrupts.
net PHI0     X1.OUT U1.PHI0
net PHI2     U1.PHI2 U2.PHI2 U3.PHI2 U15.1B
net VCC      U1.RDY U1.SO
net RES_N    U1.RES_N U2.RES_N U3.RES_N U25.RST_N R5.2 SW23.1
net NMI_N    U1.NMI_N R6.2 SW22.1 SW24.2
net IRQ_N    U1.IRQ_N R7.2
net GND      SW22.2 SW23.2
net VCC      R1.1 R2.1 R3.1 R4.1 R5.1 R6.1 R7.1

# Single step: SST_N low on an opcode fetch outside the 002's ROM (K7 high); SW24 on joins it to NMI_N.
net SYNC     U1.SYNC U15.3A
net SST_N    U15.3Y SW24.1

# Address bus: A0-A9 to the RAMs and both 6530s, A10-A12 to the decoder.
net A0       U1.A0 U2.A0 U3.A0 U5.A0 … U12.A0
…
net A9       U1.A9 U2.A9 U3.A9 U5.A9 … U12.A9
net A10      U1.A10 U4.A
net A11      U1.A11 U4.B
net A12      U1.A12 U4.C
net GND      U4.D

# Decoding: K0 the 6102s, K5 both 6530s' RAM and ports, K6 the 003's ROM, K7 the 002's.
net K0       U4.Y0_N R1.2 U16.2A U5.CE_N … U12.CE_N
net K5       U4.Y5_N R2.2 U2.CS1 U3.CS1
net K6       U4.Y6_N R3.2 U3.RS0
net K7       U4.Y7_N R4.2 U2.RS0 U15.3B

# Read and write (the bench's): RAM_RW, BUF_N.
net RW       U1.RW U2.RW U3.RW U16.1A U15.2A
net RW_N     U16.1Y U15.1A
net K0_N     U16.2Y U15.2B
net RAM_RW   U15.1Y U5.RW … U12.RW
net BUF_N    U15.2Y U13.1OE_N … U14.4OE_N
net VCC      U15.4A U15.4B
net GND      U16.3A U16.4A U16.5A U16.6A

# Data bus: DIN straight on it, DOUT through the 74125s (KIM Fig. 3.4), both 6530s.
net D0       U1.D0 U2.D0 U3.D0 U12.DIN U13.1Y
… (as the bench, with U3.Dn on each)
net DOUT0    U12.DOUT U13.1A
…

# Display: U24 outputs 4-9 sink the digits' cathodes; PA0-PA6 through U17 light the segments.
net PB1      U2.PB1 U24.A
net PB2      U2.PB2 U24.B
net PB3      U2.PB3 U24.C
net PB4      U2.PB4 U24.D
net DIG1     U24.Y4_N DS1.CC1 DS1.CC2
…
net DIG6     U24.Y9_N DS6.CC1 DS6.CC2
net PA0      U2.PA0 U17.1A1 SW7.2 SW14.2 SW21.2
net PA1      U2.PA1 U17.1A2 SW6.2 SW13.2 SW20.2
net PA2      U2.PA2 U17.1A3 SW5.2 SW12.2 SW19.2
net PA3      U2.PA3 U17.1A4 SW4.2 SW11.2 SW18.2
net PA4      U2.PA4 U17.2A1 SW3.2 SW10.2 SW17.2
net PA5      U2.PA5 U17.2A2 SW2.2 SW9.2 SW16.2
net PA6      U2.PA6 U17.2A3 SW1.2 SW8.2 SW15.2
net SEG_A    U17.1Y1 DS1.A DS2.A DS3.A DS4.A DS5.A DS6.A
…            # SEG_B 1Y2, SEG_C 1Y3, SEG_D 1Y4, SEG_E 2Y1, SEG_F 2Y2, SEG_G 2Y3
net GND      U17.1OE_N U17.2OE_N U17.2A4

# Keypad: a key joins its row (U24 outputs 0-2) to its column (PA6 for the first key of a row).
net ROW0     U24.Y0_N SW1.1 SW2.1 SW3.1 SW4.1 SW5.1 SW6.1 SW7.1
net ROW1     U24.Y1_N SW8.1 … SW14.1
net ROW2     U24.Y2_N SW15.1 … SW21.1
```

Key `SW(k+1)` has code `k` (P5k's `GETKEY`): row `k / 7`, column `PA(6 − k % 7)`.

`U24` `Y3_N` (pad 4) is alone on `STRAP`, a one-pin Net: an unconnected pin never joins a Net (`ElectricalNetlist` builds Nets from the netlist's connections only), so a Bodge Wire from a bare pad would do nothing. Unconnected on purpose: `U4` `Y1_N`–`Y4_N`, `Y8_N`, `Y9_N`; `U2` `PA7`, `PB0`, `PB5`, `PB7` (TTY and the timer's interrupt, left out); every port pin of `U3` (P5k: the self-test writes its `PADD`); `U17` `2Y4`; the digits' `DP`; `U1` `PHI1`; `A13`–`A15`. The 6530s' port pins are pull-ups and the 74145's outputs open collector: no "floats high" warning. If the builder warns on an unconnected TTL input, it is tied (phase 3 notes it).

### What the netlist must show

- `PA0`–`PA6` are both outputs (segments, while the monitor scans digits) and inputs (columns, while it reads rows): `U17` and the keys sit on the same Nets.
- `NMI_N` has three Drivers: `R6`, ST to `GND`, SST to `U15`'s output; a held ST beats `SST_N` high (a rail through a switch beats a TTL output, P5i).
- `SYNC` and `K7` settle well before the next `PHI0` edge, at which the 6502 samples `NMI_N` (P5c): `K7`'s 74145 glitch at a block change is never seen.

### Timing at 1 MHz

From `delays.hpp` and P5j: a 6530 port output valid 1 µs after `PHI2` falls; `U24` and `U17` a few tens of ns; the monitor reads `PA` at least 3 µs after selecting a row (P5k design §2.5). Data and selects as on the bench (P5j §7), which passes its RAM tests on seeds 1–3.

## Behaviour checked (fault-free tests)

`tests/machines/bw1_test.cpp`, tag `[machines]` (`[slow]` where noted). A `Bw1` helper around `LoadedCircuit` + `Simulator`:

- `type("AD 0300 DA A9 +")`: each key held 20 ms, then released 20 ms (`Switch::setPressed` + `wake`); names `AD`, `DA`, `+`, `GO`, `PC`, `ST`, `RS`, hex digits; blanks separate.
- `display(20 ms)`: polls every 20 µs: the selected digit (one `DIGn` Net at `0`, driven), the segments (`SEG_A`–`SEG_G` at `1`); per digit the share of time selected and each segment's share of it; decoded as the model's (`Bw1DigitChar`, a digit selected under 1/20 of the window is ' ', a segment lit at least half its digit's time is on).
- `setSst(bool)` (`SW24`), `stop()` (ST held 20 ms).

Cases (seeds 1–3 where the RAMs' power-on matters, 1 otherwise):

- `bw1: netlist loads without warnings` (`LoadNetlistFile`).
- `bw1: reset shows 0200` — 60 ms, then `display()`: `0200` and two hex digits; seeds 1–3.
- `bw1: keys set the address and write memory` — `AD 0300 DA A9 +`: `0301` and hex; `AD 0300`: `0300A9`.
- `bw1: a program typed in runs` `[slow]` — `DA A9 + 42 + 85 + 10 + 00 AD 0200 GO`: `0206` and hex; `AD 0010`: `001042`.
- `bw1: ST stops a loop` `[slow]` — `DA 4C + 00 + 02 AD 0200 GO`, 50 ms, `stop()`: `02004C`.
- `bw1: SST steps one instruction` `[slow]` — `DA A9 + 11 + A9 + 22 AD 0200`, `setSst(true)`, `GO`: `0202A9`; `GO`: `0204` and the RAM's byte there.
- `bw1: RS returns to 0200` — `AD 1234`: `1234` and hex; `RS`: `0200` and hex.
- `bw1: the strap runs the self-test` `[slow]` — the netlist's text with its `net STRAP U24.Y3_N` line rewritten as `net PA0 U24.Y3_N` (a pin on two Nets is a builder error; `ParseNetlist`, `ResolveContentsPaths`, `BuildCircuit`, as the bench test). Times from power-on, the POR releasing at 20 ms: `display(5 ms)` at 22 ms `1` and five blanks, at 300 ms `888888`, at 3.2 s `PA55` and two blanks; GO held: `PA5513`.
- After the Board (phase 4): `bw1: a Bodge Wire on the strap runs the self-test` `[slow]` — `LoadNetlistMachine`, `addWire({"U24", "4"}, {"U2", "2"})`, `build()`: `888888` at 300 ms from power-on (the Copper path; the full run is the case above). The other cases move to the Board's Circuit (`BoardCircuit`), as ADD_MACHINE §3 says.

Typing holds each key 20 ms and releases it 20 ms (the model's 40/40 halved: still two scans of 3.5 ms each way). Debug cost measured in phase 3 (about 0.03× real time): the strap case 83 s, the typing cases 18–31 s (all `[slow]`), reset 4 s, RS 9 s.

## Board

`boards/bw1/bw1.place` (ADD_MACHINE step 3). The keypad at the front right as the KIM's, 6 rows of 4, its bottom row on the front edge; the display to its left, level with the AD row; the three DIP-40s along the back; power on the left.

```
# 120 BW-1: panel parts (docs/machines/120-bw1/BW1_DESIGN.md, Board)
edge J1 left back                          # power
row back middle U1 U2 U3                   # the 6502 and the two 6530s: DIP-40s, no slot in the chip grid
row front right SW1 SW2 SW3 SW4            # 0 1 2 3
next SW5 SW1 back                          # 4 5 6 7
next SW6 SW2 back
next SW7 SW3 back
next SW8 SW4 back
next SW9 SW5 back                          # 8 9 A B
…
next SW13 SW9 back                         # C D E F
…
next SW17 SW13 back                        # AD DA PC +
next SW18 SW14 back
next SW21 SW15 back
next SW19 SW16 back
next SW20 SW17 back                        # GO ST RS SST
next SW22 SW18 back
next SW23 SW21 back
next SW24 SW19 back
next DS6 SW17 left                         # the display, DS1 on the left
next DS5 DS6 left
…
next DS1 DS2 left
```

The other 16 chips, `X1` and the 7 resistors fill the chip grid by annealing. A draft `bw place` comes first; a failed rule (a `next` falling off its edge, overlapping courtyards) changes the `.place`, noted in the plan. `SW24`'s slide courtyard is smaller than a key's: `next` centres it across `SW19` and puts it nearer the AD row than the other top-row keys; accepted.

Layout constraints for the user: the parts as placed (no rotation); F.Cu/B.Cu, DRC 0 errors, 0 unconnected. `U24` pad 4 (`STRAP`) stays a bare pad with no track, reachable for the strap's Bodge Wire. Optional: the keypad's `ROW0`–`ROW2` and `PA0`–`PA6` routed as a visible matrix under the keys, the data and address buses grouped.

## Puzzles

`machine bw1` in each file (BW1.md, Puzzles). Each Fault was tried on seeds 1–3, the monitor and strapped, before its brief was written (P5m plan notes).

| File | Fault line | Symptom (seed 1; seeds 1–3 the same unless said) | Repair |
|---|---|---|---|
| `odd_seven.bwp` | `fault cell U2.D3 rom $3EE 1` | `AD 0277`: the 7s are a, b, c, d; self-test `4C00xx` | swap `U2` |
| `missing_bar.bwp` | `fault stuck U17.2Y3 0` | no `SEG_G` on any digit | swap `U17` |
| `half_segment.bwp` | `fault crack DS3.A DS4.A 50%` | `DS4`–`DS6` without `SEG_A` | mend |
| `bright_mess.bwp` | `fault stuck U24.Y6_N 0` | `DS3` lit with every digit: `0x7F` | swap `U24` |
| `dead_da.bwp` | `fault crack U17.8 SW18.2 50%` | DA, `A`, `3` dead (`456 DA 77` shows `5677`); `SEG_D` still lit | mend |
| `dead_row.bwp` | `fault crack U24.3 SW20.1 50%` | E, F, AD, DA, `+`, GO, PC dead | mend |
| `deaf_keypad.bwp` | `fault switch SW19 closed` | `0200 xx`, no key; self-test `PA5512` | swap `SW19` |
| `bad_byte.bwp` | `fault cell U5.DOUT $300 1` | `AD 0300 DA 00` shows `030080`; self-test `130080` | swap `U5` |
| `aliased_page.bwp` | `fault stuck U7.A8 0` | `1D2746` at power-on, `AD 0300` shows `0320`; self-test `100020` | swap `U7` |
| `no_stop.bwp` | `fault cell U2.D0 ram $3A 1` | `BRK` stops, ST shows a wrong address (it changes with what ran); self-test `27FA01` | swap `U2` |
| `self_test_fails.bwp` | `fault cell U3.D2 ram $15 1` | works; self-test `379504` (75 ms) | swap `U3` |
| `rom_test_fails.bwp` | `fault cell U3.D0 rom $380 0` | works; self-test `5800FF` (100 ms) | swap `U3` |
| `dead_machine.bwp` | `fault column U9.DOUT 0` | dark; self-test `110008` | swap `U9` |
| `wrong_suspect.bwp` | `fault crack U9.DOUT U13.3A 50%` | a few frozen characters by seed (`18F` seed 1); self-test `100008`, also after a `U9` swap | mend |
| `half_byte.bwp` | `fault crack U14.1 U13.4 50%` | `PA55` at power-on, keys ignored; self-test `11000E` | mend |
| `wrong_block.bwp` | `fault stuck U4.A 0` | dark, strapped too; `K5`, `K7` never low; `U4` pin 15 high while `K6` low | swap `U4` |
| `held_reset.bwp` | `fault switch SW23 closed` | dark, strapped too; `RES_N` low | swap `SW23` |
| `sst_kills.bwp` | `fault stuck U15.3B 1` | SST at ON: one digit lit, no key; at OFF `1C0085` | swap `U15` |
| `two_bits.bwp` | `fault bridge U1.31 U1.30` | dark or one frozen digit by seed, strapped too; `D2` = `D3` | cut the bridge |

Self-test codes are read through a Bodge Wire from `U24` pad 4 to `U2` pad 2 (`Bw1Circuit(machine, true)` in `bw1_rig`); the self-test writes before it reads, so its codes are checked on seed 1. A repair is checked by the monitor's `0200`, or for a Puzzle the monitor does not show, the self-test's `888888` at 300 ms (tests 1–5 passed); only `deaf_keypad` runs to `PA55` (3.2 s simulated, about 2 min in debug).

Not used: a cell or Trace the monitor never reads with no self-test test over it (`U3`'s port pins); a crack on `PA0`–`PA6` before `U17` (the segment floats lit, not a key fault); `NMIV` bits 1, 4, 6 stuck (they land on working instructions: ST still stops); `TABLE` bit 7 (`PA7` unconnected); ghosting (two keys held).

## Manuals

- `manuals/bw1/user.md`: power `J1`; the display shows an address and the byte there; AD, DA, `+`, GO, PC, ST, RS and SST as BW1.md, When it works; typing a program at `$0200` and running it; the PC keys.
- `manuals/bw1/service.md`: Components and Self-test (P5m: how to strap it, the tests' numbers, reading a failure, which chip holds which bit), one bullet per reference in netlist order (`X1`, `J1`, each `U5`–`U12`, `DS1`–`DS6`, `SW1`–`SW24`, `R1`–`R7` its own): `U4` address decoder (`A10`–`A12` to `K0`, `K5`, `K6`, `K7`); `U2` the monitor at $1C00–$1FFF, RAM at $17C0, ports at $1740, `PA0`–`PA6` segments and keypad columns, `PB1`–`PB4` to `U24`; `U3` the self-test at $1800, RAM at $1780, ports at $1700 (unused); `U24` display and keypad scanner, "output 3 (pad 4) is the self-test strap: joined to `PA0`, reset runs the self-test"; `U17` segment driver; `SW24` "SST: on, `NMI_N` follows `SST_N`".

## Per-Machine checks

ADD_MACHINE: `manuals_test.cpp` list; after the Board, `boards_test.cpp`, `electrical_test.cpp`, `board_scene_test.cpp` loops; `drc_bw1`, `place_drc_bw1` appear by themselves (`asm_bw1_002`, `asm_bw1_003` exist).

Session (`tests/session/session_test.cpp`, `session: bw1 with every instrument` `[session][slow]`): 48 lamps (6 digits × 8, `DP` unconnected: 42), 13 Nets (`SEG_A`–`SEG_G`, `DIG1`–`DIG6`); 32 Clips (the groups `U1 A`, `U1 D`, `U1 PHI` by shift-click, then `RW`, `SYNC`, `RES_N`, `IRQ_N`, `NMI_N`), 4 Hooks (`PHI0`, `SYNC`, `NMI_N`, `K7`), the probe on `ROW0`, the meter on `PA0`: 13 + 32 + 4 + 1 + 1 = 51 ≤ 96. At 300 ms `DS1`–`DS4` show `0200`: segments of `0` and `2` bright on their digits (each digit lit about 1/6 of the time: brightness compared between lit and dark segments, not to a fixed level).

Release: `bw run netlists/bw1.bwn --time 2s` idle and with `--press SW1:1ms:2s` (a duration needs a unit and the press must start after 0); `bw_ui` on the Board with every instrument, 1.00×, UI under ~1 ms; screenshots viewed (BW1_MACHINE_DESIGN §6).

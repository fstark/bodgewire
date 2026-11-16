# 110 — LED computer — Machine Design

How the Machine in [LED_COMPUTER.md](LED_COMPUTER.md) is built: netlist, program, Board, manuals. The code it needs (`bw asm`, 74LS138, 74LS273, `POR`) comes with P5d ([ROM_PROGRAMS.md](../../../docs/p5d-rom-programs/ROM_PROGRAMS.md), [design](../../../docs/p5d-rom-programs/ROM_PROGRAMS_DESIGN.md)). Recipe: [ADD_MACHINE.md](../../../docs/recipes/ADD_MACHINE.md).

## Files

Machine name `led_computer`:

```
led_computer.bwn                              design netlist
roms/led_computer.asm, roms/led_computer.rom  program and its assembled ROM
board/led_computer.place                      panel parts, CPU and ROM (bw place rules)
board/led_computer.kicad_pcb, .kicad_pro      Board (placed by bw place, routed by the user)
manuals/user.md, service.md
tests/machines/led_computer_test.cpp
puzzles/NN-*.bwp                              7 Puzzles, easiest first
```

## Memory map

| Block (`A15`–`A13`) | Addresses | Select | Chip |
|---|---|---|---|
| 0 | `$0000`–`$1FFF` | `RAM_N` (`U5` `Y0_N`) | `U3`, `U4`: 1K at `$0000`–`$03FF`, mirrored 8 times |
| 4 | `$8000`–`$9FFF` | `LATCH_N` (`Y4_N`) | `U7`, written only |
| 7 | `$E000`–`$FFFF` | `ROM_N` (`Y7_N`) | `U2`: 2K at `$F800`–`$FFFF`, mirrored 4 times |
| 1–3, 5, 6 | | `Y1_N`–`Y3_N`, `Y5_N`, `Y6_N` unconnected | nothing: a read floats the bus |

`A10`–`A12`: `A10` to the ROM only; `A11`, `A12` unconnected (their copper ends at `U1`).

## Netlist

`machines/110-led-computer/led_computer.bwn`, header comment as ADD_MACHINE step 2. Eight chips and `X1`, `J1`, `SW1`, 8 LEDs, 11 resistors.

```
part X1  OSC freq=1MHz
part J1  POWER
part U1  6502
part U2  2716 contents=roms/led_computer.rom
part U3  2114        # DB0-DB3
part U4  2114        # DB4-DB7
part U5  74LS138     # address decoder
part U6  74LS02      # write strobes
part U7  74LS273     # LED latch
part U8  POR time=20ms
part SW1 BUTTON      # Reset
part D1 LED          # L0
...
part D8 LED          # L7
part R1 R value=330  # D1's cathode
...
part R8 R value=330
part R9  R value=4k7 # RES_N
part R10 R value=3k3 # IRQ_N
part R11 R value=3k3 # NMI_N

# Clock, reset, interrupts off.
net PHI0     X1.OUT U1.PHI0
net VCC      U1.RDY U1.SO
net RES_N    U1.RES_N U8.RST_N R9.2 SW1.1 U7.CLR_N
net IRQ_N    U1.IRQ_N R10.2
net NMI_N    U1.NMI_N R11.2
net VCC      R9.1 R10.1 R11.1
net GND      SW1.2

# Address bus.
net A0       U1.A0 U2.A0 U3.A0 U4.A0
...
net A9       U1.A9 U2.A9 U3.A9 U4.A9
net A10      U1.A10 U2.A10
net A13      U1.A13 U5.A
net A14      U1.A14 U5.B
net A15      U1.A15 U5.C

# Data bus.
net DB0      U1.D0 U2.O0 U3.IO1 U7.1D
net DB1      U1.D1 U2.O1 U3.IO2 U7.2D
net DB2      U1.D2 U2.O2 U3.IO3 U7.3D
net DB3      U1.D3 U2.O3 U3.IO4 U7.4D
net DB4      U1.D4 U2.O4 U4.IO1 U7.5D
net DB5      U1.D5 U2.O5 U4.IO2 U7.6D
net DB6      U1.D6 U2.O6 U4.IO3 U7.7D
net DB7      U1.D7 U2.O7 U4.IO4 U7.8D

# Decoding: 8K blocks.
net VCC      U5.G1 U2.VPP
net GND      U5.G2A_N U5.G2B_N U2.OE_N
net RAM_N    U5.Y0_N U3.CS_N U4.CS_N
net LATCH_N  U5.Y4_N U6.3A
net ROM_N    U5.Y7_N U2.CE_N

# Write strobes: WR high while PHI1 and RW are low (the second half of a write cycle).
net PHI1     U1.PHI1 U6.1A
net RW       U1.RW U6.1B
net WR       U6.1Y U6.2A U6.2B
net WR_N     U6.2Y U3.WE_N U4.WE_N U6.3B
net LATCH_WR U6.3Y U6.4A U6.4B
net LATCH_CLK U6.4Y U7.CLK

# LEDs.
net L0       U7.1Q D1.A
net LK0      D1.K R1.2
...
net L7       U7.8Q D8.A
net LK7      D8.K R8.2
net GND      R1.1 R2.1 R3.1 R4.1 R5.1 R6.1 R7.1 R8.1
```

Unused: `U1` `PHI2`, `SYNC`, `A11`, `A12` (no copper beyond the pin; the probe-on-unrouted-pin issue of P5c applies until P5e), `NC1`–`NC3`; `U5` `Y1_N`–`Y3_N`, `Y5_N`, `Y6_N`. `U2` `OE_N` tied low: the ROM drives the bus whenever `ROM_N` is low; the program never writes in block 7.

Data Nets `DB0`–`DB7`, LED Nets `L0`–`L7` and `LK0`–`LK7`, as 100 Heartbeat does, so `` `D1` `` in a manual means only the LED.

### Timing at 1 MHz (from `delays.hpp`)

- Read: the address is out 130 ns after `PHI0` falls; `ROM_N` or `RAM_N` 21 ns later; the 2716 has the data at about 600 ns (450 ns from its address and from `CE_N`); the 2114 at about 270 ns after a cycle elsewhere (120 ns from `CS_N`, the model's delay for that change) or about 580 ns after another RAM cycle (450 ns from the address). Both before the CPU takes it at the next fall (1000 ns).
- RAM write: `WR_N` falls about 40 ns after `PHI0` rises (`PHI1` falls 20 ns after it, two NORs of 10 ns) and rises about 60 ns after `PHI0` falls; the CPU drives the data from 180 ns after the rise to 90 ns after the fall, and the address until 130 ns after the fall, so the 2114 keeps the right word. The 2114 lets go of the bus 100 ns after `WR_N` falls (`k2114Off`), at about 640 ns, before the CPU drives at 680 ns.
- Latch: `LATCH_CLK` rises about 80 ns after `PHI0` falls (`PHI1` rises at 40 ns, then four NORs of 10 ns: `WR` falls, `WR_N` rises, `LATCH_WR` falls, `LATCH_CLK` rises); the CPU holds the data until 90 ns. The 74LS273 takes `D` as it was before the edge. A 10 ns margin, as tight as on many real 6502 boards.

## Program

`machines/110-led-computer/roms/led_computer.asm` (assembled with `bw asm`; the cycle counts in the comments are the 6502 data sheet's):

```
; 110 LED computer (machines/110-led-computer/docs/LED_COMPUTER.md): a light bouncing along
; 8 LEDs. Memory map: RAM $0000-$03FF (mirrored to $1FFF), the LED latch at $8000 (any write in
; $8000-$9FFF), this ROM at $F800-$FFFF (mirrored from $E000).

        .rom $F800, $800

LEDS    = $8000         ; U7, the 74LS273
light   = $00           ; the lit LED: one bit set
dir     = $01           ; 0: moving left (towards D8), 1: moving right
STEPS   = 97            ; delay: 97 x 1286 cycles, about 125 ms at 1 MHz

        .org $F800
reset:  ldx #$FF
        txs             ; stack from $01FF down
        cld
        lda #$01
        sta light
        lda #0
        sta dir
loop:   lda light
        sta LEDS
        jsr delay
        lda dir
        bne @right
        asl light
        lda light
        cmp #$80
        bne loop
        inc dir
        jmp loop
@right: lsr light
        lda light
        cmp #$01
        bne loop
        dec dir
        jmp loop

delay:  ldy #STEPS
@outer: ldx #0
@inner: dex
        bne @inner      ; 256 x 5 - 1 cycles
        dey
        bne @outer
        rts

irq:    rti             ; IRQ_N and NMI_N are never pulled low

        .org $FFFA
        .word irq, reset, irq   ; NMI, RESET, IRQ/BRK
```

A step, from one `STA LEDS` to the next: `delay` is `2 + 97 × 1286 − 1 + 6` = 124,749 cycles with the `RTS`, plus `JSR` 6, the loop's own instructions about 25: about 124.78 ms, a turn step about 7 cycles longer. The test computes nothing from this: it checks the measured steps against 124.78 ms ± 30 µs.

## Behaviour checked (fault-free test)

`tests/machines/led_computer_test.cpp` (`[machines]`, as `heartbeat_test.cpp`; Board-built once the Board exists, ADD_MACHINE step 3). `L0`–`L7` read off `U7`'s `Q` pins, `RES_N`, `LATCH_CLK` and `U1`'s address and `SYNC` recorded.

- `led_computer: netlist loads without warnings`.
- `led_computer: power-on reset` — seeds 1–3, 21 ms: `RES_N` low until 20 ms − 1 ps and high from exactly 20 ms; `L0`–`L7` all 0 from 100 ns to 20 ms (cleared through `CLR_N`); after the release, a fetch (`SYNC` high) at the address in the ROM's reset vector (`$FFFC`–`$FFFD` read from the `.rom` via `ReadRomContents`, so the test follows the program); `L0`–`L7` = `$01` within 100 µs of the release.
- `led_computer: the light bounces` `[slow]` — seed 1, 20 ms + 16 steps (2.02 s): counting only `LATCH_CLK` rises after `RES_N` rises (the 74LS02 starts with random outputs, so one rise can come in the first ns), `L0`–`L7` 100 ns after each rise give `01 02 04 08 10 20 40 80 40 20 10 08 04 02 01 02`; consecutive rises 124.78 ms ± 30 µs apart.
- `led_computer: the variables live in RAM` — seed 1, 1 ms after the third `LATCH_CLK` rise (the latch shows `$04`, the program is in `delay`): `Ram2114::word(0)` of `U3` (low nibble) and `U4` (high nibble) give `light` = `$04`; `word(1)` = `dir` = 0; the return address `JSR` pushed at `$01FE`–`$01FF` (high byte at `$01FF`) is in the ROM (`$F8xx`).
- `led_computer: Reset restarts the light` — seed 1: run to the third step, hold `SW1` 1 ms (`ButtonOf`): `L0`–`L7` all 0 within 1 µs of the press and while held; after the release, `$01` within 100 µs and `$02` one step later.

Debug cost: the slow case simulates 2 s of a CPU with its ROM busy every cycle: an estimated 20–40 s at debug speed (measured in phase 3 and noted).

## Board

`machines/110-led-computer/board/led_computer.place` (ADD_MACHINE step 3):

```
# 110 LED computer: panel parts, the CPU and the ROM (machines/110-led-computer/docs/LED_COMPUTER_DESIGN.md, Board)
edge J1 left middle              # power
row front left D8 D7 D6 D5 D4 D3 D2 D1   # L7 on the left
edge SW1 front right             # Reset
row back middle U1 U2            # the 6502 and the 2716: DIP-40 and DIP-24, no slot in the chip grid
```

`U3`–`U8`, `X1` and the 11 resistors fill the chip grid by annealing. A draft `bw place` comes before the user routes; a failed rule changes the `.place`, noted in the plan. The LED resistors are not put behind their LEDs with `next` (an axial courtyard, 9.72 mm, is wider than the row's LED pitch, as on 100 Heartbeat); the annealing may scatter them, as it did there, and the user may move them before routing.

Layout constraints for the user: the parts as placed (no rotation); F.Cu/B.Cu, DRC 0 errors, 0 unconnected. The crack Puzzle cuts a two-pad Net (`LATCH_CLK`), so a new route keeps it. Optional: keep the data bus (`DB0`–`DB7`) and address bus tracks grouped as buses, since a player will follow them.

## Puzzles

`machine led_computer` in each file (LED_COMPUTER.md, Puzzles):

| File | Fault line | Where |
|---|---|---|
| `lights_out.bwp` | `fault crack U6.4Y U7.CLK 50%` | `LATCH_CLK` at the latch |
| `sleeper.bwp` | `fault stuck U8.RST_N 0` | `RES_N` |
| `full_house.bwp` | `fault bridge U5.15 U5.16` | `RAM_N` to `U5`'s VCC pin |
| `three_lights.bwp` | `fault bridge U6.8 U6.9` | `LATCH_N` to `WR_N` |
| `first_light.bwp` | `fault cell U4.IO4 $1FF 0` | the stack, inside `U4` |
| `lightless.bwp` | `fault stuck U3.A0 0` | `A0` inside `U3` |
| `blackout.bwp` | `fault stuck U2.CE_N 1` | `ROM_N` inside `U2` |

Titles and briefs:

- Lights out: "The LEDs never light."
- Sleeper: "The LEDs never light, and pressing Reset does not wake the computer."
- Full house: "All eight LEDs are lit, and they stay lit."
- Three lights: "D1, D2 and D5 are lit and stay lit: the light never moves."
- First light: "D1 lights, but the light never moves on: it starts again at D1 every time."
- Lightless: "The LEDs never light."
- Blackout: "The LEDs never light."

Four Puzzles show dark LEDs on purpose: from outside they are alike, the instruments tell them apart (the program running or not, `RES_N`, `LATCH_CLK` at each end, `ROM_N` against what `U2` drives).

### Electrical Nets on a damaged Board

- Lights out: `U7` `CLK` alone on its Net (TTL, reads high, never rises); `U6` `4Y` pulses once a step as usual. The latch keeps the 0s `RES_N` cleared it to.
- Sleeper: `RES_N` held at 0 by `U8` (Strong) against `R9`; `SW1` only pulls it lower. The CPU stays in reset, the latch cleared.
- Full house: `RAM_N` and `U5`'s VCC pin are one Net with VCC (Supply) winning: the RAMs never answer. `lda light` reads the open bus, `$FF`, which the program writes to the latch (the test sees `$FF` at every step on seeds 1–3). A new `U5` drives the same Contention.
- Three lights: `LATCH_N` and `WR_N` one Net, low whenever either output is: every write cycle also makes `LATCH_CLK`. The last write before the delay is `JSR`'s push of its return address low byte, `$13` (the return address is `$F813`), which stays on the LEDs. A new `U6` gives the same.
- First light: P5g's stack cell; `RTS` goes to `$7814`, the open bus reads `$FF`, the 6502 runs it up to `$E000` (the ROM's mirror of `$F800`) and the program starts again.
- Lightless: P5g's internal address line; `light` and `dir` share a nibble, `sta dir` clears the light; `RTS` goes to erased ROM, which runs on to the start.
- Blackout: `ROM_N` is right on its Net; `U2` reads it as 1 and never drives the bus. The CPU runs the open bus and the RAMs' random power-on contents. On seed 1, the one `bw_ui` powers on with, it never writes `$8000`–`$9FFF`: dark LEDs. Other seeds (the Debug panel's Seed) wander differently: about one in six writes there once and the LEDs freeze on a random pattern (seed 8: `$46` from about 146 ms); none shows a moving light, so the Puzzle still reads as broken.

The test (`tests/machines/led_computer_test.cpp`) checks each symptom (the latch byte, `RES_N`, `LATCH_CLK` at each end) on seeds 1–3 where it does not depend on the seed, then the repair: the light at `D1`, `D2`, `D3`. Full house and Three lights swap the chip first, which does not help.

## Manuals

- `machines/110-led-computer/manuals/user.md`: power `J1`; after a moment one LED lights and the light runs from right to left and back, about 8 steps a second; `SW1` restarts it.
- `machines/110-led-computer/manuals/service.md`: Components, one bullet per reference in netlist order, `J1` included (`CheckServiceManual` wants each; `U3` and `U4`, each `Dn` and `Rn` a bullet of its own), roles: `X1` clock into `PHI0`; `U1` processor; `U2` program ROM at `$F800`–`$FFFF` (mirrored from `$E000`), selected by `ROM_N`; `U3`/`U4` RAM, low/high half of each byte, at `$0000`–`$03FF` (mirrored to `$1FFF`), selected by `RAM_N`, written by `WR_N`; `U5` address decoder, `A13`–`A15` to `RAM_N`, `LATCH_N`, `ROM_N`; `U6` write strobes `WR`, `WR_N`, `LATCH_WR`, `LATCH_CLK`; `U7` LED latch, loaded by `LATCH_CLK`, cleared by `RES_N`; `U8` power-on reset on `RES_N`; `SW1` Reset; `Dn` "LED: lit while `L(n−1)` is high"; `R1`–`R8` LED resistors; `R9`–`R11` pull-ups.

## Per-Machine checks

ADD_MACHINE: `manuals_test.cpp` list; once the Board is routed, `boards_test.cpp`, `electrical_test.cpp`, `board_scene_test.cpp` loops; `drc_led_computer`, `place_drc_led_computer` and `asm_led_computer` appear by themselves.

Session (`tests/session/session_test.cpp`, `session: led computer with every instrument` `[session][slow]`): 8 lamps `D1`–`D8`; 32 Clips (P5e: the groups `U1 A`, `U1 D`, `U1 PHI` by shift-click, then `RW`, `SYNC`, `RES_N`, `IRQ_N`, `NMI_N`; a bus Trigger on `8000`, Captured by 300 ms), Hooks on `U6` `4Y` (`LATCH_CLK`), `U1` `PHI0`, `RW` and `SYNC`, the probe on `U5` `Y7_N` (`ROM_N`), the meter lead on `D1` `A` (`L0`, already watched). Watched Nets: at most 16 for the LEDs + 32 + 4 + 1 = 53 ≤ `kMaxWatched` (96). After 300 ms the light is `$04` (latch writes at about 20.03, 144.8 and 269.6 ms): `D3` lit (brightness > 0.9), the other seven dark (< 0.1).

Release: `bw_ui machines/110-led-computer/led_computer.bwn` (then the Board) with every instrument, 1.00×, UI under 1 ms, screenshot viewed.

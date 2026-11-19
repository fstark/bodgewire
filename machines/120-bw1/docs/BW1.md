# 120 — BW-1

The first computer the player can program: a 6502 single-board computer on the KIM-1's own design, with a hex keypad and six LED digits on the Board and a monitor in ROM. The player types bytes into memory, runs them, stops them and steps them, as a KIM owner did. Its chips came with P5j ([BW1_CHIPS.md](../../../docs/p5j-bw1-chips/BW1_CHIPS.md)), its programs with P5k ([BW1_MONITOR.md](../../../docs/p5k-bw1-monitor/BW1_MONITOR.md)), the Machine with P5l ([BW1_MACHINE.md](../../../docs/p5l-bw1/BW1_MACHINE.md)).

## Lesson

A real computer's address map is a handful of chip selects from one decoder, and the same few port pins do several jobs at once: the 6530's port A lights the segments and reads the keypad's columns, its port B picks a digit or a row of keys through a second decoder. A key read wrong, a digit dark, a segment on every digit: each is one link of that scan. The monitor itself needs RAM (its variables in page 0, its stack in page 1), so bad RAM kills the very tool that would find it; the self-test in the other ROM, reached by one wire, uses none.

## Circuit

From the KIM-1 User Manual (MOS, 1976), Figs. 3.2–3.5 and 3.13, with the BW-1's changes (P5j–P5l):

- `X1` 1 MHz `OSC` → `PHI0` of `U1`, the 6502. `PHI2` clocks the 6530s and gates the RAM writes.
- `U25` power-on reset holds `RES_N` low for 20 ms, so `U1`, `U2` and `U3` start reset; the RS key `SW23` pulls the same line low; `R5` pulls it up.
- `U4` 74145 decodes `A10`–`A12` into 1K blocks, `A13`–`A15` not decoded (its `D` on `GND`): `K0` the RAM at `$0000`, `K5` both 6530s' RAM and ports (`CS1`), `K6` the 003's ROM at `$1800`, `K7` the 002's ROM at `$1C00` (their `RS0`). Open collector, pulled up by `R1`–`R4`. Everything is mirrored 8 times across the 64K; the 6502's vectors at `$FFFA` read the 002's `$1FFA`.
- `U5`–`U12` 6102s (1K × 1): `U5` holds `D7` … `U12` `D0`; `DIN` straight on the data bus, `DOUT` through the 74125s `U13` (`D0`–`D3`) and `U14` (`D4`–`D7`), enabled by `BUF_N` while the 6502 reads block 0. `U15` 74LS00 and `U16` 74LS04 make `RAM_RW` (low while `PHI2` is high in a write) and `BUF_N` (KIM Fig. 3.4).
- `U2` 6530-002, the monitor (`machines/120-bw1/roms/bw1_002.asm`): ROM `$1C00`–`$1FFF`, RAM `$17C0`–`$17FF` (the NMI and IRQ vectors at `$17FA`–`$17FF`), ports at `$1740`. `PA0`–`PA6` the segments and the keypad's columns; `PB1`–`PB4` select a digit or a row through `U24`.
- `U3` 6530-003, the self-test (`machines/120-bw1/roms/bw1_003.asm`): ROM `$1800`–`$1BFF`, RAM `$1780`–`$17BF`, ports at `$1700`, its port pins unconnected (cassette and TTY are left out).
- `U24` 74145 scans the display and the keypad: outputs 0–2 the keypad's rows (`ROW0`–`ROW2`), 3 the strap (`STRAP`, a bare pad), 4–9 the six digits' common cathodes (`DIG1`–`DIG6`), each low while selected.
- `U17` 74LS244 drives the segments: `PA0`–`PA6` → `SEG_A`–`SEG_G`, on the anodes of all six digits. A segment is lit on the one digit whose cathode `U24` holds low.
- `DS1`–`DS6` common-cathode digits, left to right, showing `AAAA DD`: the address and the byte there.
- `SW1`–`SW21` the keypad's 21 matrix keys: `0`–`F`, AD, DA, `+`, GO, PC. Each joins a row of `U24` to a column on `PA0`–`PA6`; a held key pulls its column low while its row is scanned, against the 6530's own pull-up.
- `SW22` ST pulls `NMI_N` low: the 6502 stops into the monitor. `SW23` RS: reset.
- `SW24` SST, a slide switch: on, it joins `NMI_N` to `SST_N`, a NAND (`U15`) of `SYNC` and `K7`: every opcode fetch outside `$1C00`–`$1FFF` pulls `NMI_N` low, so the 6502 stops after each instruction of the player's program.
- `R6`, `R7` pull up `NMI_N`, `IRQ_N`. `J1` power.
- The strap: `U24`'s output 3 to `PA0` (the KIM's TTY jumper). A Bodge Wire there makes reset run the self-test.

## Keypad

As on the KIM, from the top:

| | | | |
|---|---|---|---|
| GO `SW20` | ST `SW22` | RS `SW23` | SST `SW24` (slide) |
| AD `SW17` | DA `SW18` | PC `SW21` | `+` `SW19` |
| `C` `SW13` | `D` `SW14` | `E` `SW15` | `F` `SW16` |
| `8` `SW9` | `9` `SW10` | `A` `SW11` | `B` `SW12` |
| `4` `SW5` | `5` `SW6` | `6` `SW7` | `7` `SW8` |
| `0` `SW1` | `1` `SW2` | `2` `SW3` | `3` `SW4` |

`SW(n+1)` is the key whose code (`GETKEY`) is `n`. PC keys (Typing, always): the hex digits, Enter GO, `+`, `/` AD, `*` DA, `P` PC, Escape ST, Backspace RS.

## When it works

- At power-on the digits are dark for 20 ms, then show `0200` and the byte at `$0200` (random: the RAM's power-on).
- `AD 0 3 0 0` shows `0300 xx`; `DA A 9` writes `$A9` there; `+` steps to `0301`.
- A program typed at `$0200` runs with `AD 0200 GO`; a `BRK` stops it, showing the address two past the `BRK` (the 6502 pushes the `BRK`'s address plus 2: a `BRK` at `$0204` shows `0206`).
- ST stops a running program and shows where; SST on, each GO runs one instruction.
- RS returns to `0200`.
- With the strap bodged: reset shows `1`, then `2` … `5` for a moment, `888888`, each digit `8` alone, then `PA55` and the code of a held key (`PA5513` for GO).
- `U24`'s `PB1`–`PB4` select a digit about every 0.55 ms, `0200xx` scanned about 290 times a second; `RW` pulsing even while the monitor waits (it writes its stack and the ports all the time); `SYNC` high on every opcode fetch.

## Puzzles

Nineteen Puzzles (P5m), in three kinds. The self-test (User Manual) is a fair way to show a fault, as pressing the dice's button is: a Puzzle may only show there.

Seen in use:

- Odd seven: every `7` has a bottom bar: one glyph wrong, the others right, so the fault is in the monitor's segment table, not the wiring. Self-test `4C00xx`; swap `U2`.
- Missing bar: no digit lights its middle bar: the segment driver `U17`, which feeds all six.
- Half a segment: the last three digits lose their top bar: `SEG_A` cracked between `DS3` and `DS4`.
- Bright mess: `DS3` shows a bright jumble, nearly an 8: `U24`'s output for `DIG3` stuck low, so it lights with every digit.
- DA does nothing: DA, `A` and `3` dead, bytes typed after DA shift into the address: the `PA3` column cracked past `U17`.
- AD and GO dead: the seven keys of `ROW2` dead: cracked at `U24`.
- Deaf keypad: no key works, because `+` is stuck closed and the monitor waits for every key's release. The self-test's keypad shows `PA5512` with no hand on it.
- Bad byte: `$0300` keeps bit 7 set (`DA 00` shows `80`): one cell of `U5`. Self-test `130080`.
- Aliased page: starts on `1D27`, and `AD 0300` shows `0320`: `U7`'s `A8` stuck inside, page 1 (the stack) on page 0 in bit 5. Self-test `100020`.
- No stop: `BRK` stops, ST does not (a meaningless address): `U2`'s RAM holds `NMIV`, one bit stuck. Self-test `27FA01`.

Named by the self-test:

- Self-test fails: works, self-test `379504`: test 3 is `U3`'s RAM.
- ROM test fails: works, self-test `5800FF`: `U3`'s ROM sum.
- Dead machine: dark; self-test `110008`: bit 3 of main RAM is `U9`. The standard RAM Puzzle: swap it.
- Wrong suspect: frozen nonsense; self-test `100008`, bit 3 again, but swapping `U9` repairs nothing: `DOUT3` is cracked between `U9` and its buffer `U13`.
- Half a byte: shows `PA55` at power-on; self-test `11000E`: bits 0–3 together, so `U13`, not a RAM chip: `BUF_N` cracked off its enables.

Dead with the strap too, found with the instruments:

- Wrong block: dark: `U4`'s input `A10` stuck inside: its leg reads high while it selects `K6`; `K5` and `K7` never fall.
- Held reset: dark: RS stuck closed holds `RES_N` low.
- SST kills it: works until SST is at ON, then one digit lit and no key; back at OFF it shows `1C00`: `U15`'s `K7` input stuck, so SST stops inside the monitor.
- Two bits: dark: `U1`'s `D2` and `D3` bridged; the logic analyzer shows them equal.

Files, Fault lines and seeds: [BW1_DESIGN.md](BW1_DESIGN.md#puzzles).

Netlist, Board and manuals: [BW1_DESIGN.md](BW1_DESIGN.md).

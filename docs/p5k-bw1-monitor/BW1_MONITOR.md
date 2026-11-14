# P5k — BW-1 monitor

Roadmap step P5k ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), the second of four building the BW-1 on the KIM-1's own design (P5j chips, P5k monitor, P5l Machine, P5m Puzzles). This step writes the two programs the BW-1's 6530s hold, in `bw asm`, and tests them on the 6502 core with the rest of the KIM modelled in C++, before a netlist or a Board exists:

- the **monitor** in the 6530-002 (`$1C00`–`$1FFF`): the keypad and six digits, AD, DA, `+`, GO, PC, ST and SST, as a KIM owner uses them;
- a **self-test** in the 6530-003 (`$1800`–`$1BFF`) that uses no RAM: main RAM, both RIOT RAMs, both ROMs' checksums, the display and the keypad. It runs from `1800` GO, or at reset when a strap is closed, so a bodge wire reaches it on a machine whose monitor is dead.

And one assembler directive, `.checksum`, so each ROM carries the byte that makes its sum 0.

## Decided

With the roadmap row (2026-11-14): our own monitor, not MOS's, in the two masks at the KIM's addresses; variables in zero page and the stack in page 1 like the KIM's (bad RAM there stops it, as on the real one); the self-test in the 003, using no RAM; the strap on the scan line the KIM uses for its TTY jumper, read before any RAM is touched; tested against the 6502 core with the 6530 ports modelled.

Asked at the start of the step (2026-11-14):

- **KIM addresses.** The KIM's RAM locations and its published entry points, so programs from the KIM books run: saved registers `$EF`–`$F5`, `INH`, `POINTL`, `POINTH` at `$F9`–`$FB`, the user vectors `$17FA`–`$17FF`, `SAVE` at `$1C00`, `START` at `$1C4F`, `SCANDS` at `$1F1F`, `GETKEY` at `$1F6A` and the others below, the segment table at `$1FE7`. The code behind them is ours.
- **Reset sets the vectors.** On the KIM, ST and SST do nothing until the owner types `$1C00` into `$17FA`/`$17FB`. Our reset writes `$1C00` into the NMI and IRQ vectors, so ST, SST and `BRK` stop into the monitor from power-on. A bad RIOT RAM byte there breaks ST: a Puzzle link.
- **Self-test display: step, fail, echo.** Each test's number lit while it runs; a failure stops on a six-digit code: the test, an address and the bad bits (or the bad sum); all passed: every segment lit, then each digit alone, then a keypad echo forever.
- **Tests on the core and a model** (`tests/machines/bw1_model`): `Mos6502Core` with the two ROMs, 1K RAM, both RIOT RAMs, the 6530 ports, the 74145, keypad, strap, ST, SST and the six digits modelled in C++. The real Parts run them in P5l.

## The KIM it runs on

From the KIM-1 User Manual (MOS, 1976; Figs. 3.3, 3.5, 3.6, 3.13 and Appendix I's listing and symbol table), the parts the programs see. P5l wires the BW-1 so.

- **Memory.** A 74145 (U4) decodes `A10`–`A12` into 1K blocks, `A13`–`A15` undecoded: `K0` the 1K RAM at `$0000`; `K5` both 6530s' RAM and I/O (`CS1`); `K6` the 003's ROM at `$1800` (its `RS0`); `K7` the 002's ROM at `$1C00` (its `RS0`). RIOT RAM: 003 `$1780`–`$17BF`, 002 `$17C0`–`$17FF`. I/O: 003 `$1700`, 002 `$1740` (`SAD`, `PADD`, `SBD`, `PBDD`). The 6502's vectors at `$FFFA` read the 002's `$1FFA`.
- **Display and keypad** (Fig. 3.5). The 002's `PB1`–`PB4` drive a second 74145 (U24). Its outputs 4 to 9 switch digits 1 to 6, left to right (`SBD` = `$08`, `$0A` … `$12`); a segment is lit while its `PA` line is high: `PA0` segment a … `PA6` segment g, no decimal point. Outputs 0, 1, 2 are the keypad's rows (`SBD` = `$00`, `$02`, `$04`); a held key pulls its column, a `PA` line, low while its row is scanned, against the 6530's own pull-up. Keys, column `PA6` to `PA0`: row 0 `0`–`6`, row 1 `7`–`D`, row 2 `E`, `F`, AD, DA, `+`, GO, PC. Key codes (`GETKEY`): `0`–`F` → `$00`–`$0F`, AD `$10`, DA `$11`, `+` `$12`, GO `$13`, PC `$14`, none `$15`.
- **The strap.** Output 3 (`SBD` = `$06`) goes to the application connector; the KIM's TTY jumper joins it to `PA0`, and the KIM's monitor reads `PA0` with output 3 selected to choose TTY or keypad. Ours reads the same strap to choose the self-test.
- **ST, RS, SST** (Fig. 3.3). ST and RS go through a 556 to `NMI` and `RES`. SST closed joins `NMI` to a NAND of `SYNC` and `K7`: an opcode fetch outside `$1C00`–`$1FFF` pulls `NMI` low, so the 6502 stops after each instruction that is not in the 002's ROM. Code in the 003's ROM (`$1800`) is stepped too.

## The monitor (6530-002)

What the player does is the KIM's (User Manual, chapter 4), seen on the digits as `AAAA DD`: the address `POINTH POINTL` and the byte there.

- **Reset** (RS, power-on): reads the strap, no RAM touched; closed: the self-test. Open: stack at `$01FF`; `SPUSER` = `$FF`; NMI and IRQ vectors `$1C00`; saved `A`, `X`, `Y`, `P` = 0; address `$0200` (where the KIM books' programs start); address mode. Shows `0200 xx`.
- **AD**: address mode: each hex key shifts into the address from the right (`AD 0 3 0 0` → `0300`). **DA**: data mode: each hex key shifts into the byte at the address, written to memory. **`+`**: the next address, either mode. **PC**: the address becomes the saved PC.
- **GO**: runs from the address with the saved registers (`$EF`–`$F5`) and stack pointer.
- **ST** (NMI), **`BRK`** (IRQ vector), **SST** (NMI after each instruction): `SAVE` stores `A`, `P`, `PC`, `Y`, `X`, `S` (KIM order and places), shows the next instruction's address and opcode, address mode. GO goes on.
- **Keys** are read once each: the monitor waits for every key to be released, then for one held through two scans.
- **Entry points** at the KIM's addresses, ours behind them:

| Address | KIM name | What it does |
|---|---|---|
| `$1C00` | `SAVE` | NMI / `BRK` entry: saves the registers, shows the PC (`SAVE1` `$1C05` from `JSR`, `SAVE2` `$1C0F`, as the KIM's) |
| `$1C1C` | `NMIT` | `JMP ($17FA)`: the 6502's NMI vector points here |
| `$1C1F` | `IRQT` | `JMP ($17FE)`: the IRQ vector |
| `$1C22` | `RST` | the reset vector |
| `$1C4F` | `START` | the monitor's loop: `JMP $1C4F` returns to it |
| `$1EFE` | `AK` | is a key held? `A` = 0 if not |
| `$1F19` | `SCAND` | `INH` ← the byte at `POINTL`/`POINTH`, then `SCANDS` |
| `$1F1F` | `SCANDS` | shows `POINTH POINTL INH` once (about 3.5 ms), then `AK` |
| `$1F40` | (`KEYIN` in the KIM books) | port A to inputs, then `AK` |
| `$1F48` | `CONVD` | shows hex digit `A` on the digit `X` selects, `X` += 2; `Y` kept |
| `$1F63` | `INCPT` | `POINTL`/`POINTH` + 1 |
| `$1F6A` | `GETKEY` | `A` = the key held (codes above), `$15` if none |
| `$1FE7` | `TABLE` | segments of `0`–`F`, 16 bytes |

`$1F40` is not in the listing's symbol table (its pages 18–25 are missing from our scan); the KIM books call it `KEYIN`. Tape, TTY, `GETCH`, `OUTCH`, `PRTBYT` and the rest are left out (the row leaves out cassette and TTY): a program that calls them does not work.

## The self-test (6530-003)

Entered at `$1800`, by `1800` GO or at reset with the strap closed. It uses the 6502's `A`, `X`, `Y` and `S` and the 003's port A direction register (`$1701`, a register inside the 6530; P5l leaves the 003's `PA` pins unconnected) — no RAM, no stack, no `JSR`. SST must be off (the 003's ROM is stepped) and ST does nothing useful (its vector is in RAM).

1. **Main RAM** `$0000`–`$03FF`, test `1`: a pattern a page (`X`, `X ^ $FF`, `X ^ $55`, `X ^ $AA` at offset `X` of pages 0 to 3) written then compared, then its complement: every bit of every byte holds both values, and an address line stuck or two joined make two pages or two bytes alias.
2. **002 RIOT RAM** `$17C0`–`$17FF`, test `2`; **003 RIOT RAM** `$1780`–`$17BF`, test `3`: `X ^ $A5` then its complement.
3. **ROMs**, test `4` (002) and `5` (003): the 8-bit sum of the 1024 bytes must be 0 (`.checksum`).
4. **Display**, test `6`: every segment of every digit lit for about 0.8 s, then each digit alone (`8`) for about 0.3 s, left to right.
5. **Keypad**: `PASS` on digits 1–4 and the held key's code on digits 5–6 (blank with none), forever.

While tests 1–5 run, digit 1 shows the test's number alone. A failure stops on six digits, scanned forever: the test, three hex digits of the address, two of the bad bits (the XOR of written and read) or, for a ROM, of the sum:

| Code | Means |
|---|---|
| `123408` | test 1, `$0234`, bit 3 bad: the 6102 holding `D3` |
| `27D501` | test 2, `$17D5`, bit 0 |
| `3795F0` | test 3, `$1795`, bits 4–7 bad |
| `4C00xx` | test 4, the 002's ROM sums to `xx` |
| `5800xx` | test 5, the 003's ROM |

## `.checksum`

`bw asm`: `.checksum` emits one byte, chosen after the rest of the program so that the 1024 (or `.rom` SIZE) words of the image, unwritten `$FF`s included, sum to 0 modulo 256. Once a program; `.checksum given twice`; before `.rom`, the usual `no .rom BASE, SIZE before the first byte`.

## Tests

On `tests/machines/bw1_model` (the core, a cycle a microsecond):

- Assembler: `.checksum` makes the sum 0 (written before and after other code), its value in the `.rom` text; the errors. Both programs match their committed `.rom` (`asm_bw1_002`, `asm_bw1_003`).
- Monitor, seeds of the RAM's random power-on 1–3: reset shows `0200 xx`; AD and hex keys set the address, DA and hex keys write memory, `+` steps; a program typed in and run with GO, ending in `BRK`, shows the address after the `BRK` and left its result in RAM; PC shows the saved PC; GO uses the saved `A` typed at `$F3`; ST stops a `JMP *` loop and shows `0200 4C`; SST on: each GO runs one instruction and shows the next, the saved `A` changing as it should; a program calling `SCANDS` and `GETKEY` at the KIM addresses (design §5.1) reads a key; a key held is read once.
- Stack and zero page: `$01FF` stuck at `$00` (the return address of the reset's first `JSR`), or a stuck bit in `POINTH`, stops the monitor from showing `0200` (the bad RAM link).
- Self-test: from reset with the strap, and by `AD 1 8 0 0 GO`: `PASS` after about 3 s, the display test's frames on the way, a key's code echoed (`PASS13` for GO); each failure code in the table, from a stuck RAM bit, a stuck RIOT RAM bit and a changed ROM byte; with every RAM byte reading `$FF` (dead RAM) it still reports test 1, so it uses no RAM; the strap open, reset runs the monitor.
- The model: nothing touches the 6530 timers (the model flags an access), except in the bad-RAM cases, where the CPU runs random bytes.

Design: [BW1_MONITOR_DESIGN.md](BW1_MONITOR_DESIGN.md). Plan: [BW1_MONITOR_PLAN.md](BW1_MONITOR_PLAN.md).

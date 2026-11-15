# P5p — BW-2001 firmware

Roadmap step P5p ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), the third of the BW-2001's (P5n chips, P5o Tiny BASIC spec, P5p firmware, P5q Machine, P5r Puzzles). It writes our own ROMs for the PET 2001's sockets in `bw asm`:

- **Tiny BASIC** to the P5o spec ([TINY_BASIC.md](../p5o-tiny-basic/TINY_BASIC.md));
- a **full-screen editor** as the PET's: the cursor goes anywhere, and RETURN enters the line under it;
- a small **machine-code monitor** in the manner of the PET's TIM, reached from BASIC;
- a **self-test** of the video RAM, the RAM, the ROMs and the keyboard.

They are tested on the 6502 core with the PET around it modelled in C++, then on a minimal netlist of the real Parts. Both run the P5o conformance set. No picture yet: the video chain is P5q's. The tests read the screen from the video RAM.

## Decided

With the roadmap row (2026-11-15): our own ROM in `bw asm`; Tiny BASIC to the P5o spec; a full-screen editor as the PET's; a small monitor reached from BASIC; a self-test of the RAM, video and keyboard; headless on a minimal netlist, with the P5o conformance set passing.

Asked at the start of the step (2026-11-15):

- **ROMs split by function, as the PET's.** BASIC is in the `$C000` sockets, the editor in `$E000` (the PET's "edit ROM"), and reset, the monitor and the self-test in `$F000`–`$FFFF` with the vectors. Each chip has its own checksum, and a dead chip kills one function. Sockets that we leave unused stay empty, and P5q decides whether they stay on the Board. Offered instead: everything in `$F000`–`$FFFF` (two chips), or BASIC in `$C000` and the rest in `$F000`.
- **Self-test from DIAG at reset, and from the monitor.** As the PET: PIA1 `PA7` (DIAG, user port pin 5) held low at reset runs it before any RAM is used, like the BW-1's strap. The monitor's `.T` runs it too. It needs no working RAM and reports on the screen (see "The self-test" for the two bytes it borrows once page 0 has passed). Offered instead: DIAG only, or the monitor only.
- **A TIM-like monitor.** It uses the command set of the PET's own monitor: `.M` dumps memory as `.:` lines, which the player edits in place and enters with RETURN; `.R` and `.;` for the registers; `.G` to go; `.X` back to BASIC. There is no `.L`/`.S` (no tape). BASIC enters it through `USR(61440)`, and a `BRK` lands there too. Offered instead: a smaller command set of our own.
- **Tests: the model in every build, and the full set on the netlist in release.** The whole conformance set, plus what P5o leaves to each ROM, runs on `Mos6502Core` with the PET modelled (`tests/machines/bw2001_model`). The minimal netlist boots and runs a short session and the self-test in every build. It runs the whole set only in the release build. The reason: about 2,900 typed keys at about 60 ms each is about 3 minutes on the netlist in release, but hours in debug. Offered instead: the model and a netlist smoke test only, or the netlist only.

Chosen in this spec (not asked):

- 8 KB of RAM sized at power-on;
- the banner;
- the RAM map and the `{RAM}` address;
- the keyboard read at each retrace;
- the shifted keys type what the unshifted ones do (no graphics);
- the editor's logical lines link two rows at most;
- the screen is written at any time, so P5q decides whether CPU access shows snow;
- the self-test's screen layout;
- the monitor's exact formats.

## The PET it runs on

The chips are P5n's, at the PET 2001's addresses ([BW2001_CHIPS.md](../p5n-bw2001-chips/BW2001_CHIPS.md)):

| Address | What |
|---|---|
| `$0000`–`$1FFF` | RAM, 8 KB (UI1–UI8, UJ1–UJ8) |
| `$8000`–`$83E7` | the screen, 40 × 25 screen codes in the 1 KB video RAM (UC3, UC4); `$83E8`–`$83FF` unused |
| `$C000`–`$CFFF` | BASIC: UH1 `$C000`, UH5 `$C800` |
| `$D000`–`$DFFF` | UH2, UH6: empty |
| `$E000`–`$E7FF` | the editor: UH3 |
| `$E810` | PIA1: `PA0`–`PA3` the keyboard row (UG9, a 74LS145), `PA7` DIAG; `PB0`–`PB7` the columns; `CB1` the retrace |
| `$F000`–`$F7FF` | the monitor: UH4 |
| `$F800`–`$FFFF` | reset, interrupts, the self-test and the vectors: UH7 |

PIA2 and the VIA are not used. BASIC is expected to fit in 4 KB. If it does not, it goes on into UH2, and the notes say so.

The self-test is in UH7 with the reset code, so it runs even when UH4, UH3 or the BASIC chips are dead.

**Screen codes.** BASIC's characters `$40`–`$5F` are screen codes `$00`–`$1F`, and `$20`–`$3F` are the same. Bit 7 is reverse video, used only for the cursor.

**The keyboard** is the PET 2001's (the chiclet "graphics" keyboard). Its matrix is read from the real editor ROM's decode table (901447-05 at `$E75C`) and from MAME's `pet.cpp`. A row is scanned when its number is on `PA0`–`PA3`, and a held key pulls its column low. Every character the conformance set types is on an unshifted key: the top two rows type `! " # $ % ' & \ ( ) ←` unshifted, and the digits are on the keypad.

| Row | `PB7` | `PB6` | `PB5` | `PB4` | `PB3` | `PB2` | `PB1` | `PB0` |
|---|---|---|---|---|---|---|---|---|
| 0 | CRSR → | HOME | `←` | `(` | `&` | `%` | `#` | `!` |
| 1 | DEL | CRSR ↓ | | `)` | `\` | `'` | `$` | `"` |
| 2 | `9` | `7` | `↑` | `O` | `U` | `T` | `E` | `Q` |
| 3 | `/` | `8` | | `P` | `I` | `Y` | `R` | `W` |
| 4 | `6` | `4` | | `L` | `J` | `G` | `D` | `A` |
| 5 | `*` | `5` | | `:` | `K` | `H` | `F` | `S` |
| 6 | `3` | `1` | RETURN | `;` | `M` | `B` | `C` | `Z` |
| 7 | `+` | `2` | | `?` | `,` | `N` | `V` | `X` |
| 8 | `-` | `0` | SHIFT (right) | `>` | | `]` | `@` | SHIFT (left) |
| 9 | `=` | `.` | | STOP | `<` | SPACE | `[` | RVS |

`←` types `_` (`$5F`) and `↑` types `^` (`$5E`), BASIC's codes for them. With SHIFT, CRSR → is ←, CRSR ↓ is ↑, HOME is CLR and DEL is INST. Any other key types the same with or without SHIFT. RVS does nothing. STOP is the break key.

## Power-on

The reset code is in UH7. It first sets PIA1 and reads DIAG (`PA7`). If DIAG is low, the self-test runs. Otherwise:

1. **Size the RAM.** Each byte from `$0400` up is written and read back, with `$55` and then `$AA`. The first byte that fails, or `$2000`, is the top of BASIC's program area. On a fault-free BW-2001 that is `$2000`. A bad cell above `$0400` gives a smaller machine, which is a Puzzle link.
2. Clear the screen, start the editor, enable the retrace interrupt.
3. Print the banner:

```
*** BW-2001 TINY BASIC ***

7166 BYTES FREE

READY
```

7166 is the top (`$2000`) minus the program base (`$0400`) minus the end marker's two bytes. The banner is the Machine's. BASIC prints `READY` after its power-on `NEW` (P5o).

## RAM

| Address | What |
|---|---|
| `$0000`–`$007F` | BASIC, the editor, the kernel (zero page) |
| `$0080`–`$00FF` | free for `USR` routines |
| `$0100`–`$01FF` | the 6502 stack |
| `$0200`–`$02FF` | the entered line (64 characters and a terminator), the tokenised direct line, the control stack (8 entries), the key buffer |
| `$0300`–`$0333` | the variables `A`–`Z` (P5o layout) |
| `$0334`–`$03EF` | the editor's row links, the monitor's saved registers |
| `$03F0`–`$03FF` | free for the player: the harness's `{RAM}` is 1008 (`$03F0`) |
| `$0400`–top | the program (P5o layout), from `$0400` |

A bad cell in pages 0 to 3 stops the machine somewhere before or soon after `READY`. These pages are not sized. The self-test names the cell.

**`USR(A)`** (P5o) `JSR`s to `A`. The routine returns with `RTS`, the result's low byte in `A` and its high byte in `Y`. It may change `A`, `X`, `Y`, the flags and `$0080`–`$00FF`, and it must leave the stack as it found it. The decimal mode flag must be clear when it returns.

## The editor

The screen is 40 columns by 25 rows. The cursor blinks (reverse video) while the editor waits for a key, and is hidden while BASIC prints.

- **Keys.**
  - A printable key writes its character at the cursor, and the cursor moves right.
  - CRSR → ← ↓ ↑ move the cursor without changing anything. Past the right edge it goes to column 0 of the next row, and before column 0 to column 39 of the row above. Moving down from the bottom row scrolls the screen up, and moving up from the top row stays there.
  - HOME puts the cursor top left, and CLR also clears the screen.
  - DEL deletes the character left of the cursor. The rest of the logical line moves left, and a space comes in at its end. INST does the opposite at the cursor and loses the last cell of the logical line. At column 0 of a logical line's first row, DEL does nothing.
- **Logical lines.** A row is joined to the one above it when typing or printing runs off the end of the row above. A logical line is one row, or two rows joined. A third row is never joined: a character typed or printed past column 79 of a logical line starts a new one.
- **The 64-character limit.** A printable key at position 64 or later of its logical line is ignored.
- **RETURN.**
  - The editor takes the logical line the cursor is on. It drops the reverse-video bit, converts the screen codes to BASIC's characters, keeps the first 64, and removes trailing spaces from those.
  - It moves the cursor to column 0 of the row after the logical line, scrolling at the bottom, and hands the line to BASIC (the P5o contract's "line").
  - For `INPUT` ("read a line"), it hands over only what is right of the place where the read began, if the cursor is still on that logical line. Otherwise it hands over the whole logical line under the cursor.
- **Print.**
  - `$0D` moves to column 0 of the next row, which then starts a logical line of its own.
  - `$0C` clears the screen and the links.
  - At the bottom, the screen scrolls up one row, and so do the links.
- **Keys and break.**
  - The keyboard is read at each retrace interrupt (PIA1 `CB1`, 60 Hz). A key counts once, when it is first seen held. There is no repeat.
  - Keys go to an 8-key buffer, which is emptied each time the editor starts to read a line. So keys typed while BASIC runs are dropped.
  - STOP sets the break flag, which BASIC reads once ("break?").
  - While the editor waits at `READY`, or for a monitor command, STOP is discarded. While `INPUT` waits, STOP makes "read a line" return *break*.
- **The harness signal** (P5o, "When to type"). `WAITING` in zero page is 1 while the editor waits for keys to make a line, and 0 otherwise. `LINES` counts the lines handed over, modulo 256. The design gives both addresses. A harness types the next line once `WAITING` is 1 and `LINES` has moved since the last line it typed.

## The monitor

The monitor is entered in two ways:

- `USR(61440)` (`$F000`) prints `C*`;
- a `BRK` prints `B*`.

Then it prints the registers, and `.` on a new row as its prompt:

```
C*
   PC  SR AC XR YR SP
.; C123 30 00 05 00 F6
.
```

Each command is a line that starts with `.`, entered with RETURN. Because the editor is full-screen, any `.:` or `.;` row on the screen can be changed in place and entered again.

| Command | Does |
|---|---|
| `.M aaaa bbbb` | prints `.: aaaa` and 8 bytes for each row from `aaaa` through `bbbb` (`aaaa` taken down to a multiple of 8); STOP ends it early |
| `.: aaaa bb …` | writes the 1 to 8 bytes from `aaaa` |
| `.R` | prints the header row and the `.;` row |
| `.; pppp ss aa xx yy ss` | sets the saved PC, SR, AC, XR, YR and SP |
| `.G [aaaa]` | runs from `aaaa`, or the saved PC, with the saved registers and SP (`RTI`) |
| `.X` | back to BASIC: `READY` on a fresh line, the program and variables kept; a program that called `USR` ends |
| `.T` | the self-test (it never returns: reset to leave it) |

- **Formats.** Addresses are 4 hex digits and bytes 2, separated by spaces. Anything else prints `?` on its own row, then the prompt.
- **`C*` registers.**
  - The saved PC is the return into BASIC's `USR`, and SP is the stack pointer there.
  - `.G` alone returns from the `USR`, with AC (low byte) and YR (high byte) as its value: `.; …` then `.G` sets what `USR` returns.
- **`B*` registers.**
  - The saved PC is the `BRK`'s address plus 2, as the 6502 pushes it.
  - `.G` alone goes on there.

## The self-test

DIAG low at reset, or `.T`, runs it, with interrupts off. It first tests RAM page 0 using only the 6502's registers, the video RAM and PIA1. A 6502 cannot walk 8 KB without a pointer in zero page, so after page 0 passes, `$00`–`$01` are the only RAM it uses until the whole RAM has passed. A machine whose RAM is dead still reports `ZERO PAGE BAD 0000`. Each test writes its row:

```
BW-2001 SELF-TEST

ZERO PAGE OK
VIDEO RAM OK
RAM OK
ROM C000 OK
ROM C800 OK
ROM E000 OK
ROM F000 OK
ROM F800 OK

KEY 65 RETURN
```

0. **Zero page**, `$0000`–`$00FF`: `X ^ $A5`, then its complement. A failure writes `ZERO PAGE BAD 00xx bb` on its row and stops, leaving the rows below it blank.
1. **Video RAM**, `$8000`–`$83FF`.
   - First pass: a pattern that depends on each byte's page and offset is written everywhere, then compared everywhere, so two addresses joined or a line stuck are found.
   - Second pass: the same with the pattern's complement, so every bit holds both values.
   - A failure clears the screen if it can, writes `VIDEO RAM BAD 8123 08` (the address and the bad bits, written XOR read), and stops there. The screen may be garbled: that is the fault.
2. **RAM**, `$0002`–`$1FFF`, the same two passes, each writing it all and then comparing it all, so a page aliased onto page 0 is found too. A failure writes `RAM BAD 1234 08` and stops. An address fault that reaches `$00`–`$01` turns the pointer, and the test then fails at some address: still `RAM BAD`.
3. **ROMs.** Each chip holding our code must sum to 0 modulo 256 (`.checksum`). A bad one writes `ROM C800 BAD 5A`, with the sum, and the test goes on.
4. **Keyboard.** The `KEY` row shows, forever, the first held key in scan order: the row and column digits, then its legend (`A`, `5`, `SPACE`, `RETURN`, `STOP`, `SHIFT L`, `SHIFT R`, `DEL`, `CRSR DN`, `CRSR RT`, `HOME`, `RVS`). With no key held, it shows `KEY` alone. A dead row, column or key is seen here.

## Tests

On `tests/machines/bw2001_model` (the core at 1 MHz, the ROMs from `roms/`, 8 KB of RAM random from a seed, the video RAM, PIA1, the keyboard matrix, DIAG, and the retrace every 16 667 cycles):

- **Power-on**, seeds 1 to 3: the banner and `READY`. With RAM ending at `$1800`, `5118 BYTES FREE`. With a stuck cell at `$1234`, `3634 BYTES FREE`.
- **The conformance set**: all 31 sessions in `tests/data/basic/`, typed key by key through the matrix, the full view compared (cursor bit ignored). `{RAM}` is 1008.
- **What P5o leaves to the ROM**:
  - `USR` to a routine `POKE`d at `{RAM}` returns `A` and `Y`;
  - `OUT OF MEMORY` on a 1.25 KB machine (top `$0500`): a new line refused, and an existing line entered again with a longer text keeps its old text;
  - STOP at `READY` discarded.
- **The editor**:
  - typed text echoes;
  - CRSR moves without changing anything;
  - editing a listed line in place and RETURN replaces it in the program (`LIST` shows the change);
  - DEL and INST;
  - a 50-character line wraps onto a second row and is entered whole;
  - the 64-character limit;
  - a 30-line listing scrolls, and the links scroll with it;
  - HOME and CLR;
  - `INPUT` takes only what is right of its prompt.
- **The monitor**:
  - `PRINT USR(61440)` shows `C*` and the registers;
  - `.M` dumps the program's bytes (`10 PRINT A` as `0A 00 07 80 20 41 00`);
  - a dump row edited in place writes memory;
  - `.:` typed writes memory;
  - `.;` then `.G` makes `USR` return the value set;
  - a routine with a `BRK` shows `B*` and its PC, and `.G` goes on after it;
  - `.X` gives `READY` with the program kept;
  - a bad command gives `?`.
- **The self-test**:
  - DIAG at reset, and `.T`: every row `OK`, then a held key's row, column and legend;
  - a stuck video RAM bit, a stuck RAM bit and a patched ROM byte each give their row;
  - with every RAM byte reading `$FF` it still reports `ZERO PAGE BAD 0000 xx`, so it needs no RAM to say so;
  - DIAG high: BASIC.
- **The ROMs**: each chip's 2 KB sums to 0. Each entry point in the design's tables holds a `JMP`. The three `.asm` files match their committed `.rom` files (`asm_bw2001_*`).
- **The tools**: `.include` and `.checksum BASE, SIZE` in `bw asm`, and `offset=` on the ROM Parts, with their errors.

On the minimal netlist `tests/data/bw2001_firmware.bwn` (`tests/netlist/bw2001_netlist_test.cpp`) (P5n's bench with our ROMs in the sockets, the full 73-key matrix and a DIAG switch; `[slow]`):

- **Every build**: power-on to the banner and `READY`; session `27_cls`; DIAG to the `KEY` row with a key held.
- **Release only** (skipped in debug, where it would take hours): the whole conformance set.
- **Headless ratio**: at `READY` (`bw run`) and while typing (the release case's wall time against its simulated time), measured and recorded.

A difference between the ROM and the reference interpreter is a question for the P5o spec, as P5o says. Either the ROM is fixed, or the spec is fixed with its interpreter and sessions, in the same commit.

Design: [BW2001_FIRMWARE_DESIGN.md](BW2001_FIRMWARE_DESIGN.md). Plan: [BW2001_FIRMWARE_PLAN.md](BW2001_FIRMWARE_PLAN.md).

# P5p BW-2001 firmware — Detailed Design

Implements [BW2001_FIRMWARE.md](BW2001_FIRMWARE.md). It adds:

- two `bw asm` directives;
- one parameter on the ROM Parts;
- three ROM programs and the file they share;
- a model of the PET around the 6502 core;
- a session harness that drives the model and the netlist alike;
- a minimal netlist.

It changes no kernel, UI or Board code.

Recipes: [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §2 covers the program: `roms/<m>.asm`, its header comment, `bw asm`, the `asm_<m>` ctest, fixed entry points by `.org` with the "written twice" check, `.checksum`, and a model before the netlist. [ADD_PART.md](../recipes/ADD_PART.md) covers a Part's parameters: required and file parameters, and the datasheet's `## Function` paragraph that describes one (as `select=` in `datasheets/2316B.md`). The language and the editor contract are [TINY_BASIC.md](../p5o-tiny-basic/TINY_BASIC.md) (cited "P5o §Section"). All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. `bw asm` — `src/asm/asm6502.{hpp,cpp}`

### 1.1 `.include "file"`

- Before pass 1, the source's lines are expanded: a line whose code is `.include "name"` is replaced by the named file's lines.
  - The name is resolved against the including file's directory.
  - Includes nest up to 8 deep (`.include nested too deep`).
  - A file that cannot be read gives `<file> line N: cannot read '<name>'`.
- Each expanded line keeps its file name and line number, so an error inside an included file names that file: `bw2001.inc line 12: …`.
- The line table becomes `struct Line { std::string text; std::string file; int number; }`.
- API:
  - `Assemble6502(source, file, std::optional<std::filesystem::path> includeDir = std::nullopt)`.
  - `Assemble6502File` passes `path.parent_path()`, or `"."` when that is empty (`bw asm x.asm` run inside `roms/`).
  - With no `includeDir`, `.include` fails with `.include needs a file on disk`, which the in-memory unit tests check.
  - Today the lines are views into the one source text. Included files' texts are owned by the assembler (a `std::deque<std::string>`), so the views stay valid.
- The `.rom` text carries every expanded line, as it does today, so the committed `.rom` changes when an included file does, and `asm_<m>` catches a stale one.
- The header comment in `asm6502.hpp` lists the new directive.

### 1.2 `.checksum BASE, SIZE`

- `.checksum` with no value keeps today's meaning: the whole `.rom` window.
- `.checksum BASE, SIZE` makes the `SIZE` bytes from `BASE` (unwritten `$FF`s included) sum to 0 modulo 256.
  - The byte itself must lie in that window: `.checksum byte outside its window`.
  - The window must lie in the `.rom` window: `.checksum window outside .rom`.
  - Several are allowed if their windows do not overlap: `.checksum windows overlap`.
  - The plain form and the windowed form do not mix: `.checksum given twice`, as today.
  - One value, or three or more: `.checksum takes nothing or BASE, SIZE`. This replaces today's `.checksum takes no value`, so the expectation for `.checksum 5` in `tests/asm/asm6502_test.cpp` (`asm: .checksum makes the ROM sum to 0`) changes in phase 1.
- `checksumAt_` becomes a vector of `{offset, base, size, value}`. `run()` computes each byte from its own window, then reruns pass 2 once, as today.

Tests (`tests/asm/asm6502_test.cpp`):

- `asm: .include`: a temporary directory holding `a.asm` that includes `b.inc` (a constant and a routine); the bytes match the same source written inline. Error cases:
  - an error in `b.inc` names `b.inc` and its line;
  - a missing file;
  - nine levels of nesting;
  - the in-memory form.
- `asm: .checksum BASE, SIZE`: a `.rom $F000, $1000` program with one checksum in each 2 KB half; each half sums to 0. Each of the three errors above is checked.

## 2. `offset=` on the ROM Parts — `src/parts/catalogue.cpp`

- `CreateRom` (2716, 2316B, 2513) accepts `offset=N`, `$hex` or decimal.
  - With an offset, the contents file is read at the full 64K (`ReadRomContents(path, 65536, width)`), because `ParseRomContents` refuses any word past the size it is given (`rom_contents.cpp`). The Part keeps words `offset` to `offset + size − 1`.
  - Words that the file never wrote are `$FF`, as everywhere else. A slice past the file's last word is an erased chip, not an error.
- Errors:
  - `bad offset '<v>'`;
  - an offset that is not a multiple of the Part's own size (`offset must be a multiple of <size>`), or one past 64K;
  - an offset on a `.bin` raw dump (`offset= is for .rom text files`: a dump is one chip).
- So the BW-2001's netlist names one `.rom` for several sockets: `contents=../roms/bw2001_basic.rom offset=$800`.
- Docs:
  - `datasheets/2316B.md`, `datasheets/2716.md` and `datasheets/2513.md`: one paragraph on `offset=` in `## Function`, as `select=` is in the 2316B's (ADD_PART's rule for a file parameter: relative to the netlist);
  - `docs/GLOSSARY.md` "ROM contents": "a slice of it with `offset=`".
- Tests (`tests/parts/rom_test.cpp`): a 4 KB `.rom` text into two 2316Bs at offsets 0 and `$800`, each reading its own half. Each of the three errors above is checked.

## 3. The shared file — `roms/bw2001.inc`

The file every BW-2001 ROM includes. It holds no code, only `NAME = value` lines. Its suffix keeps the `roms/*.asm` glob (ctest `asm_*`, the `asm:` test) from assembling it alone.

- **I/O**: `SCREEN = $8000`, `COLUMNS = 40`, `ROWS = 25`; `PIA1 = $E810` (`+0` PA or DDRA, `+1` CRA, `+2` PB or DDRB, `+3` CRB).
- **Zero page**:
  - `$00`–`$3F` BASIC (§6.1);
  - `$40`–`$5F` the editor (§5.1): `WAITING = $40`, `LINES = $41`;
  - `$60`–`$7F` the kernel: `MEMTOP = $60` (2 bytes, the top that reset found), `PTR = $62` (2), the monitor's own bytes.
- **Pages 2 and 3**:

| Name | Address | Size |
|---|---|---|
| `LINBUF` | `$0200` | 65: the line from the editor, `$00`-terminated |
| `DIRBUF` | `$0241` | 72: the tokenised direct line, laid out as a record (P5o §The program in RAM) |
| `CSTACK` | `$0290` | 72: 8 entries of 9 bytes |
| `KEYBUF` | `$02F0` | 8 |
| `VARS` | `$0300` | 52 |
| `LINKS` | `$0334` | 25: 1 when the row goes on from the row above |
| `SAVED` | `$0350` | 7: PC (2), SR, AC, XR, YR, SP |
| `USRRAM` | `$03F0` | 16, the player's (`{RAM}` 1008) |
| `PROGRAM` | `$0400` | the program base |

- **Entry points**: each is a `JMP` at a fixed address, placed by `.org` in its ROM (ADD_MACHINE §2). The other ROMs reach a ROM only through these.

| Name | Address | ROM | Contract |
|---|---|---|---|
| `COLD` | `$C000` | BASIC | power-on `NEW`, `READY`, the main loop; never returns |
| `WARM` | `$C003` | BASIC | the 6502 stack reset, `READY` on a fresh line, the program and variables kept (the monitor's `.X`) |
| `NUMOUT` | `$C006` | BASIC | prints `A` (low) and `Y` (high) as a signed decimal through `EDPRINT` |
| `EDINIT` | `$E000` | editor | clears the screen, the links, the key buffer and the flags |
| `EDPRINT` | `$E003` | editor | P5o "print": `A` is the character; `A`, `X`, `Y` kept |
| `EDLINE` | `$E006` | editor | P5o "line": waits, then `LINBUF` holds the line and `X` its length |
| `EDINPUT` | `$E009` | editor | P5o "read a line": as `EDLINE`, from where the cursor is; carry set on *break* |
| `EDBREAK` | `$E00C` | editor | P5o "break?": carry set once for each STOP press |
| `EDIRQ` | `$E00F` | editor | the retrace's work: the key scan and the cursor blink |
| `MONITOR` | `$F000` | monitor | `USR(61440)`: `C*` |
| `MONBRK` | `$F003` | monitor | `B*`, from the IRQ handler with the 6502's interrupt frame on the stack |
| `SELFTEST` | `$F800` | kernel | the self-test; never returns |
| `RESET` | `$F803` | kernel | the reset vector's target |

A test (§9) reads the opcode at each entry point from the committed `.rom` files: it must be `$4C`.

## 4. The kernel — `roms/bw2001_kernel.asm`, `roms/bw2001_kernel.rom`

`.rom $F000, $1000`; `.include "bw2001.inc"`. UH4 holds `$F000`–`$F7FF` (the monitor, §4.3) and UH7 `$F800`–`$FFFF` (§4.1, §4.2, §4.4). Its last bytes are `.checksum $F000, $800` and `.checksum $F800, $800`, each just below its chip's end (UH7's below the vectors).

### 4.1 Reset (UH7)

1. `sei`, `cld`, `ldx #$FF`, `txs`.
2. PIA1:
   - `CRA` = 0, then `DDRA` = `$0F` (rows out; `PA4`–`PA7` in, `PA7` DIAG);
   - `CRA` = `$04`; `ORA` = 0;
   - `CRB` = 0, then `DDRB` = 0 (columns in), then `CRB` = `$04` (port B, `CB1` on its falling edge, interrupt off for now).
3. If `PA7` reads 0: `jmp SELFTEST`. No RAM has been touched.
4. Clear `$00`–`$7F` and pages 2 and 3, then size the RAM:
   - with `PTR` from `$0400`: write `$55`, compare, write `$AA`, compare;
   - stop at the first failure or at `$2000`; `MEMTOP` = that address.
5. `jsr EDINIT`, `CRB` = `$05` (the `CB1` interrupt on), `cli`.
6. Print the banner (a `$00`-terminated table through `EDPRINT`): `*** BW-2001 TINY BASIC ***`, a blank row, `MEMTOP − PROGRAM − 2` by `NUMOUT`, ` BYTES FREE`, a blank row.
7. `jmp COLD`.

The NMI vector points at an `rti`. The PET's NMI has no source on 320137's bus (P5q's question).

### 4.2 The IRQ handler (UH7)

- `pha`, `txa`, `pha`, `tya`, `pha`; `tsx`; read the stacked P (`$0104,x`).
- If B is set: `jmp MONBRK`, with the handler's three bytes still on the stack (the monitor knows the frame's shape).
- Else, if `CRB` bit 7 is set: `jsr EDIRQ` (its first `ORB` read clears the flag).
- Pull the three bytes back, `rti`.

### 4.3 The monitor (UH4)

- **Entries**:
  - `MONITOR` saves `A`, `X`, `Y` and `P` (`php` first) into `SAVED`. The return address it pulls, plus 1, is the saved PC, and `S` after the pull is the saved SP. Then it prints `C*` and goes to the loop.
  - `MONBRK` pulls the handler's `Y`, `X` and `A`, then the 6502's P and PC (PC is the `BRK` plus 2), and saves them all with SP. Then it prints `B*`.
  - Both then `cli`: the 6502 set I on the `BRK`, and the editor's keys come only from the retrace interrupt.
  - **The stack**: the monitor runs on the stack below the saved SP. `S` stays where the entry left it, so the caller's frames above it (BASIC's main loop, statements and expressions, or a routine's return into BASIC) survive for `.G`. Only `.X` and `.T` reset `S`.
- **The loop**: print the registers (header row, `.;` row), then repeat:
  - print `$0D`, `.`; `jsr EDLINE`;
  - skip spaces and one `.`, then dispatch on the next character: `M`, `:`, `R`, `;`, `G`, `X`, `T`; anything else, or a parse failure, prints `$0D`, `?`.
- **Parsing**: `HEX4` and `HEX2` read exactly 4 or 2 hex digits after any spaces; carry set on a failure.
- **Printing**: `HEXOUT` prints `A` as 2 hex digits through `EDPRINT`.
- **Commands**:
  - `.M`: rows of `.: aaaa` and 8 ` bb` from `aaaa & $FFF8` while `aaaa ≤ bbbb`, stopping also when the address wraps past `$FFFF`. Between rows it calls `jsr EDBREAK`, and stops on carry.
  - `.:`: up to 8 `HEX2` after the address, each stored at once. It stops at the first non-hex text, so a row edited in place with its trailing text still works.
  - `.R`: the header and the `.;` row. `.;`: six fields into `SAVED`.
  - `.G`: `aaaa` (optional) into the saved PC. Then `S` = the saved SP; push PC high, PC low and SR; load `Y`, `X`, `A`; `rti`.
  - `.X`: `jmp WARM`.
  - `.T`: `jmp SELFTEST`.

### 4.4 The self-test (UH7)

`sei` first: no interrupts and no `jsr` until the RAM has passed. It writes its rows at fixed screen addresses (`SCREEN + 40 * row`) with `abs,X` loops.

- **Page 0 first, with no RAM**:
  - pass 1: `X ^ $A5` stored at `$0000,x` for every `X`, then compared;
  - pass 2: its complement.
  - A failure writes `ZERO PAGE BAD 00xx bb` on row 2 and stops. Rows 3 and below stay blank.
  - Then `$00`–`$01` serve as the pointer, `PTR0`, for the rest. Nothing else in RAM is used until the RAM passes.
- **Video RAM**, then **RAM**, each in two passes:
  - write the whole range with `(PTR0),y` and the pattern `Y ^ page ^ $A5` (the complement in pass 2). For the RAM, each pass writes both `$0002`–`$00FF` (by `abs,X`) and `$0100`–`$1FFF`;
  - then compare all of it, so an alias between pages, page 0 included, fails the compare.
  - After the video RAM passes, the screen is cleared and the title and earlier rows are written again.
  - An address fault that hits `$00`–`$01` turns the pointer: the test then writes elsewhere, and fails somewhere. A Puzzle with such a fault expects only "a RAM failure", not a particular address.
- **After the RAM passes**: `S` = `$FF`, and `jsr` is allowed.
  - ROM sums from a table of `(base page, row)`: `$C0`, `$C8`, `$E0`, `$F0`, `$F8`. Each chip's 2048 bytes are summed with `(PTR0),y`, and each row gets `OK` or `BAD` and the sum.
  - Then the key loop forever: scan rows 0 to 9 (as §5.3, but its own copy, since the editor ROM may be dead), find the first held key, and rewrite the `KEY` row from UH7's own legend table (80 entries of up to 7 characters).
- **Rows**: 0 title, 2 `ZERO PAGE`, 3 `VIDEO RAM`, 4 `RAM`, 5–9 the ROMs, 11 `KEY` (the spec's sketch).

## 5. The editor — `roms/bw2001_edit.asm`, `roms/bw2001_edit.rom`

`.rom $E000, $800`; `.include "bw2001.inc"`. The entry `JMP`s are at `$E000`–`$E011`, the decode table `KEYTAB` at `$E700` (80 bytes, row 0 `PB0` first), and `.checksum $E000, $800` at `$E7FF`.

### 5.1 State (zero page `$40`–`$5F`)

| Name | What |
|---|---|
| `WAITING`, `LINES` | the harness signal (spec) |
| `CROW`, `CCOL` | the cursor's row and column |
| `ROWPTR` (2) | `SCREEN + 40 * CROW` |
| `UNDER` | the screen code under the cursor while it is shown |
| `BLINK` | jiffies to the next blink; `SHOWN` whether the reversed cell is on screen |
| `LASTKEY` | the key code seen in the last scan, `$FF` for none |
| `KHEAD`, `KTAIL` | the `KEYBUF` ring |
| `BRKFLAG` | set by a STOP press, cleared by `EDBREAK` |
| `SROW`, `SCOL` | where `EDINPUT` began |
| `MODE` | 0 at a line read, 1 at an input read |

### 5.2 Print

`EDPRINT`:

- `$0D`: `CCOL` = 0, then `NEXTROW`.
- `$0C`: fill the screen with `$20`, clear `LINKS`, cursor home.
- A character from `$20` to `$5F`: map it to a screen code (`$40`–`$5F` minus `$40`), store it at `(ROWPTR),CCOL`, and `CCOL`++. At 40:
  - `CCOL` = 0;
  - if the row is a logical line's first row, `NEXTROW` and set the new row's link;
  - otherwise `NEXTROW` alone, which starts a new logical line.
- `NEXTROW`: `CROW`++. At 25, `SCROLL` and `CROW` = 24. The new row's link is cleared, unless the caller sets it.
- `SCROLL`: copy rows 1–24 to rows 0–23, blank row 24, shift `LINKS` down one place and set `LINKS[0]` = 0. During an input read, `SROW` moves up one; at row 0 it is forgotten, and RETURN then takes the whole logical line. `SCROLL` runs with interrupts on (about 15 ms of copying), so no retrace is missed.
- **Interrupts in the editor**: nothing in the editor runs with `sei` for longer than a few instructions. Hiding and showing the cursor are each one short `sei`…`cli`. The blink writes only `UNDER | $80` (shown) or `UNDER` (hidden), and only while `SHOWN` says the cursor is out, so the IRQ never undoes a write of the main loop.

### 5.3 Keys — `EDIRQ`

- **Scan**: for rows 0 to 9, `ORA` = the row and read `ORB`, keeping a held key's index (row × 8 + bit) and whether a SHIFT is held. The first non-SHIFT key found is the scan's key.
- **A new key**: if it differs from `LASTKEY` and is not `$FF`, it is new. Its code is `KEYTAB` by its index:
  - the printable characters;
  - `$0D` RETURN;
  - `$03` STOP;
  - `$11` CRSR ↓, `$1D` CRSR →, `$13` HOME, `$14` DEL;
  - `$12` RVS, `$00` for SHIFT and the unused places.
  - With SHIFT, the editor's codes become `$91` ↑, `$9D` ←, `$93` CLR and `$94` INST.
- **Where it goes**: STOP sets `BRKFLAG`, and anything else non-zero enters `KEYBUF` (dropped when full). `LASTKEY` = the scan's key.
- **Blink**: if `WAITING`, `BLINK`−− and at 0 (every 20 jiffies) flip the cursor cell's bit 7.

### 5.4 Reading a line — `EDLINE`, `EDINPUT`

1. Empty `KEYBUF`, clear `BRKFLAG` (for `EDLINE`; `EDINPUT` keeps it, so a press already made breaks). Set `SROW`/`SCOL` from the cursor, then `WAITING` = 1 and show the cursor.
2. Loop: if `MODE` is input and `BRKFLAG` is set, return *break* (carry set, `WAITING` = 0, cursor hidden). With `MODE` at line, a set `BRKFLAG` is cleared and ignored. Take a key from `KEYBUF` (wait if none), hide the cursor, act, show it again:
   - **printable**: refused at position 64 of the logical line or later. Otherwise stored as `EDPRINT` would, but a wrap from a logical line's second row to a new row is refused too, so typing never makes a third row.
   - **CRSR, HOME, CLR**: as the spec says.
   - **DEL, INST**: shift the cells of the logical line from the cursor's position.
   - **RETURN**: see step 3.
3. On RETURN:
   - find the logical line: go up while `LINKS[row]`, and down one row if the next row's link is set;
   - for an input read on the same logical line as `SROW`, start at `SCOL` (counted along the logical line);
   - copy to `LINBUF` with bit 7 dropped and the screen codes mapped back; cut at 64; then trim trailing spaces; end with `$00`;
   - put the cursor at column 0 of the row after the line (`NEXTROW`);
   - `WAITING` = 0, `LINES`++; return `X` = length, carry clear.

### 5.5 Size

About 1.2 KB of code and tables, plus the 80-byte `KEYTAB`.

## 6. BASIC — `roms/bw2001_basic.asm`, `roms/bw2001_basic.rom`

`.rom $C000, $2000` (UH1, UH5, and UH2 and UH6 if needed); `.include "bw2001.inc"`. There is one `.checksum $C000+n*$800, $800` for each chip holding code, at its last byte; the self-test's table (§4.4) lists the same chips. Everything follows P5o; only how is said here.

### 6.1 State (zero page `$00`–`$3F`)

| Name | What |
|---|---|
| `TXT` (2) | the byte being read in the current line |
| `CURLINE` (2) | the current line's record, 0 for the direct line |
| `TOP` (2) | the end of the program: the first byte of its `00 00` |
| `ACC` (2), `ARG` (2), `TMP` (4) | the expression's value and scratch |
| `DEPTH` | P5o §Expressions depth |
| `CSP` | the control stack's entry count |
| `COL` | BASIC's column (P5o §The editor contract) |
| `SEED` (2) | `R` |
| `RUNNING` | 0 at a direct line, 1 in a program |
| `ERRSP` | `S` at the main loop, for an error's stack reset |

### 6.2 The main loop

`COLD`: `NEW`, print `READY`, then forever:

1. `jsr EDLINE`; `COL` = 0.
2. Drop leading spaces. If the line starts with a digit:
   - read the number (above 9999 or 0 is `BAD VALUE`, error 7);
   - drop the spaces after it;
   - tokenise the rest into a scratch record in `DIRBUF`;
   - store it (§6.4).
3. Otherwise tokenise into `DIRBUF`, run it as the direct line (`CURLINE` = 0), and print `READY` once at the end, on a fresh line.

`WARM` resets `S` and `ERRSP`, sets `COL` = 0 (the editor left the cursor at column 0), `RUNNING` = 0, does `cli`, and enters the same loop at `READY`.

### 6.3 Tokenising

The keyword table is P5o's in token order, each keyword's last letter with bit 7 set.

- At each position outside quotes and before a `REM` token, try each keyword in order. The first that matches is replaced by `$80 + index`.
- A `"` toggles quote mode. After `REM` the rest is copied as it is.
- `LIST` prints a token by walking the table to its index.

### 6.4 Storing a line

1. Find the record with the number, or the first one above it.
2. The new size is `TOP + 2 − old record + new record`. If that would pass `MEMTOP`, the error is `OUT OF MEMORY` and nothing has moved.
3. Otherwise close the old record's gap (a block move down) and open the new one's (a block move up), then copy it in. `TOP` moves.

A number with no text only deletes the line.

### 6.5 Running

- **Statements** are dispatched by token through a table of addresses (`$80`–`$8F`, with `LET` also taken when the statement starts with a letter).
- **After a statement**:
  - `:` goes on to the next statement;
  - `$00` ends the line. In a program, go to the next record, and end at line number 0. In a direct line, the line is finished;
  - anything else is `SYNTAX ERROR`.
- **Break**: `jsr EDBREAK` before each statement, between `LIST` lines and inside `INPUT`'s read (`EDINPUT`'s carry).
- **Jumps**: `GOTO`, `GOSUB` and `THEN` with a number search from `PROGRAM`. A direct-line `GOTO` sets `RUNNING`, empties `CSTACK` and starts there.
- **`CSTACK` entries**:
  - byte 0 is `$80` for a `GOSUB`, or the variable's index for a `FOR`;
  - then the limit and step (2 each, unused for a `GOSUB`), then `CURLINE` (2) and `TXT` (2) of the place to go back to.
  - Each rule of P5o's `FOR`, `NEXT` and `RETURN` is a walk down from `CSP`.
- **Errors**: `ERROR` takes the code in `A`.
  - Print `$0D` if `COL` is not 0, then the message from a table indexed by the code, then ` ERROR`, then ` IN ` and the number if `CURLINE` is not 0, then `$0D` and `READY`.
  - `S` = `ERRSP`, and back to the main loop.
  - `BREAK` is the same without a code or ` ERROR`.

### 6.6 Expressions

- **Recursive descent**: one routine a level (P5o's eight), each leaving its value in `ACC`. A binary operator pushes `ACC` on the 6502 stack, evaluates its right side, then combines it with the pulled left side.
- **Depth**: `DEPTH`++ on each `(` (a function's too) and on each pending unary operator, and `DEPTH`−− when it closes. At 7 the error is `TOO DEEP`.
- **Stack use**: each open parenthesis goes back down through all eight levels, about 16 bytes of `jsr` plus up to 5 pushed left values, about 26 bytes in all. At P5o's depth of 6 that is about 160–200 bytes. Add about 10 for the main loop and the statement, 6 for an IRQ frame, and the monitor's frames when `USR(61440)` is called deep in an expression (about 20). That leaves about 30 bytes spare in the page. `GOSUB` and `FOR` live in `CSTACK`, not on the 6502 stack. The notes record the measured worst case (session `15_depth`).
- **Arithmetic**: 16-bit shift-and-add multiply; division on magnitudes with the sign fixed afterwards (truncating toward zero); comparisons signed (P5o's results, -1 and 0).
- **`RND`**: P5o's xorshift on `SEED`, then an unsigned 16-bit modulo by N.
- **`USR`**: `jsr` through a pushed address, `ACC` from `A` and `Y`.
- **`PEEK`** and **`POKE`**: `(TMP),y`.

### 6.7 Numbers

- **In**: a run of digits into 16 bits; past 65535 is `BAD VALUE`. `INPUT`'s answers use the same routine, with P5o's sign and spaces around it. For an answer, more than 5 digits or a value past 65535 gives `REDO`, not an error (session `25_input`: `70000`).
- **Out** (`NUMOUT`): sign, then digits by repeated subtraction of 10000, 1000, 100, 10, without leading zeros.

### 6.8 Size

About 3.5 KB, within UH1 and UH5. If it overflows, `.org $D000` goes on in UH2, and these change with it:

- a third `.checksum`;
- the self-test's ROM table and its layout (a `ROM D000` row, and the `KEY` row one lower);
- the spec's screen sketch and memory table;
- the netlist's UH2 (`offset=$1000`);
- the model's empty-socket check.

The notes say so.

## 7. The model — `tests/machines/bw2001_model.{hpp,cpp}`

As `bw1_model` (P5k), one bus cycle a microsecond around `Mos6502Core`.

```cpp
struct Bw2001Key { int row; int column; };  // PIA1 PA row 0-9, PB bit 0-7

class Bw2001Model {
public:
    struct Options {
        std::uint64_t seed = 1;
        std::uint32_t ramTop = 0x2000;  // RAM is $0000 to ramTop - 1, at most 8 KB
        bool diag = false;              // true: DIAG (PA7) held low from power-on
        // Each ROM window by its base ($C000, $E000, $F000); empty: the three committed
        // roms/bw2001_*.rom. A window not given is empty sockets.
        std::map<std::uint16_t, std::vector<std::uint8_t>> roms;
    };
    explicit Bw2001Model(Options options);  // RAM random from the seed; powered on and reset
    void run(std::uint64_t us);
    void reset();
    void setKey(Bw2001Key key, bool held);
    void setDiag(bool low);
    std::uint8_t peek(std::uint16_t address) const;
    void poke(std::uint16_t address, std::uint8_t value);       // RAM and video RAM
    void stick(std::uint16_t address, unsigned bit, bool one);  // RAM and video RAM
    void deadRam();                                             // every RAM read $FF
    void patchRom(std::uint16_t address, std::span<const std::uint8_t> bytes);  // as bw1_model
    std::uint8_t screen(std::size_t offset) const;              // video RAM, 0-999
    bool touchedUnmodelled() const;                             // PIA2, VIA, $D000-$DFFF read
    const Mos6502Core& core() const;
};
```

- **ROMs**: `Options::roms`, or by default read from `BW_ROM_DIR/bw2001_{basic,edit,kernel}.rom` (`ReadRomContents` at the `.rom` window's size: `$2000`, `$800`, `$1000`). So phase 2 tests the model with its own assembled image before any firmware exists. Reads of an address with no chip return the last bus value.
- **RAM** is `$0000` to `ramTop − 1`. Above it, reads return the last bus value and writes are lost, so the RAM sizing sees the top.
- **PIA1**:
  - `DDRA`/`ORA`, `CRA`, `DDRB`/`ORB` and `CRB` as the R6520's (P5n §5.2), modelled only as far as the firmware uses them;
  - `PB` reads the matrix: column `c` is low when a held key on the row on `PA0`–`PA3` is in column `c` (rows 10–15 select nothing);
  - `PA7` reads DIAG;
  - `CB1`: a fall every 16 667 cycles sets `CRB` bit 7, and with `CRB` bit 0 set drives IRQ;
  - reading `ORB` clears the flag.
- PIA2, the VIA and the empty sockets are flagged when touched. The tests check that nothing is.

## 8. The harness — `tests/machines/bw2001_harness.{hpp,cpp}`

One runner for both targets:

```cpp
class Bw2001Target {
public:
    virtual ~Bw2001Target() = default;
    virtual void run(std::uint64_t us) = 0;
    virtual void setKey(Bw2001Key key, bool held) = 0;
    virtual std::uint8_t ram(std::uint16_t address) const = 0;  // WAITING, LINES
    virtual std::uint8_t screen(std::size_t offset) const = 0;
};
```

- **`Bw2001KeyFor(char)`**: the spec's matrix for every typable character, plus `kReturn` (6, 5), `kStop` (9, 4), `kShift` (8, 0), `kDel`, `kCursorDown`, `kCursorRight` and `kHome`. A test checks the map against the editor's `KEYTAB` in the committed `.rom`.
- **Pressing**: `press(key, shift)` holds the key 30 ms and releases it for 30 ms. That is more than one retrace (16.7 ms) each, with margin for a scan that runs late.
  - `type(text)`: each character in turn.
  - `enter(text)`: `type`; read `LINES` and keep it as `last`, before RETURN is pressed; then RETURN.
- **Waiting**: `waitForLine()` runs in 1 ms steps until `WAITING` = 1 and, once anything has been entered, `LINES` differs from `last`. The first wait (power-on) needs `WAITING` = 1 only. A run past 5 s simulated fails "BASIC never asked for a line".
- **Reading**: `rows()` reads the 25 rows, drops bit 7, maps to ASCII and trims trailing spaces.
- **Sessions**: `RunSession(target, path)` follows P5o's format and runner (P5o §Conformance set, P5o design §`Session`) with the full view:
  1. power-on: `waitForLine`, then `enter("CLS")`;
  2. for each typed line: `waitForLine`, then `enter`;
  3. a `~break` line: after its `enter`, run 100 ms, press STOP;
  4. at the end, `waitForLine`;
  5. compare `rows()` with the expected rows from the last `~clear` on, blank below. `{RAM}` is 1008.
  - It returns the first difference as text (session, row, expected, got), or none. The P5o lint stays in Python. The C++ parser takes the rows and the four directives only.

## 9. Tests

Every case's name starts with `bw2001`, so the fences' `-R bw2001` reaches it (ctest matches names, not tags). Each phase that adds a source file adds it to `bw_tests` in `tests/CMakeLists.txt`, which lists every source by name. The netlist test includes the harness as `"../machines/bw2001_harness.hpp"`, as `bw1_puzzles_test.cpp` includes `bw1_rig.hpp`.

`tests/machines/bw2001_model_test.cpp` (the model and the tools on it, `[bw2001]`):

- **The model**: a small program assembled in the test (`Assemble6502`, a `.rom $F800, $800` loaded over the kernel's UH7 window) reads a held key's row through PIA1 and counts retraces in the `CB1` interrupt.
- **The ROMs**:
  - each 2 KB chip of the three `.rom` files sums to 0;
  - each entry point of §3 holds `$4C`;
  - `Bw2001KeyFor` agrees with `KEYTAB`.

`tests/machines/bw2001_firmware_test.cpp` (on the model, `[bw2001]`, `[slow]` where it runs more than about 2 s simulated):

- **Power-on**: the banner rows for seeds 1–3; `ramTop` `$1800` gives `5118 BYTES FREE`; a stuck bit at `$1234` gives `3634 BYTES FREE`. `touchedUnmodelled()` stays false.
- **`bw2001 conformance`**: one case with one `DYNAMIC_SECTION` a session file over the `tests/data/basic/*.txt` glob, so a new session joins by itself.
- **BASIC's own**:
  - `POKE`s of `A9 34 A0 12 60` at 1008, then `PRINT USR(1008)` gives `4660`;
  - `ramTop` `$0500` (254 bytes free):
    - lines of 30 characters are entered until `OUT OF MEMORY` and `READY`, with `LIST` unchanged by the refused line;
    - then an existing line number is entered again with a longer text: `OUT OF MEMORY`, and `LIST` still shows the old text (P5o: "the old line stays");
  - STOP at `READY` is discarded (the next line runs).
- **The editor**: the spec's list, each a short case reading `rows()` and the program's `LIST`.
- **The monitor** and **the self-test**: the spec's lists.
  - The self-test reaches its `KEY` row at about 0.8 s simulated: the RAM's two passes over 32 pages at about 37 cycles a byte (P5n's bench measure) take about 0.6 s.
  - Dead RAM gives `ZERO PAGE BAD 0000 xx` at once.

`tests/netlist/bw2001_netlist_test.cpp` (`[netlist][slow]`):

- **`Bw2001Netlist`** is a `Bw2001Target` over `tests/data/bw2001_firmware.bwn`:
  - `ram()` reads UI/UJ words as the bench's `screen()` does;
  - keys are the `KEY` Parts by the reference map of §10;
  - `run` is `runUntil`.
- **Every build**:
  - power-on to `READY` (about 0.5 s simulated, about 30 s in debug);
  - session `27_cls` with its power-on and the harness's `CLS` (about 2.3 s simulated at 60 ms a key, about 140 s in debug);
  - DIAG held at power-on, to the `KEY` row with `A` held (about 0.8 s, about 50 s in debug).
  - Debug times scale from P5n's bench: 82 s for 3 × about 0.5 s.
- **`bw2001 netlist conformance (release)`**: `#ifndef NDEBUG` → `SKIP("release only: hours in debug")`. Every session: about 2,930 keys at 60 ms, about 180 s simulated, about 150 s in release. It prints its wall time and the simulated time, which gives the ratio while typing.
- **Idle ratio**: `bw run tests/data/bw2001_firmware.bwn --time 2s` (idle at `READY`), median of 3. Both ratios go in the notes. No budget is set: P5q measures the Machine.

## 10. The minimal netlist — `tests/data/bw2001_firmware.bwn`

P5n's bench (`tests/data/bw2001_bench.bwn`) copied, with:

- **The sockets**:
  - UH1 and UH5: `contents=../../roms/bw2001_basic.rom`, offsets 0 and `$800`;
  - UH3: `bw2001_edit.rom`;
  - UH4 and UH7: `bw2001_kernel.rom`, offsets 0 and `$800`;
  - UH2 and UH6: `empty.rom`.
- **The keyboard**: SW1 and SW2 removed; `SW1`–`SW73` `KEY` in matrix order (row 0 `PB0` first, the seven unused places skipped). Each joins `ROWn` (UG9 `Y0_N`–`Y9_N`) to `KINc`.
- **DIAG**: `SW74 KEY` from `DIAG` (UG8 `PA7`, the PIA's own pull-up) to `GND`.
- **Unchanged**: the bench's result nets (PIA2, the VIA's) stay as they are. Our firmware leaves them alone.

The header comment says what it is: P5p's minimal PET, not the Machine (P5q).

## 11. Docs and recipes

- **ADD_MACHINE §2**:
  - firmware over several ROM chips: one `.asm` a function, a shared `.inc` with the entry table, `.checksum BASE, SIZE` per chip, `offset=` in the netlist;
  - a harness with one runner over a model and a netlist target;
  - netlist cases that take hours in debug go behind `NDEBUG`.
- **`docs/GLOSSARY.md`**: "ROM contents" (`offset=`).
- **`datasheets/2316B.md`**, **`datasheets/2716.md`**: `offset=`.
- **`docs/ARCHITECTURE.md`**: the P5p row, at the end.
- **No release screenshot**: there is no picture before P5q. The release check runs the netlist tests and `bw run` for the ratio instead.

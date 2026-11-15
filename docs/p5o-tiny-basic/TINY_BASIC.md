# P5o — Tiny BASIC spec

Roadmap step P5o ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)). It defines the one Tiny BASIC that both of our computers' ROMs carry: the BW-2001's (6502, P5p) and the BW-80's (Z80, P5w). The spec does not depend on the CPU. It covers the language, the program's bytes in RAM, the errors, the limits, and the contract between BASIC and each Machine's own editor and screen. It also brings a conformance set: BASIC sessions with the screen output they must give, the same on both ROMs.

The spec is checked before any ROM exists by a reference interpreter in Python, written from this document. It runs the conformance set on 32- and 40-column screens. P5p and P5w then run the same set on their ROMs, headless.

## Decided

With the roadmap row (2026-11-15): one dialect on both CPUs; integer variables `A`–`Z`; the program tokenised in RAM; an editor contract so each Machine writes only its own editor; a conformance set run headless on both ROMs, typed in as key presses, with the screen read back from video RAM.

Asked at the start of the step (2026-11-15):

- **Loops and small extras**: on top of the row's statements, `REM`, `FOR`/`TO`/`STEP`/`NEXT`, `CLS`, `RND`, `ABS`, `<>` `<=` `>=`, and `AND`/`OR`/`NOT` (bitwise). No strings beyond literals in `PRINT` and `INPUT`. Offered instead: the row's list and `REM` only, or string variables as well.
- **Numbers wrap**: arithmetic is 16-bit two's complement and wraps without an error. Literals go from 0 to 65535 and values print signed, so `POKE 32768,1` reaches the PET's video RAM. Offered instead: an error on overflow.
- **Errors in words**: `SYNTAX ERROR IN 30`. Each error also has a code number, used by the ROMs' tables and the manuals. Offered instead: ZX80-style `2/30`, or `?2 SYNTAX IN 30`.
- **A Python reference interpreter** checks the conformance set before P5p. This is test code, beyond the row's "no new code". Offered instead: expected outputs written by hand and first checked at P5p.

Chosen in this spec (not asked): the 64-character set below, line numbers 1–9999, more than one statement on a line (`:`), a deterministic `RND`, and a nesting depth of 6.

## Characters

BASIC sees and prints only the 64 ASCII codes `$20`–`$5F`: space, `!"#$%&'()*+,-./`, `0`–`9`, `:;<=>?@`, `A`–`Z`, `[\]^_`. There are no lower-case letters. Each Machine's editor maps the keys it reads into these codes (a PC's lower case to upper case), and its screen maps them to its own screen codes. On the PET these are screen codes `$00`–`$3F`. On the BW-80 they become the ZX80-style character codes of our own 64-glyph character ROM (P5w). Two control codes go to the screen: `$0D` (new line) and `$0C` (clear). A BW-80 player cannot type every one of the 64 characters: the ZX80's keys have no `! # % & ' @ [ \ ] ^ _`. They can still be printed.

## Lines and the editor

The editor hands BASIC one line at a time, 0 to 64 characters, when the player presses RETURN. BASIC drops leading spaces.

- A line that starts with a digit is a **program line**. Its number is the run of digits, and spaces after it are dropped.
  - A number with no text deletes that line. Deleting a line that does not exist is not an error.
  - A number with text stores the line, replacing any line with the same number. If there is no room, the old line stays and the error is `OUT OF MEMORY`.
  - Line numbers go from 1 to 9999. Anything else is `BAD VALUE`, and no line is stored.
  - The text is not checked when it is stored: a wrong statement fails when it runs.
  - Storing a line prints nothing, and BASIC waits for the next line without printing `READY`. A refused line prints its error, then `READY`.
  - Editing the program keeps the variables.
- Any other line is a **direct line**, run at once. When it ends, BASIC prints `READY` once, even if the line ran the program (`RUN`, `GOTO`). An empty line prints only `READY`.
- At power-on BASIC does `NEW`. The Machine prints its own banner, then BASIC prints `READY`.

A line holds statements separated by `:`. An empty statement does nothing, so `::` and a trailing `:` are allowed. Spaces between items are ignored outside quotes. A number is a run of digits with no spaces in it. A keyword has no spaces in it either: `PR INT` is the letters `P` and `R`, then `INT`.

A statement runs as it is read. Anything it printed before an error stays on the screen: `PRINT 1 2` prints `1`, then `SYNTAX ERROR`.

## Values and variables

Values are 16-bit integers, printed signed from -32768 to 32767. `+`, `-` and `*` keep the low 16 bits of the result: `32767+1` is -32768 and `200*200` is -25536. A literal is 0 to 65535 and keeps its low 16 bits as a value, so `65535` is -1 and `40000` prints as -25536. A literal above 65535 is `BAD VALUE`. `/` divides signed and truncates toward zero (`-7/2` is -3; `-32768/-1` is -32768). Division by 0 is `DIVIDE BY ZERO`.

There are 26 variables, `A` to `Z`, one letter each: `AB` is a syntax error. `NEW` and `RUN` set them to 0.

## Expressions

From the tightest binding to the loosest:

| Level | Operators | Notes |
|---|---|---|
| 1 | number, variable, `( )`, `PEEK( )`, `RND( )`, `ABS( )`, `USR( )` | |
| 2 | unary `-`, unary `+` | `-2*3` is (-2)*3 |
| 3 | `*` `/` | left to right |
| 4 | `+` `-` | left to right |
| 5 | `=` `<>` `<` `>` `<=` `>=` | signed; true is -1, false 0; left to right: `A=B=C` is `(A=B)=C` |
| 6 | `NOT` | bitwise complement, `NOT 0` is -1 |
| 7 | `AND` | bitwise |
| 8 | `OR` | bitwise |

Since true is -1, `AND`, `OR` and `NOT` work on comparisons as logic: `IF A>0 AND A<10 THEN …`. `NOT` binds more loosely than the comparisons, so it cannot appear inside one without parentheses. `1+NOT 0` and `A=NOT B` are syntax errors; `1+(NOT 0)` is 0. The two-character operators are written with no space between their characters.

**Depth.** Each open parenthesis (a function's included) and each unary `-`, `+` or `NOT` that is still waiting for its operand adds 1 to the depth. A depth of 7 is `TOO DEEP`: `((((((1))))))` is fine and `(((((((1)))))))` is not; `------1` is fine. This bounds the CPU stack that a recursive evaluator uses on both machines.

Functions:

- `PEEK(A)`: the byte at address `A`, its low 16 bits taken as unsigned (`PEEK(-1)` reads `$FFFF`), 0 to 255.
- `RND(N)`: a pseudo-random number from 0 to N-1, for N from 1 to 32767. N of 0 or less is `BAD VALUE`.
  - The generator is a 16-bit xorshift on a seed `R`, which is not one of the variables.
  - For each `RND`: `R ^= R << 7`, `R ^= R >> 9`, `R ^= R << 8` (16 bits, logical shifts). The result is `R` mod N, with `R` unsigned.
  - Power-on, `NEW` and `RUN` set `R` to 1, so every run gives the same numbers. From there, `RND(1000)` gives 153, 609, 801, 787.
- `ABS(N)`: `ABS(-32768)` is -32768.
- `USR(A)`: calls machine code at `A` (unsigned) and returns its 16-bit result. The calling convention depends on the CPU: on the 6502, `JSR`, the result in `A` (low byte) and `Y` (high byte); on the Z80, `CALL`, the result in `HL`. Which registers and RAM the routine must keep is each ROM's to say (P5p, P5w).

## Statements

| Statement | Does |
|---|---|
| `PRINT` items | Items are expressions and `"strings"`, with separators between and around them. `;` prints nothing; `,` prints spaces up to the next column that is a multiple of 8, at least one. Each separator does its own work, so `PRINT ,5` and `PRINT 1;;2` are allowed. A new line comes at the end unless the statement ends with a separator. `PRINT` alone prints a new line. Numbers print with no spaces around them: `-12`. A string runs to the next `"`, or to the end of the line; it cannot contain `"`. Two items with no separator between them are a syntax error |
| `INPUT` [`"prompt";`] var [`,` var …] | For each variable it prints `? ` (the first after the prompt: `INPUT "AGE";A` shows `AGE? `), and the editor reads a line. The answer must be an optional `+` or `-` and 1 to 5 digits worth 0 to 65535, with spaces around it allowed. It is kept as for literals, negated by `-` (`-0` is 0, `-65535` is 1). Anything else prints `REDO` on its own row and asks again with the same prompt. Refused in a direct line (`ILLEGAL DIRECT`) |
| [`LET`] var `=` expr | assigns; `LET` may be left out |
| `IF` expr `THEN` statements | if expr is not 0, runs what follows `THEN`. If it is 0, the rest of the line is skipped, every statement after `:` included. When `THEN` is followed by a digit, that literal number is a `GOTO`: `IF A THEN 100` |
| `GOTO` expr | goes to that line: `NO SUCH LINE` if it does not exist, `BAD VALUE` if expr is outside 1–9999. In a direct line it empties the stack and runs the program from that line, keeping the variables. So does `THEN` with a number. Whatever follows in the direct line never runs |
| `GOSUB` expr | as `GOTO`, but pushes a return point; `ILLEGAL DIRECT` in a direct line |
| `RETURN` | pops `FOR` entries until a `GOSUB` entry, then goes back to the statement after that `GOSUB`; `NO GOSUB` if there is none |
| `FOR` var `=` a `TO` b [`STEP` c] | sets var to a. If a `FOR` of the same variable is on the stack above the innermost `GOSUB` entry, that entry is dropped with every entry above it. Then it pushes var, b, c (1 if left out) and the place after the `FOR`. The body always runs at least once |
| `NEXT` [var] | finds the innermost `FOR` (of var, if given) above the innermost `GOSUB` entry and drops the entries above it; `NO FOR` if there is none. It adds the step to var. If that addition overflows 16 bits signed, or var is now past b (above it for a step ≥ 0, below it for a step < 0), it pops the entry and goes on after the `NEXT`. Otherwise it goes back after the `FOR`. So `FOR I=1 TO 32767` ends, with `I` at -32768 |
| `END` | ends the program |
| `REM` text | the rest of the line is ignored, `:` included |
| `POKE` a`,` v | writes the low byte of v at address a (unsigned) |
| `CLS` | clears the screen; the cursor goes to the top left |
| `LIST` [n [`-` m]] | lists the program, or line n only, or lines n to m, both included. n and m are literal numbers from 1 to 9999; n above m lists nothing. Each line prints as its number, one space and the text as it was typed. In a program it lists and goes on |
| `RUN` | sets the variables to 0 and `R` to 1, empties the stack, runs from the first line. In a program it starts it again |
| `NEW` | deletes the program, sets the variables to 0 and `R` to 1; it ends the line or the program it is in |

A statement must end at `:` or at the end of the line; anything else after it is a syntax error. A program ends at `END`, at `NEW`, or after its last line, and then BASIC prints `READY`.

**The control stack** holds 8 entries, each a `GOSUB` or a `FOR`. A ninth is `TOO DEEP`. Every direct line starts with the stack empty. So `FOR I=1 TO 3:PRINT I:NEXT` works as a direct line.

## Errors

An error stops the program or the direct line. If BASIC's column is not 0, it first prints a new line. Then it prints the message and the word `ERROR`. When the program was running, it adds ` IN ` and the line number: `SYNTAX ERROR IN 30`, or `SYNTAX ERROR` in a direct line. Then `READY`. The variables are kept.

| Code | Message | When |
|---|---|---|
| 1 | `SYNTAX` | the line cannot be read as this grammar: an unknown word, two letters, a missing `)`, `THEN`, `TO` or `=` |
| 2 | `NO SUCH LINE` | `GOTO`, `GOSUB` or `THEN` to a line that does not exist |
| 3 | `NO GOSUB` | `RETURN` without a `GOSUB` |
| 4 | `NO FOR` | `NEXT` without a matching `FOR` |
| 5 | `TOO DEEP` | a ninth stack entry, an expression nested 7 deep |
| 6 | `DIVIDE BY ZERO` | `/` by 0 |
| 7 | `BAD VALUE` | a literal above 65535, a line number outside 1–9999 (entered, `GOTO`, `GOSUB`, `LIST`), `RND` of 0 or less |
| 8 | `OUT OF MEMORY` | no room to store a line |
| 9 | `ILLEGAL DIRECT` | `INPUT` or `GOSUB` in a direct line |

The longest line, `DIVIDE BY ZERO ERROR IN 9999`, is 28 characters, so every message fits on one row of the BW-80's 32 columns.

**Break** has no code. Each Machine has a break key (P5p, P5q, P5w and P5x choose which). BASIC checks it between statements, between listed lines and while `INPUT` waits. It then stops on a fresh line with `BREAK IN 30`, or `BREAK` in a direct line, then `READY`. A press while BASIC waits for a line at `READY` is discarded.

## The program in RAM

Each ROM puts the program and the variables where its RAM is (P5p, P5w). Their shape is the same on both CPUs, little-endian as both are:

- **Variables**: 26 words, `A` first, each value's low byte first.
- **Program**: lines in ascending order from the program base. Each line record holds:
  - the line number, 2 bytes, low byte first;
  - the length of the whole record in bytes, 1 byte;
  - the tokenised text;
  - `$00`.

  After the last line come two `$00` bytes (line number 0). The free memory runs from there to the top of the program area, which each ROM sets.
- **Tokens**: BASIC reads the line left to right. Outside quotes, and before any `REM`, it replaces each keyword with one byte from `$80`.
  - It does this wherever the letters stand, so `FORI=1TO9` gives `FOR I=1 TO 9`, and so would `TORU`: `TO`, `R`, `U`.
  - No keyword is the beginning of another one.
  - Everything else stays as typed, spaces included, and numbers stay as digits.
  - Text inside quotes and after `REM` is never tokenised.
  - `LIST` spells the tokens back out, so a line lists as it was typed.

| Token | Keyword | Token | Keyword | Token | Keyword | Token | Keyword |
|---|---|---|---|---|---|---|---|
| `$80` | `PRINT` | `$87` | `FOR` | `$8E` | `RUN` | `$95` | `NOT` |
| `$81` | `INPUT` | `$88` | `NEXT` | `$8F` | `NEW` | `$96` | `PEEK` |
| `$82` | `LET` | `$89` | `END` | `$90` | `THEN` | `$97` | `RND` |
| `$83` | `IF` | `$8A` | `REM` | `$91` | `TO` | `$98` | `ABS` |
| `$84` | `GOTO` | `$8B` | `POKE` | `$92` | `STEP` | `$99` | `USR` |
| `$85` | `GOSUB` | `$8C` | `CLS` | `$93` | `AND` | | |
| `$86` | `RETURN` | `$8D` | `LIST` | `$94` | `OR` | | |

`10 PRINT A` is stored as `0A 00 07 80 20 41 00`: line 10, a 7-byte record, `PRINT`, space, `A`, end.

## Limits

| What | Limit |
|---|---|
| Line numbers | 1–9999 |
| An entered line | 64 characters (the editor stops there) |
| Control stack | 8 entries, `GOSUB` and `FOR` together |
| Expression depth | 6 |
| Program size | the Machine's free RAM; on the BW-80 a few hundred bytes (its display file shares the 1K) |

## The editor contract

The calls between BASIC and each Machine's own editor and screen code:

| Call | Direction | Contract |
|---|---|---|
| line | editor → BASIC | The player pressed RETURN on a line. It hands over 0–64 codes from `$20`–`$5F`, with trailing spaces removed. The editor has already moved the cursor to column 0 of the row after the line's last row: the PET's editor takes the logical line under the cursor, up to two rows. An editor with keyword keys (a ZX80-style one, if P5x chooses it) hands the keyword spelled in letters |
| print `c` | BASIC → screen | `c` in `$20`–`$5F` is shown at the cursor, which moves right. At the end of a row the cursor goes to the next row, and at the bottom the screen scrolls up one row. `$0D` moves to column 0 of the next row, scrolling at the bottom. `$0C` clears the screen and puts the cursor top left. Print never fails as far as BASIC can see: a screen short of RAM (a collapsed display file) is the Machine's to handle (P5w) |
| read a line | BASIC → editor | For `INPUT`, the player types after what BASIC printed. The editor returns only what was typed after that point, as for line, with the cursor on the next row. Or it returns *break* |
| break? | BASIC → Machine | true once for each press of the break key |
| width | Machine → BASIC | the screen's columns: 40 on the BW-2001, 32 on the BW-80 |

BASIC keeps its own column count for `,` and for "a fresh line" (errors, `READY`). It is the number of characters printed since the last new line or clear, modulo the width, and it is set to 0 after each line the editor returns. The screen owns the scrolling. Keys pressed while a program runs are dropped, except break.

## Conformance set

Sessions in `tests/data/basic/`, one file each. Each one starts from power-on, after the harness has typed `CLS`. Lines in a file:

- `~ …`: a comment.
- `~break`: the break key is pressed after the last typed line was entered, while what it started runs, or while the next `INPUT` waits. The line running at that moment must print nothing while it loops (`10 GOTO 10`, `FOR I=0 TO 0 STEP 0:NEXT`), so when the press lands changes nothing.
- `~clear`: the screen was cleared here (`CLS`). The rows above it were typed, but they are not compared.
- `~typed`: the session needs keywords typed letter by letter (it lists them back as typed). A harness for an editor with keyword keys may skip it.
- Anything else, an empty line included, is one screen row as it must look at the end of the session.
  - A `|` splits what BASIC printed (before it) from what the player typed and entered (after it). `|10 PRINT "HI"` is a typed line, `? |5` an `INPUT` answer, `HI` a printed row.
  - The first row is the `READY` that the harness's `CLS` leaves on the top row (or, after a `~clear`, whatever comes first).
  - Rows below the last one are blank, and trailing spaces do not count.
- `{RAM}` anywhere stands for the decimal address of a free RAM byte, at most 5 digits, that neither the ROM nor the session's program uses. Each harness gives its own. `PEEK` and `POKE` cases use it, since the two Machines share no RAM address.

Rules on the files, checked by the lint:

- A row has at most 31 characters, `|` not counted and `{RAM}` counted as 5 digits. A full row of the BW-80's 32 would wrap before its new line and differ between the screens.
- A session has at most 21 rows, so it fits on both screens without scrolling.
- Typed text uses only letters, digits, space and `"$()*+,-./:;<=>?`, the characters on both Machines' keys.
- A session's program fits in 256 bytes, so it runs in the BW-80's RAM.

**Two views.** The full view is the rows as written: the PET's editor echoes each typed line where it was typed. The printed view is for an editor that types on its own line (a ZX80-style bottom line). Each row keeps only its printed part, and the rows that were only typed are left out. Every harness compares one of the two views. It ignores the cursor cell (the PET's reversed cursor, the ZX80's inverse `K`) and any edit line below the output.

**When to type.** A harness types a line once BASIC waits for one. Each ROM gives its harness a way to see this, such as a known address the CPU loops at or a flag in RAM (P5p, P5w).

`USR` and `OUT OF MEMORY` are left out because they depend on the Machine; P5p and P5w test them on their own.

The set covers:

- each statement and each function;
- every precedence level, and the depth;
- wrap-around and signed printing;
- `PRINT` separators and columns;
- `INPUT` with a prompt and with `REDO`;
- `IF` skipping the rest of a line;
- nested `FOR`, nested `GOSUB`, and `RETURN` through a `FOR`;
- `NEXT` dropping inner loops and stopping at a `GOSUB`;
- errors 1–7 in a program and in a direct line, and error 9;
- break while running and while `INPUT` waits;
- storing, replacing and deleting lines, with `LIST` and its ranges;
- tokenising: `FORI=1TO9`, keywords inside quotes and after `REM`;
- `RND`'s sequence;
- `PEEK` and `POKE`.

## Tests

- The reference interpreter (`tests/basic/tiny_basic.py`) runs every session at widths 32 and 40 with a 256-byte program area. Each must produce its full view with an echoing editor, and its printed view with a non-echoing one, exactly.
- The reference interpreter stores the program in a byte array laid out as above, so the example's bytes and the token table are checked.
- The harness lints each file to the rules above.
- A changed expected row makes the check fail, with the session, the row and both texts.
- `ctest -R "^basic_"` runs the check. CMake adds the tests only when it finds Python 3.9 or later.

Design: [TINY_BASIC_DESIGN.md](TINY_BASIC_DESIGN.md). Plan: [TINY_BASIC_PLAN.md](TINY_BASIC_PLAN.md).

# P5o Tiny BASIC — Implementation Plan

Design: [TINY_BASIC_DESIGN.md](TINY_BASIC_DESIGN.md)
Spec: [TINY_BASIC.md](TINY_BASIC.md)

Implemented 2026-11-15.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | `011e03d` |
| 1 | Reference interpreter and runner | `5e89220` |
| 2 | Conformance sessions, ctest | `35e91c7` |
| 3 | Roadmap | this commit |

## Notes

- **The review** (before code) led to these changes in the docs:
  - Tokenising "before any `REM`"; the seed named `R`, apart from the variable `S`.
  - The BW-80's 1K: print never fails as far as BASIC can see, and sessions run with a 256-byte program area.
  - A printed view of each session for a bottom-line editor. Typed keys limited to those on both Machines. `~typed` for sessions a keyword-key editor may skip.
  - Comments `~ `, since `#` can be printed. `~break` only on a silent loop or a waiting `INPUT`.
  - One `READY` after a direct `RUN`/`GOTO`; nothing after it runs. `THEN n` empties the stack like `GOTO`. The `FOR` re-entry search stops at a `GOSUB`.
  - The longest error is `DIVIDE BY ZERO ERROR IN 9999`.
  - Rows at most 31 characters (a full 32 wraps before its new line). The depth cut from 8 parentheses to 6, with unary operators counting (CPU stack).
  - Z80 `USR` in `HL`.
  - `PRINT` separators on their own, `INPUT` `-0` and `REDO` with the same prompt, leading spaces, the old line kept on `OUT OF MEMORY`, `LIST n-m` with n > m, `LIST`/`RUN`/`NEW` in a program, a break at `READY` discarded, keys typed while running dropped.
  - `{RAM}` at most 5 digits, snake_case session names, Python 3.9 floor, `-R "^basic_"`.
- **Found while writing the sessions, spec updated:**
  - A statement prints as it runs, so `PRINT 1 2` shows `1` before `SYNTAX ERROR`.
  - `~clear` lets a session show a `CLS`: the rows above it are typed but not compared.
  - The first-row `READY` lint was dropped, since a `CLS` session has none.
- **Expected outputs the interpreter corrected,** each traced to the spec, not tuned to pass:
  - `RND(32767)` as the 4th value is 11787, not a guess.
  - A stale line 100 left in a session.
  - The interpreter reset the column before a break in `INPUT`: a bug in the interpreter, fixed there.
- **Coverage, session by session:**

| Coverage item | Sessions |
|---|---|
| Statements | `PRINT` 01, 02; `INPUT` 25; `LET` 04; `IF` 20; `GOTO`, `GOSUB`, `RETURN` 21, 43; `FOR`/`NEXT` 22–24, 44; `END`, `NEW` 26; `REM` 31, 32; `POKE` 50, 51; `CLS` 27; `LIST` 30, 31; `RUN` 14, 24 |
| Functions | `PEEK` 50, 51; `RND` 14; `ABS` 13 |
| Precedence and depth | 10–13, 15 |
| Wrap-around | 03, 10, 23 |
| Errors in a program | 41, 42, 43, 46 |
| Errors in a direct line | 03, 04, 13, 15, 31, 33, 40 |
| Break | 45 |
| Tokens | 32 |

- **Checks:** both ctest entries take 0.2 s. Full debug suite at the end: 1119/1119 in 623 s.

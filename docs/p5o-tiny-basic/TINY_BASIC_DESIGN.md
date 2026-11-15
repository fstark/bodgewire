# P5o Tiny BASIC — Design

Spec: [TINY_BASIC.md](TINY_BASIC.md). Plan: [TINY_BASIC_PLAN.md](TINY_BASIC_PLAN.md).

No C++ changes. The step adds one Python script, the conformance sessions and two ctest entries. No recipe covers this; nothing in a recipe changes.

## Files

| Path | What |
|---|---|
| `tests/basic/tiny_basic.py` | the reference interpreter, the session runner and its command line |
| `tests/data/basic/NN_name.txt` | conformance sessions in the spec's format, numbered by subject (snake_case, NAMING.md) |
| `tests/data/basic/README.md` | the file format in short, pointing to the spec |
| `tests/CMakeLists.txt` | `basic_conformance` and `basic_mutation` |

The script uses the Python standard library only, runs on Python 3.9 or later (`/usr/bin/python3` on macOS), and runs with `python3 -I` (no `venv` needed). It is not a model of either ROM. It follows the spec as written. A difference found at P5p or P5w is a question for the spec: either the ROM is fixed, or the spec is fixed with its interpreter and sessions.

## `tiny_basic.py`

One file, four parts, written for reading against the spec:

1. **`Screen`**: width W, a list of rows (no scrolling, since sessions fit), and an `echo` flag.
   - `put(c)` writes a code from `$20`–`$5F` and wraps at W. `$0D` starts a new row and `$0C` clears.
   - `typed(text)` is the editor's side of an entered line: with `echo` on it writes the text, then moves to a new row; with `echo` off it only moves to a new row, and only when the cursor is not at column 0. A bottom-line editor leaves the output area alone for a command, and its `INPUT` prompt row ends there.
   - `rows()` returns the rows with trailing spaces and the blank rows at the end stripped.
2. **`Program`**: the spec's "The program in RAM" in a `bytearray` program area of a given size. The `check` command uses 256 bytes, the spec's session limit; `run` uses 7000.
   - It has `store(number, text)` (with `None` to delete), `find(number)`, `after(number)` and `records()`, which parses the bytes.
   - It raises `OUT OF MEMORY` when the records and the final `00 00` would not fit, and leaves the old bytes as they were.
   - `tokenise(text)` and `detokenise(bytes)` share one keyword table, the spec's token table in order. The tokeniser matches the keywords in table order at each position that is outside quotes and before any `REM`.
   - An assertion at import time checks that no keyword begins another.
   - `PEEK` and `POKE` use their own 64K `bytearray`, apart from the program area: no session reads the program's bytes, since the base address is each Machine's.
3. **`Basic`**: the interpreter.
   - **State**: the variables, the seed `R`, the control stack and BASIC's column. The stack is a list of at most 8 entries, `("GOSUB", line, at)` or `("FOR", var, limit, step, line, at)`, where `line` is `None` for the direct line.
   - **Execution** reads the tokenised bytes of the current line from `Program`, so `LIST`, `GOTO` and the return points all go through the RAM layout. After each statement there is either a jump (`GOTO`, a false `IF`, `RUN`, `RETURN`, `NEXT`), or the `:` or end-of-line step.
   - **Expressions**: a recursive-descent parser over the spec's eight levels, with one depth counter for open parentheses and pending unary operators. All arithmetic goes through `wrap16`. `/` truncates toward zero on the signed values.
   - **Errors**: `BasicError(code)` carries the spec's code, and one table gives the messages. `USR` raises `NotImplementedError`, since no session uses it.
   - **I/O** goes through the three calls of the editor contract that the runner provides: `print_char(c)`, `read_line()` (it returns the text, or `BREAK`) and `break_pressed()`, polled where the spec says.
   - `enter(line)` is the contract's "line": it stores, deletes or runs, and prints `READY` after a direct line or a refused one.
4. **`Session`**: parses a session file, lints it, runs it and compares.
   - **Parse**:
     - `~ ` comments are dropped.
     - `~break` marks the last typed line before it.
     - `~typed` is noted (nothing to do here).
     - Every other line is a row, split at the first `|` into printed and typed text.
     - `{RAM}` is replaced by `--ram`, by default `16384` (5 digits, as the lint counts).
   - **Lint** to the spec's rules: row length, row count, the typed characters, the first row `READY`, a `~break` after a typed line. A failure names the file and line. The 256-byte program area makes a larger program fail with `OUT OF MEMORY` in its rows.
   - **Run**: a fresh `Basic` (power-on: `NEW`, then `READY`), then `CLS` typed as the harness does, which leaves `READY` on row 0. The typed lines are then handed out in order, whether BASIC asks for a line at `READY` or for an `INPUT` answer.
   - **Break**: a `~break` arms a press when its typed line is handed out.
     - The press fires at the 1000th poll of `break_pressed()`, so a silent loop reaches it.
     - If `INPUT` asks for a line first, `read_line()` returns `BREAK`.
     - If the line finishes before either, the session fails ("~break never reached").
   - **Run errors**: a request for input with no typed line left, more than 200 000 statements without a press ("runs forever"), and a typed line never asked for all fail the session.
   - **Compare** the `Screen`'s rows against the expected view, row by row. The first difference fails the session, naming the view, the width, the row and both texts. Each session runs four times: widths 32 and 40, each with the echoing screen (full view) and the non-echoing one (printed view).

### Command line

```
python3 -I tests/basic/tiny_basic.py check [--width W ...] [--ram N] [--mutate] FILE...
python3 -I tests/basic/tiny_basic.py run [--width 40]
```

`check` runs `self_check()` and then the sessions. It prints one line a session and exits 1 if any fails.

`--mutate` checks the check. In each session it changes the last character of the last row that has printed text (`READY` → `READ#`), runs the full view, and inverts the result: every session must now fail. A check that passes everything is caught.

`run` is an interactive prompt on the terminal for trying the dialect; Ctrl-C is the break key. It reads stdin, so a fence can pipe lines into it.

`self_check()` checks what no session can see. These are the layout example (`10 PRINT A` → `0A 00 07 80 20 41 00`), three token codes, tokenising `FORI=1TO9`, quotes and `REM`, and the first four values of `RND(1000)`.

## Sessions

About 30 files, each under 21 rows, grouped by number:

| Numbers | Subject |
|---|---|
| `01`–`09` | printing and values |
| `10`–`19` | expressions |
| `20`–`29` | statements |
| `30`–`39` | program editing and `LIST` |
| `40`–`49` | errors and break |
| `50`–`59` | memory (`PEEK`, `POKE`) |

The spec's coverage list maps onto them, one or two items each. Each expected output is worked out from the spec by hand, then confirmed by the interpreter. When the two disagree, the spec decides. When the spec does not settle the question, the spec is fixed.

## ctest

In `tests/CMakeLists.txt`, after the `asm_` loop and before the KiCad block:

```cmake
# The Tiny BASIC conformance sessions on the reference interpreter (P5o).
find_package(Python3 3.9 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  file(GLOB sessions CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/data/basic/*.txt)
  set(tiny_basic ${CMAKE_CURRENT_SOURCE_DIR}/basic/tiny_basic.py)
  add_test(NAME basic_conformance COMMAND ${Python3_EXECUTABLE} -I ${tiny_basic} check ${sessions})
  add_test(NAME basic_mutation COMMAND ${Python3_EXECUTABLE} -I ${tiny_basic} check --mutate ${sessions})
endif()
```

Both take about a second. `ctest --preset debug -R "^basic_"` runs them.

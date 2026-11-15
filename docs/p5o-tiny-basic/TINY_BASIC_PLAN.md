# P5o Tiny BASIC — Implementation Plan

Design: [TINY_BASIC_DESIGN.md](TINY_BASIC_DESIGN.md)
Spec: [TINY_BASIC.md](TINY_BASIC.md)

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 0 | Docs reviewed | |
| 1 | Reference interpreter and runner | |
| 2 | Conformance sessions, ctest | |
| 3 | Roadmap | |

## Phase 1 — Reference interpreter

`tests/basic/tiny_basic.py`: `Screen`, `Program` (tokeniser, RAM layout), `Basic`, `Session` (parse, lint, run, compare), `self_check()`, and the `check` and `run` commands (design).

Fence:
- `python3 -I tests/basic/tiny_basic.py check` with no files runs `self_check()` and passes, with both the venv's Python and `/usr/bin/python3` (3.9).
- Piped into `run`: `printf '10 FORI=1TO3\n20 PRINT I;\n30 NEXT\nLIST\nRUN\nPRINT 1/0\n' | python3 -I tests/basic/tiny_basic.py run` lists `10 FORI=1TO3` as typed, prints `123`, then `DIVIDE BY ZERO ERROR`.
- Commit: `basic: Tiny BASIC reference interpreter (P5o)`, naming the checks.

## Phase 2 — Conformance sessions

`tests/data/basic/*.txt` covering the spec's list, `README.md`, and the two ctest entries.

Fence:
- `cmake --preset debug && ctest --preset debug -R "^basic_"`: `basic_conformance` and `basic_mutation` pass.
- Each spec coverage item maps to a session (a table in the notes).
- Commit: `basic: Tiny BASIC conformance set, run by the reference interpreter (P5o)`, with `tests: ctest -R "^basic_"` in its message.

## Phase 3 — Roadmap

- ARCHITECTURE: row P5o done, with the date and links. Its "no new code" becomes "a reference interpreter in Python".
- This plan is cut down to the table and the notes.
- Checks:
  - The debug build configures and builds.
  - `ctest --preset debug -N` counts as many tests as before, plus two.
  - The full debug suite runs, since `tests/CMakeLists.txt` is shared.
- No release check: no C++ changed and nothing is drawn on screen.

Commit: `docs: P5o implemented`.

## Notes

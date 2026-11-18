# P6i — Manuals that explain

Roadmap step P6i ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the manuals explain the Machine to the player, as the manuals of the time did. Design: [MANUALS_DESIGN.md](MANUALS_DESIGN.md). Plan: [MANUALS_PLAN.md](MANUALS_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

A Puzzle asks the player to find a fault in a circuit. Today the User Manual says how to power the Machine, and the Service Manual lists the parts. Nothing says how the circuit works. On a small Machine the player can work it out; on the BW-80 they cannot, so a Puzzle turns into reverse engineering the Machine before any fault finding (the user, 2026-11-18, playing the BW-80's first Puzzles).

Manuals of the 1970s and 80s explained the machine to its owner. Service manuals added test points and fault-finding charts. Bodgewire's manuals do the same, in plain prose a human can read (the user: earlier prose passes came out unreadable).

## User decisions (2026-11-18)

| Question | Decision |
|---|---|
| Scope | the code, the writing rules and the Counter as the pilot; the other 14 Machines in later rows, in batches, the BW-80 last |
| `What it does` | a welcome to the Machine: what it is and what it does, without hookup steps (the player connects nothing). The Counter: five counters divide the 1 MHz oscillator by 1 048 576, so the LED blinks about once a second. A computer later: its CPU, RAM, ROM, program and display |
| User Manual sections | `What it does`, `Using it` (only where there is something to operate, such as the BW-80's keyboard), `How it works` |
| Service Manual sections | `Components` (kept), `Test points`, `Troubleshooting` |
| `How it works` | the story of the circuit: the goal first, the blocks in the order the signals flow, a familiar picture before each mechanism (the Counter's five chips as an odometer), no formulas, unused features in a table |
| `Test points` | where to probe and what a working Board reads there |
| `Troubleshooting` | from a symptom to where to start, as ordered checks, written from the working Board: it never names a fault. On the Counter it almost solves a Puzzle; that is fine for a tutorial Machine, where the method is the lesson |
| Tables | added to the manuals' Markdown: pipe tables, links working in cells |
| Links | beyond a part (`` `U1` ``) and "pin N": a run of parts (`` `U1`–`U5` ``), a Net (`` `C19` ``), a pin by name (`` `U2.ENT` ``); a bare pin name (`` `QA` ``) stays plain code |
| A Net link | hovering it lights the Net on the Board, as hovering its copper does; a click frames the whole Net and keeps it lit for a moment, as a part link flashes its chip |
| Writing rules | in `docs/STYLE.md`, a Manuals section |

Decided while designing (2026-11-18), for the user to overrule:

- **The new sections are optional** in the checks, in their order: a Machine not yet rewritten keeps passing with `What it does` and `Components` alone. They become required when every Machine has them.
- **Runs, Nets and pins by name link only in a Machine's own manuals**, not in datasheets: a datasheet is shared by every Machine, and its `CLK` or `D0`–`D7` are pin names, not one Machine's Net or parts. Datasheets keep part links and "pin N".
- **A name that is both a part and a Net** (the Typewriter's `R1` and `X1`) links the part, as today. The Typewriter's Service Manual writes its Nets `` `R1` ``–`` `R3` ``, so its `R1` links the resistor there, as it does today; fixed at its rewrite.
- **A run of parts** is two part references joined by an en dash (`–`), the same letters, the first number smaller. It covers every part with those letters and a number in between. Hovering it outlines them all; a click frames them all and flashes them; nothing is selected (a selection is one part). Hovering a single part's link outlines it too (today it shows nothing).
- **A pin by name** is `REF.PIN`, the netlist's own syntax: a link to that pad, as "pin N" is. Only a chip's or connector's pin names (a resistor has none: `` `R1` pin 2 ``). A right side that does not look like a pin name (upper case, digits, `_`) is not checked: the BW-2001's `` `UH1.bin` `` stays a file name.
- **A misspelt Net** (`` `RESET_NN` ``) is not reported: it stays plain code. Accepted: only reference-looking spans are checked, as today.

## What is new

1. **Tables** in the manuals' Markdown (`src/manual/markdown`), drawn in the binder.
2. **Links** (`src/manual/manual`): runs, Nets, pins by name; checked like part references.
3. **The Board** (`src/ui/board_view`): a target is one part, a run of parts or a Net.
4. **Section checks**: the optional sections in order.
5. **The Counter's manuals**: `What it does` rewritten, `How it works`, `Test points`, `Troubleshooting`.
6. **Docs**: `docs/STYLE.md` (Manuals), `docs/recipes/ADD_MACHINE.md` (the sections), GLOSSARY, ARCHITECTURE.

The kernel, Parts, the builder, devices, Session and Puzzles are untouched.

## Tests

- **Markdown** (`tests/manual/markdown_test.cpp`): a table's header and rows, cells with code spans, the separator row dropped, a table ended by a blank line or another block; a line starting with `|` but no separator stays a paragraph.
- **Links and checks** (`tests/manual/manual_test.cpp`): a run, a Net, a pin by name found by `LinkAt` in a Machine manual, none of them in a datasheet (`` `CLK` `` a Net link in a Machine manual, plain in a datasheet: the existing `manual: links` case updated); a part wins over a Net of the same name; the checks report a run with an unknown end or reversed numbers, a pin name the part lacks, references in table cells; the optional sections pass in order and fail out of order (the existing `manual: checks` messages updated); `` `UH1.bin` `` not reported.
- **Every Machine's manuals** (`tests/machines/manuals_test.cpp`): still clean; a new assertion that the Counter's have their four new sections.
- **The Counter's readings**: each `Test points` row checked against the working Machine (`bw run --probe` at 1 s, 1.5 s and 2 s: a slow Net such as `C19` reads high at one and low at another) before it is written.
- No UI tests exist: links and tables are checked by screenshots (`--manual user|service`) and by the user.

## Later

- The other Machines' manuals, in batches (new rows), the BW-80 last.
- Analyzer setups and waveforms in `Test points` for the TV and computer Machines.
- How Puzzles are ordered within a Machine (discussed 2026-11-18: by the stage of the Machine each needs, from the screen inward, dead Machines last; not decided).

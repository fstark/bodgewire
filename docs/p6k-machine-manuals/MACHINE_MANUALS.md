# P6k — Manuals for every Machine

Roadmap step P6k ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the manuals of the 14 Machines after the Counter explain their Machine, as P6i's pilot did for the Counter. Design: [MACHINE_MANUALS_DESIGN.md](MACHINE_MANUALS_DESIGN.md). Plan: [MACHINE_MANUALS_PLAN.md](MACHINE_MANUALS_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

P6i gave the manuals their sections and writing rules ([STYLE.md](../STYLE.md) Manuals) and rewrote the Counter's as the model. The other Machines still have the old manuals: a User Manual with hookup steps and a paragraph of behaviour, a Service Manual listing the parts. A player facing a Puzzle on the Tennis or the BW-80 has to reverse engineer the Machine before finding a fault. P6k gives every Machine the Counter's treatment, then makes the sections required so a new Machine cannot skip them.

## User decisions (2026-11-18)

| Question | Decision |
|---|---|
| The Typewriter's Nets `R1`–`R3` (the character ROM's line address, `U13`'s gated `V1`–`V3`) | renamed `GL1`–`GL3` ("glyph line"), in the netlist, the Board and the Machine's docs; `R1` then links only the resistor |
| Batches | one go, every batch without stopping; the user reads the manuals at the end |
| `Troubleshooting` | general charts only: a few broad symptoms per Machine (dead, no picture, a wrong count…), written from the working Board, never naming a fault; the Puzzles are not covered one by one |
| How Puzzles are ordered within a Machine | left out of P6k, still open |

From the row and P6i (already decided):

- The 14 Machines in ladder order (directory number), in batches, the BW-80 last.
- User Manual: `What it does` (a welcome, no hookup steps: the player connects nothing), `Using it` where there is something to operate, `How it works`. Service Manual: `Components`, `Test points`, `Troubleshooting`.
- Analyzer setups in `Test points` for the TV and computer Machines (050 and after).
- Once every Machine has them, the new sections are required in the checks.

Decided while designing (2026-11-18), for the user to overrule:

- **`Using it` is required only where there is something to operate**, so it stays optional in the checks: the LED Chaser, the Test Pattern, the Bouncing Ball and the Message have nothing to press (the Heartbeat's and the LED computer's `RESET` buttons are something to operate). `How it works`, `Test points` and `Troubleshooting` become required.
- **An analyzer setup is text** in a `Test points` row: which pins to clip, which channel triggers on which edge, the sample time, and what a working Board shows. No new link kind or code: the player sets the analyzer up by hand, as the Counter's oscilloscope rows ask.
- **Each Machine's design doc** gets the Counter's `## Manuals` section (what each manual's sections say, how the readings were checked), replacing the old one.

## What is new

1. **The Typewriter's Nets** `GL1`–`GL3`: `typewriter.bwn`, `board/typewriter.kicad_pcb`, `docs/TYPEWRITER*.md`, and its Service Manual's `U13` and `U23` bullets (an `R3` would be an unknown reference).
2. **The manuals of 14 Machines**, in seven batches: the LED Machines (Chaser, Dice, Stopwatch, Knob Meter); the first TV Machines (Test Pattern, Bouncing Ball, Tennis); the text Machines (Message, Typewriter); the first processors (Heartbeat, LED computer); the BW-1; the BW-2001; the BW-80.
3. **The checks** (`src/manual/manual.cpp`): `How it works`, `Test points` and `Troubleshooting` required; `Using it` optional between `What it does` and `How it works`.
4. **Docs**: `docs/recipes/ADD_MACHINE.md` §2 (the sections required), `docs/STYLE.md` Manuals if the rollout finds a rule missing, ARCHITECTURE.

The kernel, Parts, the builder, devices, the UI, Session and Puzzles are untouched.

## Tests

- **Every Machine's manuals** (`tests/machines/manuals_test.cpp`, `every Machine's manuals pass their checks`): clean after each batch (every backticked reference a part, a run, a pin by name or a Net). After the checks change, this is what holds every Machine to the new sections; the Counter-only case `the Counter's explain how it works` becomes a check in it that every Machine's sections are not empty (`Using it` when present).
- **The checks** (`tests/manual/manual_test.cpp`): `manual: optional sections` and `manual: checks` updated: a User Manual without `How it works` fails, with or without `Using it` before it passes; a Service Manual without `Test points` or `Troubleshooting` fails; the messages read `sections must be: What it does[, Using it], How it works` and `sections must be: Components, Test points, Troubleshooting`.
- **The Typewriter's rename**: `bw check-board` on its netlist and Board prints `ok`; `drc_typewriter` and `boards: typewriter matches its netlist` pass; its own tests and Puzzles pass (no test or Puzzle names `R1`–`R3` as Nets).
- **Every reading** in a `Test points` table is checked on the working Machine before it is written: the probe with `bw run --probe` at several times (a slow Net changing between them); an analyzer setup with `bw_ui --clip … --screenshot`, the picture viewed.
- No UI tests exist: each batch's manuals are viewed in screenshots (`--manual user|service`), links resolved, tables drawn.

## Later

- How Puzzles are ordered within a Machine (discussed 2026-11-18: by the stage of the Machine each needs, from the screen inward, dead Machines last; not decided).
- Troubleshooting charts that cover each Puzzle's symptom (the user chose general charts for now).

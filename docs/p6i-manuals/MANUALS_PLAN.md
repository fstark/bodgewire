# P6i — Manuals that explain — Implementation Plan

Spec: [MANUALS.md](MANUALS.md). Design: [MANUALS_DESIGN.md](MANUALS_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Tables in the manuals' Markdown, drawn in the binder | |
| 2 | Links: runs, Nets, pins by name; the Board's targets; optional sections | |
| 3 | The Counter's manuals | |
| 4 | STYLE, recipe, glossary, ARCHITECTURE | |

## Phase 1 — Tables

- `BlockKind::Table`, `Block::rows`; the parser (design, Tables); `markdown_test` cases.
- `ManualView` draws a Table (design, The binder); no manual has one yet: a table added to the Counter's `user.md` for a screenshot (`--manual user`), then `git checkout` of that file.
- `CheckReferences` walks table cells (a table with an unknown reference reported: a `manual_test` case).

Fence: release build; `ctest --preset release -j -R "manual|markdown|manuals"`; debug build, `ctest --preset debug -j -R "manual|markdown"` (parser logic). Screenshot viewed. Commit: `manual: pipe tables in the manuals, drawn in the binder (P6i phase 1)`.

## Phase 2 — Links and sections

- `LinkScope`, the new `ManualLink`, `LinkAt` (design, Links); `CheckReferences` with the scope and the new reports; `CheckShape` with optional sections, the User and Service Manuals' lists, datasheets with no optional ones.
- `BoardTarget` reshaped; every builder and reader updated; `BoardScene::netNamed`; hover, focus, flash and `route` for runs and Nets (design, The Board).
- `ManualView::draw` takes the scope; `manual_window.cpp` passes `Machine` for the User and Service Manuals, `Parts` for datasheets; `chip_panel.cpp` likewise for its datasheet text.
- `manual_test` cases: the spec's list (runs, Nets, pins by name, scope, a part over a Net, bad runs only between parts, unknown pins, `UH1.bin` not reported, sections in and out of order); the existing `manual: links` and `manual: checks` cases updated.

Fence: release build; `ctest --preset release -j` (shared code: `BoardTarget`, the manual checks run for every Machine); debug `ctest --preset debug -j -R "manual|markdown|board scene"`. Screenshots viewed: the BW-80's Service Manual (`--manual service`), whose `MD0`, `RESET_N` and `A8`–`A15` become Net links, underlined. Hover and click are checked by reading the code (the agent cannot move the mouse); user check. Commit: `manual: links to runs of parts, Nets and pins by name; optional sections (P6i phase 2)`.

## Phase 3 — The Counter's manuals

- Readings first: `./build/release/src/cli/bw run machines/010-counter/counter.bwn --time T --probe U1.CLK,…` at T = 1s, 1.5s and 2s, for `U1.QA`, `U1.QD`, `U1.RCO` (`CARRY1`), `U4.RCO` (`CARRY4`), `U5.QD` (`C19`), `D1.K`, `U1.CLR_N` (`RESET_N`): the probe's lamps for each, written into the notes below.
- `user.md`: `What it does`, `How it works`; `service.md`: `Test points`, `Troubleshooting` (design, The Counter's manuals), in STYLE's Manuals rules (the draft rules from the spec, written into STYLE in phase 4).
- Every link resolves (the manuals test); every reading in `Test points` matches the notes; `manuals_test` asserts the four new sections on the Counter; `COUNTER_DESIGN.md` §Manuals updated.

Fence: `ctest --preset release -j -R "manual|markdown|manuals|counter"`. Screenshots viewed: `--manual user` and `--manual service` on `machines/010-counter/puzzles/01-dark-led.bwp`, both new sections readable, the tables drawn. Commit: `counter: manuals that explain: a welcome, how it works, test points, troubleshooting (P6i phase 3)`.

## Phase 4 — Docs

- `docs/STYLE.md` `## Manuals`; `docs/recipes/ADD_MACHINE.md` §2; GLOSSARY **Test Point**; ARCHITECTURE: P6i done with the date and links (its Service section named `Test points`), and a new row after it for the other Machines' manuals (in batches, the BW-80 last).
- Finish (AGENTS.md): release check with the Counter's two manuals and the BW-80's Service Manual screenshotted and viewed; the plan cut down to the phase table and notes.

Fence: full release suite. Commit: `docs: P6i done, manuals that explain`.

## Implementation notes

(filled in as phases complete)

# P6i — Manuals that explain — Implementation Plan

Spec: [MANUALS.md](MANUALS.md). Design: [MANUALS_DESIGN.md](MANUALS_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Tables in the manuals' Markdown, drawn in the binder | 0e614fb |
| 2 | Links: runs, Nets, pins by name; the Board's targets; optional sections | 0908b35 |
| 3 | The Counter's manuals | c4b7787 |
| 4 | STYLE, recipe, glossary, ARCHITECTURE | this commit |

P6i implemented 2026-11-18, user check pending (hovering and clicking the new links).

## Implementation notes

- Phase 1: release `-R "manual|markdown|manuals"` 21 passed; debug `-R "manual|markdown"` passed. Screenshot viewed: a three-row table added to the Counter's `user.md` (then checked out): the header on a tinted band, rules between rows, a long cell wrapping in its column, `U1` pin 2 and `D1` links inside cells.
- Phase 2: full release suite 1450 passed (every Machine's manuals still clean: the BW-2001's `UH1.bin`, the Typewriter's `R1`–`R3`, the Message's `F5`–`F1` unreported); debug `-R "manual|markdown|board scene"` 25 passed. Screenshot viewed: the BW-80's Service Manual, Nets underlined as links (`VIDEO`, `SYNC`, `A8` and `A15`, `MD0`, `RESET_N`, `CLK`, `NMI_N`), `IC6`–`IC8` one run link; `INT_N` plain (the CPU's pin, wired to Net `A6`). Hover outlines and a click's frame and flash for a run or a Net are checked by reading the code: user check. A single part's link now outlines it on hover too. A Net's flash rings its pads; while it lasts the Net stays lit.
- Phase 3 readings (`bw run counter.bwn --time T --probe …`, T = 1 s, 1.5 s, 2 s; the probe's level is the value at that instant, P lit within 100 ms of a change): `CLK`, `U1.QA`, `U1.QD`, `U1.RCO`, `U2.RCO`, `U4.RCO`, `U2.ENT`: P at every T (`CARRY4`, high 1 µs every 65.5 ms, keeps P lit); `U5.QD` (`C19`) and `D1.K`: HIGH PULSE, HIGH, LOW (it changes every 0.524 s); `RESET_N`, `ENP`: HIGH. `release -R "manual|markdown|manuals|counter"` 42 passed. Screenshots viewed: both manuals, links resolved (`U1`–`U5` one run link, `CLK`, `CARRY1`, `C19`, `RESET_N` Net links, `U1.QA` pin links), the Test points table drawn. Bold lead-ins ("The goal.") barely stand out: the binder has no bold font, `kBold` is a colour only. Left for the rollout.
- Phase 4: STYLE Manuals, ADD_MACHINE §2, GLOSSARY **Test Point**, ARCHITECTURE (P6i done, P6k added for the other Machines). Full release suite passed.

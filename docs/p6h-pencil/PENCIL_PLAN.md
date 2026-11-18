# P6h — Pencil — Implementation Plan

Spec: [PENCIL.md](PENCIL.md). Design: [PENCIL_DESIGN.md](PENCIL_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Pencil marks, `Owner::Pencil` | b20ba46 |
| 2 | The tool on the shelf and the Board, `--mark` | 387e0ec |
| 3 | Glossary, recipe, ARCHITECTURE | this commit |

P6h implemented 2026-11-18, user check pending.

## Implementation notes
- Docs review (before the docs commit), fixed: no double-click fit under the pencil (a dot to a cross is two quick clicks); `--mark` parsed in `ParseOptions`, refused without a Board in `main()`, its bodies found in `RunWindow`; `case Owner::Pencil` added in phase 1 (`-Werror`); the last mark on a tie (drawn on top); a Board key under the pencil gives the keys to the bench; ADD_INSTRUMENT's Focus row cited and to be fixed; GLOSSARY's Focus red; a screenshot of the hint after the extra cell; screenshot coordinates named.
- Phase 1: release and debug `-R "pencil marks|focus"`, 13 passed each.
- Phase 2: the mark enlarged to **4 mm** across (the design's 2.5 mm read small at the Stopwatch's fit zoom: the dot on U9 lost in the part number), the dot a quarter of the size in radius, the stroke 0.18 of it; the floor stays 10 px. Screenshots viewed: the Stopwatch with `--focus pencil --mark dot:88.5:70.6,check:65.69:116.71,cross:76:96,check:111.5:70.6` (a dot on U9's body, a check on `START` (SW1), a cross on bare Board between U5 and U6, a check on U10), the shelf's pencil framed in gold; the same with `--look plain`; the BW-80 at fit zoom (`check:29.2:63.5,cross:61:127,dot:31.7:127`: IC3, IC16, IC14), every mark readable beside the TV and keyboard; `--focus wire` with the power on: `Power off first` still fits after the extra cell. `--mark blob:1:2` and `--mark dot:1` give the usage; `--mark` on `tests/data/bw1_bench.bwn` (no Board): `no Board to mark`, exit 2. Clicks (a dot, cross, check, none; Shift-click and right-click; a swap clearing a chip's marks; a switch not pressed; no fit on a double-click) are checked by reading the code only: the agent cannot drive the mouse. User check.
- Phase 2 checks: full release suite 1442 passed (99 s); debug `-R "pencil marks|focus"` 13 passed. Speed gate: the 110 with every instrument and two marks 1.00×, UI 0.6 ms.
- Phase 3: GLOSSARY (**Pencil Mark**, the Shelf's tools, Focus's red), ADD_INSTRUMENT's Focus row (a Board tool), the spec and design at 4 mm, ARCHITECTURE.

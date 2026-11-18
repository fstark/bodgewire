# P6h — Pencil — Implementation Plan

Spec: [PENCIL.md](PENCIL.md). Design: [PENCIL_DESIGN.md](PENCIL_DESIGN.md).

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Pencil marks, `Owner::Pencil` | b20ba46 |
| 2 | The tool on the shelf and the Board, `--mark` | |
| 3 | Glossary, ARCHITECTURE | |

## Phase 1 — Pencil marks, `Owner::Pencil`

1. `src/view/pencil_marks.{hpp,cpp}` as designed; `src/view/CMakeLists.txt`.
2. `tests/view/pencil_marks_test.cpp` (the spec's cases); `tests/CMakeLists.txt`.
3. `Owner::Pencil` in `src/view/focus.hpp`; the `focus: the pencil is a tool` case. `main.cpp`'s and `shelf.cpp`'s switches over `Owner` and `Item` still compile: add `case Owner::Pencil: break;` to `main.cpp`'s `switch (focus.owner())` (no `default`, `-Werror`); `shelf.cpp`'s `OwnerOf` has a `default`.

**Fence:** release and debug builds; `ctest --preset release -j -R "pencil marks|focus"` and the same on debug pass. clang-format on the changed C++ files.
Commit: `view: pencil marks, Owner::Pencil (P6h phase 1)` (tests: `-R "pencil marks|focus"`, release and debug).

## Phase 2 — The tool on the shelf and the Board, `--mark`

1. `shelf_drawings`: `Item::Pencil`, `Pencil(Pen&)`.
2. `shelf.cpp`: the entry, `GroupEnds`, `OwnerOf`, key `G`.
3. `board_view`: `setMarks`, `markReach`, `takeRightClick`, `buttonAt` under the pencil, no double-click fit under the pencil, the drawing.
4. `main.cpp`: the click, the right-click, the swap, `setMarks`, `--mark`, `--focus pencil`, the usage.
5. Screenshots (release), each viewed: `bw_ui machines/040-stopwatch/*.bwn --focus pencil --mark dot:…,cross:…,check:… --screenshot …` with a dot on a chip's body (a counter), a cross on bare Board, a check on the `START` button (coordinates read from `machines/040-stopwatch/board/stopwatch.kicad_pcb`, recorded in the notes); the same with `--look plain`; the BW-80 at fit zoom with marks on two chips' bodies (the 10 px floor); one at 1280 wide with a hint showing (`--focus wire` with the power on: `Power off first`), to see the hint still fits after the extra cell; a bad `--mark` and `--mark` on a Board-less netlist give the usage error.

**Fence:** release build; the full release suite passes (`main.cpp` and the shelf are shared by every Machine); debug build and `ctest --preset debug -j -R "pencil marks|focus"`; the 110 with every instrument 1.00×, UI about 1 ms; screenshots viewed.
Commit: `ui: the grease pencil (G): grey dot, cross or check on the Board, a click on a mark changes it, shift- or right-click clears; a chip's marks leave with it when swapped (P6h phase 2)`.

## Phase 3 — Glossary, ARCHITECTURE

1. GLOSSARY: **Pencil Mark**, the Shelf's tools, Focus's red; ADD_INSTRUMENT's Focus row (a Board tool).
2. ARCHITECTURE: P6h done (2026-11-18) with the links.
3. This plan cut down to the phase table and the notes.

**Fence:** docs only, no build.
Commit: `docs: P6h done, the pencil`.

## Implementation notes
- Docs review (before the docs commit), fixed: no double-click fit under the pencil (a dot to a cross is two quick clicks); `--mark` parsed in `ParseOptions`, refused without a Board in `main()`, its bodies found in `RunWindow`; `case Owner::Pencil` added in phase 1 (`-Werror`); the last mark on a tie (drawn on top); a Board key under the pencil gives the keys to the bench; ADD_INSTRUMENT's Focus row cited and to be fixed; GLOSSARY's Focus red; a screenshot of the hint after the extra cell; screenshot coordinates named.
- Phase 1: release and debug `-R "pencil marks|focus"`, 13 passed each.
- Phase 2: the mark enlarged to **4 mm** across (the design's 2.5 mm read small at the Stopwatch's fit zoom: the dot on U9 lost in the part number), the dot a quarter of the size in radius, the stroke 0.18 of it; the floor stays 10 px. Screenshots viewed: the Stopwatch with `--focus pencil --mark dot:88.5:70.6,check:65.69:116.71,cross:76:96,check:111.5:70.6` (a dot on U9's body, a check on `START` (SW1), a cross on bare Board between U5 and U6, a check on U10), the shelf's pencil framed in gold; the same with `--look plain`; the BW-80 at fit zoom (`check:29.2:63.5,cross:61:127,dot:31.7:127`: IC3, IC16, IC14), every mark readable beside the TV and keyboard; `--focus wire` with the power on: `Power off first` still fits after the extra cell. `--mark blob:1:2` and `--mark dot:1` give the usage; `--mark` on `tests/data/bw1_bench.bwn` (no Board): `no Board to mark`, exit 2. Clicks (a dot, cross, check, none; Shift-click and right-click; a swap clearing a chip's marks; a switch not pressed; no fit on a double-click) are checked by reading the code only: the agent cannot drive the mouse. User check.
- Phase 2 checks: full release suite 1442 passed (99 s); debug `-R "pencil marks|focus"` 13 passed. Speed gate: the 110 with every instrument and two marks 1.00×, UI 0.6 ms.

# P6e — Manuals binder — Implementation Plan

Spec: [MANUALS_BINDER.md](MANUALS_BINDER.md). Design: [MANUALS_BINDER_DESIGN.md](MANUALS_BINDER_DESIGN.md).

## Phases

### 1. Binder state and the datasheet index

- `src/view/binder.{hpp,cpp}`, in `src/view/CMakeLists.txt`; `tests/view/binder_test.cpp` in `tests/CMakeLists.txt`: the spec's Binder cases (`TEST_CASE("binder: …", "[binder]")`).
- `DatasheetIndex` in `src/manual/manual.{hpp,cpp}`; `manual: datasheet index` on the counter netlist (`Counter()`): Parts in order of first use, references in netlist order, each Part once.
- `FirstSize(Manual)` 600 × 500; `bench places` still passes (the manual inside the bench at its place).

Fence: `cmake --build --preset release`; `ctest --preset release -j -R "binder|manual|bench places"`; debug build, `ctest --preset debug -j -R "binder|manual: datasheet index|bench places"`. Commit `view: the binder's dividers and the datasheet index (P6e)`.

### 2. The binder drawn

- `manual_window.{hpp,cpp}`: the case, cover, rings, page, tabs, the four dividers, the index; `TextFlow` out of `manual_view.cpp`; `PageTooltip`; paper colours in `manual_view.cpp` and `chip_panel.cpp`.
- Shelf tooltip and drawing; `main.cpp`'s `--manual user|service|datasheet|repairs`.

Fence: release build; full `ctest --preset release -j`; screenshots viewed (spec, Tests: also a BW-2001 chip's page and the shelf drawing out and away; the tab labels at 0.85× legible), the 110 with every instrument (1.00×, UI about 1 ms). Commit `ui: the manuals as a ring binder: User, Service, Datasheets, Repairs (P6e)`.

### 3. Finish

GLOSSARY, ADD_INSTRUMENT's CLI row, ADD_PUZZLE and `BW1_DESIGN.md`'s Service Manual lines, ARCHITECTURE's P6e row done; this plan cut to the status table and notes. Commit `docs: P6e done, manuals binder`.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Binder state, datasheet index, the binder's first size | |
| 2 | The binder drawn; shelf drawing; `--manual` dividers | |
| 3 | Glossary, recipes, ARCHITECTURE | |

## Implementation notes

- Docs review (before the docs commit), fixed: a pin click selects its chip whatever the focus, so it turns the binder (stated, for the user); `bw1_bench.bwn` has no manuals; plain references without a Board; `All datasheets` a button in `manual_window`; a `PushID` per Datasheets mode; the colours `TextFlow` needs; the rings an ellipse; the minimum width 500 for a BW-2001 pinout; `--manual repairs` without a Board; the scrollbar's hover colours; ADD_PUZZLE, `BW1_DESIGN.md` and the ARCHITECTURE wording.
- Written while the review ran: phase 1, most of phase 2.

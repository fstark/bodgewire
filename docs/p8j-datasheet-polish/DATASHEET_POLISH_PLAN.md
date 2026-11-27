# P8j — Datasheet polish — Implementation Plan

Spec: [DATASHEET_POLISH.md](DATASHEET_POLISH.md). Design: [DATASHEET_POLISH_DESIGN.md](DATASHEET_POLISH_DESIGN.md).

## Status

| Phase | Description | Commit |
|---|---|---|
| 1 | Pin looks and groups (pure) | `983c74b` |
| 2 | A sheet's chip scope, the role's spans, the generic sheet gone | |
| 3 | One-pin Nets removed, the manuals' check | |
| 4 | The chip page: title, keys, no folds, Back, each chip's place | |
| 5 | The pinout's symbols, looks and group tint | |
| 6 | The Board: group marks, tooltips | |
| 7 | Sheets, docs, finish | |

## Phase 1 — Pin looks and groups

1. `src/view/pin_marks.hpp`, `.cpp` (design §1), in `src/view/CMakeLists.txt`.
2. `tests/view/pin_marks_test.cpp` (in `tests/CMakeLists.txt`).

Fence: release and debug `ctest -R "pin marks"`. Commit: `view: pin looks and groups (P8j phase 1)`.

## Phase 2 — A sheet's chip scope, the role's spans, the generic sheet gone

1. `LinkScope::Chip` replacing `Parts`, `ManualLink::pins` (last), `LinkAt(…, chip)`, `ExpandRange` (numeric suffixes) shared with `BulletPins` (design §3); `ManualLink{…}` inits completed.
2. `BoardTarget::pads` (last) and every `BoardTarget{…}` init completed (design's list); the rings and flash for `pads` (design §5).
3. `FlowSpans`, `ManualView::draw(…, chip)`, the dotted pin links and their targets (design §4).
4. `ComponentRoleSpans`; `ComponentRole` gone; the chip panel draws the role sentence through `FlowSpans` (Machine scope); `manuals_test.cpp` on the spans.
5. The chip panel's Function and Pins in the Chip scope with the reference, Pins' hits merged.
6. The generic sheet gone: `sheet_`, the list's Part links, `Binder::openPart`/`closePart`/`part()`, binder_test's case.
7. Tests: `manual: chip pins`, `manual: ranges`, `manual: a sheet never links a reference`, `manual: role spans`; the Parts checks rewritten.

Fence: release and debug `ctest -R "manual|binder|focus|board scene"`; release `-R manuals`; screenshot U5 on the Counter (the list's `U5` tapped): the role's links, `CLK` dotted in Function, Pins' `A`–`D` a link. Commit: `manual, ui: a sheet links its chip's pins, the role sentence's links, no generic sheet (P8j phase 2)`.

## Phase 3 — One-pin Nets removed, the manuals' check

1. The four netlists (design §6) and their comments; `bw check-board` on each before and after.
2. `tests/netlist/sample_test.cpp` (`Count()`, `ApiCounter`), `tests/manual/manual_test.cpp` (`C3`, line 142).
3. The KiCad Boards' pad `(net "…")` lines for the removed Nets deleted by a one-off script (not kept); `board/*.net` regenerated for the Counter, Chaser and Tennis.
4. The one-pin Net warning in `LoadManuals`; `manual: one-pin Nets`. ADD_MACHINE.md: the rule and §3's case (recipe fixed in the same commit).

Fence: release `ctest -R "sample|manual|counter|chaser|tennis|bw2001|drc|procedural|fault draw|board"` (a `drc_*` hung at 0 % CPU is killed and rerun alone); debug `-R "sample|manual"`. Commit: `machines: no Net reaching one pin unless a manual names it (P8j phase 3)`, its body naming the Nets and the tests run.

## Phase 4 — The chip page: title, keys, no folds, Back, each chip's place

1. `Binder`: `Place`, `turn` and `frame` recording, `here`, `scrolled`, `back`, `showList` (design §2); `binder: back …` tests.
2. `ChipPanel`: the title as a link, the separator, no Zoom to, the `TreeNodeEx` gone (bold lines), the tooltip's format (design §4 items 1, 5, 6).
3. `ManualWindows`: Back and All chips; per-chip page ids and `scrolled`; Back by key and X1 (desktop); the scroll restored once; Contents' view id per chip.
4. Screenshots: U5 (title, keys), the list (Back), the Service Manual scrolled to a step, `U5` tapped, Back tapped: the Service Manual at its step; at 24 px.

Fence: release and debug `ctest -R "binder|manual|bench options"`; screenshots viewed. Commit: `ui: a chip's page titled by the chip, Back, each chip's place kept (P8j phase 4)`.

## Phase 5 — The pinout's symbols, looks and group tint

1. `BoardScene::pinNames`, `pinKinds`, `partKind`, `alone` (design §5).
2. The stubs, `DrawPinSymbol`, the digit's and list's rows; the mono names, the blue Nets, the faint `alone` rows; the group tint one frame late (design §4 items 3, 4).
3. Screenshots: U5 (the mock-up), U3 on the Dice with `1Y` hovered (a tap on `U3` in the list, then a tap resting on the `1Y` row, `--after` within 2.5 s of it), a 74LS273 or 74LS175 page with `CLK` hovered, the 6502 on the LED computer (`D7` plain to the LED, the `PHI` pulses), a digit (the Stopwatch).
4. Speed: the BW-2001 in `bw_ui` with the binder on a chip's page, before (phase 4's build) and after.

Fence: release and debug `ctest -R "board scene|pin marks|manual"`. Commit: `ui: the pinout shows each pin's kind and what it works with (P8j phase 5)`.

## Phase 6 — The Board: group marks, tooltips

1. `DrawPinNames` with the hovered pad and its marks (design §5); `hoverText` and `Tooltip` with ` · `; "view: hover text" updated.
2. Screenshots: the Dice's U3 hovered from the page (phase 5's shot, the Board side); a finger held on the `1Y` pad (`--touch --hold X:Y`); the Board zoomed out (no marks).

Fence: release `ctest -R "board scene|view: hover|board"`; debug `-R "board scene|view: hover"`. Commit: `ui: a hovered pin marks what it works with on the Board (P8j phase 6)`.

## Phase 7 — Sheets, docs, finish

1. The sheets (design §7); `ctest -R manuals`.
2. Docs (design §8): GLOSSARY, ADD_PART, AGENTS helpers, the ARCHITECTURE row done with the date, one line and links.
3. The web build (nothing new outside ImGui's draw lists; X1 is desktop-only).
4. The full release suite; screenshots viewed again (U5, U3 hovered, the 6502).
5. The plan cut to this table and the notes.

Fence: the full release suite; the web build. Commit: `docs: P8j done, sheets name their pins (P8j phase 7)`.

## Implementation notes

- 2026-11-27, review (a fresh agent, the three docs only): 11 must-fixes, all fixed in the docs. The Net sweep missed `sample_test.cpp` and `manual_test.cpp:142`; `ComponentRole` used by `manuals_test.cpp`; phase 2 would not build while the generic sheet used `LinkScope::Parts` (it now goes in phase 2), nor phase 4 with binder_test's Part case; the `-R` filters used underscores where test names have spaces; Back would have recorded its own selection and turned Contents to Datasheets (the Binder now becomes the place, `selected_` included); the scroll for Back was the last frame's child, possibly a hovered chip's (now kept per place, written only by the page drawn); `Place::operator==` compared the scroll; `LookOf` mapped switches (`Output`) and connectors (`Input`) to arrows and `PullUp` to Passive (now by `PartKind`, `PullUp` → Both); the 74LS244's enable is `1OE_N`; `BoardTarget` lives in `view/board_scene.hpp` and `-Wmissing-field-initializers` needs every brace-init completed. Should-fixes taken: numeric ranges (`A0`–`A15`), `CLK1`/`CLK2` clocks, `alone` from `netPads_`, no `folds` parameter, no list flag, X1 desktop-only, nine kept Nets, the BW-2001 without `.net`, Pins' hits merged, the speed check. Left: sections by a trailing index (spec, Later).
- 2026-11-27, phase 1: as designed; the tests build each Part's sample Component for its pin names and kinds (no catalogue helper). Checks: release and debug `-R "pin marks"`.
- 2026-11-27, phase 2: as designed. A Chip-scope link keeps the code's colour and face with a dotted underline; a range's dash is not underlined. The list's hint without a Board reads "no Board". Checks: release and debug `-R "manual|binder|focus|board scene"` (46 cases, every Machine's manuals included); screenshot of U5 on the Counter (the role's `U4`, `C19`, `D1` links; `CLK` dotted).

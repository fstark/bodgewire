# P4c Manual Cleanup — Implementation Plan

Design: [MANUAL_CLEANUP_DESIGN.md](MANUAL_CLEANUP_DESIGN.md)
Spec: [MANUAL_CLEANUP.md](MANUAL_CLEANUP.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Short manuals and their checks | |
| 2 | Driver pads in BoardScene | |
| 3 | UI: removals, hover text, Driver marks | |
| 4 | Docs, release and user check | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate:  `ctest --preset debug -j 12`

---

## Phase 1 — Short manuals and their checks

Data and checks change together so the per-Machine manual tests stay green. The UI keeps compiling: the Service Manual window now shows only Components, and the Puzzle section's "When it works" shows the new paragraph until phase 3.

### 1.1 — Checks

[src/manual/manual.cpp](../../src/manual/manual.cpp): `CheckUserManual` uses `{"What it does"}`; `CheckServiceManual` uses `{"Components"}` and calls `CheckShape(doc, problems, kSections)` without optional sections (delete `kOptional`). Nothing else changes (design §2.1).

### 1.2 — Content

Rewrite `manuals/{counter,chaser,test_pattern}/user.md` with the text in design §2.2. In each `service.md`, delete `## How it works` and `## Troubleshooting` with their bodies; keep the title and `## Components` word for word.

### 1.3 — Tests

[tests/manual/manual_test.cpp](../../tests/manual/manual_test.cpp):
- `kGoodService`: drop the `## How it works` heading and paragraph and the `## Troubleshooting` heading and bullet. Fix the line numbers in the expected messages of `manual: checks` (everything moves up 2 lines: e.g. `14: unknown reference 'U9'` → `12: ...`, `4: Components: missing U5` → `2: ...`). The `U1 pin 17` line is appended after the list, so recompute it from the new text.
- Replace the shape check with `# S\n## How it works\n## Components\n` → front message `"2: sections must be: Components"`.
- The "each bullet starts with a `REF`" case becomes `# S\n## Components\nx\n` → `"3: Components: each bullet starts with a `REF`"`.
- User manual: `# Counter\n## What it does\nConnect +5 V to `J1`.\n` passes; `# Counter\n## What it does\nCounts.\n## Connections\n- `J1`: power\n` → front message `"2: sections must be: What it does"` (reported at the first heading); the no-title case becomes `## What it does\nSee `J7`.\n` → `{"1: must start with a # title", "2: unknown reference 'J7'"}`.

[tests/machines/manuals_test.cpp](../../tests/machines/manuals_test.cpp): unchanged, must pass on the new files.

### Fence

- [ ] Six manual files rewritten; `manual_test` and `manuals_test` pass
- [ ] Full build clean; `ctest --preset debug -j 12` passes
- [ ] Commit: `"manuals: phase 1 — short User Manuals, Service Manuals keep roles only"`

---

## Phase 2 — Driver pads in BoardScene

Headless first: the UI marks in phase 3 only read this.

### 2.1 — Interface and construction

[src/view/board_scene.hpp](../../src/view/board_scene.hpp): add `ScenePad` (`component`, `pad`, defaulted `==`), `std::span<const ScenePad> drivers(NetId net) const` (out-of-range id → empty span) and `std::vector<std::vector<ScenePad>> driverPads_; // [NetId]`. Include `<span>` is already there.

[src/view/board_scene.cpp](../../src/view/board_scene.cpp): include `netlist/net_pins.hpp`; at the end of the constructor fill `driverPads_` as in design §3.2 (`componentIndex`, then `padIndex(k, std::to_string(pin.pin + 1))`, skip pins with no pad).

### 2.2 — Tests

`git mv tests/machines/net_panel_test.cpp tests/machines/net_drivers_test.cpp`; update [tests/CMakeLists.txt](../../tests/CMakeLists.txt). Rename the two test cases to `net drivers: ...`. Add, tag `[machines]`:
- `net drivers: crack does not move the mark` — broken_trace.bwp: `scene.drivers(H0)` equals `{U1, pad "14"}`.
- `net drivers: stuck output keeps its mark` — stuck_gate.bwp: `drivers(GRID)` equals `{U12, pad "11"}`.
- `net drivers: pull-up and supply` — dark_led.bwp (counter): `drivers(RESET_N)` is the one R1 pad on RESET_N; `drivers(GND)` is the one J1 pad on GND.

Look NetIds up with `scene.padNet` of a known pad, as the existing test does.

### Fence

- [ ] `BoardScene::drivers` exists; `net_drivers_test` passes
- [ ] Full build clean; all tests pass
- [ ] Commit: `"view: phase 2 — Driver pads of a Net in BoardScene"`

---

## Phase 3 — UI: removals, hover text, Driver marks

All `bw_ui` changes in one commit: they touch the same few files and none is testable headlessly.

### 3.1 — Net panel and Net selection

- Delete `src/ui/net_panel.{hpp,cpp}`; remove `net_panel.cpp` from `bw_ui` in [src/ui/CMakeLists.txt](../../src/ui/CMakeLists.txt).
- [side_panel.hpp](../../src/ui/side_panel.hpp)/[.cpp](../../src/ui/side_panel.cpp): delete the include, `NetPanel net_`, the `Net` section, and "Net" from the class comment.
- [board_view.hpp](../../src/ui/board_view.hpp): delete `net()`, `selectNet()`, `net_`. [board_view.cpp](../../src/ui/board_view.cpp): `lit` = `highlight_`; `route()` drops the `net_` block.
- [main.cpp](../../src/ui/main.cpp): drop both `selectNet` calls (`--probe` setup, click handler).

### 3.2 — Puzzle section and Service Manual window

- `SidePanel`: delete the "When it works" tree node and `ManualView puzzle_`.
- `ManualWindows::draw(bool& open, const Manuals&, BoardView&, const LoadedCircuit&)`, member `user_` only (design §4.5). Delete `kTroubleshooting` and the "(easy mode)" label; all headings default open.
- [toolbar.hpp](../../src/ui/toolbar.hpp)/[.cpp](../../src/ui/toolbar.cpp): delete `serviceManualOpen`, the "Service Manual (H)" button and the `H` key.
- `main.cpp`: delete `Options::serviceManual`; `--manual` accepts only `user`; usage `[--manual user]`; update the `manualWindows.draw` call.

### 3.3 — Hover text

`Tooltip` in `main.cpp`: replace the `ComponentRole` block with the `PartInfo::description` suffix (design §4.6); comment "Board hover text plus the Part's description, or the pin's Datasheet line"; include `parts/catalogue.hpp` if not already reachable.

### 3.4 — Driver marks

`BoardView::draw`: after the hovered-pad ring, draw `kDriverMark` rings `kDriverMarkGap` px outside each pad of `scene_->drivers(*highlight_)` (design §4.1). Constants next to `kHighlight`.

### Fence

- [ ] `grep -rn "NetPanel\|selectNet\|serviceManual\|easy mode" src` finds nothing
- [ ] Full build clean (no unused includes or members warnings); all tests pass
- [ ] `build/debug/src/ui/bw_ui puzzles/dark_led.bwp --manual user --screenshot /tmp/p4c.png --after 0.2s` writes a PNG showing: Puzzle section with title and brief only, no Net section, User Manual window with one paragraph
- [ ] Commit: `"ui: phase 3 — no Net panel or Service Manual window; Driver marks, Part description on hover"`

---

## Phase 4 — Docs, release and user check

### 4.1 — Docs

Edit as in design §5: GLOSSARY (User Manual, Service Manual), GAME_DESIGN (Tools, Presentation), ARCHITECTURE (design netlist row, no-leak rule, no-schematic note, P4c row "done YYYY-MM-DD: …"), COUNTER_DESIGN and LED_CHASER_DESIGN `Manuals` sections. Commit only these paths (the user edits shared docs in parallel).

### 4.2 — Release check

`cmake --build --preset release`; `build/release/src/ui/bw_ui puzzles/sync_bridge.bwp --screenshot /tmp/p4c_rel.png --after 1s` prints ratio 1.00× and ui time no higher than before (≈0.7 ms).

### 4.3 — User check

The user plays one Puzzle per Machine and confirms: User Manual window (U) shows the short text; no Service Manual button; Chip panel shows the role; Component hover shows the Part description; hovering a Trace or pin marks its Driver pad(s); no Net section.

### Fence

- [ ] Docs updated; release 1.00×
- [ ] User confirmed
- [ ] Commit: `"docs: P4c done"`

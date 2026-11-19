# P6o — Shelf and keys — Implementation Plan

Spec: [SHELF_KEYS.md](SHELF_KEYS.md). Design: [SHELF_KEYS_DESIGN.md](SHELF_KEYS_DESIGN.md).

Only `bw_ui` and its ImGui-free `src/view` change, so between phases the checks are the release build, the `src/view` unit tests and screenshots (viewed). No ctest case runs `bw_ui`; no kernel, Part, builder or device code changes, so no debug build (AGENTS.md). The full release suite runs at the end (phase 6).

`UI_TESTS` below stands for `ctest --preset release -j -R "^(focus|machine slot|power cycle|flip corner|board scene|bench places|pencil marks|binder|view):"`.

Screenshots: `build/release/src/ui/bw_ui FILE --screenshot /tmp/p6o-NAME.png --after 1s`, each viewed; the speed ratio and UI ms printed are read on the 110 (LED Computer) and the BW-2001.

## Status

| Phase | Description | Commit |
|-------|-------------|--------|
| 1 | Keys: every key to the Machine; shortcuts, F2, Esc, gold for keys, `--keys` gone; manuals, Machine docs, recipes | `4ca30fc` |
| 2 | Shelf: trays, run switch, Machine slot, double-click pops out, power-off tools never blocked; binder control and home gone | `3c95d58` |
| 3 | Board: the flip corner and `` ` ``; the pencil on the Board only | `eb0c3a7` |
| 4 | Repairs with the power on: the plan, the popup, the cycle | this commit |
| 5 | Bench objects: the binder's height, the brief dragged | |
| 6 | Docs, release check, full release suite | |

## Phase 1 — Keys

Design §1, §7 (manuals, Machine docs, recipes).

1. `main.cpp`: the routing of §1 (the four `on` flags from `textField`; Tab and `` ` `` in the event loop with `dropText`; Cmd to ImGui but the matrix's Cmd+V; the rest dropped unless a text field). `MachineKeys`, `applyKeys`, `toMachine`/`toBench`, the F2 branch, `--keys`, `Options::keysToMachine` and its `main()` check, the Esc switch on `takeEscape` go. `kUsage` without `--keys`. `ImGui::IsKeyPressed(ImGuiKey_Tab)` goes.
2. Remove `src/view/machine_keys.*`, `tests/view/machine_keys_test.cpp`; their CMake lines.
3. `focus.*`: `EscapeStep`, `escape()`; `focus_test.cpp`'s "Esc steps back" case and the `escape(false)` lines of "the pencil is a tool"; comments that name Esc or C, O, V. `main.cpp`: a probe click on bare Board lifts the tip (Esc's `LiftTip` gone).
4. `board_view.*`: `keys()` and its call, `escape_`, `takeEscape`, `setKeypadGold`, `keypadGold_`, `kKeypadMargin`, the keypad frame, `takePress`, `BoardPress`, `press_`.
5. `shelf.*`: the shortcut blocks, `Entry::key`, `kKeyLetter`, the `(X)` in tooltips, `KeysClick`/`takeKeysClick`, `boardKeys`, `keyboardTyping`, the keyboard frame, the F2 hint. The keyboard drawing on the BW-1 (no keyboard, Board keys) is inert like any absent device until phase 2 replaces it.
6. `tv_window.*`, `knob_window.*`: `gold` and the return value; the knob tooltip. `keyboard_window.*`, `matrix_keyboard_window.*`: no gold frame, `draw` returns `void`; comments without F2. `window_order.hpp`: `BringToFront`'s comment.
7. `keyboard_capture.hpp`, `matrix_capture.hpp`, `board_keys.hpp`: comments (on while the Machine has a keyboard and no text field is edited; no F2).
8. `analyzer_window.*`, `scope_window.*`: the Esc blocks, `focused()`, `focused_`; "Clip, then…", "Hook, then…".
9. `bench.hpp`: `kFocusGold`'s comment.
10. Manuals and docs: `machines/045-knob-meter/manuals/user.md`, `machines/070-tennis/manuals/user.md`, `machines/120-bw1/manuals/user.md`, `machines/130-bw2001/manuals/user.md`, `machines/140-bw80/manuals/user.md`, `machines/120-bw1/docs/BW1.md`, `machines/140-bw80/docs/BW80.md`, `docs/recipes/ADD_BOARD_DEVICE.md`, `docs/recipes/ADD_CONNECTOR_DEVICE.md`, `docs/recipes/ADD_INSTRUMENT.md` (its shelf key, `focused()`) (design §7). `grep -rn "clicked on the knob box\|click the box" machines/*/manuals` finds nothing. `grep -rn "F2\|--keys" src machines/*/manuals machines/*/docs docs/recipes` shows only Nets.

**Fence**: release build clean (no warnings); `UI_TESTS` pass; `ctest --preset release -j -R "manual|manuals"` (the manuals' link checks) pass; screenshots viewed: the BW-2001 with `--type` and the keyboard out (no gold), the BW-1 (no keypad frame), Tennis (no knob gold), the LED Computer's shelf (no letters). Commit: `ui: every key goes to the Machine; the bench shortcuts, F2, Esc and the keys' gold gone (P6o phase 1)`, its body naming the tests run (`UI_TESTS`, the manuals' checks).

## Phase 2 — Shelf

Design §2.

1. `src/view/machine_slot.*` (new), `tests/view/machine_slot_test.cpp`: `ClickSlot` (any away → BringOut; all out, one behind → ToFront; all out, all in front → PutAway; one object each way), `PopAll` (all popped → in; one not → out), `MachineSlotCells` (0, 1, 2 devices).
2. `bench_places.*`: `ReturnPlace`, tests in `bench_places_test.cpp` (inside: unchanged; past the right or bottom: moved in by the margin; larger than the bench: at the margin).
3. `bench.*`: `TogglePop`, `benchAt_`, pop-in to `ReturnPlace`; `SendHome`, `PopOut`, `PopIn`, `home_` removed. `window_order.*`: `InFrontOf`. `ADD_INSTRUMENT.md`: no `SendHome` or home double-click; the double-click pops out.
4. `manual_window.cpp`: `PopControl` and its constants removed.
5. `shelf_drawings.*`: the new `Item`; `RunSwitch` drawing (replaces `Run`, `Speed`); `Machine` drawing from the slot's devices; `Layer`, `Fit` drawings removed; `ItemLook` (`run`, `powered`, `slot`).
6. `shelf.*`: `Tray`, `Entry`, the trays drawn, the right-aligned Machine tray, the hint between; the run switch (`RunSwitch`, `PowerOn`); the slot's click and double-click; instruments' and binder's double-click (`TogglePop`); power-off tools never grayed or red, their tooltip; the hint without "Power off first".
7. `main.cpp`: `RunSwitch run` in place of `Speed speed`; start-up through `PowerOn` (`--run pause` through `powerOn(seed, 0.2 s)`, the switch left at `Pause`); `--run`, `--away`. The keyboard window's own `×` still clears `keyboardOut`.
8. `GLOSSARY.md`'s Shelf, Machine Slot, Equipment written now (the trays' look may move in phase 6).

**Fence**: release build clean; `UI_TESTS` pass; screenshots viewed, each Machine's shelf: Counter, Chaser, Dice, Stopwatch, Knob Meter, Heartbeat, LED Computer, Test Pattern, Bouncing Ball, Tennis, Message, Typewriter, BW-1, BW-2001, BW-80 (the five slot drawings, the trays); Tennis with `--pop knob` and with `--away tv`; the BW-2001 with `--pop keyboard`, with `--away keyboard`, with `--run pause` and with `--run max`. The trays' colours shown to the user (stop only if they ask for a change; it goes in phase 6 otherwise). Commit: `ui: the shelf in trays, the run switch, the Machine slot; a double-click pops out (P6o phase 2)`, naming the tests run.

## Phase 3 — Board

Design §3.

1. `src/view/flip_corner.*` (new), `tests/view/flip_corner_test.cpp`: `FlipCornerSide` (8 mm at high zoom, the floor at low), `OnFlipCorner` (the corner itself, on the diagonal, just inside it, outside the bounds, the opposite corner); and a case that loads every Machine's Board and finds no pad within the 8 mm dog-ear (the design's size check; `flip corner:` names, so it runs with `UI_TESTS`).
2. `board_scene.*`: `InsideOutline` (the anonymous `Contains`, made public); a case in `board_scene_test.cpp` (a square and an L-shaped outline; a point in the notch).
3. `board_view.*`: the corner drawn last on the Board, both looks; its click flips and makes no `BoardClick`; `buttonAt()` and the right-click give nothing on it; its tooltip.
4. `main.cpp`: the pencil's click and right-click only inside the outline; `--side`.

**Fence**: release build clean; `UI_TESTS` pass; screenshots viewed: the Counter from the front and with `--side back`, the Counter `--look plain`, the BW-2001 zoomed out (the floor). Commit: `ui: the flip corner on the Board; the pencil marks only on the Board (P6o phase 3)`, naming the tests run.

## Phase 4 — Repairs with the power on

Design §4.

1. `src/view/power_cycle.*` (new), `tests/view/power_cycle_test.cpp`: `StepFor` for the eight combinations of (would repair) × (powered) × (don't ask).
2. `src/ui/power_cycle_popup.*` (new), `src/ui/CMakeLists.txt`.
3. `main.cpp`: `RepairPlan` built by the four tools (the iron only inside the outline); `apply`, the `Ask` and `Cycle` paths (the cycle at the click-handling point, OK's on the next frame through `cycleNext`), `powerOnNext` before the event loop; the popup drawn last; `--ask`.
4. The iron, cutters and wire no longer refuse on their own (the `session.powered()` checks in the click switch go: the plan decides).

**Fence**: release build clean; `UI_TESTS` pass; screenshots viewed: a Puzzle with `--ask swap:U1` (the popup), and the same Puzzle with `--repair` applied powered off as before (unchanged). By reading: the wire's first pin with the power on (no popup), a cut on bare Board (refused, no popup), the iron off the Board (nothing), latched buttons released by the cycle. Commit: `ui: the power-off tools with the power on, through an OK/Cancel popup that power-cycles (P6o phase 4)`, naming the tests run.

## Phase 5 — Bench objects

Design §5.

1. `bench.*`: `KeepAspect`'s `stretch`.
2. `manual_window.*`: `stretch_`; `--binder-height`.
3. `brief_window.*`: `BriefNote`, the drag; `--brief`. If it takes more than about ten lines, it is dropped: this plan's notes and the spec say so.

**Fence**: release build clean; screenshots viewed: the binder at `--binder-height 160%` on the bench and popped out (`--pop manual`), its tabs and pages; a Puzzle's brief at `--brief 300:200`. Commit: `ui: the binder taller or shorter, the brief dragged (P6o phase 5)`, naming the checks.

## Phase 6 — Docs and release check

1. `GLOSSARY.md`: Focus, Typing, Pencil Mark, Active Layer, Flip Corner, Brief, and Shelf's final wording (design §7).
2. `ARCHITECTURE.md`: the P6o row done, with the date and links.
3. Release check: the 110 with every instrument (`--clip`, `--hook`, `--meter`): 1.00×, UI about 1 ms; the BW-2001 with its keyboard out.
4. Full release suite (`ctest --preset release -j`).
5. This plan cut to the status table and notes.
6. Handed to the user to check (spec Tests, by reading and by the user): typing with the keyboard put away; Tab, `` ` ``, Cmd+Q and Cmd+V on the BW-2001; a text field in the analyzer; the wire cancelled by a second click on its first pin; dragging the binder's bottom edge and the brief; the popup's OK, Cancel and "Don't ask again"; the trays' colours.

**Commit**: `docs: P6o done (P6o phase 6)`.

## Implementation notes

- Spec and design (2026-11-19): the user dropped home (an object moved stays moved), made the switch jump to `1×` at power on, and removed the binder's pop-out control. The Message's `F2` is a Net: three User Manuals, not four.
- Review (2026-11-19), fixed: the flip corner 8 mm (10 mm covers the Typewriter's `SW1.2` and Tennis's `J4.4`) and winning at the press too; the popup's OK cycles on the next frame so an unpowered frame always happens, the power-on before `session.tv()`; `--run pause` through `powerOn`'s `pauseAt`, the switch kept at `‖`; the binder's stretch only from an active top or bottom border, its minimum height lowered; the Knob Meter's and Tennis's manuals and `ADD_INSTRUMENT.md`; `focus_test.cpp`'s pencil case; eight combinations; `--away` for the put-away screenshots, the BW-2001 popped out, the user's checks in phase 6. Also: a probe click on bare Board lifts the tip (Esc's last step), the iron acts only inside the outline, every cutters plan refuses as today, "Don't ask again" counts only with OK, the brief's drag spares its ×, `Contains` reused, `focused()` and the windows' return values removed, `TogglePop` without a first size, the right tray's divider, `RunSwitch` in `shelf_drawings.hpp`, the dead-key risk, the commit messages naming their tests.
- Phase 1 (2026-11-19): the event loop takes Tab and `` ` `` first (by the character, `dropText` for their text), lets Cmd combinations through to ImGui but the matrix's paste, and drops every other key unless a text field is edited; `applyKeys` runs again right after `textField` updates, before the captures' `sync`. The BW-2001 runs at its known 0.33× in `bw_ui` (P5z: 0.36×).
- Phase 2 (2026-11-19): every Machine's shelf viewed (the five slot drawings: a grayed monitor on 010–040, 100–120; the knob box on 045; the monitor on 050, 060, 080; monitor and knob box on 070; monitor and keyboard on 090, 130, 140). The pause sign is drawn as two bars (the UI font has no `‖`). `--run pause` stops at 0.2 s, the BW-2001's screen still booting there: enough to see the switch.
- Phase 3 (2026-11-19): the flip corner is 4 mm, not 8: the pad test found the Typewriter's `SW1.2` and Tennis's `J4.3`, `J4.4` under 8 mm (a point is on the dog-ear when its distances to the two edges sum to at most the side; the review's 6.7 mm was that sum). 4 mm clears every pad; the floor is 24 px. Drawn as the corner in the other side's copper colour, a light fold line and a shadow inside it.
- Phase 4 (2026-11-19): the plan's tries replace the iron's resolder-then-mend chain; every cutters plan refuses "Cut: click a trace or a wire" if none is Done, as before. `--ask` opens the popup at the bench's centre; the checkbox's box is drawn lighter (ImGui's frame colour vanished on the popup's dark). OK's cycle and "Don't ask again" are checked by reading here and by the user (phase 6): a screenshot cannot click.

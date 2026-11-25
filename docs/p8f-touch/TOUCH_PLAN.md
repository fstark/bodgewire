# P8f — Touch on tablets — Implementation Plan

Spec: [TOUCH.md](TOUCH.md). Design: [TOUCH_DESIGN.md](TOUCH_DESIGN.md) (its sections cited as D§n).

On the `touch` branch, in its own checkout (`.claude/worktrees/touch`). Every phase: `clang-format` on the C++ files touched; only the tests that reach the change; the web build; one commit. The unit tests' pattern, called `UNIT` below:

```
ctest --preset release -j -R "^(fingers|number pad|view|time view|bench options|bench places|binder|shelf layout): "
```

`main` is brought into `touch` before the first push (a rebase: nothing is published yet) and before each sitting (a merge): its web fixes for the iPad must be in the `/touch/` page. The user's sittings on the iPad (after phases 2, 5 and 8) hold up no phase: what they find is fixed in the phase then open.

## Status

| Phase | Description | Commit |
|---|---|---|
| 0 | Spec, design and plan, reviewed | `b07d734` |
| 1 | Plumbing: the fingers, their glue, `--touch` and the scripted fingers | `0f46b16` |
| 2 | The Board's zoom and picks; the view buttons; `/touch/` on Pages | `91fba8b` |
| 3 | What has no control today: Stop, Hold, the cursor, the flag, pin / bus | `9cddc7e` |
| 4 | Sizes and numbers: the style, the stepper, the choices, the number pad | `159506e` |
| 5 | The loupe and lit-first repairs | `5ea375a` |
| 6 | Pages: drag to scroll, links at the release, "show" | |
| 7 | Objects and the Launcher: grip, pinch, fill, the drags, the narrow Shelf | |
| 8 | The web page's manifest, the turn notice; docs; the full suite; the row done | |

## Phase 0 — Docs

1. The three documents; the review's must-fixes in them.
2. `docs/ARCHITECTURE.md`'s row and `docs/architecture-review/ARCHITECTURE_REVIEW.md` point to `p8f-touch/TOUCH.md` (D§11).
3. `touch` rebased on `main`.

Commit: `docs: P8f Touch on tablets, its spec, design and plan, reviewed`

## Phase 1 — Plumbing

1. `src/view/fingers.hpp/.cpp` with `Fingers`, `FingerScript`, `ScriptEvents`, and `tests/view/fingers_test.cpp` (D§1).
2. `Options`: `--touch`, `--tap`, `--double-tap`, `--hold`, `--drag`, `--pinch`, `--window`, with their cases in `bench_options_test.cpp` (D§2).
3. `src/ui/touch.hpp/.cpp`; `main.cpp`: `TouchInit`, `TouchStart`, `TouchEvent`, `TouchNewFrame` in both loops; the window's size from `--window`; `touch` and `window` kept for a bench opened from the Launcher (D§2).
4. The cancel: `BoardView::input` and `TimeCanvas::read` drop the press on `Touch().cancelled`; the time screens' slop is the pointer's (D§3, D§6).
5. `TouchTypingZone` and `NoFingerPadding` in both keyboards' windows (D§2).
6. `src/ui/gestures.hpp`'s pointer to `TOUCH.md` (D§11).

Fence:

- `UNIT`; the same pattern on the debug preset (`cmake --build --preset debug --target bw_tests` first: the checkout has no debug build).
- Screenshots: the Counter with `--tap` on a pin at `--after 1s` (its Net lit, its tooltip above) and at `--after 4s` (nothing lit); the same with `--touch` alone (as without); the BW-2001's keyboard after two `--tap`s on two keys (both typed on the TV).
- The web build.
- Commit: `ui: fingers, the one pointer they give ImGui, --touch and scripted fingers (P8f phase 1)`

## Phase 2 — The Board's zoom and picks

1. `Viewport::frameUpTo` with its test; B1, B2, B3, B7 in `board_view.cpp`; no highlight while two fingers are down (D§3).
2. `kViewLane` and the two first places, `bench_places_test.cpp`'s default-places case rewritten; `view_buttons.hpp/.cpp` with Fit, Look, Copper, Flip; `copper_` (D§3, D§4).
3. `Binder::frame` offering the Contents tab, with its test; `ManualWindows::draw` (D§3).
4. I1 in `TimeCanvas::read` (D§6).
5. The page's hold: `shell.html`'s `user-select`, `-webkit-touch-callout`, `overscroll-behavior` (D§10, the first of P4).
6. `web.yml` building both branches and copying the manifest when there is one (D§10): the same file committed here and, by this session, in the primary checkout on `main` (that path alone). Pushing `main` and `touch`, and the workflow's run, wait for the user's word.

Fence:

- `UNIT`.
- Screenshots: the BW-2001 at fit and after `--pinch 640:400:3`; the view buttons, the plain look and the back side lit; a chip framed by `--double-tap`; a refusal above the pointer; a memory chip selected with the binder out (the Contents tab); a scope's screen after `--pinch`.
- The web build.
- Commit: `ui: the Board zoomed by a pinch, a chip framed by a double-click, the view buttons (P8f phase 2)`
- Then: `main` merged; the first sitting's list for the user.

## Phase 3 — What has no control today

1. L1: Stop on the making's plate (D§9).
2. D1: Hold among the view buttons and on the matrix keyboards' cases (D§3, D§4, D§8); D4: a clicked key typed in the Machine's time.
3. I2, I3: the placed cursor, the flag's click and size (D§6).
4. I9, I10: pin / bus, the group's two choices (D§7), with plain buttons until phase 4's controls.

Fence:

- `UNIT`.
- Screenshots: the Launcher with `--make` (Stop); the BW-1 with Hold lit and a key latched by `--tap`; the BW-2001's keyboard with HOLD; a scope and an analyzer with `--hold` on their screens (the cursor placed, `dt` shown) and the wider flag; the analyzer's pin / bus; a group's two choices.
- The web build.
- Commit: `ui: Stop, the Hold switch, a finger's cursor, the Marker's flag, pin / bus (P8f phase 3)`

## Phase 4 — Sizes and numbers

1. `src/view/number_pad.hpp/.cpp` with its tests; `SnapToEdge` with its tests (D§5, D§6).
2. `src/ui/controls.hpp/.cpp`: `ApplyStyle` (M2 with it), `Stepper`, `Choice`, `ChoicePopup`, `NumberField`; `textField` in `RunBench` (D§5).
3. The scope, the analyzer and the Multimeter on them; rows and overview at 28 px; their columns and sizes measured again; I4, I5 (D§6, D§7).
4. M4, M6: the pinout's rows, the binder's small buttons, the Contents page's settings (D§8).
5. O5, D2, L3, L7, L9: the close disc, the TV's hits, the chevrons, the popups, the forget hint (D§8, D§9).

Fence:

- `UNIT`.
- Screenshots, every window once: each Instrument at its normal and minimal sizes; a stepper's list open; a Trigger's strip open; the number pad open on a pattern and on the position; the binder's chip page and Contents page; the power-cycle question; the Thank-you Card; the TV; the Launcher; each Machine's bench (fourteen shots) for what the style moved.
- The web build.
- Commit: `ui: one size of controls, steppers, choices and a number pad for the dropdowns and fields (P8f phase 4)`

## Phase 5 — The loupe and lit-first repairs

1. `Viewport::magnified` with its test; `BoardView::drawBoard`; the loupe (D§3).
2. `BoardClick::finger` and `loupe`; `SameRepair`; `Armed` in `RunBench`; `setArmed`; the question above the pointer, kept inside the window (D§3).
3. T2: a wire by the loupe (D§3).
4. D3: the TV's glass (D§8).

Fence:

- `UNIT`, on the release and the debug presets.
- Screenshots: the BW-2001 with `--hold` on a pin (the loupe, the pick lit, its name) and near the panel's top (the loupe below); the cutters with a `--tap` on the Shelf's Power (off), then one on a Trace (armed, "Cut here: tap again", at `--after 1.3s`) and a second (cut, in the Repairs log, at `--after 2s`); the same with the power on (the question, above the pointer); the puller with two taps on two places of one chip; the TV with `--hold` on its glass.
- The UI's time a frame with the loupe up (the screenshot's `ui` ms) against without.
- The web build.
- Commit: `ui: the loupe on a hold, repairs lit first for a finger, the TV's glass a control (P8f phase 5)`
- Then: `main` merged; the second sitting's list.

## Phase 6 — Pages

1. `DragScroll` in the binder's pages and the Contents page's views; those children `NoMove` (D§5): M1, O1.
2. M3: `TextFlow::text` clicking at the release, a finger's hit a line high (D§5).
3. I13: the vertical drag in `TimeCanvas::read`, the analyzer's rows scrolled by it and by `DragScroll` on the labels (D§5, D§6).
4. M5: "show" on the pinout's pins; `TargetHits::frame`; the binder's width (D§8).
5. L4: the post-its' row and a long post-it's text (D§9).

Fence:

- `UNIT`.
- Screenshots: the binder's Service Manual after `--drag` up (scrolled, the binder where it was), after a `--drag` begun on a link (scrolled, no link followed), after `--tap` on a link (followed) and after `--drag` on its spine (moved); the analyzer's rows after a vertical `--drag` on its lanes (scrolled, no Marker); the chip page with "show"; the Launcher's row after `--drag`.
- The web build.
- Commit: `ui: pages scrolled by a drag, links followed at the release, the binder moved by its cover, "show" on a pin (P8f phase 6)`

## Phase 7 — Objects and the Launcher

1. O6, O4: the first size on a small bench; `NextPreset` with the full size and its tests; the fill where there are no desktop windows (D§8).
2. O2, O3: `CornerGrip`, `PinchResize` (D§5, D§8).
3. L2: the Machines' lane dragged (D§9).
4. L5: `ShelfWidth`, the narrow metrics, with their tests (D§9).
5. L6: the wording (D§9).

Fence:

- `UNIT`.
- Screenshots: the BW-2001's bench in `--window 1080:810` (the Shelf whole, the objects inside the bench); an Instrument at its full size; the TV after `--pinch`; the grips; the Launcher after `--drag` on its lane.
- The web build.
- Commit: `ui: objects resized by a grip and a pinch, the bench filled, the Launcher's lane dragged, the narrow Shelf (P8f phase 7)`

## Phase 8 — The web page, docs, finish

1. `manifest.json`, the home-screen tags, the notice for phones only, `CMakeLists.txt`; the app's turn notice (D§10): the rest of P4, L8.
2. Docs (D§11): GLOSSARY, AGENTS.md, ADD_INSTRUMENT.md.
3. The full release suite.
4. `main` merged; the third sitting's list; ARCHITECTURE's row; this plan cut down to its table and notes.

Fence:

- `ctest --preset release -j` (the whole suite).
- A screenshot with `--touch --window 820:1180` (the turn notice).
- The web build.
- Commit: `web: the home-screen app, the turn notice; docs: P8f done (P8f phase 8)`

## Implementation notes

Phase 1:

- The glue's `TouchNewFrame` runs after the backend's `NewFrame` and tells ImGui its pointer again every frame while the fingers have it: on a desktop the SDL3 backend queues the real cursor's place each frame. With that, a scripted tap gives the same screenshot wherever the real mouse is.
- A scripted finger turns `--touch` on, so the real mouse cannot take the pointer back during a shot (its motion with no button is dropped).
- A scripted pinch's fingers start 200 px apart (the design said 60): the 10 px they must pass first then costs 5 % of the factor, not 17 %.
- The keyboard's check is a `--hold` on a key, not two taps: the BW-2001 runs at about a third of real time here, so a 50 ms tap is 16 ms for the Machine, under its keyboard scan. A quick tap on a key of a slow Machine can be missed, with the mouse as with a finger; on an iPad, slower still, it will matter. To look at with the devices (phase 3).

Phase 2:

- The view buttons' column stands over the Board's right margin: on a Board fitted to the panel's width (the Counter) its top button covers the Flip Corner. Flip is in the column, so nothing is lost; the fit could leave the lane free if it bothers on the iPad.
- A Board button's reach for a finger gives way to any other pick there, but not to the button's own pads, which lie under its cap.
- The page's hold (no selection, callout or bounce) is in `shell.html` from this phase, for the first sitting; the manifest waits for phase 8.

Phase 3:

- `main` (its `web.yml`) and `touch` were pushed after phase 2, on the user's choice of the `/touch/` page (2026-11-25); `touch` rebased on `main` just before, so the hashes above are the pushed ones.
- D4 (a clicked key typed in the Machine's time) joined this phase from phase 1's finding.
- The Hold button shows only on a Board with a push button or a key; the keyboards' HOLD is a slide switch on the case, at the right of the BW-2001's strip and of the BW-80's bottom lip.
- The pin / bus switch and the group's two choices are plain buttons here; phase 4 gives them the shared controls.

Phase 4:

- A line that holds a stepper has every control 44 px high (`Choice(…, kBig)`, `BigText`): two heights on one line could not be centred with ImGui's `SameLine`.
- Measured again: the analyzer's label column 340 → 360 px (its x at 324), its normal size 940 × 440 → 940 × 520, its minimal 560 × 260 → 820 × 340 (its first header line needs about 800 px); the scope's 680 × 460 → 680 × 520 and 420 × 300 → 600 × 380; the Multimeter's 320 × 200 → 320 × 220.
- The scope's − and + still zoom about the screen's centre (one step of the stepper); a value picked from its list sets the timebase alone.
- A number field's value is confirmed whenever something was typed, the same value too (Go to twice).
- The benches' sweep was the four that hold each kind of object (the Counter's Puzzle with its Brief, the Message, the BW-2001, the analyzer and scope on the Counter), not all fourteen: the style reaches framed controls only, and a bare bench has none.

Phase 5:

- The loupe costs about 1.3 ms a frame on the BW-2001 (UI 2.3 ms against 1.0 ms, release, native): a second pass of the Board's drawing, culled to its square.
- The loupe's name is above it, not under it: under it is where the finger is.
- While a repair is armed the Board's tooltip is not shown: the armed action's name is in its place.
- A double-click on the TV's glass still pops the TV out (the glass, now a control, asks for it itself).
- The second tap is "on the armed target" when it lands within the finger's reach of the first, or when the tool names the same part again (`SameRepair`: a chip, a pin, a wire's two pins).

Phase 6:

- A drag scrolls only where there is something to scroll: on a page that fits, or on the analyzer's rows when they all fit, a press and a drag are what they were (the analyzer's labels still move its window then). The binder's page never moves the binder (O1).
- The lanes scroll by the canvas (no glide), the labels and the pages by `DragScroll` (a glide of about 0.25 s); two fingers scroll both through `DragScroll`.
- With rows that scroll, the analyzer's pan waits for the drag to pass the slop and choose its direction, then catches up at once: a pan no longer begins at the first pixel there.
- A link's click needs its press on the same link (a press elsewhere released on a link follows nothing), and a press a second finger cancelled follows nothing (`PressCancelled`).
- "show" is on the digits' pinout too: above its top row, below its bottom one.
- A pin's row in a table sat 7 px under its "show" button (ImGui lowers a row's text to a framed item's baseline): the row's baseline is put back to 0.
- The Launcher's row follows the finger, without a glide after the lift; a drag on the lane lets go of the post-it's press, for the mouse too (a click that slides more than 6 px no longer selects).
- The analyzer's hint says "drag: pan or scroll".

# P8f — Touch on tablets — Implementation Plan

Spec: [TOUCH.md](TOUCH.md). Design: [TOUCH_DESIGN.md](TOUCH_DESIGN.md).

Built on the `touch` branch and tried on the user's iPad from the page under `/touch/`; merged to `main` on 2026-11-25, once it worked there. The unit tests that reach it:

```
ctest --preset release -j -R "^(fingers|number pad|view|time view|bench options|bench places|binder|shelf layout|launcher lanes): "
```

## Status

| Phase | Description | Commit |
|---|---|---|
| 0 | Spec, design and plan, reviewed | `b07d734` |
| 1 | Plumbing: the fingers, their glue, `--touch` and the scripted fingers | `0f46b16` |
| 2 | The Board's zoom and picks; the view buttons; `/touch/` on Pages | `91fba8b` |
| 3 | What has no control today: Stop, Hold, the cursor, the flag, pin / bus | `9cddc7e` |
| 4 | Sizes and numbers: the style, the stepper, the choices, the number pad | `159506e` |
| 5 | The loupe and lit-first repairs | `5ea375a` |
| 6 | Pages: drag to scroll, links at the release, "show" | `7842c2a` |
| 7 | Objects and the Launcher: grip, pinch, fill, the drags, the narrow Shelf | `1187833` |
| 8 | The web page's manifest, the turn notice; docs; the full suite; the row done | `a2d78cb` |

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

Phase 7:

- `--one-window` gives the native build the web's single window (no desktop windows), so the fill can be seen in a screenshot: `bw_ui … --one-window --double-tap X:Y`.
- An object's first place is moved in as well as its first size shrunk (`ReturnPlace`): at 1080 × 810 the binder's default place put its foot under the bench's edge.
- The fill had to stop the window's move: a double-click's second press begins one at that frame's end, and the move put the window back where it was a frame after the fill.
- An Instrument's size button on the bench goes minimal, normal, full; full is the same fill, so the next press gives the minimal size where the Instrument was. In its own desktop window the button keeps its two sizes.
- The Machine slot's double-click with no desktop windows fills the bench with one object, the slot's first (the TV when there is one).
- A pinched object stays inside the bench and grows no larger than it; its centre is kept from the size it really got, so one held at its smallest size does not creep.
- ImGui's own corner grip still answers (about 14 px) but is no longer drawn: `CornerGrip` draws the one seen. In the analyzer and the scope the grip's 44 px item stops at their rows' and screen's child window: it answers on the footer's height there, and its full width.
- The lane's drag uses the dragged picture's own pitch (`PictureLeft` either side of the centre), so that picture follows the pointer; a drag shorter than half the way settles back, the selected post-it kept.
- The Shelf's widths by the constants: the BW-2001's 1,074 px, 954 px with the narrow cells; a 1080 px window leaves 1,064.
- The pencil's tooltip no longer names shift- or right-click (they still clear a mark; its taps go round to none). The Shelf's tooltips say "the bench filled, or back" for a double-click where there are no desktop windows.

Phase 8:

- The full release suite: 1,686 of 1,686 in 98 s (2026-11-25), with the main checkout's `external/` linked into this one for the run (the 6502's and the Z80's test data are not in git).
- The turn notice is the app's own (a card on ImGui's foreground list over a dimmed screen), shown while a finger is the pointer and the window is taller than wide; it takes no input. A desktop window made tall with the mouse never shows it.
- The page's notice is for phones only now (a coarse pointer and a screen under 600 px on its shorter side), reworded; `main`'s fix that keeps it in view over SDL's window (`ed63d1b`) comes with the merge.
- `manifest.json` has no icon: a home screen shows the page's picture. An icon is for a later row.
- The user's check on the iPad (2026-11-25, after this phase): "it works very well", with a few rough edges not listed yet; the sizes here stay a first setting until they are. Left for a later row, as the spec's "Later" says: fingers that keep what they hold (two knobs at once), a glide after the Launcher's drags.


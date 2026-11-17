# P6f — TV controls

Roadmap step P6f ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the TV object of P6d ([EQUIPMENT_OBJECTS.md](../p6d-equipment-objects/EQUIPMENT_OBJECTS.md)) redrawn in the look of a small 9" monochrome monitor of the late 1970s, the Sanyo VM-4209 (its look, not its name or badge), with working controls; among them the picture's position, P7's note on the BW-2001's off-centre picture. Design: [TV_CONTROLS_DESIGN.md](TV_CONTROLS_DESIGN.md). Plan: [TV_CONTROLS_PLAN.md](TV_CONTROLS_PLAN.md). Recipe: [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md) (UI layer: a case window). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

After P6d the TV is a dark case with the picture across nearly its whole width, a green pilot lamp following the Machine's power and two knobs that do nothing. The BW-2001's text sits right of centre (TV columns 144–463 of 512): the Tv starts a line at the drive's edge and shows a fixed window, where a real monitor's picture is placed by its controls, set once (P7's note, the user 2026-11-15). P6b asked for the TV's position controls; the user chose the VM-4209 as the model (2026-11-17) and gave a photo of one.

## The real set (from the user's photo)

A near-square case (about 0.96 high for its width): a cream metal shell wrapping the top and sides, a charcoal front; the 4:3 screen high and centred, its corners rounded, in a recessed bezel; along the bottom a framed control strip with four chrome-capped knobs labelled `H-HOLD`, `V-HOLD`, `BRIGHT`, `CONTRAST`, a red pilot lamp, and a `POWER` slide switch with `ON` at its left and `OFF` at its right. A badge (`SANYO`) top left and the model (`VM 4209`) on the strip: not drawn.

## User decisions (2026-11-17)

| Question | Decision |
|---|---|
| The look | the VM-4209's, from the photo: cream shell, charcoal front, the screen high and centred, the control strip along the bottom, no badge or model name |
| The first two knobs | `H-POS` and `V-POS`, moving the picture. Real hold knobs set the line and frame oscillators' free-running speed: in our Tv they could only make the picture roll or tear, never centre it, and no Machine needs them, so they are not modelled |
| `BRIGHT` and `CONTRAST` | they work, on the drawing only (the Tv device unchanged); at their marks the picture is today's |
| `POWER` and the lamp | the monitor's own switch, ON at start: OFF blanks the screen and puts the lamp out. The lamp no longer follows the Machine's power |
| The picture's starting position | the same on every Machine (centred knobs); the player adjusts, for the run. The BW-2001 starts right of centre as today |
| Hold knobs | not now; kept for later: a computer whose signal is not perfect, or a monitor not exactly calibrated at start |
| Overscan | the screen shows what a real CRT shows, the frame's edges hidden (the BW-80's sync junk at the left, its dotted rows at the top); a hidden trimmer on the case, unlabelled, shows the whole frame (underscan) |
| The early Machines | their pictures start at the sync edge, so the real window cuts them (the Typewriter's first text row, the Ball's and Tennis's left 16 columns, the Test Pattern's top): kept, the player can move the picture; a later roadmap row gives them a back porch and a top margin |

Decided while designing (2026-11-17), for the user to overrule:

- **Turning a knob**: a vertical drag or the mouse wheel, as the paddle's knob (its rates); a double-click puts it back at its mark (its starting turn). A tooltip names it with its turn in %. No keyboard keys (the arrows belong to the paddles and the keyboards).
- **Ranges**: `H-POS` and `V-POS` move the picture up to a quarter of the screen's width or height either way (clockwise right, and up). `CONTRAST` is a gain from 0 to 4/3, 1 at its mark (75 %); `BRIGHT` a black level from −0.1 to +0.3, 0 at its mark (25 %).
- **The visible window** (measured 2026-11-17 on each Machine's frame): Tv columns 80 to 496 (the first 10 µs of a line, sync and back porch, and the last 2 µs hidden) and rows 20 to the frame's last painted row (at most 320), from the last locked frame (the window keeps still while the Tv is unlocked): the vertical retrace's lines hidden, and the bottom at the frame's last line, as a monitor whose vertical size is set for the computer (the BW-2001's 260 lines and the BW-80's 312 both fill the height). Shown at 4:3. Before a first frame, 320. Underscan: the whole 512 × 320 frame at 4:3, as before P6f.
- **The picture** is the whole frame drawn so that the window fills the screen; moved, the frame's hidden edges come into view on one side and the other side's go off. Where the frame does not reach, black. With the monitor OFF the screen is dark glass.
- **The trimmer**: a small round screwdriver slot in the front below the strip's right end, unlabelled, the front's colour; a click toggles underscan; its tooltip `UNDERSCAN trimmer: the whole frame` (or `… the picture as a real monitor shows it`). Kept for the run.
- **Size**: the case is 0.96 high for its width (was 0.84), its screen 0.79 of the width (was 0.88). The first width stays 440, so the picture is 348 px wide (was 387); the player can enlarge it. The keyboard's default place, under the TV, moves down 53 px: on the 1264 × 716 bench the Typewriter's keyboard comes out about 571 wide (was 640), the BW-80's about 455 (was 552), the BW-2001's unchanged. At least 380 wide (labels 9 px).
- **The controls keep their turns for the run**, the TV out or away.
- **`--tv hpos:N%,vpos:N%,bright:N%,contrast:N%,off,underscan`** (any of them, any order): the controls at start, for screenshots and checks; a usage error on a Machine with no TV.
- **The shelf drawing**: the same set small: a cream case, a dark front, the screen, a strip with four dots and a red lamp.

## What is new

1. **TV controls** — `src/view/tv_controls.{hpp,cpp}` (ImGui-free, unit tested): the four knobs' turns, the switch and the trimmer, their marks, the visible window, the picture's shift, the tone (black level and gain) and a cell's grey, `--tv` parsing.
2. **The frame's line count** — `TvPicture::lines` (the completed frame's lines, from `Tv::completedStats()`), set by `TvConsumer`: the window's bottom.
3. **The tone on upload** — `TvView` keeps the last frame and uploads it through the tone, again when the tone changes.
4. **The monitor** — `src/ui/tv_window.{hpp,cpp}` drawn as the VM-4209: shell, front, screen with the window and the picture moved and clipped, the strip, knobs, lamp, switch and trimmer, each live.
5. **Shelf and CLI** — the TV's shelf drawing; `--tv`.
6. **Docs** — GLOSSARY (**Equipment**), ADD_CONNECTOR_DEVICE (the monitor's controls), ARCHITECTURE (P6f done, P7's note answered, a row for the early Machines' margins).

Below the UI only `TvPicture` gains a field: the Tv device, the Session, engines, Parts, Machines and Puzzles are untouched.

## Tests

- **TV controls** (`tests/view/tv_controls_test.cpp`): at the marks the grey is today's (`lround(clamp(cell) · 255)`) for cells across 0..1 and beyond; turns clamped to 0..1; a reset puts a knob back at its mark; the shift is 0 centred, ±0.25 at the ends, clockwise right and up; the tone's ends (`CONTRAST` 0: all black at `BRIGHT`'s mark; `BRIGHT` 100 %: black at 0.3); the window: real (80–496, 20 to the line count + 1, 320 before a frame or past it) and underscan (the whole frame); `--tv` parsing: each name, `off`, `underscan`, several, and errors (unknown name, no `%`, above 100 %, negative, exponent, empty item).
- No UI tests exist: dragging the knobs, the double-click, the switch and the drag on the case are checked by reading the code, by screenshots and by the user.
- **TV consumer** (`tests/runner/` where `TvConsumer` is tested): a published picture carries its frame's line count.
- **Screenshots** (release, viewed): the Test Pattern with the TV as it comes out; the BW-2001 at start (right of centre) and with `--tv hpos:N%` (centred, N measured); the BW-80 normal (no sync junk, no dotted rows) and `--tv underscan` (the whole frame); the Typewriter (its first row cut: expected); `--tv bright:60%,contrast:40%`; `--tv off`; the shelf drawing out and away.
- **Release:** the 110 with every instrument as in P6e (1.00×, UI about 1 ms), and the BW-2001 with its TV out: the speed ratio unchanged from before the step. The full release suite passes.

## Later

- P6g the Board's devices in their real look, P6h the keyboard drawer under the TV.
- Hold knobs (the user, 2026-11-17: kept for later): a computer whose signal is not perfect, or a monitor not exactly calibrated at start, so the picture rolls or tears until the player turns them.
- The early Machines' margins (a roadmap row, P6i): a back porch and top blank lines so their pictures sit inside the real window.

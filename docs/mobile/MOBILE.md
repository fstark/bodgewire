# P8f — Touch on tablets

Roadmap step P8f ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)), wanted: `bw_ui` played with fingers on a tablet in landscape, the web build first. This file is the step's starting point: what a finger lacks today, every interface change, their order and their tests. Its spec, design and plan go in `docs/p8f-touch/` when the step is implemented ([ROADMAP_STEP.md](../recipes/ROADMAP_STEP.md)).

Written 2026-11-25 from a study of `src/ui/` at `f717ec6`; it replaces the draft of 2026-11-23. Read from the code: nothing here was run on a touch device.

## User decisions

The user, 2026-11-25:

- **Tablets only**, in landscape. No phone in this step.
- **One interface.** A small change to the desktop version that makes it tablet friendly is preferred to a touch-only one, so the touch version differs as little as possible from the desktop and web ones.
- **Its own branch**, `touch`, merged when it works on the user's iPad. A push to `main` publishes the web build.
- **Few tests.** No question is tested twice; nothing slow is run for a change that cannot reach it ([Tests](#tests)).
- A hold on the Board is the loupe, and no action anywhere is a long press.
- The tools that change the Board show their target first, then act.
- Latching a button or a key is a Hold switch.
- Steppers for the timebase and the sample period, not rotary knobs.
- Text stays 13 px.
- Where there are no desktop windows, a double-click on an object fills the bench.

The user, 2026-11-23: no keyboard is needed. Every Machine with keys has its window of keys, and the Board's buttons are clicked.

Chosen without asking, as what "one interface" leads to; each can be overturned:

- **One size of controls for the mouse and the finger** (28 px drawn, answering within 40 px; the new controls 44 px), not a second set of sizes for touch.
- **The Marker stays a click, and a finger places the cursor by a hold and a slide.** The report agreed on 2026-11-25 had a tap place the cursor and an M button pin the Marker: a second behaviour, and a button of no use to a mouse.
- **The pointer lingers 2.5 s after a tap.** Every tooltip and highlight then shows after a tap with no code of its own.
- The number pad, the drag that scrolls a page and the new controls are the mouse's too.
- One wording for the hints.
- Left out as not needed: a wider keyboard, the paddle box's key caps hidden, a browser test suite.
- The step's number (P8f, with the ports) and the branch's name.

## Where it stands

The web build runs on an iPad (the user, 2026-11-22): the Launcher, the Boards and the TV work, slowly. A finger is a left mouse button and nothing else: SDL turns the first finger into mouse events, and no line of `src/ui/` reads a touch event.

What a finger already does, because the mouse's path fits:

- taps on the Launcher, the Shelf (48 px cells) and the binder's tabs (44 px);
- a one-finger drag pans the Board, the Oscilloscope's screen and the Logic Analyzer's lanes;
- a Board button or a key is down while the finger is;
- a vertical drag turns a paddle's Knob or a TV knob;
- a drag on an object's case moves it.

What stops play:

| What | Why |
|---|---|
| The Board cannot be zoomed | only the wheel zooms it; the binder's "Zoom to" frames one chip |
| A pin cannot be hit | 4 to 14 px across at fit zoom, under a fingertip of about 44 px |
| Four actions are out of reach | a Bus clipped (Shift), the Copper under a chip (Option), a button latched and the Marker cleared (right-click) |
| Six number fields cannot be typed in | no on-screen keyboard comes up: SDL 3.4's browser backend has no code to raise one |
| No time can be measured | the status line's `dt` runs from the Marker to a cursor that hovers |
| A Random fault being made cannot be stopped | Esc only |

And three things that go wrong quietly:

- **Hover is stale, not absent.** ImGui keeps the pointer where the finger lifted, and SDL's browser backend reports a pointer leaving only for a mouse. The Net under the last tap stays lit, its tooltip stays up, the TV's loupe stays on the glass. After a pan the highlight belongs to whatever slid under that point.
- **The thresholds are a mouse's.** A press that moves 6 px is a drag, so a tap that slides a little pans instead of probing. Two taps are a double-click only within 6 px and 0.30 s.
- **The pick reaches 12 px.** On the BW-2001 at fit zoom that is two pins on each side.

A pin's 2.54 mm pitch at fit zoom, on an iPad of 1180 × 820 px in landscape:

| Board | Size (mm) | Pin pitch at fit (px) | Zoom for a 44 px pitch |
|---|---|---|---|
| Counter | 126 × 45 | 22 | × 2.0 |
| Test pattern | 162 × 113 | 16 | × 2.8 |
| BW-80 | 156 × 142 | 12.5 | × 3.5 |
| Paddle tennis | 203 × 153 | 12 | × 3.8 |
| BW-1 | 189 × 255 | 7 | × 6.3 |
| BW-2001 | 293 × 269 | 6.6 | × 6.7 |

Zooming alone serves the small Boards; the large ones need the loupe.

## The rule: one interface

Everything on screen is the same for a mouse and a finger: the controls, their sizes, the wording. There is no touch mode. A finger differs in three ways, all of them in the input:

1. **Two gestures a mouse does not have**: the pinch and the two-finger drag. And one it does not need: the hold.
2. **Wider tolerances**:

    | | Mouse | Finger |
    |---|---|---|
    | A press becomes a drag after | 6 px | 10 px |
    | A double tap | 0.30 s, 6 px | 0.35 s, 24 px |
    | The Board's pick reaches | 12 px (Copper 10) | 22 px |
    | A link answers | on its word | within 22 px |
    | The cursor and the Marker land on an edge | never | within 12 px |

3. **No pointer between touches.** The pointer lingers 2.5 s where a tap ended, so the tap's highlight and tooltip show, then it is parked off the screen. A drag's end parks it at once.

What each gesture does:

| Gesture | The Board | Scope screen, analyzer lanes | Pages | Objects on the bench | Launcher |
|---|---|---|---|---|---|
| Tap | the tool in hand acts on the nearest pick | sets the Marker | follows a link | to the front; an Instrument takes the Focus | selects; on the selected one, opens |
| Double tap | bare Board: the whole Board; a chip: framed with finger-sized pins | the whole Capture (analyzer), the Trigger back (scope) | | the object fills the bench, or goes back | opens the Machine |
| Hold, 0.4 s without moving | the loupe: slide to aim, lift to act | the cursor: slide to move it | | on the TV's glass: the picture's loupe | |
| Drag, one finger | pans | pans time, or the scope's position | scrolls | moves the object by its case or title bar; turns a knob | moves the Machines' lane; scrolls the post-its |
| Drag, two fingers | pans, whatever is under them | pans | scrolls | | |
| Pinch | zooms about its centre | zooms time about its centre | | resizes the object, proportions kept | |

- **A second finger cancels the first.** A pinch that starts on a pin never probes or cuts.
- **Every Shift, Option and right-click action gets a control on screen**, for the mouse too. The modifiers stay as shortcuts.
- **A hold is never a second click.** The Dice's button is held and then released to roll, and a key is held to be probed. A hold means "look closer", on the Board, on a time screen and on the TV.

## The changes

About fifty. "Both" is for the mouse and the finger; "finger" is in the input only.

### The Board

| # | For | Change | Today |
|---|---|---|---|
| B1 | finger | **Pinch zooms about its centre; two fingers pan.** | The wheel. `Viewport::zoomAt` already takes a point and an amount. |
| B2 | both | **A double-click on a chip frames it** so its pin pitch is 44 px, or the chip fills 90 % of the bench (a 40-pin chip: about 31 px). | On bare Board it fits the whole Board; on a chip it does nothing. "Zoom to" frames a chip with 14 px pads. |
| B3 | finger | **The 22 px reach** for pads, Copper, Pencil Marks, Board buttons, slide Switches and a memory chip's window; the nearest wins. | 12 px, 10 px, 5 px, and the drawn size: a 10 mm key is 27 px at the BW-1's fit zoom. |
| B4 | finger | **The loupe.** A finger held 0.4 s brings a magnified view about 140 px across, 90 px above it, the pick lit in it and its name under it. Sliding aims, lifting acts, sliding off the Board cancels. Not on a Board button: there a finger down is the button down. | Nothing. The TV's loupe is the model for its look. |
| B5 | both | **Four view buttons, 44 px each, fixed at the bench's bottom right**: Flip, Look, Copper, Fit. Copper is Option as a toggle: picks go under chip bodies while it is on. | The Flip Corner is on the Board, off screen once zoomed in. Look is Tab. Copper is Option held. Fit is a double-click. All four stay. |
| B6 | both | **Selecting a memory chip shows the binder's Contents tab.** | The tab appears once the chip's small window is clicked. |
| B7 | both | **A refusal ("Wire: solder to pins only") shows above the pointer.** | At the pointer: under a finger. |

The loupe draws the Board a second time through a zoomed `Viewport`, clipped to a square with a drawn rim. A round one needs a render target.

### Tools and repairs

One rule: **a tool that changes the Board acts only on a target that is already lit.** A mouse's hover lights it, so a click acts as today. A finger's first tap lights it and names the action above it ("Cut this Trace"); a second tap within the 2.5 s does it. Since 2026-11-24 the hover shows what the cutters will cut: this gives a finger the same preview.

| # | For | Change | Today |
|---|---|---|---|
| T1 | finger | **Chip puller, iron, cutters and a wire's second pin: lit first, then done.** Lifting from the loupe acts at once: the loupe showed the target. | One click, after the hover. |
| T2 | finger | **A wire by the loupe**: hold on the first pin, slide to the second, lift. | Two clicks, or a drag from pin to pin. A finger's drag pans. |
| T3 | both | **The power-cycle question opens above the pointer.** When it comes up, its OK is the confirmation: no second tap is asked. | At the pointer. |

Nothing to change: the Logic Probe, the Hooks, the Test Leads (a tap); the pencil, whose taps already go round dot, check, cross, none.

### Instruments

The scope and the analyzer share `TimeCanvas::read`, written so that touch lands in one place.

| # | For | Change | Today |
|---|---|---|---|
| I1 | finger | **Pinch zooms time about its centre; two fingers pan.** | The wheel and the horizontal wheel. |
| I2 | finger | **A hold, then a slide, moves the cursor**; it stays where the finger lifts, as `--cursor` already places one. | The cursor is the pointer. A tap sets the Marker and the cursor at one point: `dt` is 0. |
| I3 | both | **A click on the Marker's flag removes it**; the flag 28 px. | Right-click, which stays. The flag is 14 × 13 px. |
| I4 | finger | **The cursor and the Marker land on an edge within 12 px.** | No snap: a mouse is exact. |
| I5 | both | **The hint line**: "wheel or pinch: zoom, drag: pan, click: marker, hold and slide: cursor, double-click: fit". | "wheel: zoom, drag: pan, click: marker, right-click: clear, double-click: fit". |

Choosing a number:

| # | For | Control | Where | Today |
|---|---|---|---|---|
| I6 | both | **Stepper**: −, the value, +, the two buttons 44 px. A click on the value opens the whole list in two columns of 44 px lines. | The scope's timebase (20 values), the analyzer's sample period (16), the Contents page's Rows (1 to 16). | Dropdowns at 17 px a line; a 64 px slider. |
| I7 | both | **Buttons side by side, one lit**, for 2 to 5 choices. | The scope's Trigger as Channel 1 2 3 4 and edge rise / fall; auto / normal; the analyzer's Before T; the Multimeter's V / Continuity; the Contents page's view, Characters and CPU. A Channel's Trigger condition shows its value, and a click opens the five (any, 0, 1, rise, fall) as a strip. | Dropdowns (the scope's Trigger: one of 8 lines) and 19 px radio buttons. |
| I8 | both | **Number pad**, opened beside a number field when it is clicked. Hex: 0 to F, x for a pattern, backspace, OK. Decimal: 0 to 9, the point, backspace, and ns, µs, ms keys that also confirm. Keys 48 px, the pad about 200 px wide. Typing still works. | Six fields: the analyzer's Bus Trigger pattern, the scope's position, the Contents page's Address, Go to, From and Length. | Text fields, typed. |

The analyzer:

| # | For | Change | Today |
|---|---|---|---|
| I9 | both | **A pin / bus switch** beside "n of 64 clipped". On bus, a click on a pin clips its whole Bus, and on a clipped pin unclips it. | Shift-click, taught by one hint line. |
| I10 | both | **A Channel Group's × opens two choices**: Ungroup, Unclip all. | × ungroups; Shift and × unclips. |
| I11 | both | **Rows 28 px high.** The whole name cell selects a Channel for a group; the fold arrow is as high as the row. | 20 px lanes, a 13 px name, a 19 px arrow. |
| I12 | both | **The overview strip 28 px high.** | 14 px. |
| I13 | both | **A vertical drag scrolls the rows, a horizontal one pans time.** The first 10 px decide. | The wheel scrolls the rows. |

I11 costs rows: about 10 in view instead of 15 at the normal size.

### Objects on the bench

| # | For | Change | Today |
|---|---|---|---|
| O1 | both | **The binder moves by its cover only**: the spine (34 px) and the tab strip. A drag on its page scrolls (M1). | A drag on the page moves the binder. |
| O2 | both | **A drawn corner grip, 28 px, answering within 44 px.** | A corner of about 14 px and edges of about 4 px. |
| O3 | finger | **A pinch on an object resizes it**, proportions kept. | |
| O4 | both | **Where there are no desktop windows (the web build), a double-click on an object or on its Shelf drawing fills the bench with it**; again, it goes back. Everywhere, the Instruments' size button goes round minimal, normal, full. | A double-click gives the object its own desktop window: nothing on the web. |
| O5 | both | **The close disc 22 px, always seen, answering within 44 px.** | 14 px, faint until hovered: for a finger, never. |
| O6 | both | **An object's first size fits the bench**: a larger one is shrunk in proportion, as `FitBench` does for an object popped back in. | Constants: the analyzer 940 × 440, the scope 680 × 460, the binder 600 × 500, the keyboard 640 wide, the TV 440 wide. |

Nothing to change: a tap brings an object to the front; the keyboards are moved by the strip above their keys, about 35 px high; the Instruments' title bars grow with the controls ([Sizes](#sizes)).

### The binder and the manuals

| # | For | Change | Today |
|---|---|---|---|
| M1 | both | **A drag scrolls**, with a short glide after it. Everywhere a list scrolls: the manuals, the Datasheets, the Contents page's three views, Repairs, the analyzer's rows, a long post-it. | The wheel, which stays, or a 14 px scrollbar. |
| M2 | both | **Scrollbars 20 px wide.** | 14 px. |
| M3 | finger | **A tap follows the nearest link within 22 px.** One place: `TextFlow::text`. | A link is a word 13 px high; in the Datasheets index the references sit side by side. |
| M4 | both | **Pinout rows 28 px high.** A click on a row already makes the tool in hand measure that pin, whatever the Board's zoom: this is the large way to a pin. | 17 px a row. |
| M5 | both | **A "show" button at the end of each pinout row** frames the pin on the Board. | Ctrl-click or Cmd-click, which stay. |
| M6 | both | **The Contents page's settings**: the number pad (I8), buttons side by side (I7), a stepper for Rows (I6). | Hex fields 46 px wide, dropdowns, a 64 px slider. |

Nothing to change: the tabs (44 × 72 px). What a link or a pinout row shows while hovered (its part outlined on the Board, a pin's Datasheet line) shows after a tap, by the lingering pointer. No swipe turns the dividers: a sideways drag on a page would compete with its scroll and its links.

### The Machine's devices

| # | For | Change | Today |
|---|---|---|---|
| D1 | both | **A Hold switch**: one beside the Board's view buttons, one on each matrix keyboard's case (BW-2001, BW-80). While it is on, a click latches a button or a key down, and another releases it. | A right-click latches, and stays. |
| D2 | both | **The TV's controls answer within 44 px.** | At the TV's first width: a knob 28 px across, the POWER slot 53 × 13 px, the underscan trimmer 12 px. |
| D3 | both | **The TV's glass is a control**: a press on it shows the loupe, above the pointer, and does not move the TV. | The loupe follows a pointer with no button down; a drag on the glass moves the TV. |

Why a switch for D1 and not a gesture: a hold is already the button held, a double tap is a doubled letter on a keyboard, and a finger sliding off a key is ordinary typing.

Nothing to change: a paddle's Knob (88 px, a vertical drag); Shift on the keyboards (a tap already latches it until the next key); the lamps and digits. Without a PC keyboard the BW-1's keypad is tapped on the Board itself: B1 and B3 make that workable.

### Launcher, Shelf and popups

| # | For | Change | Today |
|---|---|---|---|
| L1 | both | **A Stop button on the plate while a Random fault is made.** | "Esc stops", which stays. |
| L2 | both | **A drag on the Machines' lane** moves the pictures; they settle on the nearest Machine. | The arrow keys, a click on a neighbour or on a chevron. |
| L3 | both | **The chevrons answer within 44 px.** | 29 px. |
| L4 | both | **A sideways drag scrolls the post-its' row.** | The row moves only when a post-it is selected. |
| L5 | both | **On a narrow window the Shelf's cells shrink from 48 to 44 px.** | By its constants the BW-2001's Shelf needs about 1,075 px; a 1080 px iPad leaves 1,064. The Machine's tray, laid from the right edge, would overlap the Instruments'. |
| L6 | both | **The hints and tooltips name the control, not the modifier**, and say "wheel or pinch". | "shift-click", "right-click", "Option", "mouse wheel". |
| L7 | both | **Popup buttons 110 × 44 px**, the "Don't ask again" line 44 px high; the Thank-you Card's buttons 44 px high. | OK and Cancel are 80 × 19 px. |
| L8 | web | **The web page's notice ("Not made for phones and tablets") goes for tablets.** | Shown to every touch device at start. |

Nothing to change: a tap on a post-it selects it and a second opens it; a double tap on the live picture opens the Machine. A Shelf item's name, today a tooltip, shows after a tap by the lingering pointer.

### Sizes

One size for the mouse and the finger. WCAG 2.2 asks for 24 × 24 CSS px at level AA ([2.5.8](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html)) and 44 × 44 at level AAA ([2.5.5](https://www.w3.org/WAI/WCAG22/Understanding/target-size-enhanced.html)). On a 10 to 11 inch iPad a CSS px is about 0.19 mm: 44 px is about 8.5 mm.

| Control | Where | Today (px) | New (px) |
|---|---|---|---|
| Buttons, fields, tick boxes | every window | 19 high | 28 high, answering within 40 |
| Small buttons: ×, "Zoom to", "All datasheets", Hex / Code / Glyphs | analyzer and scope rows, Multimeter, binder | 13 high, the × about 15 wide | 28 |
| Title bar, with its size and close buttons | Instruments | 19 | 28 |
| Close disc | TV, keyboards, paddle box, binder, Brief | 14 | 22, answering within 44 |
| Scrollbar | pages, rows | 14 | 20 |
| Row; overview strip | analyzer | 20; 14 | 28; 28 |
| − and + | scope, analyzer | 24 × 19 | 44 × 44 |
| A line of a list | the steppers' lists | 17 | 44 |
| OK, Cancel | popups | 80 × 19 | 110 × 44 |

Already large enough: a Shelf cell (48), a binder tab (44 × 72), a paddle's Knob (88), a post-it (190 wide).

How: two ImGui style values (frame padding for 28 px frames, 8 px between items) and 6 px of `TouchExtraPadding`, which widens every control's hit area without drawing it larger. The sizes the code draws itself are constants. Text stays 13 px, so every layout constant stays valid: the analyzer's 340 px label column, the 90 px fields.

The figures are a first setting, to be tuned on the iPad.

### Under the interface

| # | Piece | What it does |
|---|---|---|
| P1 | **A gesture layer** in `src/view`, unit tested, without ImGui | In: SDL's finger events. Out, each frame: the one pointer handed to ImGui, and tap, double tap, hold, pinch (centre and factor), two-finger drag, cancel. SDL stops making a mouse from the first finger (`SDL_HINT_TOUCH_MOUSE_EVENTS` off), so the layer decides. It parks the pointer (rule 3) and releases the first finger's press off the screen when a second lands. |
| P2 | **The pointer's source** | The tolerances follow the source of the last pointer event (ImGui's `MouseSource`). An iPad with a trackpad gets the mouse's. SDL's browser backend reports a pen apart from a finger, so an Apple Pencil should too: to check. |
| P3 | **`bw_ui --touch`** | The mouse stands in for one finger: no hover between presses, no wheel, no right button, a finger's tolerances. A `--screenshot` then shows what a tablet shows, on the desktop. |
| P4 | **The web page** (`src/ui/web/shell.html`) | No text selection or callout on a long press, no overscroll bounce, the height from `100dvh`, the safe-area insets. A manifest and Apple's home-screen tags: opened from the home screen, the page has no browser bars. In portrait, a notice to turn the tablet. To check on the iPad. |

The layer's outputs go where the code already takes them: `Viewport::zoomAt` and `pan`, the zoom and pan fields of `TimeCanvas::read`, a page's scroll, an object's size. SDL already sets `touch-action: none` on the canvas.

## Tests

Each question is answered once, by the cheapest check that can answer it.

| Question | Check | When |
|---|---|---|
| Does the layer tell a tap, a double tap, a hold, a pinch, a cancel? | unit tests, native: `ctest --preset release -j -R gesture` | a phase that changes the layer |
| Do the helpers place things right (pinch on the `Viewport`, the narrow Shelf, a first size on a small bench)? | unit tests of the files touched, by `-R` | the phase that touches them |
| Do the windows look right, and are the targets the sizes above? | `bw_ui --touch --screenshot`, native, viewed: about six shots a phase | a phase that changes a window |
| Memory errors, undefined behaviour | the debug build, the gesture and view tests only | phases 1 and 5 |
| Does it build for the web? | `cmake --build --preset web` | once a phase |
| Do the browser's fingers reach the layer? Does the page behave? | the page opened once | phases 1 and 8 |
| Does it feel right? | the user, on the iPad, with a short list | after phases 2, 5 and 8 |
| Is anything else broken? | the full release suite | once, before the merge |

What is not done, and why:

- **No browser test suite** (Playwright or another). What it could check, the layer's unit tests and the `--touch` screenshots check natively, in seconds. It would ask the same questions again on the slowest platform (the web build runs at about 0.4× native).
- **No Machine, Puzzle or design-rule case between phases.** The step touches `src/ui`, `src/view` and the web page: no Part, Machine, Board or kernel. Those cases cannot change before the final suite.
- **No full debug suite**, as [AGENTS.md](../../AGENTS.md) already says.

## Order of work

Eight phases, each leaving something a finger can do that it could not before. The small Machines are playable after phase 3, the large ones after phase 5.

1. **Plumbing** (P1 to P3). Nothing new on screen: the stale hover is gone, and `--touch` lets every later phase be checked by screenshot.
2. **The Board's zoom and picks** (B1 to B3, B5 to B7, I1). The Machines up to the Test pattern can then be probed.
3. **What has no control today** (L1, D1, I2, I3, I9, I10). Nothing then needs a key or a modifier but the number fields.
4. **Sizes and numbers** (Sizes, I4 to I8, I11 to I13, M4, M6, D2, L3, L7). The Instruments are then usable, Bus Triggers included.
5. **The loupe and lit-first repairs** (B4, T1 to T3, D3). The BW-1 and the BW-2001 can then be repaired.
6. **Pages** (M1 to M3, M5, L4).
7. **Objects and the Launcher** (O1 to O6, L2, L5, L6).
8. **The web page** (P4, L8).

## Later

- **Fingers that keep what they hold**: a finger down on a Board button, a key or a knob owns it until it lifts, while the others go on working. RESET held while a pin is probed, Shift and a key together, Paddle Tennis for two on one tablet.
- A PASTE key on the matrix keyboards' cases (Cmd-V today).
- Rotary knobs for the timebase and the sample period, the day the Instruments get a drawn face.
- Scaled text: every layout constant of `src/ui/` through one scale.

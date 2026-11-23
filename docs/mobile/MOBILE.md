# Mobile support — draft

A draft for a later roadmap step, in the Freezer (ARCHITECTURE.md#roadmap) until then: `bw_ui` played with fingers on a tablet or phone. Written 2026-11-23, after the web port (P8b) ran on the user's iPad.

Touch, not the browser, is the problem. The same work serves the web page on an iPad, a desktop touchscreen and a native iOS or Android build if one ever comes. When this becomes a step, this file becomes its spec's starting point (`docs/pXy-mobile/`).

## Where it stands

The web build runs on an iPad (the user, 2026-11-22): the launcher, the Boards and the TV work, slowly. SDL turns one finger into a left mouse button, so a tap is a click, a drag is a drag and a double tap is a double-click. The canvas has `touch-action: none`, so the browser leaves the gestures to the page.

The web page tells a touch device at start that Bodgewire is not made for it yet, with a "Try it anyway" button (2026-11-23): a finger as the main pointer (`pointer: coarse`, which catches an iPad that says it is a Mac), or `?mobile` in the address to see it on a desktop. Once per tab. When this step is done, the notice goes.

What a finger cannot do is everything else the mouse and keyboard give:

- a right button;
- modifier keys;
- a wheel;
- hover;
- a second pointer.

No keyboard is needed (the user, 2026-11-23): every Machine with keys has its window of keys, and the Board's buttons are clicked.

## What is missing

Every input of `src/ui/` a finger lacks, found 2026-11-23:

| Input | Where | What it does |
|---|---|---|
| Shift-click | Board, Clip | clips a pin's whole group (a bus); on a clipped pin, unclips the group |
| | Board, Pencil | erases marks |
| | analyzer, a group's `×` | unclips the whole group |
| Option held | Board | picks the copper under a chip's body instead of the chip |
| Ctrl/Cmd-click | a pin clicked from the chip panel | selects instead of measuring |
| Right-click | Board | a push button latched down |
| | Board, Pencil | erases marks |
| | analyzer, scope | clears the marker |
| | matrix keyboard | a key latched down |
| Right or middle drag | Board | pans (a left drag on bare Board also does) |
| Wheel | Board, analyzer, scope | zooms at the pointer |
| | knobs, the TV's knobs | turns them (a drag also does) |
| | launcher, manuals, binder | scrolls |
| Horizontal wheel | analyzer, scope | pans the time axis |
| Tab | bench | the real or plain look: **no control on screen** |
| `` ` `` | bench | flips the Board (the flip corner also does) |
| Arrows, Enter, Space | launcher | moves and opens (taps on Stills and post-its also do) |
| Q/A, Up/Down, the Board's keys | bench | knobs and Board buttons (all also clicked) |
| Cmd-V | matrix keyboard | pastes typed text |
| Text field | analyzer | a group's trigger pattern: needs the device's keyboard (does SDL raise it on iPadOS? to check) |
| Hover | Board | the net lit, pin and part tooltips |
| | shelf | the tools' names |
| | everywhere | how-to tooltips ("drag up or down, mouse wheel, or Q and A") |
| | analyzer, scope | the readout cursor follows the pointer (the marker, set by a click, stays) |

Too small for a finger:

- the `×` SmallButtons (unclip, the meter's);
- the windows' close buttons;
- radio buttons;
- the analyzer's 60 px trigger dropdowns;
- the combo lists at 13 px a line;
- above all a pin: about 10 px at the Board's fit zoom on an iPad, under a finger's 40–50.

The instruments' dropdowns are the wrong control for a finger: the scope's timebase and trigger, the analyzer's period, position and each channel's trigger.

## Ideas

### 1. Long press: the second click

A finger held about 0.5 s without moving does what a right-click or a Shift-click does, and the plain click is dropped. One rule for every modifier, and in this UI no target needs both:

- Clip on a pin: the group clipped, or unclipped on a clipped pin.
- Pencil: marks erased.
- A push button: latched. A matrix keyboard key: latched.
- The analyzer's `×` on a group: the group unclipped.
- An analyzer or scope trace: the marker cleared.
- A pin in the chip panel: selected instead of measured.

### 2. View toggles on the Board

Option and Tab change how the Board is seen, not one target, so they get no gesture. They become two small controls next to the flip corner:

- the real or plain look (Tab);
- "copper": Option held, as a toggle.

The desktop gains them too.

### 3. A gesture layer for two fingers

- A pinch zooms at its centre. The Board, analyzer and scope already zoom at a point by the wheel.
- A two-finger drag scrolls (manuals, binder, launcher) or pans (the Board, the time axes).
- A second finger cancels what the first one's press started.

This is a small class in `src/view`, unit tested, fed SDL's `SDL_EVENT_FINGER_*`. Its outputs are what the views already take: a zoom at a point, a pan, a scroll.

### 4. A loupe for pins

While a finger is down on the Board, a magnified circle above it shows what it would pick, the pad lit. Sliding moves the pick; lifting acts. Actions already happen on release.

The loupe also replaces hover: the net lit and the tooltip while the finger is down. The TV window's loupe is the model. The chip panel (tap a chip, then a pin's row) is already a finger-sized way to a pin.

### 5. Bigger targets, same look

- `ImGuiStyle::TouchExtraPadding` widens every widget's hit area without drawing it larger.
- On touch, somewhat larger frame padding and list lines.
- The `×`s and close buttons get room of their own.

### 6. Instrument controls

- The scope's timebase and the analyzer's period become rotary knobs, as on the real instruments, turned by a drag like the TV's. This is the drawn-hardware look and works with a mouse too.
- A channel's trigger condition: a tap cycles 0 → 1 → ↑ → ↓ → X, instead of a dropdown.
- Trigger source and position: to settle with the user.

### 7. Text for the input in use

Touch mode follows the last pointer event's source: ImGui's `MouseSource`, SDL's touch events. It is not a build flag, so an iPad with a trackpad is back to the mouse. On touch:

- tooltips say "drag", not "mouse wheel" or "Q and A";
- the key hints under the knobs hide.

### 8. Two fingers on two knobs (optional)

SDL makes one mouse pointer from the first finger. Paddle Tennis for two players on one tablet needs the knob window to follow fingers itself, one finger a knob. A demo's charm, not a need.

## Tablet and phone

The bench just fits an iPad in landscape: the Board, the TV, the knobs, two instruments and the binder. A phone does not. It needs its own layout: one object full screen at a time, a switcher between them.

Proposed: tablets first, a phone running but cramped; the phone layout a later step.

Browser details for either:

- iOS Safari's bars and the page height;
- a manifest so the page can be added to the home screen and open full screen;
- orientation changes (the canvas already follows the window).

## Testing

- **Unit tests:** the gesture layer.
- **Agent checks:** Playwright drives Chrome with touch (taps, long presses, and pinches through the DevTools protocol), with screenshots viewed.
- **The user:** the feel on an iPad.

## To decide

1. Tablets only, or phones as well?
2. Long press as the second click, and the look and copper toggles by the flip corner?
3. Knobs for timebase and period, or +/− steppers?
4. Multi-touch knobs for two players: with the step, later, or never?
5. The step's number when it leaves the Freezer: after the ports (P8), or on its own.

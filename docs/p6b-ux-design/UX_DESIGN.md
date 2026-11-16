# P6b — UX design

Roadmap step P6b ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)). No new code: the choices for the single-board game's screen, settled with the user, the issues known today, and the P6 rows that build them. The Board's own look was settled by prototyping it first (below); the rest of the screen was explored in discussion and in one rejected spike.

## The rule

**Realism for how things look, never for how they behave** (decided 2026-11-16). The Board, its parts and the Machine's devices look like the real things, because the player reads them as the machine. What the player does stays abstract, precise and in fixed places: a cursor, readouts where the eye learns to look, flags on pins. No drawn probes following the mouse, no clip leads, no dangling wires.

One window, possibly large, holds everything. No docked or tiled panels: panels at arbitrary sizes suit an IDE, not a game aiming at a little skeuomorphism (tried as a spike, branch `p6-docking-spike`, rejected 2026-11-16). No extra desktop windows.

## The Board (done, P6 prototype)

Built and reviewed screen by screen on 2026-11-16 (commits `09296a5` to `996a631`), now the default look:

- Dark green solder mask over the copper; front copper green, back copper teal; the active layer bright and on top, the other dim; `L` swaps them and nothing else.
- Parts drawn as the parts: black DIP bodies with legs and a pin-1 notch, the part number alone along the body, as large as it fits; resistors with their colour bands; the oscillator a can marked with its frequency (`1.000MHz`, from `freq=`); the power input a green screw terminal; the LED a dome. No date codes, no outlines around parts; references printed above each part.
- Selections are gold, whatever the layer: the Net under the cursor bright gold; a hovered chip's wiring dim gold, every other trace dimmed, the pins at both ends of its wiring marked, and its own pins on VCC or GND marked when connected. VCC and GND never glow, nor a Net that reaches no other pad.
- The hovered chip goes see-through with its pin names; a single Net under the cursor replaces the chip's glow.
- The selected part has a white box; a hover has none. A click on the selected part's body, or on bare board, deselects.
- `Tab` (`--look plain`) gives the specialist's plain look with the same selections.

Left for later: the Board's devices (buttons, slide switch, digits; see Issues).

## The shelf

Today's toolbar and side panel give way to one **shelf** along the bottom of the window (decided 2026-11-16; the user did not mind where, and one strip is simpler than two). It holds small drawings of everything the player can use, in three groups, left to right:

1. **Controls**: actions, no object. The power switch drawn as a toggle showing its own position, the current-limit LED beside it, run/pause, speed. Never "selected": they are in a position.
2. **Tools**: one in use at a time. Probe, chip puller (swap), soldering iron (mend, resolder), cutters, wire, clips, hooks, meter leads. The tool in use is drawn as taken off the shelf (its slot an empty outline). The mouse keeps a cursor, perhaps one per tool, never a drawn tool (decided 2026-11-16: a slippery road). The logic probe's three lamps are on its shelf drawing, a fixed place, not near the tip where they would hide pins.
3. **Equipment**: objects brought out onto the bench or put away, several at once. TV, keyboard, logic analyzer, oscilloscope, multimeter, the manuals binder, a paddle's knob. One that is out is drawn differently on the shelf (switched on: a lit screen, a pilot lamp). Some tools belong to equipment: taking the clips brings out the analyzer, the hooks the scope, the leads the meter, as today.

The shelf is the same on every Machine, each item in the same place, for muscle memory. An item that does not apply is **grayed, not hidden** (the TV on the counter). Graying can later mean other things (an instrument not yet unlocked), though unlocking is not wanted for now (P7).

## Equipment on the bench

- Each object floats over the Board, in a **default place** per kind (the TV top right, the keyboard under it, an instrument along the bottom), back in that place each time it comes out (decided 2026-11-16: muscle memory).
- **Free size**, with two presets, **minimal** and **normal** (decided 2026-11-16). Objects keep their proportions when drawn: a TV is a TV, a keyboard a keyboard.
- A click on the object's shelf drawing brings it out or puts it away.
- **TV**: the picture in a monitor, with its controls (horizontal and vertical position at least: P7's note on the BW-2001's off-centre picture) hidden by default. Later, the keyboard pulls out from under it, so the player does not bring out a screen and then a keyboard (decided 2026-11-16: later; the keyboard stays its own object for now).
- **Manuals binder**: one object for the User Manual, the Service Manual and the datasheets, as tabs of a ring binder (the user left the form open; a binder chosen as one object rather than a pile). It opens at the selected chip's datasheet. The Chip section's role and pinout move here.

## The Puzzle brief

A **post-it note** on the Board, the owner's complaint, which folds into the shelf (decided 2026-11-16).

## Today's side panel, section by section

| Section | Becomes |
|---|---|
| Puzzle | the post-it |
| TV | the TV object |
| Knob | the paddle's knob, equipment |
| Probe | the lamps on the probe's shelf drawing |
| Chip | the binder (role, pinout, datasheet) |
| Repairs | open: a work log, folded away by default, in the binder or on the shelf |
| Debug | behind a developer flag |

## Issues

Known on 2026-11-16, from this step's screenshots and the user's review; an agent's sweep of every Machine and Puzzle kind runs again once the shelf exists:

- The top hint line ("Probe: click a pin…") is large and always there; the shelf's tools replace it, a short hint at most.
- Floating windows cover the Board (the BW-2001's keyboard hid half of it); default places and putting away answer it.
- The stopwatch's buttons are small grey discs, their labels over their caps; the slide switch is a speck at Board zoom.
- On machines with no video, a black "no video" TV takes half the side panel (gone with the TV object).
- References are not drawn at fit zoom on large Boards (the BW-2001): too small to read. Decided 2026-11-16: look again when needed.
- Screenshots taken while the user's mouse is over the window show that hover: the agent's screenshots are not reproducible then.

## Rows

The P6 rows after this one, in order:

| | Row | Content |
|---|---|---|
| P6c | Shelf | the bottom shelf replacing the toolbar: controls, tools, equipment as drawings; tool taken off the shelf; graying; the probe's lamps on its drawing |
| P6d | Equipment objects | TV, keyboard, analyzer, scope, meter, knob as objects: default places, free size with minimal and normal, out or away from the shelf; the side panel removed |
| P6e | Manuals binder | User Manual, Service Manual and datasheets in one object, opening at the selected chip; the Chip section moves in |
| P6f | Post-it brief | the Puzzle brief as a post-it on the Board, folding into the shelf |
| P6g | Board devices | buttons, slide switch and digits in the Board's real look |
| P6h | TV controls and keyboard drawer | the TV's position controls; the keyboard pulled out from under the TV |

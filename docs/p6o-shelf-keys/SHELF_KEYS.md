# P6o — Shelf and keys

Roadmap step P6o ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the PC keys always go to the Machine, the shelf regrouped around one slot for the Machine's own devices, the repair tools usable with the power on, and smaller bench fixes. It takes in the former P6p (the keys' inconsistencies left by P6g) and replaces the keyboard drawer. Design: `SHELF_KEYS_DESIGN.md`. Plan: `SHELF_KEYS_PLAN.md` (both to come). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

P6g sent the PC keys either to the bench (the shortcuts) or to the Machine (its keyboard, Board keys, knobs), switched by clicks and F2. That gave the inconsistencies the old P6p listed:

- on the BW-1, a click on a Board key did not give the Machine the keys, though P6g's design said it should;
- a Board button with no PC key (Tennis's `START`, the Typewriter's `CLEAR`) took the keys back to the bench, so the knobs stopped;
- on Tennis, the shelf showed nothing while the Machine had the keys;
- Esc was the Machine's, so F2 was the only way back.

The player seldom uses the bench shortcuts. Removing them removes the mode, and every one of those issues goes with it.

The keyboard drawer (a keyboard pulled out from under the TV, decided 2026-11-16) came before the realistic keyboards (P6l, P6l2) and pop-out windows (P6j). A PET has its keyboard in the same case as the screen, and a ZX80's keyboard is the computer, so neither sits in a drawer. Its purpose, not making the player bring out a screen and then a keyboard, is better met by one shelf slot for all of the Machine's devices.

The shelf has grown item by item. Pause, layer and fit took places in it without being discussed. The tools needing the power off are grayed and refused while the Machine runs, so every repair costs two trips to the power switch.

## User decisions (2026-11-19)

| Question | Decision |
|---|---|
| Scope | one milestone: P6o and P6p merged, the drawer dropped |
| Where keys go | every key always goes to the Machine, even when its keyboard or knob box is put away |
| Bench shortcuts | removed, all of them (YAGNI), except Tab (plain look) and `` ` `` (flip the Board) |
| The Machine's devices | one shelf slot for the TV, keyboard and knob box, with a drawing made for each combination so it never looks incomplete |
| The Machine slot's place | at the shelf's right end, set apart; one cell per device it holds |
| Double-click on equipment | pops the object out to a desktop window, and brings it back in; the expected behaviour, replacing home |
| Pause, layer, fit | visual clutter on the shelf: pause joins speed, fit goes, layer moves onto the Board |
| The layer control | a corner of the Board itself, coloured like the other side's traces; key `` ` `` |
| Gold | only the focus, the tool or instrument that acts on the Board; no gold for keys |
| Board buttons with no PC key | stay mouse-only (YAGNI); some are small, accepted |
| Tools needing the power off | always usable: a popup offers to power off, apply the tool and power on again |
| The popup's buttons | generic (OK, Cancel); the text says what happens, so there is no list of verbs to keep |
| The binder | resizable in height; its slot on the right, with the Machine slot |
| Shelf groups | a space between the controls and the tools; a background per group so the player sees the groups |
| The brief | can be dragged, if it takes no more than about 10 lines; otherwise dropped |
| The pencil | marks only on the Board; today it also marks the background around it |

Decided while grilling (2026-11-19), accepted by the user:

- **Exceptions to "every key to the Machine"**: a text field being edited (the analyzer's pattern, the scope's delay) takes the keys until it loses focus. Cmd combinations stay the app's: quit, switching apps, and the BW-2001's existing Cmd+V paste. No paste is added on the other keyboards. No Machine uses Tab or `` ` ``: the Typewriter's keyboard stops at `_` (0x5F) and sends Tab as Ctrl+I, and the PET, ZX80 and BW-1 have neither key. `` ` `` is matched by the character, so it works wherever the layout puts it.
- **What goes with the shortcuts**: F2, the clicks that took or gave back the keys, the gold for keys on the shelf and on the keyboard and knob windows, the shelf drawings' corner letters (`G`, `P`, `S`, `M`, `K`, `W`, `C`, `O`, `V`, `U`, `L`, `F`), Esc everywhere (back to the probe, clear a marker, cancel a wire), `+`/`−` zoom, and `--keys machine`. Each has a mouse equivalent: a click on the tool or instrument, the wheel, a right-click to clear a marker, a click on the wire's first pin again (or another tool) to cancel a wire. The BW-1's keyboard drawing no longer toggles anything.
- **Only Tab keeps the plain look**; it has no shelf control.

## What the player sees

### Keys

1. Every key goes to the Machine: its keyboard (out, put away or popped out), its knob box (`Q`/`A`, `↑`/`↓`, out or not), its Board keys (the BW-1's `pc=` keys). A Machine with none of these ignores keys.
2. Exceptions: a text field being edited; Cmd combinations; Tab flips the plain look; `` ` `` flips the Board.
3. Nothing shows who has the keys: there is no longer a choice. Gold means only the focus.
4. The User Manuals that mention F2 (Message, BW-1, BW-2001, BW-80) are reworded.

### The shelf

5. Left to right, each group in its own recessed tray (a shade darker or lighter than the shelf's wood), with a gap between trays:

   ```
   [Power ‖·1×·Max]  [Pencil Probe Puller Iron Cutters Wire]  [Analyzer Scope Meter]   …hint…   [Brief Binder | Machine slot]
        controls                      tools                         instruments                        this Machine
   ```

   The left half is the bench, the right end is this Machine: its complaint, its manuals, its devices. The hint keeps the free space between. The trays' look is tuned on screenshots with the user.
6. **Controls**: the power switch with its LIMIT lamp, unchanged; one three-position slide switch for pause and speed: `‖`, `1×`, `Max`. The pause button, the layer control and the fit button leave the shelf. A double-click on bare Board still fits.
7. **The Machine slot** holds the TV, keyboard and knob box, at the shelf's right end, aligned right so its width moves nothing else:

   | Machines | Drawing | Cells |
   |---|---|---|
   | Test Pattern, Bouncing Ball, Message | the monitor | 1 |
   | Knob Meter | the paddle box | 1 |
   | Tennis | the monitor and the paddle box | 2 |
   | Typewriter, BW-2001, BW-80 | the monitor and a keyboard (one generic keyboard, not each Machine's look) | 2 |
   | Counter, Chaser, Dice, Stopwatch, Heartbeat, LED Computer, BW-1 | the monitor, grayed; hint "No TV, keyboard or knobs on this Machine" | 1 |

   Each device in the drawing shows whether its object is out (a lit screen, a lit keyboard, the knob's lamp), so putting away only the keyboard shows.
   - A click: if any of its objects is away, all come out, in front; if all are out but some are behind other windows, all come to the front; if all are out and in front, all go away.
   - A double-click pops all of them out, each to its own desktop window, or brings them all back in.
   - Each object keeps its own `×` and can be put away alone.
   - The TV, keyboard and knob slots are gone.
8. **Double-click** on an equipment slot (the Machine slot, analyzer, scope, meter, binder) pops the object out to a desktop window (P6j), and a double-click while it is out there brings it back into the main window. This replaces home: an object already comes back at its default place each time it comes out. On the brief, the tools and the switches a double-click does nothing.
9. **The power-off tools** (puller, iron, cutters, wire) are never grayed or framed red with the power on. Their hint says the tool power-cycles the Machine.

### The Board

10. **The flip corner**: a dog-ear on the Board's bottom-right corner, sized in Board millimetres (about 10 mm, with a pixel floor like the pencil marks), panning and zooming with the Board. It shows the other side's trace colour: blue seen from the front (green traces), green seen from the back. A click on it flips the Board whatever the tool, the pencil included (it is a control, not part of the PCB); `` ` `` does the same. Hint: "Flip the Board". In the plain look too.
11. **The pencil** marks only on the Board: a click on the background around it does nothing.

### Repairs with the power on

12. With the power on, a click that would apply a repair opens a small popup at the click. That is the puller on a chip, the iron on a pin or crack, the cutters on a trace or wire, or the wire on its second pin; the first pin is picked with the power on. A click that would do nothing, a cut on bare Board for instance, opens nothing.
13. The popup reads: "This tool needs the power off. OK powers the Machine off, uses the tool and powers it on again; the Machine restarts." Its buttons are **OK** and **Cancel**, mouse only, since keys go to the Machine. A click elsewhere cancels. A "Don't ask again" checkbox holds for the run, for every tool, and is not saved. After it, such a click power-cycles at once.
14. After OK the Machine is as a manual power cycle leaves it: restarted (a typed program or a score is lost), run or pause and the speed as they were, the instruments' captures and sweeps kept, the pencil marks kept (a swapped chip's leave with it).

### Bench

15. **The binder** gets taller or shorter by its bottom edge (the height only); its corner still scales it in proportion. The height is kept for the run, and works the same when popped out. A taller binder still reads as a binder: the one exception to "objects keep their proportions".
16. **The brief** note can be dragged. It stays glued to the Board, panning with it from where it was dropped (an offset from the Board's corner in screen pixels), for the run, kept when folded away and brought back. Built only if it takes about 10 lines; otherwise left out without asking.

## What is new

To be settled file by file in the design:

1. **Keys**: `MachineKeys` and F2 removed; the keyboard, matrix, Board-key and knob captures always on (but for a text field, Cmd, Tab and `` ` ``); the bench shortcuts removed from the shelf, Board view, analyzer and scope.
2. **The shelf**: the new order and trays; the speed switch with pause; the Machine slot and its five drawings; double-click pops out; the power-off tools never blocked.
3. **The Board view**: the flip corner and `` ` ``; the pencil limited to the Board.
4. **Repairs**: the popup and the power cycle around `Session::repair`.
5. **Bench objects**: the binder's height; the brief dragged.
6. **Flags**: `--keys machine` removed; anything the screenshots need for the popup, the speed switch or the flipped Board, decided in the design.
7. **Docs**: the four User Manuals, GLOSSARY (the Shelf, the Machine slot, the flip corner; the keys' terms), ARCHITECTURE.

The kernel, Parts, the builder, devices, Machines and Puzzles are untouched.

## Tests

- **Unit tests** (ImGui-free code in `src/view`): the Machine slot's click on a group (any away, some behind, all in front); the popup's decision (a click that would apply a repair, one that would not, "Don't ask again"); the pencil refused off the Board; the flip corner's hit area and pixel floor. `machine_keys` tests go with it; focus tests lose Esc.
- **Screenshots** (release, viewed): every Machine's shelf (the five Machine-slot drawings, the trays); Tennis and the BW-2001 with their objects out, one put away, one popped out; the flip corner from both sides and in the plain look; the popup; a tall binder; the brief moved.
- **By reading and by the user**: typing with the keyboard put away; Tab, `` ` ``, Cmd+Q and Cmd+V on the BW-2001; a text field in the analyzer; the wire cancelled by a second click on its first pin.
- **Release**: the 110 with every instrument (1.00×, UI about 1 ms). The full release suite passes.

## Later

- A shelf control for the plain look, if Tab alone is missed in play.
- PC keys for Board buttons (Dice `ROLL`, Stopwatch, Tennis `START`), if wanted; some of those buttons are small.
- Paste on the Typewriter and the BW-80, if wanted.

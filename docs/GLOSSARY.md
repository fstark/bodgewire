# Bodgewire

A detective game about diagnosing and repairing faulty 8-bit computers, simulated at the digital signal level. This glossary covers the hardware, the faults, and how the player investigates them.

## Hardware

**Machine**:
A complete simulated computer or circuit the player diagnoses, such as the BW-80 or the Test-pattern generator.
_Avoid_: Emulator, system

**Board**:
The printed circuit board of a Machine: its Copper, Components and Sockets.
_Avoid_: PCB (except in visual descriptions), motherboard

**Part**:
A type of chip in the catalogue, such as a 74LS161 or a Z80. Parts are what the inventory holds.
_Avoid_: Chip type, model

**ROM Contents**:
The words a ROM Part holds, read from a `.rom` text file in the Machine's `roms/` that the netlist names with `contents=` (or a chip-sized slice of it, with `offset=`). Two ROMs of the same Part with other contents are the same Part.
_Avoid_: Firmware, image, dump

**Component**:
One placed instance of a Part on a Board, identified by its reference (U12).
_Avoid_: Chip, IC (when meaning a specific placed one)

**Pin**:
One electrical terminal of a Component. A bidirectional Pin, such as a RAM's data pin, both reads its Net and drives it (or lets go of it, so another chip can drive the same bus).

**Socket**:
A holder on the Board that a Component plugs into, making it removable without soldering.

**Connector**:
A Component through which power enters a Board or signals enter or leave it, such as the power connector, the video connector or a keyboard connector. Each type has one fixed job.
_Avoid_: Port, jack, header

**Joint**:
A solder connection between a Pin (or Socket contact) and the Copper.
_Avoid_: Solder point

**Dry Joint**:
A Joint that does not connect: the Pin is off its pad's Net, while the pad and its Trace still carry the signal. Nothing shows on the Board. Repaired by resoldering.
_Avoid_: Cold joint, bad solder

**Leg**:
A chip Pin's lead above the Board. A probe or meter touch on a chip pad reads the leg with F.Cu active and the pad with B.Cu active; only a Dry Joint makes them differ.

**Copper**:
The conductive parts of a Board: pads, Traces and vias, on one or more Layers.

**Trace**:
A physical strip of Copper on one Layer joining pads or vias.
_Avoid_: Track, wire, line (when meaning the physical object)

**Layer**:
One copper side of a Board. Most Boards have two.
_Avoid_: Side

**Net**:
A set of Pins that are electrically connected, and so share one logic value. Nets follow from the Copper and change when Copper is cut, broken or bridged.
_Avoid_: Signal, line, wire

**Bus**:
A named group of Nets carrying one multi-bit value, such as the address bus A0–A15.

**Bodge Wire**:
A wire the player solders on to join two points, bypassing or replacing Copper.
_Avoid_: Jumper, patch wire

**Output Device**:
The part of a Machine the player watches it through. Standard devices (a TV) sit off the Board behind a Connector; LEDs and digits whose arrangement belongs to the Board are Components on it, and can be faulty like any other.
_Avoid_: Display, screen (when meaning the device in general)

**Input Device**:
The part of a Machine the player operates it through. Standard devices (an ASCII keyboard, a paddle controller) sit off the Board behind a Connector; buttons and keys whose arrangement belongs to the Board are Switches on it, and can be faulty like any other.
_Avoid_: Control, input (when meaning the device)

**Knob**:
What the player turns on a paddle controller, from 0 at the left stop to 1 at the right; the controller answers each trigger with a pulse whose length follows it. A Vetting Script and `bw run` may set it at a time (`knob J2:87%:400ms`, P8d).
_Avoid_: Paddle, pot (when meaning the setting)

**Monitor**:
The small ROM program that runs on a Machine with a CPU: it shows memory and lets the player change it and run a program from its keys (the BW-1's AD, DA, GO); there is no BASIC.

**Snow**:
White specks on the picture where the CPU reached the video RAM while the beam drew the text: the video drew the CPU's byte instead of the screen's. On the BW-2001 (the PET 2001's board) every access to the screen in the display makes it; the editor waits for the retrace to avoid it.

**Strap**:
A jumper or wire on the Board that a program reads to choose what it does: on the BW-1, closed at reset, it runs the self-test instead of the Monitor.
_Avoid_: Jumper (when meaning what it selects)

## Signal states

**Logic Value**:
One of `0`, `1`, `Z` (high impedance, nothing driving) or `X` (unknown or conflicting).
_Avoid_: Level, voltage

**Driver**:
A Pin's contribution to the value of its Net: what it drives and how strongly. Also, a Pin that drives its Net: an output, a bidirectional Pin, a pull-up/pull-down, a series resistor or a supply.

**Pull-up Pin**:
A chip Pin that drives 0 and otherwise holds its Net at 1 through its own pull-up (an NMOS port pin, the 6530's): anything driving 0 beats its 1, and nothing else on the Net reads 1, not floating. It reads its Net too.

**Load**:
A Pin that only reads its Net, such as a gate input or a chip's power pin.
_Avoid_: Sink, receiver

**Drive Strength**:
How hard a Driver forces its Net, so stronger Drivers win over weaker ones (e.g. an output beats a pull-up).

**Tie**:
A constant Driver attached to a Net rather than a Pin: a supply rail (GND/VCC) or a pull-up / pull-down resistor.
_Avoid_: Constant Driver

**Series Resistor**:
A resistor between two signal Nets (the ZX80's data bus, P5u): a Net that nothing else drives takes the other's value through it, weakly, while a Net with a Driver of its own keeps it. One resistor network resolves them all; between two weak values on a Net, the one through less resistance wins (a pull-up on a joined Net counts as one of them, not as a Tie).

**Coupling Capacitor**:
A capacitor (`C`, P5v) between a Net a chip drives and a Net with a pull resistor: each edge of the driven Net appears on the pulled one as a pulse of the pull's ohms times its farads (the ZX80's C11 with R25 make the 74LS165's load pulse). Anywhere else a capacitor does nothing the simulation shows.

**Joined Pin**:
The resistor network's Pin on a Net that a Series Resistor touches (P5u), as the switch network has one on each Net a Switch is on (a Net may have both): it drives the Net weakly and is woken only by the Net's other Drivers.

**Floating**:
The state of a Net with no Driver.
_Avoid_: Open, undriven

**Contention**:
Two Drivers of equal strength forcing a Net to different values.
_Avoid_: Bus fight, conflict

**Sample**:
One change of a watched Net: its time and new Logic Value, as the Runner records it for the devices thread (`src/sim/recorder.hpp`).

**Slice**:
The stretch of simulated time the Runner runs between two looks at its commands (1 ms): pauses, Repairs' power-off and new watches take effect at a slice's end.

**Consumer**:
What the devices thread feeds Samples to: an output device (the TV, the LEDs) or an Instrument's engine (`src/runner/consumer.hpp`). Each watches its Nets and is told each Slice's end.

## Faults and repairs

**Fault**:
A physical defect in one object (Trace, Joint, Socket contact, Pin, or Component internals). It stays with that object when it moves, and never belongs to a Net or a Symptom.
_Avoid_: Bug, error, failure

**Symptom**:
Observable misbehaviour that emerges from a Fault when the Machine runs. Symptoms are never scripted.
_Avoid_: Effect, glitch

**Stuck-at Fault**:
A Fault where a Component's output Pin always drives 0 or always drives 1. On a bidirectional Pin it forces the Net and the chip reads the stuck value too.
_Avoid_: Dead output

**Stuck Cell**:
A Fault inside a memory chip: one bit of one word always reads 0 or 1, whatever is written. A **Stuck Column** is that bit stuck in every word. In a chip with several memories (the 6530's ROM and RAM), it is in one of them, named. Repaired by a swap.
_Avoid_: Bad bit, dead bit

**Switch Fault**:
A Fault of a Switch: stuck closed (its Nets joined from power-on, a press changes nothing) or never closes (a press does nothing). Repaired by a swap.
_Avoid_: Dead button, sticky key

**Short**:
A Fault that joins two Nets, or a Net to GND/VCC, that should be separate.
_Avoid_: Bridge (that is one kind, the Solder Bridge)

**Solder Bridge**:
A Short made by solder joining two neighbouring pins of one chip on the solder side.
_Avoid_: Blob, splash

**Rail**:
A Net that carries a supply voltage from the power connector, such as VCC (5 V) or GND (0 V).
_Avoid_: Power line, supply net

**Powered**:
A chip, or a device fed through its Connector (a paddle controller), whose every power Pin sits on a Rail of the voltage it expects; an Unpowered one drives nothing.

**Dead Rail**:
A Net joining supply pins of different voltages, such as VCC shorted to GND; it reads 0 V and powers nothing.
_Avoid_: Shorted supply

**Broken Trace**:
A Fault where a Trace is cracked, splitting one Net in two. Drawn as a 12 µm hairline across the copper, square-edged, seen only near full zoom (the user, 2026-11-18): it is found with the probe, not by eye.

**Intermittent Fault**:
A Fault that only sometimes breaks the connection, such as a loose Socket contact or a cracked Joint.

**Timing Fault**:
A Fault where the logic is correct but a signal arrives too late or too early, such as a Slow Part or a setup/hold violation.

**Slow Part**:
A Component that works logically but switches too slowly for the circuit.

**Wrong Part**:
A Component of a different Part than the one the Board expects.
_Avoid_: Wrong IC

**Repair**:
A physical change the player makes to fix a Machine: swapping a Component, cutting or mending a Trace, adding a Bodge Wire, or resoldering a Joint.
_Avoid_: Fix, patch

**Cut**:
A gap the player makes across a Trace, splitting it like a Broken Trace; a Repair, undone by mending.
_Avoid_: Break (that is the Fault)

**Crack**:
A gap across a straight Trace, at a point on its centre line: a Broken Trace (a Fault, a Puzzle's `crack` line) or a Cut (a Repair). The code's word for the gap itself, whichever made it (`Crack`, `CrackBoard`, `CrackMark`); mending removes either.

**Golden Machine**:
The fault-free version of a Machine, used as the reference a repaired Machine must behave like: a completed Puzzle builds its Circuit (Repair Validation).
_Avoid_: Reference machine (that means the real-world original)

**Reference Machine**:
The real historical computer a Machine is inspired by and checked against, such as the ZX80 for the BW-80.
_Avoid_: Original

## Puzzles

**Puzzle**:
A Machine with one or more placed Faults, presented to the player to diagnose and Repair.
_Avoid_: Scenario, level, case

**Launcher**:
The screen `bw_ui` opens with no file (P7a, its look P7b): the title BODGEWIRE in Audiowide, lit as a cyan neon tube, then two lanes. The top one shows the selected Machine live: its Board framed on its outputs (its LEDs and digits, with the Board around them; a small Board whole), or its monitor large with a corner of its Board; its name and its User Manual's first paragraph on the right; the previous and next Machines' Stills at the edges, the lane gliding between them. The bottom one, on the Shelf's wood, holds its post-its: "Start the Machine" (no fault, a white index card) first and pinned, then its Puzzles (title and Brief) easiest first, the last one in view cut at the edge when more follow; every post-it the same height, a longer Brief scrolled by the wheel over it. The selected post-it casts a deeper shadow and runs above from power-on, untouched (no keys, no knobs). Left and right move in a lane, down and up change lane, the selection in the lane with the keys underlined in gold (the picture, with gold chevrons beside it for the keys that move it, clickable too, or the post-it), a click on a post-it selects it. A drag anywhere above the wood (on a picture, the paragraph, the title or the edges) moves the pictures, which settle on the nearest Machine, or on the next one its way once it went a tenth of the way there (a finger's swipe, 2026-11-25); a sideways drag on the post-its moves their row, which then stays where it was put until another post-it is selected, and a drag up or down on a long Brief scrolls it (P8f). The Machine's underline tells how well it keeps up (2026-11-24): gold when it could run at 1× or faster, orange under that, red under 0.3×, each with a margin so a Machine at a limit does not flicker; a lighter sentence under its paragraph gives that headroom (simulated time over the time its simulation thread works): "On this computer it runs at full speed: it could go 13× as fast as the real one.", or "…at 34% of the real one's speed."; Enter, Space or a click on the selected post-it opens the bench on it, a click on the selected Machine's picture on the Machine (no Puzzle; 2026-11-25); the Shelf's arrow comes back. After a Machine's Puzzles, a gap apart, one more post-it (P8d): "Random fault" and a big "?". Selected, it runs the working Machine above (a faint "?" over the picture); opened, it first makes a Procedural Puzzle: the wood dims and a line about breaking a computer changes every second ("Spilling coffee on the board..."), until the bench opens on the Fault made; Esc or the plate's Stop button stops the making, and nothing else does anything meanwhile. A Puzzle the player has repaired keeps the owner's blue tick on its post-it, across launches (Saved progress, P8e); while one is ticked, "Delete: forget the ticks" shows faintly at the title band's right, and Delete, Backspace or a click on it asks "Forget every repaired Puzzle?" first. Every Machine and Puzzle open: no scores, no unlocking, no difficulty.
_Avoid_: Menu, level select

**Still**:
A Machine's Board drawn unpowered in the Launcher (P7b): its LEDs and digits dark, its monitor off, framed as its live picture is. The previous and next Machines show as Stills at the Launcher's edges. Loaded in the background when the Launcher first shows.
_Avoid_: Thumbnail, preview

**Brief**:
The player-facing text of a Puzzle: a title and the Symptom as a customer would report it. It never names the Fault. In `bw_ui` a post-it stuck on the Board near its top-left corner, moving with the Board; dragged elsewhere on it, it stays there for the bench (P6o); its × puts it away into the Shelf. Once the Puzzle is completed (Repair Validation), a blue tick and "Works again, thank you!" under the text; the bench's own, so a Puzzle reopened starts without it, though its Launcher post-it keeps its tick (Saved progress). A Procedural Puzzle's is the same on every Machine but for its last sentence (P8d): the title "Random fault", "It worked, and now it does not. Nobody knows what happened to it.", then what its Machine's Vetting Script asks of the player ("Hold ROLL, then let go."), when it asks anything.
_Avoid_: Description, hint

**User Manual**:
The per-Machine owner's document: what the Machine does when it works, seen from outside, and how to power it. Always available.
_Avoid_: Help, documentation

**Service Manual**:
The per-Machine list of each Component's role on this Board, a divider of the manuals binder; each chip's role also heads its datasheet page there. Authored, not generated by the game.
_Avoid_: Help, documentation

**Datasheet**:
Our simplified per-Part document: what the Part does and what each pin is for. In the manuals binder (P6e): a chip's page shows its pinout with its Nets above its Datasheet; an index lists the Machine's Parts.
_Avoid_: Spec sheet

**Contents**:
What a ROM chip holds (P6p), the binder's fifth divider, shown once a memory chip's window (the 2716's quartz window, a mask ROM's die seen while x-rayed) is clicked, whatever the tool. Three views: Hex, with a text column in a character set (ASCII, PETSCII, the ZX80's); Code, a disassembly in a CPU's code (6502, Z80) at the address the CPU sees the chip; Glyphs, the words drawn as a character set. Each starts from the Machine's defaults (`roms/contents.txt`: its CPU and character set, each chip's address and where its glyphs start) and keeps the player's settings for the bench. It shows the cells as the chip holds them now, a stuck cell included, a swap clearing it at once; a pin Fault does not show there.
_Avoid_: dump, memory view

**Saved progress**:
The Puzzles the player has repaired, kept across launches (P8e): each by its path under `machines/` (`030-dice/puzzles/01-dead-button.bwp`), saved the moment the bench's Brief gets its tick, never taken back by a later Repair. Kept in `progress.txt` in SDL's pref path (`~/Library/Application Support/bodgewire/bw_ui/` on the Mac), on the web in the browser's `localStorage`; `bw_ui --progress FILE` uses that file instead, and a `--screenshot` run without it none. Shown as the owner's tick on the Launcher's post-its; a Random fault and the working Machine are never saved. Forgotten all at once from the Launcher, asked first.
_Avoid_: Save game, score, completion

**Thank-you Card**:
The owner's white index card that opens in the middle of the bench, once per bench, 3 s after a power-on of a completed Puzzle (P8c): "Repaired!", the Puzzle's title, and three buttons: Back to the Board, Next (the Machine's next Puzzle, when there is one; on a Procedural Puzzle "Another random fault", made by the Launcher) and Back to the Machines. The Machine keeps running behind it.
_Avoid_: win screen, popup

**Procedural Puzzle**:
A Puzzle made when the player asks for one (P8d): one Fault drawn at random on a Machine, kept only if it shows (Vetting). The player reads "Random fault": the Launcher's last post-it on each Machine makes one each time it is opened. A kind of Fault is drawn by weight (stuck pin 35, crack 32, bridge 12, dry Joint 10, stuck column 6, stuck cell 3, switch 2, among those the Machine has something for), then one of the Machine's objects of that kind, each as likely; five of a kind that do not show, and another kind is drawn. It has no file: its bench is its Machine's Vetting Script opened with the Fault, forgotten when the bench is left. Its Brief is fixed; the binder's Give up page shows its `fault` line; Repair Validation ends it as any Puzzle. `bw random FILE.bwr --seed N` makes one from the terminal.
_Avoid_: Random puzzle, generated puzzle (in docs and code; "Random fault" is the player's word)

**Authored Puzzle**:
A Puzzle whose Faults are hand-placed, with designed diagnostic clues.

**Vetting**:
Checking that a candidate Fault shows (P8d): the Machine is built with it and run headless from power-on, seed 1, its Vetting Script's inputs played at their times; at each capture time what it shows (its TV's frame, or each lamp's brightness on a Machine without a TV) is compared with what the Golden Machine shows there, run once the same way. It shows when the picture is lost, when 100 cells of the picture are off by more than a quarter, or when a lamp is off by more than a quarter of the brightest fault-free lamp; the first capture time that shows ends the run. A Fault that shows nothing is thrown away: no Procedural Puzzle is made of it.
_Avoid_: Validation (that is for Repairs); capture alone (that is the Logic Analyzer's)

**Vetting Script**:
A Machine's `puzzles/random.bwr` (P8d), one a Machine: what a Vetting plays and when it looks (`press SW1:300ms:800ms`, `knob J2:87%:400ms`, `type J3:350ms:PRINT 42\n`, `capture 500ms`), and the `task` a Procedural Puzzle's Brief asks of the player. Its path names the Machine's Random fault wherever a Puzzle's file names a Puzzle: `bw_ui machines/NNN-name/puzzles/random.bwr` opens a bench on one.
_Avoid_: Random fault file

**Repair Validation**:
The game checking by itself that a Puzzle is completed: the player's Machine builds the same Circuit as the Golden Machine. No Fault is left in a Component or a Joint, and the Copper gives the Golden Machine's Nets, whichever Repairs made them: a Bodge Wire across a Broken Trace counts as a mend. No outputs are compared, and no Puzzle has a rule of its own. It is never wrong when it says "completed"; a redesign that works it does not see, so there the player still decides. Decided and built 2026-11-24 (P8c, ARCHITECTURE.md, Repair validation; `Machine::completed()`): 3 s after a power-on of a completed Machine, the post-it gets the owner's tick (it goes when a later Repair breaks the Machine again, but the saved one stays: Saved progress) and, once per bench, the Thank-you Card opens. Before P8c the player decided when a Machine was fixed (P3f).

## Instruments and views

**Instrument**:
A tool the player measures with: Logic Probe, Oscilloscope, Logic Analyzer, Multimeter. On the Shelf the four share a tray, the Logic Probe first (2026-11-20: by what it does, though it is held, not brought out: no window, its readout drawn on the Shelf). The Logic Probe and the Multimeter are held (P8g: no window); the Logic Analyzer and the Oscilloscope are Equipment.
_Avoid_: Tool (too broad); equipment (broader: also the TV, keyboard, manuals)

**Shelf**:
The strip along the bottom of `bw_ui`'s window (P6c, regrouped in P6o), each group in its own recessed tray: the way back to the Launcher (an arrow, P7a: at once, or asked first when a repair or a pencil mark would be lost), the controls (the power switch, the run switch `‖ · ¼× · 1× · Max` (¼×: slow motion, a simulated second every 4 s, to watch a fast fault, 2026-11-25), the speed reached in place of `Max` while running there, and in place of the chosen speed with the slider red when the run stays under 0.9 of it, the Reset: every Repair undone, the pencil marks kept, asked first, power-cycled when on; grayed with no Repair), the Board's tools (grease pencil, chip puller, soldering iron, cutters, wire) and the Instruments (Logic Probe, Logic Analyzer, Oscilloscope, Multimeter) from the left; at the right end this Machine's brief (only on a Puzzle), binder and Machine Slot; a hint between. Each item in the same place on every Machine; one that does not apply is grayed, not hidden, but the brief, left out without a Puzzle. No keys: the PC keys go to the Machine (Typing).

**Machine Slot**:
The Shelf's last item (P6o): the Machine's own devices, its TV, keyboard and knob box, drawn together, one cell each, each lit while out (a grayed monitor on a Machine with none). A click brings them all out (any away), to the front (some behind), or puts them all away (all out in front); a double-click pops them all out to desktop windows, or all back in (where there are none, the web build, the slot's first object fills the bench, and goes back: P8f). Each object keeps its own `×`.

**Equipment**:
The objects brought out from the Shelf or put away: TV, keyboard, Logic Analyzer, Oscilloscope, the manuals binder, a paddle's knob. One that is out is drawn lit on the Shelf. Each comes out first at its default place on the bench, then where and as large as the player left it for the bench (P6d; no way back to the default place, P6o); a double-click on its shelf drawing (the Machine Slot's for the TV, keyboard and knob box), or on its own drag zone (the Logic Analyzer's title bar, a case's body off its controls (the Oscilloscope's since P8h), the binder's cover), sends it to a desktop window of its own, beside the main one, drawn the same, and a second double-click brings it back where it was on the bench (popped out, P6j, P6o; `--pop` at start); the Instruments have two presets, minimal and normal, from a button in the Logic Analyzer's title bar and on the Oscilloscope's case (P8h), and on the bench a third, full: the bench filled, the next press back to minimal where the Instrument was (P8f); every object has a drawn grip in its bottom right corner (28 px, answering within 44) and is resized by a Finger's pinch too, but the keyboards, where two Fingers type; an object's first size is shrunk to fit a small bench; where there are no desktop windows (the web build) a double-click fills the bench with the object instead, and a second puts it back; the TV, keyboard, knob and manuals binder are drawn cases with no title bar, dragged by their body and put away by the × on the case; the cases keep their proportions, but the binder, resized freely (P6o). The TV (P6f) is a monitor in a small 1970s set's look: `H-POS` and `V-POS` move the picture, `BRIGHT` and `CONTRAST` set its tone, its own `POWER` switch and red lamp; it shows what a real monitor shows (the line's sync and back porch and the vertical retrace hidden), and an unlabelled trimmer below the strip shows the whole frame (underscan); a round loupe follows the mouse over the picture, its pixels six times larger and sharp, the cursor hidden (2026-11-22); the controls keep their turns for the bench. The binder (P6e) has four dividers (User Manual, Service Manual, Datasheets, Repairs), and a fifth, Contents, once a memory chip's window is clicked (P6p), and a sixth, Give up, on a Procedural Puzzle's bench only (P8d): its page asks, then shows the Fault's `fault` line for the rest of the bench, copied by a click; it comes out at the User Manual and turns to a chip's datasheet when a chip is selected. Its tabs keep 50 px apart, closer in a binder too short for them all.
_Avoid_: instrument (for the TV, keyboard or manuals)

**Focus**:
What a click on a pin does: exactly one Board tool or Instrument owns it, shown by a gold frame on its Shelf drawing, the only gold in `bw_ui` (P6o: never for keys). Clicking an Instrument gives it the Focus; taking a tool takes it back; putting the focused Instrument away gives it to the Logic Probe. A tool that needs the power off (puller, iron, cutters, wire) works with the power on too: its repair asks first, then power-cycles the Machine around it (P6o). A pin link clicked in a manual or the chip panel goes to the Focus owner too, when it is the Logic Probe or an Instrument: the pin is probed, clipped, hooked or touched with the red lead, flashed in place, the Board view unmoved; a double-click also frames it, `Cmd`-click only frames it (2026-11-18). Not ImGui's keyboard focus (P6c).

**Logic Probe**:
An Instrument showing the Logic Value and activity of one Net.

**Oscilloscope**:
An Instrument showing a few Nets over time at fine timing resolution. Drawn as an HP-like case with no title bar (P8h): a strip of keys along its top (the Bodge Wire logo; TIME / DIV and POSITION round red LED windows, the timebase's opening a grid by unit; the AUTO | NORM rocker and the TRIG'D lamp; the size key), its screen in a bezel under it, one Band per Hook, each named on its plate; dragged by its body, resized freely.
_Avoid_: Scope (fine in speech, not in docs)

**Logic Analyzer**:
An Instrument recording many Nets at once, with Triggers and a history from before the Trigger.

**Multimeter**:
An Instrument reading voltage levels and continuity, held like the Logic Probe (P8g): no window. The power switch is its mode: volts while the Machine is powered (black on GND, implied), continuity while it is off. Pressed on copper with it in hand, its LCD rises above its drawing on the Shelf and reads what the red Test Lead touches until the press ends (dashes while paused: a paused Board cannot be read); a pin link held in a manual is the pin held. In continuity a click clips the black Test Lead first; a click on the clip, or the power going on, lifts it; the LCD shows `bEEP` with its sign, lit green, when the two are connected, `----` when not.

**Trigger**:
A condition, such as an address appearing on a Bus, that marks when an Instrument should capture.

**Channel**:
One input of an Instrument, recorded as one waveform.

**Clip**:
A Logic Analyzer lead attached to one pad; it feeds one Channel.
_Avoid_: Probe (that is the Logic Probe)

**Hook**:
An Oscilloscope lead attached to one pad; it feeds one Channel and has its Band. On the Board a disc in its Channel's colour, numbered by its Band's place on the screen (P8h: 1 the top one; renumbered when a Band goes or moves).
_Avoid_: Probe (that is the Logic Probe)

**Test Point**:
A place on the Board the Service Manual names in its `Test points` section, with what a working Board reads there (P6i). Named by a pin (`` `U5.QD` ``), not a Net, so a click probes it: a Net can be split by a fault, and its name does not say which side.

**Test Lead**:
One of the two Multimeter tips, on any copper: the red one held while a press lasts, the black one clipped by a click in continuity (P8g) and left there.

**Sweep**:
One pass of the Oscilloscope across its screen, starting at a Trigger.

**Band**:
One Hook's strip of the Oscilloscope's screen (P8h), with its own 0 to 5 V scale, black between Bands. A plate over its left end names it: its `T` (lit on the Trigger's source, its edge an arrow; a click makes it the source, or flips the edge), the pin, its ×, and its reading (`1 MHz · 50 %`, `low 4 µs`, `H`). The Bands share the screen top to bottom in the order hooked; a plate dragged up or down moves its Band.
_Avoid_: lane (that is the Logic Analyzer's row)

**Capture**:
What a Logic Analyzer recorded around one Trigger: every Channel over the same sample points. Not a Vetting's capture time (a simulated time a Vetting Script names: what a Machine shows there is a `Shown`).

**Channel Group**:
Logic Analyzer Channels read and triggered as one Bus value, shown in hex, such as `U1 A` for the 16 address Channels.
_Avoid_: Bus (that is the circuit's group of Nets)

**Marker**:
A time picked with a click on an Oscilloscope or Logic Analyzer screen, from which the cursor's `dt` is measured. It carries a flag at its top, 28 × 20 px: a click on the flag clears it, as a right-click on the screen does (P8f). A Finger's Marker lands on an edge of the row or Band it is on when one lies within 12 px; on the Oscilloscope the mouse's too, and its cursor (P8h), the cursor's tag there showing its time and the `dt`.

**Pencil Mark**:
A grey dot, cross or check the player puts on the Board with the grease pencil (a china marker), a note for the session (P6h), on the Board only (P6o): a click on bare Board or a part puts a dot, a click on a mark turns it to a check, a cross, then none; a Shift-click or right-click clears it. Drawn whatever the tool, layer or look; one on a chip's body leaves with the chip when it is swapped; nothing is saved.
_Avoid_: Marker (that is the Instruments' time), Mark (alone: a capacitor's printed code is also drawn by `board_marks`)

**See-through Board**:
The single top-down view of a Board showing every Layer at once, the back seen through the board rather than flipped.

**Active Layer**:
The Layer drawn on top and preferred by player actions such as probing, cutting and soldering. Where it has nothing under the cursor, actions fall through to another Layer. Changed by the Flip Corner or the `` ` `` key.

**Flip Corner**:
A dog-ear on the Board's bottom-right corner in `bw_ui` (P6o), 4 mm along each edge (24 px at least), drawn in the other Layer's copper colour: a click on it flips the Board (the Active Layer) whatever the tool; a control, not part of the PCB. No pad of any Machine lies under it. The View Buttons' Flip does the same.

**View Buttons**:
A column of five 44 px buttons at the Board panel's bottom right (P8f), for what had only a key or a mouse gesture: Fit (a double-click on bare Board), Look (Tab: the real or the plain look), Copper (Option held: picks pass under the chips), Hold (the next Board button or key pressed stays down, as on a right-click; only on a Board that has one) and Flip (`` ` ``, the Flip Corner). One that is on is framed in gold. The keyboard's default place leaves the column's lane free.

**Finger**:
A touch on a tablet's screen: `bw_ui`'s pointer in place of the mouse (P8f, `docs/p8f-touch/TOUCH.md`). There is one interface: the controls, their sizes and their wording are the same for both, and a Finger differs only in its input. Two Fingers pinch and pan the Board or a time screen, scroll a page, and resize an object; one held still 0.4 s shows the Loupe (the Board, the TV's glass) or places the cursor (a time screen); it reaches further than the mouse (22 px on the Board, 6 px around a control); and between two touches there is no pointer, so nothing is shown by hovering alone: a tap's highlight and tooltip stay 2.5 s. A repair tool's first tap lights what it would act on ("Cut here: tap again"), the second does it. `--touch` lets the mouse stand in for one Finger.
_Avoid_: Touch mode, mobile version (there is no second interface)

**Loupe**:
A magnifier under the pointer. The TV's (2026-11-22) is round and follows the mouse over the picture, its pixels six times larger; under a Finger held on the glass it is drawn 80 px above it. The Board's (P8f) is a Finger's alone: held 0.4 s, a 140 px square 44 px above the Finger shows the Board three times larger, with the name of what is under it; the Finger slides to aim, and its lift is the click.
_Avoid_: Magnifier, zoom (that is the Board's own)

**Hold Switch**:
The slide switch marked HOLD on a keyboard's case (P8f). While it is on, a click on a key holds it down until it is clicked again, as a right-click does: a chord for a Finger, which has one button.

**Stepper**:
A control for one value out of a list (P8f): −, the value, +, the two buttons 44 px square; + is always the finer step, and a click on the value opens the whole list. It replaces a dropdown: the Logic Analyzer's sample period and its view, the Contents page's width (the Oscilloscope's timebase is its case's keys since P8h).

**Number Pad**:
The keys that open beside a number's field when it is clicked (P8f): the hex digits, a trigger pattern's `X`, or a duration's digits and units, with `<` to take one back and OK. The PC's keys type into it as well. It replaces a text field, which a Finger cannot type in: the web build has no on-screen keyboard.

**Schematic**:
Our own circuit diagram of a Machine, generated from its Nets and hand-adjusted. It is a view only, not a source of connectivity.
_Avoid_: Circuit diagram, original schematic

**Switch**:
A push button or key on the Board: two legs whose Nets are joined while it is held down (closed). A button usually has one leg on a Rail; a key in a matrix joins a row to a column. One switch network joins the Nets of every closed Switch, so a crack, a bridge or a dry pad on a Switch's copper acts as on any copper.
_Avoid_: Contact, push (as nouns)

**Ghost Key**:
A key read as held when it is not: three held keys at three corners of a rectangle in a key matrix join the fourth corner's row and column. Emerges from the switch network; never scripted.
_Avoid_: Phantom key

**Membrane Keyboard**:
The BW-80's keyboard (`BW80KB`): the ZX80's 40 keys in a window, their rows the address lines `A8`–`A15` read during an `IN`, their columns `KB0`–`KB4` pulled low through a diode by a held key. Faultless, as every device behind a Connector; its Faults are on the Board (the pull-ups, the buffer, the copper).
_Avoid_: Touch keyboard

**Keyword Entry**:
The ZX80 editor's way of typing: while the cursor is `K`, a letter key types the keyword printed above it (P: PRINT); after it the cursor is `L` and keys type their letters; THEN, TO, AND, OR, NOT and the symbols are SHIFT with a key. A pasted listing is typed that way by the tokenizer.
_Avoid_: Token entry

**Typing**:
Every key the player presses goes to the Machine (P6o, replacing P6g's bench shortcuts and F2): its keyboard (either kind), its Board keys (a PC key held down holds the key with that `pc=`, several at once) and its knobs, whether its keyboard and knob box are out, put away or popped out. Escape is a key of the Machine. Four exceptions: a text field being edited (the analyzer's pattern, the scope's delay) takes the keys until it loses focus; Cmd combinations are the app's (quit, and the BW-2001's paste); Tab flips the plain look; `` ` `` flips the Board. A Machine with none of these ignores keys. Nothing shows the keys: there is no choice.
_Avoid_: Keyboard capture (in player text)

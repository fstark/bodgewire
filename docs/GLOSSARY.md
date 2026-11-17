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
What the player turns on a paddle controller, from 0 at the left stop to 1 at the right; the controller answers each trigger with a pulse whose length follows it.
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
A Fault where a Trace is cracked, splitting one Net in two.

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

**Golden Machine**:
The fault-free version of a Machine, used as the reference a repaired Machine must behave like.
_Avoid_: Reference machine (that means the real-world original)

**Reference Machine**:
The real historical computer a Machine is inspired by and checked against, such as the ZX80 for the BW-80.
_Avoid_: Original

## Puzzles

**Puzzle**:
A Machine with one or more placed Faults, presented to the player to diagnose and Repair.
_Avoid_: Scenario, level, case

**Brief**:
The player-facing text of a Puzzle: a title and the Symptom as a customer would report it. It never names the Fault.
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

**Procedural Puzzle**:
A Puzzle whose Faults are randomly generated and then vetted.

**Authored Puzzle**:
A Puzzle whose Faults are hand-placed, with designed diagnostic clues.

**Vetting**:
Running a candidate Fault headlessly to reject it if it shows no Symptom or is trivial.
_Avoid_: Validation (that is for Repairs)

**Repair Validation**:
Checking a Repair by running the player's Machine against the Golden Machine and comparing their outputs. Dropped for now (P3f): the player decides when a Machine is fixed.

## Instruments and views

**Instrument**:
A tool the player measures with: Logic Probe, Oscilloscope, Logic Analyzer, Multimeter. On the Shelf the Logic Probe sits among the Board's tools (it is held, not brought out); the other three are Equipment.
_Avoid_: Tool (too broad); equipment (broader: also the TV, keyboard, manuals)

**Shelf**:
The strip along the bottom of `bw_ui`'s window holding drawings of the controls (power, run, speed, layer, fit), the Board's tools (probe, chip puller, soldering iron, cutters, wire) and the Equipment, each in the same place on every Machine; an item that does not apply is grayed, not hidden (P6c).

**Equipment**:
The objects brought out from the Shelf or put away: TV, keyboard, Logic Analyzer, Oscilloscope, Multimeter, the manuals binder, a paddle's knob. One that is out is drawn lit on the Shelf. Each comes out at its default place on the bench, every time, at the size the player last gave it (P6d); the Instruments have two presets, minimal and normal, from a button in their title bar; the TV, keyboard, knob and manuals binder are drawn cases with no title bar, dragged by their body and put away by the × on the case. The binder (P6e) has four dividers (User Manual, Service Manual, Datasheets, Repairs), comes out at the User Manual and turns to a chip's datasheet when a chip is selected.
_Avoid_: instrument (for the TV, keyboard or manuals)

**Focus**:
What a click on a pin does: exactly one Board tool or Instrument owns it, shown by a gold frame on its Shelf drawing (red while the tool cannot work, the power being on). Clicking an Instrument gives it the Focus; taking a tool takes it back; `Esc` gives it to the Logic Probe. Not ImGui's keyboard focus, which the windows' `focused()` and Typing Mode mean (P6c).

**Logic Probe**:
An Instrument showing the Logic Value and activity of one Net.

**Oscilloscope**:
An Instrument showing a few Nets over time at fine timing resolution.
_Avoid_: Scope (fine in speech, not in docs)

**Logic Analyzer**:
An Instrument recording many Nets at once, with Triggers and a history from before the Trigger.

**Multimeter**:
An Instrument reading voltage levels and continuity.

**Trigger**:
A condition, such as an address appearing on a Bus, that marks when an Instrument should capture.

**Channel**:
One input of an Instrument, recorded as one waveform.

**Clip**:
A Logic Analyzer lead attached to one pad; it feeds one Channel.
_Avoid_: Probe (that is the Logic Probe)

**Hook**:
An Oscilloscope lead attached to one pad; it feeds one Channel.
_Avoid_: Probe (that is the Logic Probe)

**Test Lead**:
One of the two Multimeter tips (red, black), held on any copper.

**Sweep**:
One pass of the Oscilloscope across its screen, starting at a Trigger.

**Capture**:
What a Logic Analyzer recorded around one Trigger: every Channel over the same sample points.

**Channel Group**:
Logic Analyzer Channels read and triggered as one Bus value, shown in hex, such as `U1 A` for the 16 address Channels.
_Avoid_: Bus (that is the circuit's group of Nets)

**Marker**:
A time picked with a click on an Oscilloscope or Logic Analyzer screen, from which the cursor's `dt` is measured.

**See-through Board**:
The single top-down view of a Board showing every Layer at once, the back seen through the board rather than flipped.

**Active Layer**:
The Layer drawn on top and preferred by player actions such as probing, cutting and soldering. Where it has nothing under the cursor, actions fall through to another Layer.

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
The BW-80's keyboard (`MEMBRANEKB`): the ZX80's 40 keys in a window, their rows the address lines `A8`–`A15` read during an `IN`, their columns `KB0`–`KB4` pulled low through a diode by a held key. Faultless, as every device behind a Connector; its Faults are on the Board (the pull-ups, the buffer, the copper).
_Avoid_: Touch keyboard

**Keyword Entry**:
The ZX80 editor's way of typing: while the cursor is `K`, a letter key types the keyword printed above it (P: PRINT); after it the cursor is `L` and keys type their letters; THEN, TO, AND, OR, NOT and the symbols are SHIFT with a key. A pasted listing is typed that way by the tokenizer.
_Avoid_: Token entry

**Typing**:
The `bw_ui` mode in which every key the player presses goes to the Machine's keyboard instead of the tools: on while the keyboard window has focus, F2 toggles it. Escape is a key of the keyboard. On a Machine with Board keys and no keyboard, F2 turns it on for the Board: a PC key held down holds the key with that `pc=`, several at once.
_Avoid_: Keyboard capture (in player text)

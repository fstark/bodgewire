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

**Component**:
One placed instance of a Part on a Board, identified by its reference (U12).
_Avoid_: Chip, IC (when meaning a specific placed one)

**Pin**:
One electrical terminal of a Component.

**Socket**:
A holder on the Board that a Component plugs into, making it removable without soldering.

**Connector**:
A Component through which power enters a Board or signals leave it, such as the power connector or the video connector. Each type has one fixed job.
_Avoid_: Port, jack, header

**Joint**:
A solder connection between a Pin (or Socket contact) and the Copper.
_Avoid_: Solder point

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
The part of a Machine the player watches it through: a TV picture or LED digits.
_Avoid_: Display, screen (when meaning the device in general)

**Monitor**:
The small ROM program that runs on a Machine with a CPU; there is no BASIC.

## Signal states

**Logic Value**:
One of `0`, `1`, `Z` (high impedance, nothing driving) or `X` (unknown or conflicting).
_Avoid_: Level, voltage

**Driver**:
A Pin's contribution to the value of its Net: what it drives and how strongly. Also, a Pin that drives its Net: an output, a pull-up/pull-down or a supply.

**Load**:
A Pin that only reads its Net, such as a gate input or a chip's power pin.
_Avoid_: Sink, receiver

**Drive Strength**:
How hard a Driver forces its Net, so stronger Drivers win over weaker ones (e.g. an output beats a pull-up).

**Tie**:
A constant Driver attached to a Net rather than a Pin: a supply rail (GND/VCC) or a pull-up / pull-down resistor.
_Avoid_: Constant Driver

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
A Fault where a Component's output Pin always drives 0 or always drives 1.
_Avoid_: Dead output

**Short**:
A Fault that joins two Nets, or a Net to GND/VCC, that should be separate.
_Avoid_: Bridge

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
The per-Machine owner's document: what the Machine does when it works and how to connect and use it. Always available.
_Avoid_: Help, documentation

**Service Manual**:
The per-Machine repairer's document: how the Machine works, the role of each Component, and optional Troubleshooting steps. Authored, not generated by the game. A future hard mode may withhold it.
_Avoid_: Help, documentation

**Datasheet**:
Our simplified per-Part document: what the Part does and what each pin is for.
_Avoid_: Spec sheet

**Procedural Puzzle**:
A Puzzle whose Faults are randomly generated and then vetted.

**Authored Puzzle**:
A Puzzle whose Faults are hand-placed, with designed diagnostic clues.

**Vetting**:
Running a candidate Fault headlessly to reject it if it shows no Symptom or is trivial.
_Avoid_: Validation (that is for Repairs)

**Repair Validation**:
Checking a Repair by running the player's Machine against the Golden Machine and comparing their outputs.

## Instruments and views

**Instrument**:
A tool the player measures with: Logic Probe, Oscilloscope, Logic Analyzer, Multimeter.
_Avoid_: Tool (too broad), equipment

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

**Capture**:
What a Logic Analyzer recorded around one Trigger: every Channel over the same sample points.

**See-through Board**:
The single top-down view of a Board showing every Layer at once, the back seen through the board rather than flipped.

**Active Layer**:
The Layer drawn on top and preferred by player actions such as probing, cutting and soldering. Where it has nothing under the cursor, actions fall through to another Layer.

**Schematic**:
Our own circuit diagram of a Machine, generated from its Nets and hand-adjusted. It is a view only, not a source of connectivity.
_Avoid_: Circuit diagram, original schematic

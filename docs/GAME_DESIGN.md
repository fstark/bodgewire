# Bodgewire — Game Design

*Every fault has a cause.*

Hardcore electronics detective game: diagnose and repair faulty late-70s / early-80s 8-bit computers, simulated at the digital signal level. Not a casual repair sim, not a cozy restoration game.

**Scope**: digital only, down to IC pins. Logic faults first; timing faults later, if they can stay fair; no analog (for now).

**Not now**: game structure (jobs, economy, scoring). Feasibility first.

Technical design: [ARCHITECTURE.md](ARCHITECTURE.md).

## Core principle

- **Faults are physical, symptoms emerge.** A fault is a hardware change ("RAM pin 7 disconnected from net VIDEO_A3"), never a scripted symptom ("video = corrupted").
- One fault, many symptoms: a dead address line can corrupt memory, garble the screen and crash the CPU, with none of it coded.
- Reference case: the screen shows garbage. Video RAM, address decoder, floating data line, cracked trace, or intermittent socket contact?

## Loop

Faulty machine + symptom → run, observe, measure, study the board and the manuals → deduce the faulty object → repair → machine works.

## Tools

- **Board** view (see-through), with the chip pinouts; hovering a net marks what drives it.
- **User Manual** per machine (what it does, seen from outside), each chip's role on the board, and simplified **datasheets** per part.
- **Logic probe**: level/activity of one net.
- **Oscilloscope**: timing between signals; required for timing faults. Over the logic analyzer: exact edge times (ns delays between two signals, no sampling), floating and X levels visible, live without arming.
- **Logic analyzer** (key tool): many channels, triggers, pre-trigger history. E.g. probe A0–A15, D0–D7, R/W and a chip select, trigger on an address, inspect the bus activity leading up to it.
- **Multimeter**, **component inspector**, **parts inventory**, **machine output** (its TV picture or LED digits).

## Repairs

- Remove / insert / replace ICs, including wrong or slower parts.
- Repair or cut traces, add bodge wires, (re)solder joints.
- The player can't damage parts (for now).

## Faults

| Fault | Difficulty |
|---|---|
| Broken trace | easy–medium |
| IC output stuck at 0/1 | easy–medium |
| Bad RAM / ROM bit | medium |
| Short between two lines | medium |
| Wrong IC fitted | medium |
| Intermittent connection (socket, joint) | hard |
| Slow part (propagation delay) | very hard |
| Setup/hold violation | expert |

- A fault belongs to a **physical object** (trace, joint, socket, IC pin, IC internals), not to a net, and moves with it. A CPU that forces A4 low stops doing so once pulled; A4 itself is not grounded.
- Faults inside a chip are part of its model (e.g. a 6502 with dead interrupt logic).
- Output stuck low ≠ pin shorted to GND: same reading in simple cases, different when the pin should be Z.

## Puzzles

- **Procedural**: random logic faults (stuck-at, broken trace, short, bad bit). Candidates are simulated headlessly; symptom-less or trivial ones are rejected.
- **Authored**: hard, hand-placed faults with diagnostic clues. Timing faults are authored only, never random.
- Hard because the machine is complex: good. Hard because the player has no reasonable way to find the fault: bug.
- Timing example: BASIC runs fine, a graphics demo shows vertical artifacts. RAM, counters and address lines look correct on the logic probe; the scope shows the address decoder output switching too late relative to the pixel clock.
- A part replaced by a slower equivalent can break a working machine.

## Machines

Likely custom machines inspired by real ones, named after them: we choose their size, write our own ROMs (a monitor is enough, no BASIC) and stay free to sell one day. Exact replicas remain possible.

Tutorial ladder, by difficulty, each spec in `docs/machines/NNN-name/`:

| # | Machine | Output / Input | Notes |
|---|---|---|---|
| 010 | Counter | 1 LED | 1 MHz crystal → 20-bit counter chain → LED blinking once a second |
| 020 | LED chaser | LEDs | shift register |
| 030 | Dice | LEDs, button | fast counter stopped by the player's release; decoding |
| 040 | Stopwatch | 7-segment digits | decade counters, decoder chip |
| 050 | Test-pattern generator | TV | crystal + TTL counters → sync + bars/grid. First playable |
| 060 | Bouncing ball | TV | Pong's ball circuit on its own |
| 070 | Rally | TV, paddle controllers | Pong without the score: paddles from off-Board controllers, hits, misses, serves |
| 080 | Pong | TV, paddle controllers | TTL only, like the 1972 arcade game: Rally plus score digits on the TV and the game's rules |
| 090 | Message display | TV | fixed text: sync counters → message ROM → character ROM → shift register; the BW-2001 video chain with ROM instead of RAM |
| 100 | TV typewriter | TV, ASCII keyboard | inspired by the 1973 TV Typewriter (half of the Apple 1): video RAM written at a cursor from an ASCII keyboard; RAM and a shared bus before any CPU |

- LEDs, digits and buttons whose arrangement belongs to the Board are Components on it (clicked or watched in the board view, faulty like any other); standard devices (TV, keyboard) sit off the Board behind a Connector.

Computers, after the ladder:

| Machine | Inspired by | Notes |
|---|---|---|
| BW-1 | KIM-1 | 6502 single-board computer: LED digits + keypad, no video |
| BW-80 | ZX80 | Z80 + ~20 standard chips, no custom IC. The CPU makes the picture, sync included: nothing on screen unless CPU, ROM and RAM work, but every gate can be probed |
| BW-2001 | PET 2001 | 6502, TTL text video independent of the CPU (garbage-screen puzzles), keyboard |

- BW-80 video: the CPU "executes" the display file while logic feeds it NOPs and turns each character code into pixels through the character ROM.
- BW-2001 video chain: counters (sync + addresses) → video RAM → character ROM → shift register → pixels.
- Analog sub-circuits (TV/RF output, cassette, sound, power-on reset, RC pulse shapers, some clock generators) get logic-only equivalents; custom chips (ULAs, PLAs) get behavioural models.
- Later inspirations: Apple II, ZX Spectrum.

## Presentation

- Pure 2D, no 3D. Panels: board, manuals, instruments, inspector, inventory, machine output.
- **See-through board** instead of flipping: one top-down view with every layer in the same frame (the back as seen through the board, i.e. mirrored compared with a physical flip), so a pin sits at the same spot on every layer. Per-layer colour and opacity; the hovered net highlighted on all layers; actions (probe, cut, solder) prefer the active layer and fall through to another where it has nothing under the cursor. Scales beyond 2 layers. Real-world equivalent: holding the board up to a lamp.
- **Schematic**: none on small machines; the board, pinouts and datasheets do its job. Reconsidered for big machines (BW-80 and up), then ours, generated from the netlist and hand-adjusted.
- **Probing** on the board only (realistic).
- Look: dark green PCB, scope traces, amber highlights, vintage terminal typography.

## Open questions

- Timing faults: worth it, or too brutal?
- Visual layout: realistic or stylised board? (Authoring: see [ARCHITECTURE.md](ARCHITECTURE.md).)
- *Bitrot* as the name of an expert mode / nasty-fault campaign?
- "Bodgewire" not yet checked against existing games and trademarks.

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

Faulty machine + symptom → run, observe, measure, study the board and the manuals → deduce the faulty object → repair → machine works → the game acknowledges it (Repair Validation, P8c).

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

- **Procedural**: random logic faults (stuck-at, broken trace, short, bad bit). Candidates are simulated headlessly; symptom-less ones are rejected. Out of the freezer (2026-11-24): ARCHITECTURE.md, P8d.
- **Authored**: hard, hand-placed faults with diagnostic clues. Timing faults are authored only, never random (Timing: P9, after v0.9).
- Hard because the machine is complex: good. Hard because the player has no reasonable way to find the fault: bug.
- Timing example: BASIC runs fine, a graphics demo shows vertical artifacts. RAM, counters and address lines look correct on the logic probe; the scope shows the address decoder output switching too late relative to the pixel clock.
- A part replaced by a slower equivalent can break a working machine.
- **Broken or flaky.** A broken Machine fails the same way at every power-on; a flaky one works, then fails, differently each time. Flakiness has many causes: intermittent contacts (socket, cracked Joint, opened by tapping, flexing or heat), thermal chips (fail warm, work again cooled), timing margins (a slower part, a strobe that glitches while the address changes), floating or contended lines, and flaky memory (weak cells, unstable bits, cells disturbed by a neighbour's write). It needs its own tools: tapping, heat and cold, long runs, RAM test ROMs (which can miss coupling faults). If RAM were its only cause, the player would just swap RAM.
- **Memory Faults pay off where the Machine shows the memory** (a character or message ROM, video RAM): each Fault draws its own picture. In program memory they mostly crash the program.

## Machines

Likely custom machines inspired by real ones, named after them: we choose their size, write our own ROMs (a monitor is enough, no BASIC) and stay free to sell one day. Exact replicas remain possible.

Tutorial ladder, by difficulty, each in its own directory `machines/NNN-name/` (spec in its `docs/`):

| # | Machine | Output / Input | Notes |
|---|---|---|---|
| 010 | Counter | 1 LED | 1 MHz crystal → 20-bit counter chain → LED blinking once a second |
| 020 | LED chaser | LEDs | shift register |
| 030 | Dice | LEDs, button | fast counter stopped by the player's release; decoding |
| 040 | Stopwatch | 7-segment digits | decade counters, decoder chip |
| 045 | Knob meter | 7-segment digits, paddle controller | a knob's pulse length counted and latched into two digits; introduces the paddle controller (a pot + 555 one-shot, off the Board) |
| 050 | Test-pattern generator | TV | crystal + TTL counters → sync + bars/grid. First playable |
| 060 | Bouncing ball | TV | Pong's ball circuit on its own |
| 070 | Paddle tennis | TV, paddle controllers, Start button | TTL only, the game of the 1972 arcade Pong: the 060 ball plus paddles, score digits on the TV and the game's rules |
| 080 | Message display | TV | fixed text: sync counters → message ROM → character ROM → shift register; the BW-2001 video chain with ROM instead of RAM |
| 090 | TV typewriter | TV, ASCII keyboard | built as the 1973 TV Typewriter (half of the Apple 1): a page in MOS shift registers going round with the beam, written at a cursor from an ASCII keyboard when its cell comes round; memory without addresses before any CPU |
| 100 | Heartbeat | LEDs, Reset button | the first processor: a 6502 free-running on a data bus held at NOP, its address lines on LEDs; the free-run test of a dead computer |
| 110 | LED computer | LEDs, Reset button | the first computer: a 6502 running a program from ROM, RAM for its variables and stack, an address decoder and an output latch; a light bouncing along 8 LEDs |
| 120 | BW-1 | six digits, hex keypad, ST, RS, SST | the first computer the player programs: the KIM-1's design, a monitor in a 6530 reading a keypad and scanning multiplexed digits through the same port; a self-test in the other 6530, reached by a wire |
| 130 | BW-2001 | TV (separate drives), PET keyboard (floating) | the PET 2001's board 320137, chip for chip: a TTL video chain reading the video RAM that the 6502 shares (snow), Tiny BASIC, a full-screen editor and a monitor of our own; a self-test reached by a wire on the user port |
| 140 | BW-80 | TV, ZX80 keyboard (floating), expansion bus | the ZX80 chip for chip, on the ZX80's own ROM: the Z80 makes the picture itself, sync included, by executing the display file through NOP gates; keyword-per-key entry; the keyboard's rows are the address lines |

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
- Machine ideas, not yet on the ladder:
  - A dimmer (the user, 2026-11-18): a knob sets the share of the time a 7-segment digit is lit (a paddle's pulse compared with a free-running counter, PWM), so the player sees brightness follow the duty cycle. The Board already draws a segment's brightness from the share of time it is lit, through a cube root (`Perceived`): the BW-1's multiplexed digits, each lit a sixth of the time, look darker than the Stopwatch's, which are lit all the time. It also introduces the idea behind multiplexing, before the BW-1.

## Presentation

- Pure 2D, no 3D. Panels: board, manuals, instruments, inspector, inventory, machine output.
- **See-through board** instead of flipping: one top-down view with every layer in the same frame (the back as seen through the board, i.e. mirrored compared with a physical flip), so a pin sits at the same spot on every layer. Per-layer colour and opacity; the hovered net highlighted on all layers; actions (probe, cut, solder) prefer the active layer and fall through to another where it has nothing under the cursor. Scales beyond 2 layers. Real-world equivalent: holding the board up to a lamp.
- **Schematic**: none on small machines; the board, pinouts and datasheets do its job. Reconsidered for big machines (BW-80 and up), then ours, generated from the netlist and hand-adjusted.
- **Probing** on the board only (realistic).
- Look: dark green PCB, scope traces, amber highlights, vintage terminal typography.

## Open questions

- Timing faults: worth it, or too brutal?
- Visual layout: realistic or stylised board? (Authoring: see [ARCHITECTURE.md](ARCHITECTURE.md).)
- The swap reflex: while a swap costs nothing, "swap every chip" solves every chip Fault. Soldered chips (desoldering takes time, can lift a pad) against socketed ones, limited spares, a clock?
- *Bitrot* as the name of an expert mode / nasty-fault campaign?
- "Bodgewire" not yet checked against existing games and trademarks.

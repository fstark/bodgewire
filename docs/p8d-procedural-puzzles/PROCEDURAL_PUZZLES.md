# P8d — Procedural Puzzles

Roadmap step P8d ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): a "?" post-it at the end of a Machine's Puzzles that makes one up when asked: a random Fault, kept only if it shows. Terminology: [GLOSSARY.md](../GLOSSARY.md).

**A draft** (2026-11-24): the framework the user settled and the research so far. It is completed (Open, below) before the design and the plan are written.

## Why

- The Authored Puzzles run out: 114 in all, a handful per Machine.
- Faults are physical and Symptoms emerge ([GAME_DESIGN.md](../GAME_DESIGN.md#core-principle)): a random Fault whose Symptom shows is a Puzzle by the game's own rule, with nothing scripted. On a Machine whose chips all work towards the picture, most of them end in a white screen, "just like the real thing" (the user).
- Since P8c the game sees by itself that any Puzzle is completed, with nothing written per Puzzle: a Puzzle nobody authored still has an end.

## User decisions

The user, 2026-11-24:

- **The post-it**: one more at the very end of the Machine's post-its, set apart, maybe with a title, and a big question mark. It makes up a Fault and opens the bench on it.
- **Made when asked, not ahead.** A pool made offline (hundreds of Puzzles in a directory, curated, the good ones promoted to Authored Puzzles) is not the goal: it goes stale as soon as a Board changes. Maybe later.
- **The interesting Puzzles are the Authored ones; these are just random.** Nothing hunts for an interesting Fault.
- **The framework, not to be changed unless it has to be.** Making a Puzzle:
  1. pick a kind of Fault, by weight;
  2. make a random Fault of that kind;
  3. if it does not show, make another of the same kind;
  4. after five that do not show, go back to 1.
- **Does it show (Vetting)**:
  1. the Machine with the Fault is powered on;
  2. the Machine's inputs are played at their times;
  3. after a set time its outputs are captured;
  4. they are compared with the Golden Machine's, captured the same way, live, before the Puzzles are made.
- **Per Machine**, in its definition: how long to run, and the inputs with their times.
- **The kinds** are the ones a `.bwp` has: stuck pin, crack, bridge, dry Joint, stuck column, stuck cell, switch. No dead LED (no such Fault exists: a dark lamp is a crack on its Trace or a stuck pin on its driver) and no dead chip (a stuck pin is one pin; an Unpowered chip is a crack on its power Trace).
- **The weights**: the 114 Authored Puzzles' mix is a starting point, no more: it is the taste of the LLM that wrote them, not the user's.
- **A single stuck cell is rare**: it sounds boring.
- **White screens are fine**; no dial between dead and misbehaving Machines (premature).
- **The difference that counts** has a threshold, tuned later; 100 pixels may be too many.
- **On the computers the input is a short typed line ending with NEW LINE** (it runs the tokenizer too). Not a BASIC program typed and run: that only tests the ROM and the RAM, and their Faults show sooner.
- **The typed line is told to the player**: the generated Brief says "do X".
- **The Dice** is captured while ROLL is held; it matters little.
- **The LED Machines** are looked at once `bw run` prints the lamps (`--leds`, 2026-11-24).

Chosen without asking (overturn any):

- A candidate the Machine refuses (a power pin, two pins that are not neighbours, a Trace with nothing crackable) is not one of the five: it costs nothing and is drawn again.
- Inside a kind every physical object is as likely: each chip signal pin, each pair of neighbouring pins, each pad for a crack (then another pad of its Net and a place between them).
- The power-on seed is 1, `bw_ui`'s: untouched, the Vetting run is the run the player sees.
- "Compared" counts what differs (pixels, lamps) against a threshold; a hash only says same or different, and the sample below has differences of 2 pixels.
- A capture is a picture of what the player sees: the TV's frame, and each lamp's brightness (`LedGlow`: the share of the last 20 ms it was lit, a digit's eight lamps each on its own). A LED blinking too fast for the eye is then a dimmer lamp, as the player sees it. A lamp's margin is relative to the Golden Machine's brightness: a multiplexed segment sits near 1/6, not 1.
- Several capture times per Machine, not one: an instant cannot tell a blinking LED from one frozen while lit. Still one run a try.
- The capture times fall where the Golden Machine's picture is stable: 1.5 s into the typed line below, the fault-free BW-80 itself has no locked picture (it is busy with a key).

Starting weights (the Authored Puzzles' mix, the cell moved down):

| Kind | The 114 Puzzles | Start |
|---|---|---|
| stuck pin | 35 % | 35 % |
| crack | 32 % | 32 % |
| bridge | 12 % | 12 % |
| dry Joint | 8 % | 10 % |
| stuck column | 4 % | 6 % |
| stuck cell | 7 % | 3 % |
| switch | 2 % | 2 % |

A kind with nothing to act on (no memory chip, no switch) drops out, the others rescaled. One table for every Machine until one needs its own.

## What the sample shows (BW-80)

`python3 -I tools/fault_sample.py machines/140-bw80 OUT` (2026-11-24): 366 candidates, their kind drawn first, 346 the Machine accepts. Each is run headless for 300 ms, untouched, seed 1; its last TV frame (512 × 320) is compared with the fault-free one.

| Kind | Drawn | Loaded | Nothing | Under 100 px | No sync | Picture, clearly different |
|---|---|---|---|---|---|---|
| stuck pin | 110 | 96 | 20 | 23 | 48 | 5 |
| dry Joint | 60 | 54 | 12 | 11 | 29 | 2 |
| bridge | 60 | 60 | 11 | 3 | 42 | 4 |
| crack | 80 | 80 | 21 | 20 | 33 | 6 |
| stuck column | 16 | 16 | 0 | 0 | 16 | 0 |
| stuck cell | 40 | 40 | 39 | 0 | 1 | 0 |

The 290 pin and copper Faults (the first four rows):

| The frame shows | Untouched, 300 ms | With a line typed, 2.5 s |
|---|---|---|
| Nothing | 64 (22 %) | 41 (14 %) |
| A difference under 100 px | 57 (20 %) | 13 (4 %) |
| A clear difference | 169 (58 %) | 236 (81 %) |

- **Most Faults show, and most of those are a dead screen**: 152 of the 290 lose sync, 99 of them as a plain white frame, 59 of those the same frame to the pixel.
- **The idle screen is one `K`**: the 57 small differences are all inside the cursor's 8 × 8 cell, from 2 to 88 pixels. A hash would accept every one.
- **A typed line fixes most of that.** The 160 candidates under 100 px were run again with `10 REM ABCDEFGHI…` typed from 350 ms and the frame taken at 2.5 s (`--from OUT --time 2500ms --inputs '--type "J3:350ms:10 REM ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"'`; the line is still being typed then, `10 REM ABCDEFGHI` on screen). 44 of the 57 become clear, and 23 of the 64 that showed nothing.
- **The 41 that never show are mostly right to be dropped**: 14 stuck pins, 6 dry Joints, 10 bridges, 11 cracks, 7 of these on lines to the expansion connector, where nothing is plugged.
- **Columns always show** (16 of 16, each a lost picture). **Cells almost never**: of 40, one shows untouched (`cell IC3.IO3 $25 0`, a RAM cell: a grid of dots, no sync), one with the typed line (`cell IC2.O0 $FBF 1`, a ROM cell: one more dot under the `R`, 2 pixels, the BW-2001's Odd E again), 38 never.
- **Time**: 1.4 s a try untouched (300 ms at 0.21×), about 11 s with the typed line (2.5 s; a key takes about 0.15 s of simulated time). The web build runs at about 0.4× of that speed.

What the sample is not: one Machine; the last frame only; chips drawn first, then one of their pins (so a 40-pin chip's pins are each less likely than a gate's); cracks half way between two random pads of a random Net; 100 px an arbitrary line.

## The other Machines

Reasoned from the netlists and the code, not sampled yet. `bw run --leds` prints each lamp's brightness at the end of a run (`led D1 0.50`, a digit's `led DS1.a 1.00` to `led DS1.dp`); `tools/fault_sample.py` still compares TV frames only. First readings, the Counter at 600 ms: `led D1 1.00` fault-free, `0.00` on Dark LED, `0.28` on LED too fast (a flicker is a dimmer lamp).

| Machine | The player sees | Inputs | Captures |
|---|---|---|---|
| 010 Counter | 1 LED | none | several, over more than a second |
| 020 LED chaser | 8 LEDs | none | several, over one pass of the light |
| 030 Dice | 7 LEDs | the button held, then released | while held |
| 040 Stopwatch | 3 digits | its buttons | while counting, and after the stop |
| 045 Knob meter | 2 digits | the knob at a position | one |
| 050 Test pattern | TV | none | one |
| 060 Bouncing ball | TV | none | several: the ball moves |
| 070 Tennis | TV | knobs, button | several |
| 080 Message | TV | none | one |
| 090 Typewriter | TV | a typed line | one |
| 110 LED computer | 8 LEDs | its button | several |
| 120 BW-1 | 6 digits | keys | one, after the keys |
| 130 BW-2001 | TV | a typed line | one or two: its cursor blinks |
| 140 BW-80 | TV | a typed line | one |

- `tools/readme_shots.py` already holds an input line that works for each Machine that needs one (`--press`, `--knob`, `--type`).
- **The Dice**: the face left after the release depends on the instant of the release, so any Fault that shifts the counting changes it, and another face is no Symptom. While the button is held, the seven brightnesses show the mix of faces it runs through.
- **The Counter** is the low-yield case: of its 140 stuck Faults (5 chips, 14 signal pins, 0 or 1), 96 change nothing (only 5 of its 25 outputs lead anywhere, the `A`–`D` inputs are never loaded), 36 freeze the LED, 8 make it flicker or glow. Counted from the netlist, not run. It simulates fast.

## Open

To settle before the design:

- The lamp Machines' numbers: `tools/fault_sample.py` taught to compare `bw run --leds` lines, at several times.
- Each Machine's inputs, run time and capture times, and the file that holds them.
- The weights, and the thresholds (pixels; lamps).
- The post-it's look and words; the generated Brief and its "do X"; what the Launcher's live picture shows for "?".
- Where the wait goes and what the player sees meanwhile: on the BW-80 a try is 1.4 s untouched and 11 s typed. A capture before the first input would let most tries end there.
- Giving up: is the Fault shown? Is a made-up Puzzle's `fault` line kept somewhere, so a good one can become an Authored Puzzle?
- Cells: rare by the five tries alone (about one draw in eight succeeds on the BW-80), or drawn among the words the Machine reads.
- A minimum number of pixels throws away the one-dot ROM cell, the kind of the BW-2001's Odd E.
- The glossary's Vetting ("…or is trivial") and Procedural Puzzle entries.

## Later

- A pool made offline and curated; a made-up Puzzle promoted to an Authored one.
- A dial between dead and misbehaving Machines.

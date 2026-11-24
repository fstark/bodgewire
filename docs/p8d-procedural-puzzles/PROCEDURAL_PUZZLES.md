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
- A capture is a picture of what the player sees: the TV's frame, and each lamp's brightness (`LedGlow`: the share of the last 20 ms it was lit, a digit's eight lamps each on its own). A LED blinking too fast for the eye is then a dimmer lamp, as the player sees it. A lamp's margin is relative to the brightest fault-free lamp: a multiplexed segment sits near 1/6, not 1 (the samples: a lamp differs when off by more than a quarter of it).
- Several capture times per Machine, not one: an instant cannot tell a blinking LED from one frozen while lit (on the Counter one capture finds 15 to 55 of the 58 Faults that four find). Still one run a try.
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

## What the first sample shows (BW-80)

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

## What the samples show (every Machine)

2026-11-24, the same tool, about 180 candidates a Machine (`--counts stuck=60,joint=30,bridge=30,crack=40,column=8,cell=16,switch=6`), each Machine with the inputs and capture times below: its definition, as proposed. A Machine without a TV is compared lamp by lamp (`bw run --leds`). "Dead" is a lost picture on a TV, every lamp dark at every capture otherwise.

| Machine | Inputs | Captures |
|---|---|---|
| 010 Counter | none | 250 ms, 500 ms, 750 ms, 1.25 s |
| 020 LED chaser | none | eight, 66 ms apart, from 600 ms |
| 030 Dice | ROLL held from 300 ms to 800 ms | 500 ms, 700 ms |
| 040 Stopwatch | STOP at 50 ms, RESET at 150 ms, START at 300 ms, STOP at 1 s | 280 ms, 600 ms, 900 ms, 1.3 s |
| 045 Knob meter | the knob at 42 % | 300 ms |
| 050 Test pattern | none | 400 ms |
| 060 Bouncing ball | none | 600 ms, 900 ms, 1.2 s |
| 070 Tennis | the knobs at 40 % and 60 %, START at 200 ms | 1 s |
| 080 Message | none | 300 ms |
| 090 Typewriter | CLEAR at 500 ms, `HELLO, WORLD` typed from 1 s | 2.5 s |
| 110 LED computer | RESET at 1 s | 200, 400, 600, 800 ms, 1.3, 1.5, 1.7 s |
| 120 BW-1 | AD 0 3 0 0 DA A 9 +, a key every 100 ms from 100 ms | 80 ms, 580 ms, 880 ms, 1 s |
| 130 BW-2001 | none | 1.4 s |
| 140 BW-80 | none, then a typed line | 300 ms, then 2.5 s |

The pin and copper Faults (stuck pin, dry Joint, bridge, crack):

| Machine | Loaded | Show | Of which dead | A try |
|---|---|---|---|---|
| 010 Counter | 147 | 58 (39 %) | 42 | 0.1 s |
| 020 LED chaser | 140 | 85 (61 %) | 12 | 0.1 s |
| 030 Dice | 149 | 55 (37 %) | 16 | 0.1 s |
| 040 Stopwatch | 138 | 89 (64 %) | 3 | 0.1 s |
| 045 Knob meter | 132 | 74 (56 %) | 3 | under 0.1 s |
| 050 Test pattern | 147 | 100 (68 %) | 53 | 0.1 s |
| 060 Bouncing ball | 149 | 99 (66 %) | 26 | 0.5 s |
| 070 Tennis | 142 | 82 (58 %) | 12 | 0.5 s |
| 080 Message | 142 | 110 (77 %) | 61 | 0.1 s |
| 090 Typewriter | 135 | 101 (75 %) | 20 | 1.8 s |
| 110 LED computer | 148 | 103 (70 %) | 42 | 0.3 s |
| 120 BW-1 | 146 | 109 (75 %) | 60 | 0.4 s |
| 130 BW-2001 | 154 | 87 (56 %) | 13 | 3.8 s |
| 140 BW-80 | 290 | 169 (58 %); typed 236 (81 %) | 152 | 1.4 s; typed 11 s |

- **Every Machine gives Puzzles, quickly**: 37 % to 81 % of random pin and copper Faults show, and a try is under a second everywhere but on the Typewriter, the BW-2001 and the BW-80.
- **Several captures matter where the outputs change slowly.** The Counter: 58 Faults show over its four captures, 15 at 250 ms alone (its LED is dark then, as with most of its Faults), 55 at 500 ms alone. The Stopwatch: 89 over four, 50 to 81 for one. The LED chaser: 85 over eight, about 70 for any one. The LED computer 103 (91 to 97), the BW-1 109 (92 to 103), the Bouncing ball 99 (89 to 92). On the Tennis and the Knob meter one capture finds them all (they were sampled with three and two).
- **A dead Machine is the BW-80's speciality**: its CPU makes the picture, and 152 of its 290 Faults lose it. The BW-2001's video runs by itself: 13 of 154 lose the picture, 74 leave a picture that is wrong.
- **The BW-2001 needs no typed line**: of the 67 candidates that showed nothing or little at 1.4 s, `10 REM HELLO` typed and entered (the frame at 4 s, 10.8 s a try) makes 2 show. Its idle screen holds a banner already, and BASIC's memory test has read the RAM.
- **The Dice**: 55 show while ROLL is held. 16 more differ only after the release (sampled at 1 s too): 10 leave another valid face, which is no Symptom; 4 leave the die dark, a Symptom the held captures miss; 2 a dimmer face.
- **The Counter**, the low-yield Machine: 37 of its 51 stuck pins change nothing (only 5 of its 25 outputs lead anywhere, the `A`–`D` inputs are never loaded).
- **Columns show** (47 of 56 over the six Machines with a memory). **Cells do not**: 1 of 16 on the Message, none on the Typewriter and the LED computer, 2 on the BW-1, 2 of 40 on the BW-80. The exception is the BW-2001, 6 of 16 (3 more under 100 px): its memory test stops at a stuck cell and the banner's count of bytes changes, and a cell of the screen memory is a wrong character.
- **A switch Fault shows where the inputs press that switch**: all of them on the Dice, the Stopwatch, the Typewriter and the LED computer; 5 of 12 on the BW-1, whose inputs press 7 of its 23 keys.

What the samples are not: one seed; a lamp's margin (a quarter of the brightest fault-free lamp) and the TV's 100 px are arbitrary lines; the inputs are a first guess; chips are drawn first, then one of their pins (a 40-pin chip's pins are each less likely than a gate's); cracks are half way between two random pads of a random Net.

## Open

To settle before the design:

- The file that holds each Machine's inputs and capture times (proposed above).
- The computers' typed line: left out on the BW-2001 (7 s more a try for 2 Faults in 67); on the BW-80 a short one ending with NEW LINE is still to choose and time (the sample's line was longer and not entered; a key is 0.15 s of simulated time there, 0.6 s of wait).
- The weights, and the thresholds (pixels; lamps).
- The post-it's look and words; the generated Brief and its "do X"; what the Launcher's live picture shows for "?".
- Where the wait goes and what the player sees meanwhile: 3.8 s a try on the BW-2001, 1.4 s untouched and 11 s typed on the BW-80, more than twice that on the web. A capture before the first input would let most tries end there.
- Giving up: is the Fault shown? Is a made-up Puzzle's `fault` line kept somewhere, so a good one can become an Authored Puzzle?
- Cells: rare by the five tries alone (about one draw in eight succeeds on the BW-80), or drawn among the words the Machine reads.
- A minimum number of pixels throws away the one-dot ROM cell, the kind of the BW-2001's Odd E.
- The Dice's dark die after the release: found only by a capture there, which also takes 10 changed faces for Symptoms.
- The glossary's Vetting ("…or is trivial") and Procedural Puzzle entries.

## Later

- A pool made offline and curated; a made-up Puzzle promoted to an Authored one.
- A dial between dead and misbehaving Machines.

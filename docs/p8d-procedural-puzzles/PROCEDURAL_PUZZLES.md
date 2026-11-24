# P8d — Procedural Puzzles

Roadmap step P8d ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): a "?" post-it at the end of a Machine's Puzzles that makes one up when asked: a random Fault, kept only if it shows. Design: [PROCEDURAL_PUZZLES_DESIGN.md](PROCEDURAL_PUZZLES_DESIGN.md). Plan: [PROCEDURAL_PUZZLES_PLAN.md](PROCEDURAL_PUZZLES_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

Read by the user before the design and the plan (2026-11-24). The framework and the samples came first; what the player sees was settled with the samples in hand.

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

Asked with the samples in hand (the user, 2026-11-24):

| Question | Decision |
|---|---|
| The post-it | its title is "Random fault", with a "?" |
| The Brief on the bench | one fixed sentence plus a "do this", fixed per Machine, and vague: not the inputs with their times |
| The wait | the Puzzle is made when the player clicks. Meanwhile a progress bar or, better, lines about breaking a computer in ridiculous ways (water spilt on it, the board thrown around), looping, as an assistant's "thinking" lines do |
| Giving up | yes, somewhere: maybe a new entry in the manuals, a page that asks "are you sure?" before it shows the `fault` line; a click on the line copies it |
| The Launcher's live picture | the working Machine; maybe a large "?" over it, if it renders well |
| The BW-80's typed line | `PRINT 42` and NEW LINE: O, 4, 2, NEW LINE |
| A knob | takes several positions during the run (the Knob meter, the Tennis) |
| The BW-2001's typed line | none: skipped (7 s more a try for 2 Faults in 67) |
| The texts (the waiting lines, the fixed sentence, each "do this") | the drafts below, and "we'll see how it goes" |
| The wait on the slow Machines (about 10 s on the BW-2001 and the BW-80) | not a problem at this stage; nothing built for it now |

Chosen without asking (overturn any):

- The post-it is on every Machine, after its last Puzzle, a gap apart; its "?" stands where the others have their brief.
- The waiting lines: a first list below, the user's to rewrite. One line stays about a second at least, so it can be read where a Puzzle is made in a tenth of a second (overturn if it annoys). No percentage: the number of tries is not known ahead. Esc stops the making and stays in the Launcher.
- The Brief's fixed sentence and each Machine's "do this": drafts below, the user's to rewrite.
- The Thank-you Card's Next on a Random fault is "Another random fault", on the same Machine: back through the Launcher, where it is made.
- Reset brings the same Fault back; leaving the bench forgets it; the post-it opened again makes another. No seed is shown.
- Giving up is a divider of the binder, "Give up", on a Random fault's bench only. Once shown, the Fault stays shown for the bench; the Machine can still be repaired, and the tick and the card come as for any Puzzle. The line is a `.bwp`'s own (`fault stuck IC17.12 0`): pasted under a `machine` line it is an Authored Puzzle.
- A Machine's inputs, capture times and "do this" are one file of its own, its Vetting Script, `machines/NNN-name/puzzles/random.bwr`, written with the strings `bw run` takes (`press SW1:300ms:800ms`, `knob J2:42%`, `type J3:350ms:PRINT 42`), a `capture 500ms` a capture and one `task "…"`. Its path is what names a Random fault wherever a Puzzle's file does: `bw_ui machines/NNN-name/puzzles/random.bwr` opens one from the terminal.
- One code vets in the game and in `bw`: a command makes a Random fault from the terminal (the author's check, the tests' way in). `tools/fault_sample.py` stays the research's tool.
- Cells are rare by their weight and the five tries; nothing draws them among the words the Machine reads.
- A try ends at the first capture that shows: on the BW-80 a lost picture is known at 300 ms, before the line is typed.
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

## What the player sees

### The post-it

1. In the Launcher every Machine has one more post-it after its Puzzles, a gap apart from them: the title "Random fault" and a big "?".
2. Selected, it shows the working Machine live, as "Start the Machine" does. A large "?" over that picture is tried, and kept if it reads well.
3. Opened as any post-it (Enter, Space, a click), it makes a Puzzle, then opens the bench on it.

### While the Puzzle is made

4. The Launcher stays; a line of text changes about every second, looping through ridiculous ways of breaking a computer. A first list (three dots, typed as dots: the UI font has no ellipsis):
   - "Spilling coffee on the board..."
   - "Dropping it down the stairs..."
   - "Letting the cat sleep on it..."
   - "Leaving it in a damp cellar for ten years..."
   - "Plugging the power in backwards..."
   - "Lending it to a nephew..."
   - "Shaking it to hear what rattles..."
   - "Poking around with a screwdriver, power on..."
   - "Checking that it no longer works..."
5. How long: the Golden Machine is run once first, then the tries. Estimated from the samples: under a second on ten Machines, one to two on the Bouncing ball and the Tennis, about 4 s on the Typewriter, about 10 s on the BW-2001 and 9 s on the BW-80 (the Golden Machine's run is 4 and 5 of them); more than twice that on the web. Measured when the Vetting is built (the plan's phase 4).
6. Esc stops it; the Launcher is as it was. No other key or click does anything meanwhile.

### On the bench

7. The Brief's post-it: "Random fault", the fixed sentence "It worked, and now it does not. Nobody knows what happened to it.", then the Machine's "do this" when it has one:

   | Machine | Do this |
   |---|---|
   | Counter, LED chaser, Test pattern, Bouncing ball, Message, BW-2001 | none |
   | Dice | "Hold ROLL, then let go." |
   | Stopwatch | "Try START, STOP and RESET." |
   | Knob meter | "Turn the knob from end to end." |
   | Tennis | "Press START and turn both knobs." |
   | Typewriter | "Press CLEAR and type a few words." |
   | LED computer | "Watch the light, then press RESET." |
   | BW-1 | "Pick an address with AD, change its byte with DA, go on with +." |
   | BW-80 | "Type PRINT 42 and NEW LINE." |

8. All else is as on an Authored Puzzle: the tools, the Reset (the same Fault back), the tick and the Thank-you Card once it is completed (P8c). The card's Next is "Another random fault".
9. Leaving forgets the Puzzle. The Launcher comes back with the "Random fault" post-it selected.

### Giving up

10. On this bench the binder has one more divider, "Give up". Its page asks whether the player is sure, with one button, "Show the fault".
11. Pressed, the page shows the `fault` line. A click on the line copies it.

## What is new

1. **A Machine's Vetting Script** (`machines/NNN-name/puzzles/random.bwr`, one a Machine): its inputs with their times, its capture times, its "do this"; read and checked with the Machine.
2. **A knob set at a time**, in the inputs and in `bw run --knob`: today a knob keeps one position for a run.
3. **Drawing a Fault**: a kind by weight, then one of the Machine's objects of that kind (a chip signal pin, two neighbouring pins, two pads of a Net and a place between, a memory's column or cell, a switch); the Machine refuses what cannot be (`Machine::inject`).
4. **Vetting**: the Machine built with the Fault, run headless from power-on with seed 1, the inputs played, the TV's frame or each lamp's brightness captured at the capture times, and compared with the Golden Machine's captures, made once.
5. **Making a Puzzle**: the framework's loop, off the UI thread, stopped on Esc.
6. **The Launcher's post-it**, its waiting lines, and the bench opened on a Puzzle that has no file.
7. **The bench**: the Brief from the Machine's file, the binder's "Give up" divider, the card's "Another random fault".
8. **`bw`'s command** that makes one, with a seed: the `fault` line and the tries it took.

Untouched: the kernel, Parts, the builder, the `.bwp` format, the Authored Puzzles and their tests, Repair Validation's rule.

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
- **`PRINT 42` and NEW LINE**, the line chosen: four keys, the frame settled from 1.1 s and taken at 1.2 s, 5.1 s a try. 59 of the 121 show clearly with it, against 67 for the longer line at 11 s: 228 of the 290 in all (79 %). No cell shows with it.
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
| 045 Knob meter | the knob at 42 %, at 87 % from 400 ms, at 5 % from 800 ms | 300 ms, 700 ms, 1.1 s |
| 050 Test pattern | none | 400 ms |
| 060 Bouncing ball | none | 600 ms, 900 ms, 1.2 s |
| 070 Tennis | the knobs at 40 % and 60 %, START at 200 ms, the knobs at 85 % and 15 % from 1.1 s | 1 s, 1.5 s |
| 080 Message | none | 300 ms |
| 090 Typewriter | CLEAR at 500 ms, `HELLO, WORLD` typed from 1 s | 2.5 s |
| 110 LED computer | RESET at 1 s | 200, 400, 600, 800 ms, 1.3, 1.5, 1.7 s |
| 120 BW-1 | AD 0 3 0 0 DA A 9 +, a key every 100 ms from 100 ms | 80 ms, 580 ms, 880 ms, 1 s |
| 130 BW-2001 | none | 1.4 s |
| 140 BW-80 | none, then `PRINT 42` and NEW LINE typed from 350 ms | 300 ms, then 1.2 s |

The Knob meter's and the Tennis's knobs were sampled at their first position only (a knob cannot move during a `bw run` yet); the later positions and their times are to check once one can.

The pin and copper Faults (stuck pin, dry Joint, bridge, crack):

| Machine | Loaded | Show | Of which dead | A try |
|---|---|---|---|---|
| 010 Counter | 147 | 58 (39 %) | 42 | 0.1 s |
| 020 LED chaser | 140 | 85 (61 %) | 12 | 0.1 s |
| 030 Dice | 149 | 55 (37 %) | 16 | 0.1 s |
| 040 Stopwatch | 138 | 89 (64 %) | 3 | 0.1 s |
| 045 Knob meter | 132 | 74 (56 %); other positions 89 (67 %) | 3 | under 0.1 s |
| 050 Test pattern | 147 | 100 (68 %) | 53 | 0.1 s |
| 060 Bouncing ball | 149 | 99 (66 %) | 26 | 0.5 s |
| 070 Tennis | 142 | 82 (58 %); other positions 91 (64 %) | 12 | 0.5 s |
| 080 Message | 142 | 110 (77 %) | 61 | 0.1 s |
| 090 Typewriter | 135 | 101 (75 %) | 20 | 1.8 s |
| 110 LED computer | 148 | 103 (70 %) | 42 | 0.3 s |
| 120 BW-1 | 146 | 109 (75 %) | 60 | 0.4 s |
| 130 BW-2001 | 154 | 87 (56 %) | 13 | 3.8 s |
| 140 BW-80 | 290 | 169 (58 %); typed 228 (79 %) | 152; typed 161 | 1.4 s; typed 5.1 s |

- **Every Machine gives Puzzles, quickly**: 37 % to 81 % of random pin and copper Faults show, and a try is under a second everywhere but on the Typewriter, the BW-2001 and the BW-80.
- **Several captures matter where the outputs change slowly.** The Counter: 58 Faults show over its four captures, 15 at 250 ms alone (its LED is dark then, as with most of its Faults), 55 at 500 ms alone. The Stopwatch: 89 over four, 50 to 81 for one. The LED chaser: 85 over eight, about 70 for any one. The LED computer 103 (91 to 97), the BW-1 109 (92 to 103), the Bouncing ball 99 (89 to 92). On the Tennis and the Knob meter one capture finds them all (they were sampled with three and two).
- **A dead Machine is the BW-80's speciality**: its CPU makes the picture, and 152 of its 290 Faults lose it. The BW-2001's video runs by itself: 13 of 154 lose the picture, 74 leave a picture that is wrong.
- **The BW-2001 needs no typed line**: of the 67 candidates that showed nothing or little at 1.4 s, `10 REM HELLO` typed and entered (the frame at 4 s, 10.8 s a try) makes 2 show. Its idle screen holds a banner already, and BASIC's memory test has read the RAM.
- **The Dice**: 55 show while ROLL is held. 16 more differ only after the release (sampled at 1 s too): 10 leave another valid face, which is no Symptom; 4 leave the die dark, a Symptom the held captures miss; 2 a dimmer face.
- **The Counter**, the low-yield Machine: 37 of its 51 stuck pins change nothing (only 5 of its 25 outputs lead anywhere, the `A`–`D` inputs are never loaded).
- **Columns show** (47 of 56 over the six Machines with a memory). **Cells do not**: 1 of 16 on the Message, none on the Typewriter and the LED computer, 2 on the BW-1, 2 of 40 on the BW-80. The exception is the BW-2001, 6 of 16 (3 more under 100 px): its memory test stops at a stuck cell and the banner's count of bytes changes, and a cell of the screen memory is a wrong character.
- **A knob has to move.** At 42 % the Knob meter only ever draws a 4 and a 2. Its 58 Faults that showed nothing there were run again with the knob at 87 %, at 5 % and at 60 % (each for the whole run): 10 show at each, 15 at one of the three, 13 at 87 % or 5 %. The Tennis's 60 that showed little or nothing, with its knobs at 85 % and 15 %: 9 show clearly.
- **A switch Fault shows where the inputs press that switch**: all of them on the Dice, the Stopwatch, the Typewriter and the LED computer; 5 of 12 on the BW-1, whose inputs press 7 of its 23 keys.

What the samples are not: one seed; a lamp's margin (a quarter of the brightest fault-free lamp) and the TV's 100 px are arbitrary lines; the inputs are a first guess; chips are drawn first, then one of their pins (a 40-pin chip's pins are each less likely than a gate's); cracks are half way between two random pads of a random Net.

## Tests

- Unit: the Vetting Script (each kind of line, its errors by line); a knob set at a time; drawing (every kind, a kind with nothing to act on dropped, the weights, the same Fault for the same seed); Vetting on the Counter (its Dark LED's crack shows, a stuck unconnected output does not) and on a TV Machine; the loop (a kind that never shows is left after five tries).
- Every Machine: its file loads; its Golden Machine's captures are stable (a locked picture at each capture time); for a few seeds a Random fault is made, loads, shows, is not completed as made, and is completed after the Repair its kind calls for (a swap, a mend, the bridge removed, a resolder).
- `bw`'s command: the same line for the same seed.
- Screenshots (release, viewed): the Launcher on the post-it, with and without the "?" over the picture; a waiting line; a Random fault's bench with its Brief; the binder's page before and after "Show the fault".
- Speed: the time to make one on each Machine (`bw random`: the Golden Machine's run and the whole making); on the web the user times the Counter, the BW-2001 and the BW-80.
- Sanitizers: debug on the unit tests and the small Machines' cases; debug `bw_ui` making one on the Counter.
- Web build (the Launcher and the bench draw something new; the making runs on a thread).
- Only the user: the waiting lines and the Briefs' words, a few Random faults played on several Machines (the card's "Another random fault", Esc during a making), the "?" over the picture, the making in a browser.
- The full release suite.

## Open

The wait on the BW-2001 and the BW-80, about 10 s and 9 s by the estimate (What the player sees, item 5; the "few seconds" first given left out the Golden Machine's run): not a problem at this stage (the user, 2026-11-24: it is not yet known whether the feature works as well as wanted). If it feels too long once played: something to please the player meanwhile, and the Golden Machine's run kept from one making to the next (no help on the first click). Nothing is built for it now.

To tune in play, with starting values here: the texts; the weights; the thresholds (100 cells of the TV's picture, each off by more than a quarter; a lamp off by a quarter of the brightest); the BW-1's keys (its inputs press 7 of 23: more keys find more switch Faults, for a longer run).

The game's draw is not the samples' in three ways, measured again when it is built: every pin is as likely (the tool drew a chip first), the oscillator and the power-on reset are chips that can be faulty, and a slide switch can be.

Known and accepted: a minimum number of pixels throws away the one-dot ROM cell, the kind of the BW-2001's Odd E; the Dice's dark die after the release is not found (a capture there would also take 10 changed faces for Symptoms); the later positions of the knobs and their times are unchecked until a knob can move.

The glossary's Vetting ("…or is trivial") and Procedural Puzzle entries are rewritten with the step.

## Later

- A pool made offline and curated; a made-up Puzzle promoted to an Authored one.
- A dial between dead and misbehaving Machines.

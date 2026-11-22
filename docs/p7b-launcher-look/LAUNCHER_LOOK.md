# P7b — Launcher look

Roadmap step P7b ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the launcher P7a made plain becomes one screen with two lanes. The top lane shows the selected Machine running, the bottom lane its post-its. Design: [LAUNCHER_LOOK_DESIGN.md](LAUNCHER_LOOK_DESIGN.md). Plan: [LAUNCHER_LOOK_PLAN.md](LAUNCHER_LOOK_PLAN.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

P7a's launcher is two lists of names. A player choosing a Machine wants to see it: its LEDs counting, its TV's picture, its Board. A player choosing a Puzzle wants to see the symptom before taking the Machine to the bench. P7b shows both, drawn in the bench's look (the same Board, the same monitor, the brief's post-it, the shelf's wood), with nothing drawn per Machine: a Machine added or removed is still only its directory.

## User decisions

From the roadmap row (the user, 2026-11-22):

| Question | Decision |
|---|---|
| Layout | one screen, two lanes; nothing drawn per Machine |
| Top lane | the selected Machine live: its Board, or its TV with the Board when it has one; framed per Machine around its outputs (LEDs, digits, TV) with enough Board around them to recognise it; a small Board whole; no framing per Puzzle (a Board has one output area) |
| Beside it | its User Manual's first paragraph on the right |
| Neighbours | the previous and next Machines hinted at the left and right by part of their still Board, no name; moving between them scrolls smoothly; a still matches its live one |
| Bottom lane | post-its, the bench's Brief drawing: "Start the Machine" (no fault) first and pinned while the Puzzles scroll past, then each Puzzle's title and brief in file order |
| What runs | the selected entry runs live above, from power-on (the BW-2001's boot seen, at its speed), with no input (a symptom that needs a key or a paddle does not show: its brief says so); only that one runs, the neighbours are stills |
| Keys | left and right move in the lane (Machines or post-its); down and up change lane; Enter or Space (or a click) launches the selected post-it from either lane; another Machine selects its "Start the Machine"; the ends stop, no wrapping |
| Difficulty | none shown (a red herring) |

Settled at the step's start (the user, 2026-11-22):

| Question | Decision |
|---|---|
| The zoom into the bench, the post-it flying to the Board, and back | not in P7b: the screens switch at once, as in P7a. The animations become a roadmap row of their own (P7c) |
| How a still is made | drawn, unpowered: each Machine's Board loaded without running (LEDs and digits dark, the TV off) and drawn with the same framing as its live one. No cache, no image files |
| A Machine with a TV | the monitor large, in front; only a corner of the Board shows behind it, at its left |
| The post-its' surface | the shelf's wood; the selected post-it lifted, with a deeper shadow |

Derived in the design (2026-11-22), reported to the user:

- **The framing rule.** A Machine without a TV is framed on its lamps (every LED and digit), with 30 mm of Board around them, clamped to the Board. The Board is shown whole when both its sides are at most 130 mm, or when the frame would cover 60 % of it anyway. Today that gives the Counter, LED Chaser, Dice and Knob Meter whole, and the Stopwatch, LED Computer and BW-1 framed on their digits or LEDs. A Machine with a TV shows its Board whole behind the monitor. The constants are tuned on the screenshots.
- **Which lane has the keys.** The top lane: gold chevrons `‹ ›` beside the live picture (only towards a neighbour that exists). The bottom lane: a gold bar under the selected post-it. The selected post-it is lifted in both lanes; with the top lane active it is always "Start the Machine".
- **Clicks.** A click on a post-it launches it, as Enter does. A click on a neighbour's still moves to that Machine, as an arrow key does. A click on the live picture gives the keys to the top lane. Hovering changes nothing: what runs follows the selection only.
- **Coming back from a bench**, the launcher selects the entry just left: its Machine, and its post-it with the bottom lane active, or "Start the Machine" with the top lane active. At start: the first Machine, top lane.
- **Loading never stops the screen.** Loading a Machine with its Board takes up to about 0.26 s in release (the BW-2001; 0.03–0.10 s for the BW-80, BW-1, Tennis and Typewriter, under 0.02 s for the small ones; ten times more in debug). So both kinds of load run on background threads. **Stills**: one worker loads every Machine's still when the launcher first shows, the selected Machine first, then outwards; a still not yet loaded is an empty dark picture. **The live run**: the old Machine stops at once when the selection changes. The new one is loaded once the selection has rested for 0.15 s (holding an arrow key loads nothing on the way), then powered on. Its still shows until then. Its Load diagnostics go to stderr as a bench's do. A file that fails to load leaves the still in place with "Could not load NN-name.bwp: see the terminal" over it.
- **A Machine without a Board** (a netlist alone, under `--machines DIR`): no still; its picture is its monitor if it has a TV, else a dark 4:3 area reading "No Board".
- **The Machine's name** stays above its paragraph, larger (P7a's), fading out as the lane scrolls.
- **`Start the Machine`'s post-it** reads "Start the Machine" with "The Machine as it should work: no fault." under it.
- **A new test flag** `--select FILE` (no file): the launcher starts on that entry (a netlist: its Machine, top lane; a Puzzle: its post-it, bottom lane), for screenshots. It is a usage error with `--visit`. The launcher's screenshot prints the live run's speed line, as a bench's does.

## What the player sees

### The top lane

1. The upper two thirds of the window, on the bench's dark background. The selected Machine's picture sits left of centre and its name and paragraph on the right.
2. The picture goes live a moment after the Machine is selected (once loaded: a fraction of a second, the BW-2001 about a quarter): power-on, then whatever it does untouched (the Counter counting, the Test Pattern's grid, the BW-2001's boot and `READY.`).
3. A Machine without a TV: its Board drawn as on the bench (the real look, front side), framed by the rule above, its LEDs and digits lit as they run. Nothing on it reacts to the mouse: no hover, no tooltip, no pressing a button.
4. A Machine with a TV: the monitor (the bench's drawing, the knobs at their marks, inert) large on the right of the picture, the Board whole behind it, its left part showing.
5. At the window's left and right edges: part of the previous and next Machines' Boards (or monitor), still: unpowered, the TV dark, no name. None past the first or the last Machine.
6. Moving to another Machine slides the whole lane: the pictures glide to their new places, most of the way in a quarter of a second; the paragraph fades out and the new one in.

### The bottom lane

7. The lower third: the shelf's wood. On it a row of post-its, each the bench's brief: yellow paper, its glued strip, its curled corner, the title and the brief in ink.
8. The first, "Start the Machine", stays at the left. The Puzzles follow in file order, scrolling under a fixed edge to keep the selected one in view.
9. The selected post-it is lifted, with a deeper shadow. It is the one running above.

### Keys and clicks

10. Left and right: in the top lane, the previous or next Machine (its "Start the Machine" selected, running); in the bottom lane, the previous or next post-it (it starts running). The ends stop.
11. Down from the top lane: the bottom lane, on the post-it already selected. Up from the bottom lane: the top lane, "Start the Machine" selected (running).
12. Enter or Space: the selected post-it's bench opens, as a click in P7a did (the live run stopped first). A click on a post-it does the same for that post-it.
13. A click on a neighbour's still: that Machine, as an arrow key would. A click on the live picture: the top lane gets the keys.
14. Back from a bench: the launcher on the entry just left. A failed load's red line, as in P7a, at the wood's top left.

## What is new

To be settled file by file in the design:

1. **The logic, unit tested** (`src/view`): the lanes and their keys; the gliding; where each Machine's picture sits as the lane scrolls; the framing rule (`BoardScene::lamp`).
2. **Pieces shared with the bench** (`src/ui`): the Board drawn still (no input, framed on a box); the monitor's drawing out of the TV window; the post-it's paper out of the brief; the Board's lights, switches and repairs read from a Session in one place; `Load` out of `main.cpp`.
3. **The launcher** rewritten: stills, the live run, the two lanes, keys and clicks; `--select`.
4. **Docs**: GLOSSARY (Launcher, Still), ADD_MACHINE and ADD_PUZZLE (the framing, a brief on a post-it), ARCHITECTURE (P7b done, P7c the animations).

The kernel, Parts, the builder, devices, Machines, Puzzles and manuals are untouched. The bench looks and behaves as before.

## Tests

- **Unit tests**: `launcher lanes: …` (start, left and right in each lane, the ends, down and up, another Machine resets the post-it, the entry selected, select by entry; the glide reaching its target; the picture's place for d = -1, 0, 1 and between); `output frame: …` (each Machine's design netlist and Board file, without loading the Machine: the Counter, LED Chaser, Dice and Knob Meter whole; the Stopwatch, LED Computer and BW-1 framed, holding every lamp, smaller than their Board, the Stopwatch's clamped at the Board's top; any of them whole with a TV; `HasTv` true for the seven TV Machines only); `board scene: lamp`.
- **Screenshots** (release, viewed): the launcher at start (the Counter live, the LED Chaser's still at the right); `--select` of the Stopwatch (its digits framed), of the BW-2001's Odd E (the monitor large, the Board's corner, the post-it lifted among 16), of the BW-1 (framed digits, the BW-2001's still at the right) and of the BW-80 (the last: no still at the right); `--visit` of a Puzzle (the launcher back on it); a bench (the Stopwatch and the BW-2001, as before the step).
- **Speed**: the BW-2001's Odd E live in the launcher at its bench's speed (P7a's release check: 0.26×, UI 1.4 ms), UI time printed; the UI time with the BW-2001 selected (three of the largest Boards drawn: its own and the BW-1's and BW-80's stills) recorded.
- **Sanitizers**: the debug build's `--select` of a TV Machine (the Typewriter) and of the Stopwatch, and `--visit` of P7a's three (the Counter's Dark LED, the Test Pattern, the Typewriter), run clean: the background loads included.
- **By the user**: the keys, the glide, clicks on post-its and stills, coming back from a bench.
- **Release**: the full release suite passes.

## Later

- **P7c**: the zoom from the live picture into the bench, the post-it flying to its place on the Board, and back.
- Stills made from a live run (lit, the TV's picture) and cached, if the dark stills disappoint.

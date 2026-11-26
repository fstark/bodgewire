# P8i — The Logic Analyzer redesigned

Roadmap step P8i (ARCHITECTURE.md#roadmap): the Logic Analyzer's window given the Oscilloscope's treatment (P8h) where its many uses allow: a drawn case by Bodge Wire, a strip of keys, its rows labelled on the screen, the cursor's tag at the cursor, no status line; and two bugs fixed where it shows a Capture that is not what happened. The Capture engine (`AnalyzerState`, `AnalyzerConsumer`) stays but for its arming; `AnalyzerSetup` gains the rows' order. **Spec in progress (2026-11-26):** the user's decisions are in; next, mock-ups, then the design and plan.

## Why

The agent, 2026-11-26, on the analyzer's window (`src/ui/analyzer_window.cpp`), from screenshots of the LED computer (SYNC, PHI2, RW, the address and data buses) and of the counter:

- **It is the last generic window on the bench**: a title bar, flat blue ImGui buttons, grey labels, next to the drawn TV, keyboards and scope.
- **Its header is two lines mixing three concerns**: the Capture (Arm, the phase as a word, the sample period, "Before T"), the Clips (`27 of 64 clipped`, `pin | bus`, a `Group` button that appears with two selected) and the view (`− 1 µs/div +`, `Fit`, `T`, `6.55 ms captured`).
- **"Before T" offers three times that change with the sample period** (655 µs, 3.28 ms, 5.9 ms at 100 ns) for what is "the Trigger early, in the middle or late in memory".
- **The label column takes 360 px**, 38 % of the 940 px normal width: a 76 px Trigger button on every row, nearly always `any`, and a value column empty but under the cursor. The Trigger as a whole (SYNC rising and the address bus at `F800`) is never shown at a glance.
- **The cursor's time, the Marker's and `dt` are in a status line** at the bottom left, far from the edges, and a gesture hint the rest of the time; the mouse does not snap to an edge.
- **The overview's box is a sliver** at any useful zoom (1 µs/div over 6.55 ms: 0.15 % of its width). The user never used it.
- **Rows are a fixed 28 px**: four Clips fill a third of the window.
- **Grouping is hidden** (a click on a name selects it, `Group` appears in the header; ungrouping is behind ×). **Colours collide** (the address bus and RW both magenta). **The Board's discs show the Channel number**, not the row.
- **Arm is one-shot**: after a Capture, seeing the circuit again means pressing Arm.
- **It shows Captures that are not what happened** (the user, 2026-11-26; reproduced by the agent): two Clips, pause, the second unclipped, another pin clipped: the new pin takes the freed Channel and its row, `U5.15 RCO`, draws the old Capture's Channel, U1.13 QB's waveform, under a faint `U1.13 QB`. And Arm pressed while paused reads `armed: waiting for trigger` above the previous Capture in full colour, which nothing will replace until the run resumes.

What works and stays: the Capture model (64 Channels, 65 536 points, the sample period, the Trigger early, middle or late), the bus rows in hex in slanted cells, a bus clipped from one pin's stem, the Trigger conditions per Channel and the bus pattern with `x`, zoom about the pointer, the Marker and `dt`.

## User decisions

The user, 2026-11-26, on the agent's list of ideas:

- **A drawn case in the scope's family**: the same frame, panel, key caps, red LEDs and BW logo; no title bar; the size key and × on the case; dragged by its body.
- **A strip of keys, one group per concern**, left to right: the badge; CAPTURE (`ARM`, lamps `ARMED`, `TRIG'D`, `DONE`); SAMPLE (◀ LED ▶, the LED opening the grid by unit); TRIGGER AT (a rocker `EARLY | MID | LATE`); VIEW (◀ LED ▶ for the zoom, `FIT`); the `PIN | BUS` rocker; the size key. Hard to judge before seeing it; taken as proposed.
- **Labels on the screen**: each row's one-line plate over its lane's left end (swatch, name, value, Trigger key, ×), about 200 px; no column headings.
- **The Trigger as a lit key on each plate**, dark on `any`, lit orange with its condition otherwise (`0`, `1`, ↑, ↓), a group's with its pattern (`F800`); the whole Trigger summed up in the strip only if it fits.
- **The cursor's tag on the screen** (its time, `dt` from the Marker), the mouse snapping to an edge; the status line and the gesture hint go ("status lines are evil").
- **The value at the Trigger** on each plate when there is no cursor or Marker.
- **The overview kept, thin**, inside the screen above the time axis. Unused so far, maybe for the window's other troubles.
- **Rows grow to fill the screen when few** (28 px up to about 56), and scroll when many.
- **Rows reordered by dragging their plates**; the Board's discs numbered by row place, as the scope's Hooks are.
- **Colours by row**, a group's members in their group's colour.
- **Grouping visible**: the plates' swatches tick, two ticked light a `GROUP` key; a group's plate has its own ungroup key.
- **One-shot Captures, re-armed at every power-on**: repair, power on, read the new Capture, no key; `ARM` captures again without a power cycle. A repeating Run rejected: stable only on a repeating signal, which the scope already shows, and studying a CPU's Capture would need Stop back.
- **`ARM` resumes a paused run**: pause saves the host's CPU, it is not a way to capture. Powered off, `ARM` waits for the power-on, which arms anyway.
- **A new Clip always arms** (today only while the analyzer waits: after a Capture the phase stays `captured`), since its lane is empty until a Capture that has it. While paused it arms without resuming, so several pins can be clipped first.
- **The analyzer never shows a Capture that is not what happened** (the two cases above): fixed in this step. Other surprises the user met are forgotten but the pause; to be found with the new design.

Chosen without asking: the plate's width (about 200 px), the overview's height (about 10 px), the grown rows' height (up to 56 px), the lamps' and keys' names.

## What the player sees

1. **The case.** A drawn case, no title bar, the scope's frame and panel: the strip along its top, the screen below in a dark bezel, the size key and × at the strip's right end, the corner grip. Dragged by its body; resized freely; the presets minimal, normal, full as now.
2. **The strip** (decided above), drawn as the scope's: key caps, red LED windows, green lamps. `ARMED` lit while waiting for the Trigger, `TRIG'D` while filling after it, `DONE` with a Capture shown. SAMPLE's LED `100 ns`, its grid by unit as the scope's timebase. TRIGGER AT's halves `EARLY`, `MID`, `LATE`, the time before the Trigger in their tooltips. VIEW's LED `1 µs/div`. `GROUP` lit only with two plates ticked.
3. **The screen.** The overview (thin, the view as a box, the Trigger and Marker as ticks), the time axis, then the rows, each a lane with its plate.
4. **A plate**: a swatch (a tick box for grouping), the name (`U1.7 SYNC`, `U1 A[15:0]` with its fold arrow), the value (at the cursor, else the Marker, else the Trigger: `1`, `F802`, `X`), the Trigger key, ×. Dragged up or down, it moves its row.
5. **The cursor's tag** at the cursor on the screen: `+2.3 µs   dt 1.3 µs`. The mouse and a Finger land on an edge of the row they are on within 12 px.
6. **The Board's discs**: each Clip's disc in its row's colour, numbered by its row's place (a group's members by the group's).
7. **Arming.** A power-on arms; a new Clip arms; `ARM` arms, and resumes a paused run. Each new Capture replaces the shown one.
8. **What a row shows** is only what its own pin recorded: a row whose pin was not clipped on that Channel for the shown Capture has an empty lane (`not in this Capture`), never another pin's waveform. While armed, the shown Capture is drawn dimmed until the new one comes.

## What is new

To settle in the design, file by file:

1. **The case and the strip**: from the scope's `scope_case` and `bw_logo`, shared where the two cases are drawn alike.
2. **The screen**: plates, Trigger keys, the cursor's tag, rows that grow, the thin overview, in a pure layout tested in `src/view` as the scope's.
3. **The rows' order** in `AnalyzerSetup`, the Board's discs numbered by it; colours by row.
4. **Arming**: re-armed at every power-on (`AnalyzerState::powerOn`) and by every new Clip (`AnalyzerSetup`); `ARM` resuming a paused run; a row's lane drawn only from its own pin's Samples (the Capture's labels per Channel).

The sampling and the Trigger, the runner, the scope and the Machines are untouched.

## Tests

To settle in the design: unit cases for the layout, the rows' order, the re-arming at power-on and by a new Clip; a case for a Channel reused while paused showing an empty lane; screenshots of the LED computer and the counter at the three sizes; the full release suite.

## Later

- **Decoding a bus**: a data bus row read as ASCII, or as 6502 / Z80 instructions at SYNC or M1 (the binder's Contents disassembles both). Not trivial: the data bus carries other traffic (the BW-80's video), and instruction bytes need not be contiguous.
- **Search**: go to the next or previous point where a row has a value (`F800`) or an edge. Much missed.

Rejected: saving a Capture to compare with the next (too complex for its use).

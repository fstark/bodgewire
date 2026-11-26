# P8i — The Logic Analyzer redesigned

Roadmap step P8i (ARCHITECTURE.md#roadmap): the Logic Analyzer's window given the Oscilloscope's treatment (P8h) where its many uses allow. **Draft, 2026-11-26: ideas to discuss, nothing decided yet.** The Captures underneath (`AnalyzerState`, `AnalyzerConsumer`, `AnalyzerSetup`) are expected to stay as they are.

## Why

The agent, 2026-11-26, on the analyzer's window (`src/ui/analyzer_window.cpp`), from screenshots of the LED computer (SYNC, PHI2, RW, the address and data buses) and of the counter (four bits):

- **It is the last generic window on the bench.** A title bar, rows of flat blue ImGui buttons, grey text labels. The TV, the keyboards and now the scope are drawn objects; next to the scope the analyzer looks unfinished.
- **The header is two lines mixing three concerns.** Line 1 is the Capture (Arm, the phase as a word, `Sample every − 100 ns +`, `Before T 655 µs | 3.28 ms | 5.9 ms`); line 2 starts with the Clips (`27 of 64 clipped`, the `pin | bus` switch, a `Group` button that appears only with two Channels selected) and ends with the view (`− 1 µs/div +`, `Fit`, `T`, `6.55 ms captured`).
- **"Before T" is given as three times that change with the sample period** (655 µs, 3.28 ms, 5.9 ms at 100 ns): odd numbers for what is really "the Trigger early, in the middle or late in memory".
- **The label column takes 360 px**: swatch, name, value, Trigger and ×, under `Name / Value / Trigger` headings. On the 940 px normal size that is 38 % of the width; the waveforms get the rest.
- **The Trigger column is a 76 px button on every row, nearly always `any`.** The Trigger as a whole (SYNC rising AND the address bus at `F800`) is never shown in one place: it is spread over the rows, a group's as a hex field, its members' greyed.
- **The value column is empty most of the time**: it shows only under the cursor or at the Marker.
- **The cursor's time, the Marker's and `dt` sit in the status line at the bottom left**, far from the edges measured (the scope's problem before P8h); the mouse does not snap to an edge, a Finger does. Without a cursor, that line is a permanent gesture hint.
- **The overview** is a 28 px black bar whose box is a sliver at any useful zoom (1 µs/div over 6.55 ms: 0.15 % of its width), labelled `overview` in the label column.
- **Rows are a fixed 28 px**: four Clips use the top third of the window, the rest is empty.
- **Grouping is hidden**: a click on a name selects it (no visible affordance but a highlight), the `Group` button then appears in the header, far from the rows. Ungrouping is in a popup behind the ×.
- **The `pin | bus` switch is a mode** for what a click on the Board does, set in the window, used on the Board.
- **Colours collide**: eight colours by Channel number, groups taking the same palette (the address bus and RW both magenta in the LED computer's shot).
- **The Board's discs show the Channel number** (1–64; 20–27 for a data bus), not the row: the scope's Hooks now show their band's place.
- **Arm is one-shot.** An armed analyzer waits through power cycles; a Capture ends it (`captured`); seeing the circuit again means pressing Arm again. The phase is a word (`armed: waiting for trigger`, `triggered: filling`, `captured`) in a fixed slot.

What works and stays: the Capture model (64 Channels, 65 536 points, sample period, the Trigger early/middle/late), the bus rows in hex with their slanted cells, groups made from a pin's stem in one click, the per-Channel Trigger conditions and the bus pattern with `x`, zoom about the pointer, the Marker, `dt`.

## Ideas

Each idea has the agent's recommendation; the user decides.

1. **A drawn case, the scope's family.** The same frame, panel, key caps, red LEDs and BW logo, so the two Equipment read as one maker's. The title bar, its size button and the blue buttons go; the size key and the × on the case, dragged by its body (P8h's `CaseClose`, `NextInstrumentSize`). The analyzer is wider (940 px normal), so its strip has room for more groups. Recommended.

2. **The strip of keys**, left to right, one group per concern:
   - the badge (logo, `BODGE` / `WIRE`);
   - CAPTURE: `ARM` as a key, three lamps for the phase (`ARMED`, `TRIG'D`, `DONE`) in place of the word;
   - SAMPLE: ◀ LED ▶ (`100 ns`), as TIME / DIV is on the scope, a click on the LED opening the grid by unit;
   - TRIGGER AT: a three-way rocker `EARLY | MID | LATE` (10, 50, 90 % of memory), the time before it in the tooltip, not on the key;
   - VIEW: ◀ LED ▶ for the zoom (`1 µs/div`), `FIT`;
   - the size key.

   The view's keys could instead live on the screen (an overview strip with FIT); the strip then holds the Capture only. Recommended: in the strip, one place for every key.

3. **Labels on the screen, as plates.** Each row's plate over its lane's left end: the swatch, the name, the value, the Trigger key, the ×; no column headings. About 200 px instead of 360. Rows keep a fixed height (there can be 64), so plates are one line, not the scope's two. Recommended.

4. **The Trigger as a lit key on each plate**, like the scope's `T`: dark when `any`, lit orange with its condition otherwise (`0`, `1`, ↑, ↓), a click opening the five choices. A group's plate shows its pattern lit (`F800`) when it has one. Every Trigger condition is then orange and visible in the rows at a glance. Optionally, the whole Trigger summed up in the strip's TRIGGER group (`SYNC↑ · A=F800`). Recommended: the lit keys; the summary only if it fits.

5. **The cursor's tag on the screen**, its time and `dt` from the Marker, as on the scope; the mouse snaps to an edge of the row it is on, as a Finger does (P8h). The status line and the gesture hint go. Recommended.

6. **The value without a cursor**: at the Trigger, instead of empty. What was on the bus when it triggered is the first thing read on a CPU (`F800`, `A9`). With a cursor or Marker, the value there, as now. Recommended.

7. **The overview**, if kept: thinner (about 10 px), inside the screen above the time axis, its box at least a few px, the Trigger and Marker as ticks. Or gone, `FIT` and the zoom keys doing its job. Recommended: kept thin; a 6.55 ms Capture viewed at 1 µs/div is lost without it.

8. **Rows grow to fill the screen when few**, up to about twice their height (28 → 56 px), and scroll when many. Recommended, as the scope's bands do.

9. **Rows reordered by dragging their plates** (P8h's `Dragging`), and the Board's discs numbered by row place, as the Hooks' are. Groups stay one row; a member moves only inside its group, or not at all. Recommended.

10. **Colours by row, not by Channel number**: the eight colours given in row order, a group's members drawn in their group's colour. Fewer collisions under eight rows; past eight they repeat, as now. Recommended.

11. **Grouping made visible**: the plates' swatches are checkboxes once two or more rows exist; with two ticked, a `GROUP` key lights in the strip (or on the first ticked plate). Ungroup: a key on the group's plate, not a popup behind ×. Alternative: drag a plate onto another to group them. Recommended: the ticks and the key; dragging already means reordering.

12. **The `pin | bus` switch** as a drawn rocker in the strip (`PIN | BUS`), lit side as now. It stays a mode of the Board's clicks; on the analyzer is where the player looks for it. Recommended.

13. **One-shot, re-armed at every power-on** (the user, 2026-11-26). A Capture still ends the arming, but a power-on now always arms (today it only carries an arming still waiting: after a Capture, the player has to press Arm again). Repair, power on, read the new Capture: no key. `ARM` stays on the strip to capture again without a power cycle. Rejected: a repeating Run. A new Capture each time the Trigger matches is stable only on a repeating signal (the counter, video sync), which the scope already shows well; on a CPU triggered on SYNC it replaces the Capture every frame, a different moment of the program each time, and studying one would need a STOP key, the Run/Stop the scope just lost.

## Questions for the user

- The drawn case (1) and its strip (2): wanted, as on the scope?
- The overview (7): kept thin, or gone?
- The value at the Trigger when there is no cursor (6)?
- Which of 3–12 are worth their cost; anything missing from the list of problems?

## Later

- **Decoding a bus**: a data bus row read as ASCII, or as 6502 / Z80 instructions at SYNC or M1 (the binder's Contents already disassembles both).
- **Search**: go to the next or previous point where a row has a value (`F800`) or an edge.
- **Saving a Capture** to compare with the next one (before and after a repair).

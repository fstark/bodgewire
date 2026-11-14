# P5f — Instrument polish

Roadmap step P5f ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the Oscilloscope and the Logic Analyzer made coherent before the computers. Inventory: [INVENTORY.md](INVENTORY.md). Design: [INSTRUMENT_POLISH_DESIGN.md](INSTRUMENT_POLISH_DESIGN.md). Plan: [INSTRUMENT_POLISH_PLAN.md](INSTRUMENT_POLISH_PLAN.md). Recipe: [ADD_INSTRUMENT.md](../recipes/ADD_INSTRUMENT.md). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

P5e made both instruments fit for a 6502 bus, but each window grew its own way of saying the same things. The scope zooms with a timebase combo and cannot be dragged; the analyzer zooms with the wheel and never says how much time is on screen. The scope has a grid without times, the analyzer times without a grid, far below its lanes. The Trigger is an orange mark in one and a white line like the cursor in the other. Where the Trigger sits is `delay` in one and `Trigger at 10 %` in the other, and the analyzer's sample setting is a frequency when everything else in both windows is a time. The analyzer opens on its whole 6.5 ms Capture, where every bus lane is a solid bar. A bus value cut by the view's edge is drawn closed, so it looks shorter than it is (`F837` beside `F838`). The marker can't be cleared, and the readout moves around and vanishes when the mouse leaves.

## User decisions (2026-11-14)

The inventory was sorted by the user: the agent's proposal accepted as is, A12 (values cut at the edge) added by the user. The `nice` items are done too, except A9 (label splitter), dropped with A10. Mockups: [the P5f mockups artifact](https://claude.ai/artifact/JaaDoQ99Avh7mJvdfAdpKR).

| Question | Decision |
|---|---|
| The analyzer's sample setting | chosen as a period (`Sample every 100 ns`), the capture length beside it (`· 6.55 ms captured`); no frequency in the header |
| The analyzer's first view (A3) | centred on the Trigger at 10 sample points per division, rounded up to a 1-2-5 step (1 µs/div at 100 ns): predictable, and the address bus reads at 100 ns |
| The scope's wheel (C5) | steps the timebase; the stored Sweep is redrawn at once at the new scale (and at a dragged position), blank where it holds no data, until the next Sweep |
| `nice` items | C9, S3, S4, S6, S7 done; A9 dropped |

Decided while designing (2026-11-14), for the user to overrule:

- **One view model.** Both windows show 10 divisions of a time per division in 1-2-5 steps, and a left edge given as a time from the Trigger. In the analyzer this is a view over the stored Capture; in the scope it *is* the settings (timebase, delay), so zooming or dragging the scope changes what the next Sweep records.
- **Grid anchored to the Trigger.** Grid lines sit at whole multiples of the time per division from T, so they slide with a pan, and their labels are times from T (`-2 µs`, `0`, `+2 µs`); a label is skipped (every 2nd or 5th) when labels would touch.
- **Zoom limits.** Analyzer: from 1 sample point per division to the smallest step holding the whole Capture (`Fit`). Scope: the existing timebases, 5 ns to 10 ms per division.
- **`Fit`** in the analyzer picks the smallest step whose 10 divisions hold the Capture and centres it; the area outside the Capture is drawn darker. Double-click does the same.
- **The record** an analyzer view moves over runs from the Capture's first sampled point to its end: right after power-on, `Fit` and the darker area show only what was sampled.
- **Pan limits.** The analyzer's view stays inside the Capture while it is shorter than the Capture. The scope's position is its delay, from 0 to 1000 divisions; a drag snaps to tenths of a division, measured from where it started, so a slow drag still moves.
- **The scope's `T` button and double-click** set the delay to 0: the Trigger back at the first division.
- **A new Capture keeps the view** (time per division and position from T) when its sample period is the previous one's; otherwise it opens at the first view. The marker is cleared, as today.
- **Horizontal wheel** (trackpad swipe) pans half a division per notch, in the direction of the swipe; checked at the user check.
- **Marker**: click sets it, right-click on the waveforms or `Esc` while the window is focused clears it; that `Esc` is not passed on to the Board (where it would drop the probe, the meter's leads or a wire's start). It is stored as a time from T in both windows and drawn with an `M` tag on the top edge.
- **Status line**, under the waveforms in both windows: `cursor +3.7 µs   M +2 µs   dt 1.7 µs (588 kHz)` (`dt`: neither UI font has `Δ`). Without a hover it shows the marker alone, or the gesture hint when there is no marker. The frequency appears only beside a `dt`.
- **Off-screen Trigger**: an orange arrow on that side of the axis strip with the distance (`T 800 ns`), never over a trace.
- **Scope values (S5)**: each Hook row shows the level and volts at the cursor, else at the marker: `H 3.5 V`, `L 0.2 V`, `Z 1.4 V` (floating), `X`, or the rail's volts.
- **Scope bands (S3, S4)**: only hooked Channels get a band; the bands share the height left by the header and the rows, at least 50 px each. No Hook: one empty band with the hint.
- **Scope Stop (S7)**: `Stop` freezes the screen on the last Sweep (Sweeps still run, not shown; zoom and drag still redraw it); `Run` shows the next one. The state reads `stopped`. Only the window stops: nothing changes in the Session.
- **Scope position (S2)**: the `delay` field is labelled `position`; the `-`, `+`, `0` buttons give way to the zoom buttons and `T`.
- **Analyzer header (A1, A2, A8)**: `Sample every [100 ns]` with `· 6.55 ms captured`; `Before T [655 µs]` lists the three Trigger positions as times at the current period; the `Trigger` button becomes `T`.
- **Analyzer rows (A6, A7)**: headings `Name`, `Value`, `Trigger` in the axis strip; a group is named with its bits, `U1 A[15:0]`.
- **Values cut at the edge (A12)**: a bus run cut by the view's edge has no slant there; its value is drawn inside the visible part, against the cut edge, when it fits.
- **`bw_ui`**: `--rate 10MHz` becomes `--sample 100ns` (no frequency anywhere); `--view 1us[:+20us]` sets the analyzer's time per division and its centre from T; `--marker +2us` and `--cursor +3.7us` put a marker and a fake hover in every open instrument window, for screenshots of the readouts.

## What is new

1. **Time view** — `src/view/time_view.{hpp,cpp}`: the shared, ImGui-free maths of both windows: 1-2-5 steps, zoom about a time, pan and limits, `Fit`, the first view, the grid and its labels, the off-screen Trigger, where a cut value's text goes, the scope's delay from a view, the status line text. `FormatTime` moves from `src/ui` to `src/view` beside a new `FormatFrequency`. Unit tested.
2. **Analyzer window** — header (A1, A2, C10), overview strip (A5), sticky axis strip with headings above the lanes (A4, A6), grid (C2), orange Trigger (C4), first view (A3), runs open at the edges (A12), group names with bits (A7), wheel, drag, horizontal wheel and buttons (C5, C6, C10), marker (C7), status line (C8, C9).
3. **Scope window** — the same view and gestures over the settings (C1, C5, C6, C10), stored Sweep redrawn at the current settings, axis strip with times and the off-screen Trigger (C2, C4), `position` (S2), bands (S3, S4), values in the Hook rows (S5), `1 V/div` (S6), `Stop`/`Run` (S7), marker and status line (C7, C8, C9).
4. **`bw_ui`** — `--sample`, `--view`, `--marker`, `--cursor` (C11).
5. **Recipe and glossary** — ADD_INSTRUMENT: the time view, the new flags, the first view replacing the "set the view by a temporary edit" gotcha, the fonts' missing glyphs. GLOSSARY: **Marker**.

Nothing else changes: engines, consumers, the Session, the kernel, Parts, Machines and Puzzles are untouched.

## Tests

- **Steps:** up and down from 1 µs (2 µs, 500 ns); the step at least 1.6 µs is 2 µs, at least 650 µs is 1 ms; limits hold.
- **Zoom about a time:** the time under the mouse stays at the same fraction of the width after a step in or out; clamped at the limits.
- **Limits:** a view inside a 6.55 ms record stays inside after a pan past either end; a view wider than the record is centred on it.
- **Fit:** a record of 65 536 points at 100 ns with the Trigger at 10 % (−655.3 µs … +5.898 ms) fits at 1 ms/div, centred.
- **First view:** at 100 ns, 1 µs/div from −5 µs; at 10 ns, 100 ns/div; at 1 ms, 10 ms/div; a record starting 300 ns before T (sampled from just after power-on) clamped to it.
- **Grid:** at 1 µs/div from −3.5 µs over 660 px, lines at −3 … +6 µs, all labelled; the same over 200 px labels every 5th: `0` and `+5 µs`; label text `-3 µs`, `0`, `+6 µs`.
- **Off-screen Trigger:** a scope view at delay 1 µs, 200 ns/div gives the Trigger 800 ns to the left; a view holding T gives none.
- **Cut values:** a run cut on the left puts its text against the left edge; cut on the right, against the right edge; cut on both, centred in the view; a run too narrow for its text gives none; a whole run centres as today.
- **Scope delay from a view:** zooming the scope about the cursor gives the delay keeping that time put, clamped to 0 … 1000 divisions; a drag snaps to tenths of a division; 10 drags of 2 px from the press move the delay (no rounding per frame).
- **Status line:** cursor only; marker only; both with `dt 1.7 µs (588 kHz)`; `dt 0` without a frequency.
- **`FormatFrequency`:** 588 kHz, 1.67 MHz, 10 MHz, 50 Hz.
- No UI tests exist: the windows are checked by release screenshots, each set from `bw_ui` flags: the analyzer's first view on the 110 at 100 ns (bus values readable around the `8000` Trigger), the same with `--view 1us:+5us --marker +2us --cursor +3.7us` (grid, overview box, marker, status line, values cut at both edges), `Fit`; the scope with 3 Hooks at 200 ns/div and a 1 µs delay (axis times, Trigger arrow in the axis strip, shared bands, values, marker, status line).
- **Release:** the 110 with every instrument as in P5e: 1.00×, UI under ~1 ms.

## Later

- An activity trace in the analyzer's overview (where each lane changes).
- A label column splitter (A9).
- Single-shot on the scope.

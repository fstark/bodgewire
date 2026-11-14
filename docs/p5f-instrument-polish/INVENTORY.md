# P5f — Inventory of the scope and the logic analyzer

State on 2026-11-14 (after P5e), from `src/ui/scope_window.cpp`, `src/ui/analyzer_window.cpp` and release screenshots on `netlists/led_computer.bwn` (32 Clips at 10 MHz with the `8000` Trigger; 4 Hooks at 200 ns/div with a 1 µs delay).

Each line: what is there, the problem, a suggested change. **Sort** column: `must` / `nice` / `drop`; the agent's proposal, accepted by the user on 2026-11-14 with A12 added; after it, the outcome in P5f (2026-11-14).

## Shared model (both windows)

| # | Now | Problem | Suggested change | Sort |
|---|---|---|---|---|
| C1 | Scope: zoom = timebase combo (applies to the next Sweep). Analyzer: zoom = wheel over the stored Capture, no number | Two ideas of zoom; the analyzer never says how much time is on screen | Both show **time per division** as the zoom, in 1-2-5 steps; the analyzer's wheel steps it (stored data), the scope's wheel steps the timebase | must, done |
| C2 | Scope: 10-division grid, no time labels. Analyzer: tick labels below, no grid | Same thing drawn two ways; neither has both | One grid of 10 divisions in both, with time labels relative to the Trigger on the divisions (`-200 ns`, `0`, `+200 ns`…) | must, done |
| C3 | Scope: position = `delay` (text, `-`, `+`, `0`). Analyzer: position = `Trigger at` 10/50/90 % of memory | Two names and units for "where the Trigger sits" | Analyzer: `Trigger at` stays a capture setting (what memory holds before T); **view** position in both is "screen centre / left edge vs T" read off the axis labels. Scope's `delay` renamed `position` and set by drag too (C5) | must, done |
| C4 | Scope: orange Trigger line + triangle, off-screen arrow `◀T -800 ns` drawn over Channel 4. Analyzer: thin white Trigger line, close to the cursor's white | Trigger mark differs; the analyzer's is confused with the cursor; the scope's off-screen label hides a trace | One Trigger mark: orange line + triangle on the top edge; off screen, an arrow on the edge in the axis strip (not over a trace) with the distance (`T ◀ 800 ns`) | must, done |
| C5 | Analyzer: wheel zoom, left drag pan, click marker, double-click fit. Scope: wheel and drag do nothing | Scope has no navigation at all; gestures differ | Same gestures in both: wheel = time/div step around the mouse, drag = pan (scope: drag changes its position/delay), click = marker, double-click = fit (scope: back to delay 0) | must, done |
| C6 | Trackpad horizontal swipe (`MouseWheelH`) ignored in both | On a Mac the natural pan gesture does nothing | Horizontal wheel pans in both | must, done |
| C7 | Marker: click sets it, nothing clears it, no label on the line | Stale marker forever; which line is which | Right-click or `Esc` clears; marker drawn with a small `M` tag on the top edge | must, done |
| C8 | Readout: scope beside Hook row 1 (`t = … dt = …`), analyzer at the window foot; both only while hovering, hint text otherwise | Different place; vanishes when the mouse leaves the canvas | One status line under the canvas in both: `cursor +1.23 µs   marker +0.80 µs   Δ 430 ns`; marker part stays when not hovering | must, done |
| C9 | `Δ` shown as a time only | Measuring a clock means doing 1/Δ by hand | `Δ 430 ns (2.33 MHz)`: the frequency appears only here, beside the time it comes from | nice, done |
| C10 | Visible controls: none for zoom/pan in either; gestures only in a grey hint | Not discoverable | Zoom `−`/`+` buttons beside the time/div readout, `Fit`, `T` (centre on Trigger); hint moved into tooltips of those buttons | must, done |
| C11 | `bw_ui`: `--rate`, `--timebase`, `--delay` only | Screenshots can't show a zoomed analyzer, a marker or a cursor readout | `--view 200ns/div` (analyzer, centred on T), `--marker +1us` (both), `--cursor +1.5us` (fake hover for the readout) | must, done |

## Oscilloscope

| # | Now | Problem | Suggested change | Sort |
|---|---|---|---|---|
| S1 | Timebase combo `200 ns/div`, Trigger combo `ch1 rise`, radios `auto`/`normal`, state text | Fine; the combo is the only zoom control | Keep; plus wheel and `−`/`+` (C1, C10) | —, kept |
| S2 | `delay` row: text field, `-` `+` `0` buttons on a second header line | Wordy; the field shows "1 µs" with no hint it is the left-edge shift | Rename `position`, keep the field for typing; drag on the screen sets it (C5); `0` → `T` button (C10) | must, done |
| S3 | Bands fixed at 70 px × 4; window resize only widens | Big window, same small traces | Bands share the window height (min 50 px) | nice, done |
| S4 | Free Hooks keep an empty band | Wasted height with 1–2 Hooks | Only hooked Channels get a band (when ≥ 1 hooked) | nice, done |
| S5 | Hook rows under the screen repeat the band labels, `×` far right at x = 220 | Duplicate labels; no value | Hook rows show the value at the cursor/marker (`H`/`L`/`X` and volts), like the analyzer's value column | must, done |
| S6 | `5 V` at top right only | Volts per division unknown | `1 V/div` beside the time/div readout | nice, done |
| S7 | No Run/Stop | The screen keeps changing while you measure; the analyzer has Arm/Stop | `Stop` freezes the last Sweep (keeps zoom/marker), `Run` resumes | nice, done |

## Logic Analyzer

| # | Now | Problem | Suggested change | Sort |
|---|---|---|---|---|
| A1 | `Rate 10 MHz` combo, label after the box | A frequency for what is a time step; nothing says what it buys | `Sample every 100 ns · 6.55 ms captured`: the combo lists periods, the capture length beside it | must, done |
| A2 | `Trigger at 10 %` combo, label after the box | Percent of memory, opaque | `Before T: 655 µs` (combo of 10/50/90 % shown as times at the current rate) | must, done |
| A3 | First view fitted to the whole Capture (6.5 ms) | Bus lanes and clocks are solid bars: nothing readable | First view centred on T at a time/div where the busiest lane's values near T are readable (bus text fits); `Fit` still shows all | must, done |
| A4 | Axis pinned at the window foot | With few rows, a large gap between lanes and axis (screenshot) | Axis strip on top of the lanes, directly under the header, sticky when rows scroll | must, done |
| A5 | No overview of where the view sits in the Capture | Zoomed in, you don't know where you are or how much is left | Overview strip (whole Capture, the view as a box, T and marker marks), draggable, doubles as scrollbar | must, done |
| A6 | Columns name / value / Trigger / `×` without headings; value empty unless hovering | The `0`/`any` column reads as a value; value column often blank | Column headings `Value` `Trigger`; value shows the marker's when not hovering (it already does) — heading makes it clear | must, done |
| A7 | Group row `U1 A  16` | `16` reads as a value | `U1 A[15:0]` | must, done |
| A8 | Trigger button name `Trigger` beside `Trigger at` | Three things called Trigger in one header | `T` button "centre on the Trigger" (C10) | must, done |
| A9 | Label column fixed 340 px | A third of the window on labels | Narrower (≈ 260 px), splitter drag | nice, dropped |
| A10 | Hand grouping: click names, a `Group` button appears in the header | Hidden until two are picked | Tooltip already says "select for a group"; keep | drop, dropped |
| A12 | A run cut by the view's edge is drawn closed at the edge (slant, value centred in what is left) | Looks like a short value (`F837` seems shorter than its neighbour) when it is only truncated (user, 2026-11-14) | Runs drawn from their true ends and clipped: no slant at the view's edge, the shape runs off the canvas; the value kept readable in the visible part, pinned to the edge | must, done |
| A11 | Status text `captured` / `armed: waiting for trigger` | Fine | Keep | —, kept |

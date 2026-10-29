# Logic Analyzer — Specification (P3c)

Third P3 milestone (see [ARCHITECTURE.md](../ARCHITECTURE.md), Roadmap and Instruments). The Logic Probe shows one Net "now". To see *how* Nets move relative to each other (a counter's bits, a decoder output against its inputs, a missing pulse), the player needs many Nets on a common time axis, frozen around an interesting moment. P3c adds the in-game Logic Analyzer: clip leads onto pins, set a Trigger, capture, read the waveforms. Builds on the per-Net counted watches (P2e) and the electrical Nets of the Logic Probe (P2g).

Terminology: [GLOSSARY.md](../GLOSSARY.md) (adds **Channel**, **Clip**, **Capture**).

## Decisions

- **8 Channels, 4096 sample points each, no decoding.** "Few channels, no decoding" (ARCHITECTURE Instruments): a difficulty lever. 8 covers a 74LS161 pair or a gate with its inputs.
- **Clips go on pads only**, like real grabbers: a new Board tool `Clip (C)`. Click a pad: the lowest free Channel clips there. Click a clipped pad: unclip. Clips stay across power cycles and Repairs.
- **Electrical Nets, like the Logic Probe.** A Clip reads the Net at its pad in the built Circuit: a cracked Trace shows on the far side. Labels name the pad (`U6.6 1Y`), never a Net.
- **Sampling at the Analyzer's own rate**, 1-2-5 steps from 1 kHz to 100 MHz (default 10 MHz). Each sample point reads the state after all changes up to that time; a pulse between two points can be missed, as on a real one.
- **Two levels plus X.** A Channel shows 0 or 1 as the logic reads it; X is drawn as a grey band. Floating is not shown (that is the Logic Probe's job).
- **Trigger: one condition per Channel, ANDed**: `any`, `0`, `1`, `rise`, `fall`. All `any`: triggers at once. The Trigger point sits at 10, 50 or 90 % of memory (default 10 %).
- **Single capture.** `Arm` waits for the Trigger, fills the post-Trigger memory, then freezes the Capture. `Stop` disarms. To capture again, `Arm` again. Arming with the power off waits for power-on (the bench workflow: arm, power on).
- **A floating window**, `Logic Analyzer`, toggled from the toolbar (`Analyzer (A)`), opened by the first Clip. Not in the right column (P3b feedback). The last Capture stays shown until the next one completes, across power-off and Repairs.
- **Look and feel are first guesses**, tuned once played.

## Scope

In: Clip tool, Clip tags on the Board, Analyzer settings, Trigger, single Capture, waveform display with zoom, pan and time readout, `bw_ui --clip`, headless capture engine.

Out: bus grouping and hex display, protocol decoding, repeat/auto-arm, pause-on-Trigger (run control), more than 8 Channels, clipping Traces or vias, export, the dev analyzer (VCD already covers it).

## Behaviour

### S1 Clips

- `Clip (C)` tool: clicking a pad clips the lowest free Channel there; clicking a clipped pad unclips it. With all 8 Channels used, a click on a free pad is refused (toolbar flashes amber, as for a refused Repair). Works powered or not.
- The Board draws each Clip as a small tag in its Channel's colour with the Channel number, at the pad, always (not only on hover).
- In the Analyzer window, each Channel row shows its colour, label (`U6.6 1Y`, or `free` greyed), Trigger condition and an `×` that unclips.
- Two Clips on pads of the same Net are allowed and show the same waveform.
- `bw_ui --clip REF.PIN[:T],... [--rate 1MHz]` (`T`: `0`, `1`, `r`, `f`) clips in order, sets the Triggers, opens the window and arms, so screenshots show a Capture.

### S2 Settings and run

- Header row: state, `Arm`/`Stop`, `Rate` (1 kHz … 100 MHz), `Trigger at` (10 / 50 / 90 %).
- States, shown as text: `idle`, `armed: waiting for power`, `armed: waiting for trigger`, `triggered: filling`, `captured`.
- Changing Clips, Rate, Trigger or position while armed restarts the wait from now (memory cleared). While idle or captured, changes apply to the next `Arm`.
- Power-off while armed: the Capture in progress is dropped and the Analyzer stays armed for the next power-on. A Repair rebuilds the Circuit; Clips and armed state carry over.
- The Trigger is accepted from the first sample point after arming: pre-Trigger memory not yet filled is shown empty.

### S3 Display

- One lane per clipped Channel, in Channel order, under its label; a time axis relative to the Trigger (`0` at the Trigger, marked by a vertical line; `-12.8 µs`, `+300 µs`).
- 0 low, 1 high, X a grey band, no data (before arming) blank. Runs narrower than a pixel draw as a filled activity block.
- Mouse wheel zooms around the cursor, drag pans, double-click fits the whole Capture. Opening a new Capture fits it.
- Hovering the lanes draws a vertical cursor and shows its time; a click sets a marker and the readout shows `Δ` between marker and cursor (period counting). Hover never touches the Board.
- Before any Capture: `arm, then power on` greyed.

### S4 Checks

- Headless (capture engine fed Samples, and on the test pattern through the Simulator):
  - sampling: a point reads the last change at or before it; a pulse between two points is missed; X shows as X;
  - Trigger: `rise`/`fall` only on a 0→1 / 1→0 between consecutive points; levels ANDed with an edge; all-`any` triggers at the first point; position 10/50/90 % puts the Trigger at point 409/2048/3686; a Trigger before the pre-Trigger memory is full leaves the start empty;
  - test pattern at 1 MHz, Trigger `fall` on `U6.6` (`LINE_END_N`): the next falling edge is 64 points after the Trigger; `U2.9` follows `U6.6`;
  - no leaks: in `broken_trace`, `U1.14` toggles and `U13.6` does not; in `stuck_gate`, `U12.11` is constant 1;
  - Session: Clips survive a Repair and resolve to the rebuilt Circuit's Nets; arming with the power off captures after power-on.
- `--screenshot` of `broken_trace --clip U1.14:r,U13.6,U6.6 --rate 1MHz`, read by the agent: tags on the Board, window, lanes, Trigger line.
- Release: test pattern at `1×` with 8 Clips including `CLK` at 100 MHz: ≈ 1.00×, `ui` ms within noise.

## Done when

- S1–S4 implemented; tests pass in debug and release.
- The user diagnoses the three Puzzles with the Analyzer, e.g. sees `H0` stop at the crack in `broken_trace`.

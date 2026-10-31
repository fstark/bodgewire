# Oscilloscope and Multimeter — Specification (P3d)

Fourth P3 milestone (see [ARCHITECTURE.md](../ARCHITECTURE.md), Roadmap and Instruments). The Logic Analyzer samples many Nets at its own rate, so it cannot show a 13 ns gate delay, and it hides floating Nets. The Oscilloscope shows two Nets live with exact edge times and real-looking levels. The Multimeter answers the two bench questions the others can't: "what voltage is here?" and "are these two points connected?". Builds on the Logic Analyzer's Consumer and window (P3c) and the probe's electrical Nets under any copper (P2g).

Terminology: [GLOSSARY.md](../GLOSSARY.md) (adds **Hook**, **Test Lead**, **Sweep**).

## Decisions

- **Where instruments go** (ARCHITECTURE, Instruments): held tips touch any copper, attached leads go on pads only. Logic probe and Multimeter Test Leads: pads, Traces, vias. Logic Analyzer Clips and Oscilloscope Hooks: pads only; a click on a Trace or via is refused with the reason (`Hook: attaches to pins only`).
- **Tool and window go together**: picking `Clip (C)`, `Hook (O)` or `Meter (V)` selects the tool and opens its window; each window has its tool's button (pressed while active); focusing a window never changes the tool. The toolbar's separate `Analyzer (A)` toggle goes: the window's `×` closes it, the tool reopens it.
- **Logic-state voltages, shared** by the Oscilloscope and the Multimeter: 0 → 0.2 V, 1 → 3.5 V, floating → 1.4 V, X → 1.0 V, power off → 0 V everywhere. A rail (a Net on a `POWER` connector pin) reads that pin's declared voltage (`VCC` 5 V, `GND` 0 V), not a hard-coded value, so later rails (+12 V, −5 V) need no instrument change. Edges are vertical (no analog rise times).
- **Oscilloscope: 2 Channels, no memory.** Each Sweep is drawn from the exact Sample times, so delays are readable to the picosecond. It shows the last completed Sweep and keeps redrawing while running; pausing the simulation freezes it (the only "memory").
- **Timebase** 5 ns/div … 10 ms/div in 1-2-5 steps, 10 divisions; the Trigger at the first division. **Vertical** fixed: 0–5 V per Channel, the two Channels in stacked bands.
- **Trigger**: rising or falling edge on Channel 1 or 2, at the TTL threshold (a logic edge). Mode `auto` (default: free-runs when no Trigger came within one Sweep length or 50 ms of simulated time) or `normal` (waits).
- **Multimeter: two modes.** `V`: the red Test Lead against GND (black Lead on the `POWER` connector's GND, implied); reads the average over the last 200 ms of simulated time, so a 50 % square wave reads ≈ 1.85 V, refreshed 4× per second. `Continuity`: red and black Leads on two points; beeps when both are on the same electrical Net (through Bodge Wires, silent across a crack and across resistors); power on: refused (`Meter: power off for continuity`), as on a real board.
- **Beep is visual** (a lit lamp and `BEEP`); sound later.
- **Counter Board output connector: dropped from P3d.** Pins can be measured directly now; the connector comes with the first counter Puzzle that needs it.
- **Look and feel are first guesses**, tuned once played.

## Scope

In: placement rule on all tools, tool/window pairing (Analyzer included), Hook tool and Hook tags, Oscilloscope window (timebase, Trigger, cursors), Multimeter tool, Test Lead tags, Multimeter window (`V`, `Continuity`), `bw_ui --hook REF.PIN,... [--timebase 20ns]` and `--meter REF.PIN` (red Lead, `V` mode), headless engines.

Out: analog levels or rise times, more than 2 scope Channels, scope memory/single-shot, resistance and current, sound, the counter output connector, other P3c "Later" items (deeper analyzer memory, navigation bugs).

## Behaviour

### S1 Placement and tools

- `Clip (C)`, `Hook (O)`: click a pad attaches, click an attached pad detaches; a Trace or via is refused with an amber flash and the reason in the toolbar action text.
- `Meter (V)`: in `V` mode, a click places the red Lead (pad, Trace or via, as the probe); in `Continuity`, clicks alternate red then black. `Esc` lifts both Leads.
- The logic probe is unchanged.
- Each window's header starts with its tool button; picking the tool (button or key) opens the window if closed.

### S2 Oscilloscope

- Board: Hook tags like Clip tags, Channel colours yellow and cyan, labelled `1`, `2`.
- Window `Oscilloscope`: header with `Hook (O)`, `Timebase` (`5 ns/div` …), `Trigger` (`ch1 rise`, `ch1 fall`, `ch2 rise`, `ch2 fall`), `auto`/`normal`, state (`no hooks`, `waiting for trigger`, `triggered`, `auto`, `power off`); a dark screen with a 10 × 2-band grid, one trace per Channel and its label (`U1.14 QA`).
- Hover: vertical cursor and `t = …` from the Trigger; click: marker, readout `dt = …`.

### S3 Multimeter

- Board: red and black Lead tags at their points.
- Window `Multimeter`: `Meter (V)` button, mode `V` / `Continuity`, large readout (`3.50 V`, `0.00 V`, `- - -` with no Lead, `BEEP` lamp or `open`), what each Lead is on (`U1.14 QA`, `Trace`, `via`).

### S4 Checks

- Headless (engines fed Samples, and on the test pattern through the Simulator):
  - voltages: 0/1/floating/X, rails at their connector pin's voltage, and power off map as above; a 25 % duty Net reads ≈ 1.03 V;
  - scope: Sweep on `CLK` rise shows `U1.14` (`QA`) rising 13 ns or falling 15 ns after the Trigger (the LS161 delays); `auto` free-runs on a constant Net; `normal` waits;
  - continuity: in `broken_trace`, the two sides of the H0 crack are open, beep after the mend; refused with the power on;
  - placement: Clip and Hook on a Trace refused, probe and Leads accepted;
  - Session: Hooks and Leads survive a Repair (rebuilt Nets).
- `--screenshot` of `broken_trace --hook U1.2,U1.14 --timebase 20ns` read by the agent: tags, window, both traces, the 13/15 ns delay visible.
- Release: `broken_trace` at `1×` with 8 Clips, 2 Hooks on `CLK` and `QA` at 5 ns/div and the meter on `CLK`: ≈ 1.00×, `ui` ms within noise.

## Done when

- S1–S4 implemented; tests pass in debug and release.
- The user measures a gate delay with the Oscilloscope and finds the `broken_trace` crack by continuity with the power off.

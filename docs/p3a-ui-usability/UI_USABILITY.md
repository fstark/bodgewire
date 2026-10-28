# UI Usability — Specification (P3a)

First P3 milestone (see [ARCHITECTURE.md](../ARCHITECTURE.md), Roadmap). The P2 first playable works but is hard to use: the player has to know what to do already, finds chips and pins by hovering pad after pad, clicks precisely, and the controls sit in one undivided column. P3a fixes that before the P3 instruments are built on the same UI. Builds on [BOARD_VIEW.md](../p2f-board-view/BOARD_VIEW.md), [BOARD_PROBE.md](../p2g-board-probe/BOARD_PROBE.md) and [BOARD_REPAIR.md](../p2h-board-repair/BOARD_REPAIR.md).

Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Decisions

- **The player always knows the goal**: each Puzzle has a Brief (title and Symptom), and the Golden Picture sits beside the live TV.
- **The player always knows the state**: one banner says whether the power is on or off, which tool is active and what that tool does now.
- **Chips explain themselves**: a Chip panel shows the part, what it does and its pinout. Pin names appear on the Board when zoomed in.
- **Tool modes stay** (Probe / Swap / Mend). The fix is a clearer layout: a toolbar above the Board for everything that acts on the Board, and labelled sections in the right column.
- **Probing stays on the Board.** Clicking a pin in the pinout highlights and frames its pad, but does not probe it.
- **No new leaks.** Brief, Golden Picture, Chip panel and pin names come from the design Netlist, the Parts catalogue and the Golden Machine, never from Faults.
- **Look and feel are first guesses**, tuned once played. The spec only fixes what the tests check.

## Scope

In: Brief in `.bwp`, Golden Picture, state banner, toolbar, grouped right column, Chip panel with pinout, pin names on the Board, snap-to-pin, navigation additions, hidden debug stats.

Out (later P3 milestones fix these): Schematic (P3b), new instruments (P3c, P3d), new Repairs (P3e), "is it fixed?" check (P3f). Also out: settings persistence, tutorials, sound.

## Behaviour

### S1 Brief

- `.bwp` gains two optional lines: `title "Dead counter"` and `brief "The bottom of the picture is missing and the rest rolls."`. Double-quoted, one line each, at most once. If they are missing, the title is the file stem and the brief is empty.
- The three existing Puzzles get a title and a brief written from their Symptoms (checked on the TV), not their Faults. The dev comment stays.
- `bw_ui` shows the title in the window title (`Bodgewire — Dead counter`) and the Brief at the top of the right column. `bw run` ignores both.

### S2 Golden Picture

- On load, the Golden Machine (the design Netlist, no Faults) runs headlessly until its TV is locked (≤ 0.5 s simulated, see P2c S5). Its last full frame is the Golden Picture.
- It is shown under the live TV at about half the TV width, labelled `Should look like`. Machines without VIDEO show neither.
- The Golden Picture is computed once per load, not per Repair. Load time stays under 1 s in release.

### S3 State banner

- One line across the top of the window, always visible:
  - power: a large `POWER ON` (green) or `POWER OFF` (grey), plus `paused` when paused;
  - active tool and what a click does now: `Probe: click a pin or trace`, `Swap: click a chip`, `Mend: click a crack`;
  - when the tool cannot act: `Swap: power off first` and `Mend: power off first`, shown in amber.
- Clicking with a tool that cannot act flashes the banner amber, so the click is not silently ignored.

### S4 Layout

- **Toolbar** above the Board panel: Power, Pause, Speed (`1×` / `Max`), then tools (Probe P, Swap S, Mend M) as large toggle buttons with their keys shown, then Layer (F.Cu / B.Cu, L) and Fit (F).
- **Right column**, top to bottom, each a collapsible section with a header: `Puzzle` (Brief), `TV` (live + Golden Picture), `Probe` (lamps + probed pin as `U1 pin 13 QA · H0`, or `not probing`), `Chip` (S5), `Repairs` (log).
- **Debug stats** (`t`, speed ratio, lock, ring wait, ui ms) and Seed move into a `Debug` section, collapsed by default. Seed only takes effect at power-on, as now.
- Default window stays 1280 × 800.

### S5 Chip panel

- Shows the hovered Chip, or the selected one when nothing is hovered. A Chip is selected by clicking it (any tool) or one of its pads, and stays selected until another is.
- Contents: `U3 · 74LS161`, the Part description from the catalogue (`synchronous 4-bit binary counter`), and a pinout: a DIP outline with pin numbers and names on both sides, pin 1 marked, power pins greyed.
- Hovering a pin in the pinout highlights that pad and its design Net on the Board. Clicking it pans the Board so the pad is in view and flashes it.
- Connectors (J1, J2) and the crystal show their pins in the same way.

### S6 Pin names and labels on the Board

- Fitted view: each Chip shows its reference and Part (`U3` and `74LS161` under it) inside the courtyard.
- When a pad is at least ~14 px wide, its pin name (`QA`, `CLK`, `VCC`) is drawn next to it, outside the courtyard.
- Hover text is unchanged (P2f S5).

### S7 Snap-to-pin

- A click picks the nearest pad within a snap radius of 12 screen pixels or half the pin pitch, whichever is larger, ahead of Traces and vias. If there is no pad in range, P2f hit priority applies.
- The same snap applies to the hover tooltip and highlight, so what is highlighted is what a click acts on.
- The probe tip stays on the probed pad until another probe click. Clicking empty board does not unprobe; `Esc` does.

### S8 Navigation

- Double-click a Chip: zoom so its courtyard fills about half the Board panel, centred.
- Left-drag on the Board pans once the drag passes a few pixels, so a click is still a click. Right/middle drag keep working.
- `+` / `-` zoom around the panel centre. `F` and Fit unchanged.

### S9 Checks

- Headless:
  - `.bwp` parses `title`/`brief`; rejects duplicates, a missing closing quote and unknown keywords; the three Puzzles have both;
  - the Golden Picture of the test pattern equals the fault-free `bw run --tv` frame (same seed, same time);
  - snap: a point 0.5 mm off a test-pattern pad picks that pad over a Trace underneath; a point between two pads picks the nearer;
  - pinout data for every Part in the catalogue: one entry per pin with its number and name, matching the Part's pins.
- `--screenshot` of each Puzzle, fitted and with `--probe`, read by the agent: banner, toolbar, sections, Golden Picture, Chip labels.
- Release: test pattern still ≈ 1.00× at `1×`; UI frame < 16 ms fitted and at full zoom with pin names.

## Done when

- S1–S9 implemented; tests pass in debug and release.
- The user plays the three Puzzles again in `bw_ui` and finds the goal, the state and the right pin without hunting.

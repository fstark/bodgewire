# P6c — Shelf and focus

Roadmap step P6c ([ARCHITECTURE.md](../ARCHITECTURE.md#roadmap)): the first build step of [UX_DESIGN.md](../p6b-ux-design/UX_DESIGN.md) (P6b). Design: [SHELF_FOCUS_DESIGN.md](SHELF_FOCUS_DESIGN.md). Plan: [SHELF_FOCUS_PLAN.md](SHELF_FOCUS_PLAN.md). Recipe: [ADD_INSTRUMENT.md](../recipes/ADD_INSTRUMENT.md) (its Tool and Window layers change). Terminology: [GLOSSARY.md](../GLOSSARY.md).

## Why

Today's top of the screen is a banner (`POWER ON`, then a long, always-there instruction such as `Probe: click a pin (F.Cu: leg, B.Cu: pad) or trace`) above a row of seventeen text buttons: power, limit, pause, speed, nine tools, layer, fit, the User Manual. Three of the nine tools are an instrument's leads (`Clip (C)`, `Hook (O)`, `Meter (V)`), and each instrument window repeats its own lead button, so the player has two ways to arm a lead and an instrument whose window is closed can still be clipped on. The logic probe's lamps are in the side panel, far from the Board.

P6b settled what replaces it: one **shelf** along the bottom with drawings of the controls, the Board's tools and the equipment, and one **focus**, what a click on a pin does, owned by a tool or an instrument and shown in gold.

## User decisions (2026-11-17)

| Question | Decision |
|---|---|
| The banner's messages | no banner; one short hint at the shelf's right end, only when there is something to say: a refusal in amber for 2 s, a wire's second pin, the meter's black lead next, typing (`Typing on KBD · F2 to stop`), a powered tool that needs the power off. The cursor's refusal flash on the Board stays. The power state is read from the drawn switch |
| Layer and Fit | in the shelf's controls group, after speed |
| The TV and the knob (still side-panel sections until P6d) | drawn on the shelf; a click shows or hides their side-panel section; grayed on a Machine without one |
| `Esc` | steps back: a started wire is cancelled; else a tool or instrument other than the probe gives the probe the focus (leads, Clips and Hooks stay); else the probe's tip is lifted. An instrument window's own `Esc` (clear the marker) stays while it has keyboard focus |
| A click on an instrument's shelf drawing | away: out with the focus; out without the focus: takes the focus; out with the focus: put away, the probe takes the focus. The keys `C`, `O`, `V` do the same, except they never put away |
| Swap, iron, cutters and wire while powered | drawn grayed but still takeable; a click on the Board is refused (`power off first`) |
| What the probe touches (`U1.1 CKB · leg`) | in the probe drawing's tooltip; the side panel's Probe section is removed |
| How the drawings are made | vector, in code (ImDrawList, as the Board's parts), no image assets |
| Mend and Resolder | one soldering iron: a click on a chip pin resolders it, anywhere else mends a crack there; key `M` (`R` kept as an alias). Five Board tools, as in UX_DESIGN |

After the user check (2026-11-17): it works; looks to improve later. The analyzer and scope drawings looked the same: the analyzer is now a dark, low box with coloured lanes and a fan of clip leads, the scope a beige box with a square screen, knobs and probe sockets. A tool that owns the focus but cannot work (the power is on) is framed in red, not gold.

Decided while designing (2026-11-17), for the user to overrule:

- **Gold.** One focus gold, brighter than the Board's dim selection gold: `IM_COL32(255, 196, 40, 255)`, a 2 px frame round the owner's shelf drawing and round its instrument window. The keyboard's typing edge (amber, P4o) is left as it is; UX_DESIGN's "if the two golds confuse" applies.
- **A click anywhere in an instrument window** gives it the focus, as UX_DESIGN says ("clicking an instrument gives it the focus"), header buttons included. Closing the window is putting it away.
- **Start-up defaults** as today: the TV and knob sections shown, the keyboard out when the Machine has one, the User Manual away (`--manual user` brings it out).
- **The meter's leads** are lifted by an `×` beside the red lead's line in its window (both leads, as `Esc` did with the meter tool); `Esc` no longer lifts them.
- **Start-up flags** bring instruments out without the focus (`--clip`, `--hook`, `--meter`, as they open windows today); a new `--focus probe|swap|iron|cut|wire|analyzer|scope|meter` gives the focus at start, bringing an instrument out, so a screenshot shows the gold.
- **No Board** (a netlist-only Session, the benches): the tools, the instruments, Layer and Fit are grayed and inert (`--focus` has no effect); the TV, keyboard and knob work as their devices allow.
- **The iron on bare board** mends there, logging a mend and leaving a blob, as Mend does today (`Session::repair` mends wherever it is asked; the Session is untouched).
- **`W` pressed mid-wire** no longer cancels the started wire (the owner does not change); `Esc` does.
- **The shelf's order**, fixed on every Machine: controls (power switch with the LIMIT lamp, run/pause, speed `1×`/`Max`, layer, fit), tools (probe, chip puller, iron, cutters, wire), equipment (TV, keyboard, logic analyzer, oscilloscope, multimeter, User Manual, knob). The User Manual is the binder's slot until P6e.
- **Equipment that is out** is drawn lit: a lit screen (TV, analyzer, scope), a lit display (meter), a lit pilot lamp (keyboard, knob), an open book (manual).
- **Each drawing** has its key letter in a corner where it has one and a tooltip with its name, key and state; a grayed one says why (`No TV on this Machine`, `Power off to use the iron`).

## What is new

1. **Focus** — `src/view/focus.{hpp,cpp}`: who owns the focus and which instruments are out, with the rules above (take a tool, an instrument's key, a shelf click, a window click, a window closed, `Esc`). ImGui-free, unit tested.
2. **Board and windows on the focus** — the `Tool` enum gives way to the focus owner; the click handling in `main.cpp` reads it; Mend and Resolder merge into the iron; `Esc` steps back. The analyzer, scope and meter windows lose their lead buttons, take the focus on a click, and wear the gold frame while they own it. The meter gets its `×`. `--focus`.
3. **The shelf** — `src/ui/shelf.{hpp,cpp}` and `src/ui/shelf_drawings.{hpp,cpp}` replace `src/ui/toolbar.{hpp,cpp}`: the banner and the button row are gone; the drawings, graying, the probe's lamps and tooltip, the hint, the tool keys. The side panel loses its Probe section; its TV and Knob sections follow the shelf.
4. **Docs** — ADD_INSTRUMENT (Tool and Window layers), ADD_CONNECTOR_DEVICE (the knob's section shown from the shelf), GLOSSARY (**Focus**, **Shelf**, **Equipment**), the ARCHITECTURE row.

Nothing below the UI changes: engines, consumers, the Session, the kernel, Parts, Machines and Puzzles are untouched.

## Tests

- **Focus** (`tests/view/focus_test.cpp`): the probe owns the focus at start, nothing out; a tool taken owns it and the instruments stay out; `C` brings the analyzer out with the focus, again `C` keeps it out; a shelf click on an away scope brings it out with the focus, on an out scope without the focus gives it the focus, on the focused scope puts it away and gives the probe the focus; a window click on an out meter gives it the focus; closing an unfocused window keeps the owner, closing the focused one gives the probe the focus; `Esc` with a started wire cancels it and keeps the wire tool, with the scope focused gives the probe the focus and keeps the scope out, with the probe lifts the tip.
- No UI tests exist; a click on a window's close button not counting as a click (so closing an unfocused window keeps the owner in play) is checked by reading the code, as gestures were in P5f.
- The UI: the shelf and windows are checked by release screenshots, each set from `bw_ui` flags: the Stopwatch (`machines/040-stopwatch/puzzles/01-stuck-at-zero.bwp`) with `--probe` (lamps on the probe's drawing, controls, the TV grayed), with `--hook U1.1 --focus scope` (gold on the scope's drawing and window, the scope lit), with `--focus iron` (the iron gold and grayed while powered, the hint); the BW-2001 (TV and keyboard out); a bench netlist (tools and instruments grayed).
- **Release:** the 110 with every instrument as in P5f: 1.00×, UI under ~1 ms. The full release suite passes (no test reaches the UI).

## Later

- P6d makes the equipment objects (default places, sizes) and removes the side panel; P6e the binder in the User Manual's slot; P6f the post-it, also on the shelf.
- A cursor per tool or instrument (UX_DESIGN: "perhaps").

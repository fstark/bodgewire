# Net Panel — Specification (P3b)

Second P3 milestone (see [ARCHITECTURE.md](../ARCHITECTURE.md), Roadmap). Small Boards have no Schematic (decided 2026-10-28, see ARCHITECTURE "Board authoring"), so the player needs another way to answer "what drives this Net, and what does it feed?". Today the Board highlights a Net only while it is hovered, and the Chip pinout names a pin's Net but not where it goes. P3b adds a Net panel: pick a Net, see its Drivers and Loads, jump to any of them. Builds on [UI_USABILITY.md](../p3a-ui-usability/UI_USABILITY.md).

Terminology: [GLOSSARY.md](../GLOSSARY.md) (adds **Load**).

## Decisions

- **Selecting a Net works everywhere a pin or Copper is clicked**: a pad, via or Trace on the Board (any tool), a pin in the Chip pinout, a pin link in a manual, an entry in the Net panel, and `--probe REF.PIN`. No new tool and no new click: the clicks that exist already also select.
- **One selected Net, like one selected Component.** It stays selected until another is selected or `Clear` is pressed. `Esc` keeps meaning "unprobe".
- **The Board keeps showing the selected Net**, highlighted like a hovered one; hovering another Net shows that one instead while hovered.
- **A new right-column section `Net`**, between `Probe` and `Chip`. Not inside the Chip panel: a Net is not owned by one chip, and following it moves from chip to chip.
- **Power Nets are listed, folded**: GND and VCC show their supply, and their Loads under a closed `Feeds (N pins)` node. Listing them is honest and cheap; folding keeps the column short.
- **Design Netlist only, no leaks.** Drivers and Loads come from the design Netlist and the Parts catalogue: a cracked Trace does not split the Net in the panel, a stuck output is still listed as an output. Only Instruments see Faults.
- **Look and feel are first guesses**, tuned once played.

## Scope

In: Net selection, Board highlight of the selected Net, the `Net` section, headless Driver/Load listing.

Out: Schematic (revisit at BW-80), bus grouping, Net search by name, live Logic Values in the panel (that is the Logic Probe's and the P3c Logic Analyzer's job), Nets through series resistors (the netlist only has pull-ups and pull-downs).

## Behaviour

### S1 Selecting a Net

- Clicking a pad, via or Trace on the Board selects its design Net, in every tool, besides what the tool does (probe, swap, mend). A pad click still selects its Component too.
- Clicking a pin in the Chip pinout, a pin link in a manual (`` `U1` pin 15 ``) or an entry in the Net panel frames the pad (as in P3a) and also selects the pin's Component and Net. A Component-only link (`` `U6` ``, `Zoom to`) selects the Component and keeps the Net.
- Clicking free board or a Component body keeps the selected Net.
- `--probe REF.PIN` selects that pin's Net as well as its Component, so screenshots show the panel.

### S2 Board highlight

- The selected Net's pads, vias and Traces are drawn in the highlight colour whenever the hover does not highlight a Net (cursor off the Board, or over a Component body or free board).
- Highlight follows the design Netlist on fault-free Copper, as the P2f hover does: both halves of a cracked Trace light up.

### S3 Net section

- With no Net selected: `click a pin or trace`, greyed.
- Otherwise, top to bottom:
  - header: `LINE_END_N · 3 pins` and a `Clear` button;
  - `Driven by`: the Net's Drivers, one per line, or `nothing` (greyed) if it has none;
  - `Feeds`: its Loads, one per line, or `nothing`.
- Entry text, like the Board tooltip:
  - output (incl. open collector): `U6 pin 6 1Y`;
  - pull-up / pull-down resistor, the end on this Net: `R1 pin 2 · 4k7 pull-up`;
  - Connector supply pin on GND/VCC: `J1 pin 1 VCC · supply`;
  - Load (any input, a chip's power pin, a Connector input, the rail end of a pull-up): `U2 pin 9 LOAD_N`.
- Drivers are outputs, pull-ups/pull-downs and supplies; everything else is a Load.
- Order: by reference in natural order (`U2` before `U10`), then pin number.
- GND and VCC: `Feeds (N pins)` is a closed tree node. Other Nets show `Feeds` open.
- Hovering an entry highlights its pad on the Board (as a pinout hover does) and shows a tooltip with the Part and the Component's role: `74LS20 · end-of-frame decoder` (role only if there is a Service Manual).
- Clicking an entry frames the pad and flashes it, and selects its Component (the Chip panel follows).

Example, test pattern, `LINE_END_N`:

```
LINE_END_N · 3 pins                    [Clear]
Driven by
  U6 pin 6 1Y
Feeds
  U2 pin 9 LOAD_N
  U9 pin 5 3A
```

### S4 Checks

- Headless:
  - counter: `CLK` is driven by `X1 pin 8 OUT` and feeds `U1 pin 2 CLK`, `U2 pin 2 CLK`; `RESET_N` is driven by `R1 pin 2 · 4k7 pull-up` and feeds both `CLR_N`; `VCC` is a rail driven by `J1 pin 1 VCC · supply`, and `R1 pin 1` is one of its Loads;
  - test pattern: `LINE_END_N` as in the example; `CLK` lists `U5` before `U14`; every pin of every Net appears exactly once, as a Driver or a Load;
  - no leaks: in `broken_trace`, `H0` from the Puzzle's design lists `U1 pin 14 QA` and `U13 pin 6 2B`; in `stuck_gate`, `GRID` lists `U12 pin 11 4Y` as its Driver;
  - an unknown Net name gives an empty listing.
- `--screenshot` of `broken_trace --probe U6.6` and `--probe U1.16` (VCC, folded), read by the agent: section, entries, highlight.
- Release: test pattern still ≈ 1.00× at `1×`; `ui` ms unchanged within noise.

## Done when

- S1–S4 implemented; tests pass in debug and release.
- The user follows a Net from its Driver to its Loads on the three Puzzles without hovering pad after pad.

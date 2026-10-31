# Bodge Wires and Trace Cuts — Specification (P3e)

Fifth P3 milestone (see [ARCHITECTURE.md](../ARCHITECTURE.md), Pins, nets, drivers and Roadmap). P2h gave the player two Repairs, Swap and Mend, so every Puzzle so far had one "right" fix. Real repairs also reroute: cut a Trace, solder a Bodge Wire around a dead part. P3e adds both as Repairs through the Session, so a broken Net can be bridged instead of mended, and later Puzzles (shorts, Wrong Parts, P3f) can be fixed in more than one way. Builds on the Repairs and rebuild of [BOARD_REPAIR.md](../p2h-board-repair/BOARD_REPAIR.md) (P2h) and the placement rule of [SCOPE_METER.md](../p3d-scope-meter/SCOPE_METER.md) (P3d).

Terminology: [GLOSSARY.md](../GLOSSARY.md) (adds **Cut**).

## Decisions

- **Two new tools, both Repairs**: `Cut (K)` and `Wire (W)`. Like Swap and Mend they work only with the power off, are unlimited and are logged. Nothing says whether they fixed anything.
- **A Cut is a player-made crack**: a gap across a straight Trace at the clicked point on the Active Layer, the same gap as a Broken Trace. Mend fixes a Cut exactly as it fixes a crack (within 1 mm).
- **Bodge Wires go on pads only** (the P3d placement rule for things attached to the Board): two clicks, two different pads. Any pad of any Component, Connectors included. Wires to Traces or vias are out.
- **A Bodge Wire joins the two pads' islands**: the Nets merge at the next power-on. A wire across a crack restores the Net; a wire between two driven Nets is a short (Contention), a wire to a rail ties the Net to it, as the electrical rules already say. A VCC–GND wire puts everything on that island on GND until real power (P3f).
- **The Cut tool also removes a Bodge Wire**: clicking a wire snips it off. Wires are drawn above the Copper, so a click on a wire never cuts the Trace under it.
- **Player work is drawn, Faults are not.** Bodge Wires are drawn as wires with solder at both ends; Cuts as a dark mark at the point on their Layer, until mended. The drawn Copper still never changes (P2h), and Net highlighting and the Net panel still follow the design Netlist, ignoring Cuts and wires (ARCHITECTURE, Connectivity and visibility). Instruments (probe, Analyzer, Oscilloscope, Multimeter) read the rebuilt Nets; continuity beeps through a wire and is silent across a Cut at once, without power-on.
- **No undo** (P2h). Snipping a wire and mending a Cut are the ways back.
- **Look and feel are first guesses**, tuned once played.

## Scope

In: `Cut (K)` on Traces and Bodge Wires, `Wire (W)` between pads, Cut marks and wire drawing, repair log lines, Repair parsing for `bw run --repair` and `bw_ui --repair`, headless tests, a Puzzle repaired by a wire instead of a mend.

Out: wires to Traces or vias, wire routing or colours chosen by the player, cutting arc Traces, lifting pins, resoldering Joints, sockets, Repair Validation (P3f), new Puzzles.

## Behaviour

### S1 Cut

- `Cut (K)` with the power off: a click on a Bodge Wire removes it (`unwire U1.14 U13.6`); otherwise a click on a straight Trace (Active Layer first, as Mend) cuts it at the nearest centre-line point (`cut F.Cu 52.30 41.00`).
- Refused with the reason in the toolbar action text: on a pad, via, Component or bare board `Cut: click a trace or a wire`; on an arc Trace `Cut: straight traces only`; powered `Cut: power off first` (amber, as Swap and Mend).
- A Cut mark stays drawn until a Mend within 1 mm removes the Cut.

### S2 Wire

- `Wire (W)` with the power off: the first click on a pad starts a wire (a line follows the cursor), the second on another pad solders it (`wire U1.14 U13.6`). A click on the same pad, or `Esc`, drops the started wire.
- Refused with the reason: a click off a pad `Wire: solder to pins only`; powered `Wire: power off first`.
- Action text: `Wire: click the first pin`, then `Wire: click the second pin`.
- Drawn as a straight insulated wire, above Traces and below instrument tags, with a solder blob on each pad.

### S3 Power on after Cuts and wires

- As P2h S3: the Nets are rebuilt from the Copper with its Cuts and wires; the probe, Clips, Hooks and Test Leads read the rebuilt Nets.
- Continuity (P3d) follows Cuts, mends and wires at once, with the power off.

### S4 Command line

- `--repair` (`bw run`, `bw_ui`) accepts `cut:F.Cu:52.30:41.00`, `wire:U1.14:U13.6` and `unwire:U1.14:U13.6` (pins by number or name, `U1.QA`), next to `swap:` and `mend:`. A bad one is a usage error (exit 2); an unknown pin or a Cut off a straight Trace is an error naming it.

### S5 Checks

- Headless:
  - parse and format of the three new Repairs; errors for missing fields, unknown Layer, a wire to the same pad;
  - a wire joins two islands; a Cut splits a Trace's island; Mend removes a Cut; unwire restores the islands;
  - `broken_trace` repaired by `wire:U1.14:U13.6` (across the crack) gives the fault-free TV frame; the fault-free test pattern with a Cut on H0 does not, and mended again does;
  - Session: Cut and Wire refused while powered; logs; continuity across the crack beeps after a wire, without power-on;
  - the drawn Board and the design Net highlight are unchanged by Cuts and wires.
- `--screenshot`, read by the agent: `broken_trace --repair wire:U1.14:U13.6` (wire, blobs, log, grid back); `netlists/test_pattern.bwn --repair cut:F.Cu:137.59:99.17` (Cut mark on H0, grid broken).
- Release: rebuild after Cuts and wires still under 100 ms (P2h S3), 1.00× running.

## Done when

- S1–S5 implemented; tests pass in debug and release.
- The user fixes `broken_trace` with a Bodge Wire instead of a Mend, and breaks then restores the picture with a Cut and a Mend.

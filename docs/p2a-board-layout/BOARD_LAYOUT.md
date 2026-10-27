# Board Layout via KiCad — Specification (P2a)

First milestone of P2 (see [ARCHITECTURE.md](../ARCHITECTURE.md), Board authoring and Roadmap). A spike: can a Board for one of our netlists be laid out in KiCad's PCB editor and read back, with its Copper checked against the netlist? Decides between KiCad and our own layout generator before the rest of P2 depends on it. Builds on [PARTS.md](../p1b-parts/PARTS.md) (netlist format, catalogue).

Terminology: [GLOSSARY.md](../GLOSSARY.md). "Footprint", "pad", "track" and "via" are KiCad terms used only at the KiCad boundary; inside Bodgewire a track is a Trace.

## Scope

In:

- Export a netlist to KiCad's netlist format.
- Layout done by hand in KiCad (the user), for `netlists/counter.bwn` and `netlists/test_pattern.bwn`.
- Read `.kicad_pcb` back into a Board model (Components, pads, Traces, vias, outline).
- Copper check: the Copper connects exactly what the netlist says, computed from geometry.
- A step-by-step guide for a KiCad newcomer.

Out (later in P2 or beyond): drawing the Board in Bodgewire, Schematic, Sockets, connectors (power entry, TV output), silkscreen and part outlines, copper zones (filled areas), surface-mount parts, more than 2 Layers, writing `.kicad_pcb` from Bodgewire, Faults.

## Behaviour

### S1 Export

- `bw export-kicad <netlist.bwn> -o <file.net>` writes a KiCad netlist (S-expression format accepted by pcbnew's "Import Netlist").
- The netlist must load with no errors; otherwise the same diagnostics as `bw run` and no file.
- One component per Part line: reference, value = Part name (`74LS161`, `4k7`), footprint from the package table below.
- One net per netlist Net, listing every connected pin by pad number (= Pin index + 1), including implicit power Pins (GND, VCC). NoConnect Pins are listed in no net.

| Catalogue package | KiCad footprint |
|---|---|
| DIP-14 (gates, 74LS74, OSC) | `Package_DIP:DIP-14_W7.62mm` |
| DIP-16 (74LS161) | `Package_DIP:DIP-16_W7.62mm` |
| axial (R) | `Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal` |

All footprints come from KiCad's standard libraries: nothing to install or draw.

### S2 Layout (in KiCad)

Done by hand, following the guide (S5). Rules the Board must respect:

- 2 copper Layers (F.Cu, B.Cu). Through-hole pads, tracks (straight or arc), vias.
- Board outline: one closed shape on Edge.Cuts.
- No copper zones, no extra copper (text or graphics on copper Layers).
- Routing by hand, with KiCad's interactive router, or with the Freerouting auto-router: any mix.
- KiCad's design rule check (DRC) reports no errors.

Files, one directory per Board: `boards/<name>/<name>.kicad_pro` and `boards/<name>/<name>.kicad_pcb` are committed. The exported `.net`, KiCad's local settings (`*.kicad_prl`) and backups (`*-backups/`) are ignored by git.

### S3 Import

- Reads `.kicad_pcb` (KiCad 9 format or later) into a Board model; coordinates in integer nanometres (KiCad's own unit), so no rounding.
- Keeps: board outline; per Component: reference, footprint, position, rotation, side; per pad: number, absolute position, shape (circle, oval, rectangle, rounded rectangle), size, drill, Layers; Traces (straight or arc, Layer, width); vias (position, diameter, drill).
- Ignores: everything not copper or outline (silkscreen, fabrication, courtyard, text), and KiCad's own net names on items.
- Errors, with the file line: unreadable syntax, copper zones, copper Layers other than F.Cu/B.Cu, surface-mount pads, copper graphics, a file older than KiCad 9.

### S4 Copper check

- `bw check-board <netlist.bwn> <board.kicad_pcb>`: prints `ok` and exits 0, or prints every problem and exits 1.
- Components: every netlist Part has a Component with the same reference and the footprint of S1; no extra Components.
- Connectivity is computed from geometry only: KiCad's net labels are ignored, because Bodgewire will cut, break and bridge Copper itself (Faults, Repairs) and must derive Nets from Copper.
- Two copper items connect when their shapes overlap on a shared Layer. Through-hole pads and vias are on both Layers. A Trace's shape is its centre line widened by its width, with round ends.
- Each connected group of copper is an island. Pass when every netlist Net with 2+ pads is exactly one island holding exactly its pads.
- Problems reported:
  - **Open**: a Net split over several islands; lists the pads of each island (`VCC: {U1.16, U2.16} | {U3.16}`).
  - **Short**: an island holding pads of 2+ Nets; lists the Nets and a pad of each.
  - **Unconnected pad**: a NoConnect pad touched by copper.

### S5 Guide

`docs/p2a-board-layout/KICAD_GUIDE.md`, for someone who has never used KiCad: install, create the project in `boards/<name>/`, import the netlist, set the board outline, place, route (by hand and with Freerouting), run DRC, save, run `bw check-board`. Screenshots are not required; menu paths and keys are.

## Tests

- Unit: export of a small netlist (pins, implicit power, NoConnect, resistor); S-expression reader; import of small hand-written `.kicad_pcb` snippets; copper check on snippets: pass, open, short, via joining Layers, arc Trace, T-junction (a Trace ending on another Trace's middle), Trace touching a pad edge.
- Integration: both committed Boards pass `bw check-board`. A copy of the counter Board with one track removed (test fixture) fails with exactly one open.

## Done when

- Export, import and check implemented, tests pass in debug (sanitizers) and release.
- The user has laid out `boards/counter` and `boards/test_pattern` using the guide; both pass the check and are committed.
- Decision recorded in [ARCHITECTURE.md](../ARCHITECTURE.md), Board authoring: keep KiCad, or switch to our own generator. Keep KiCad unless the user finds the layout work for the test-pattern Board too painful to repeat on a BW-80-sized Board; record the pain points either way.

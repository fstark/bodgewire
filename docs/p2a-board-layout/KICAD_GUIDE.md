# KiCad Guide — Laying Out a Board (P2a)

For someone who has never used KiCad. Spec: [BOARD_LAYOUT.md](BOARD_LAYOUT.md). The example is `counter`; for the test pattern, replace `counter` with `test_pattern` everywhere.

What you do in KiCad: place the chips and draw the copper tracks. What you never do: change the circuit. If the circuit is wrong, fix the `.bwn` netlist and re-import (step 9).

## 1. Install KiCad (once)

KiCad 9 or later, from [kicad.org](https://www.kicad.org/download/macos/), or `brew install --cask kicad`. The standard footprint libraries come with it.

## 2. Export the netlist

```sh
cmake --build --preset debug
mkdir -p boards/counter
./build/debug/src/cli/bw export-kicad netlists/counter.bwn -o boards/counter/counter.net
```

## 3. Create the project

1. Open KiCad. **File → New Project…**
2. Go to `boards/counter`, name it `counter`, and **untick "Create a new folder for the project"**.
3. KiCad creates `counter.kicad_pro`, `counter.kicad_pcb` and `counter.kicad_sch`. The schematic file is not used (git ignores it).

## 4. Open the PCB editor and import the netlist

1. In the project window, double-click `counter.kicad_pcb`.
2. First time only: if asked about the footprint library table, choose **"Copy default global footprint library table"**.
3. **File → Import → Netlist…**, pick `counter.net`.
4. Set **"Link footprints using: Reference designators"**. Click **Update PCB**, then **Close**.
5. The chips follow the mouse as a bunch: click to drop them anywhere.

If the import says `footprint '...' not found`: **Preferences → Manage Footprint Libraries… → Global Libraries**; the `KiCad` row must point to `/Applications/KiCad/KiCad.app/Contents/SharedSupport/template/fp-lib-table` (a fresh install may point to a non-existent `~/Applications/...` copy).

Each chip is a footprint: its outline and its pads (holes with copper rings). The square pad is pin 1. Thin straight lines between pads (the "ratsnest") show what must be connected; they are not copper yet.

## 5. Draw the board outline

1. In the **Appearance** panel (right), click the layer **Edge.Cuts** (the layer box in the top toolbar then shows Edge.Cuts).
2. Click the **square outline icon** in the vertical toolbar right of the canvas (or menu bar **Place → Draw Rectangle**). Click one corner, then the opposite corner, a few mm around the chips. Press **Esc**.

You can resize it later: select it and drag a corner. The big pink frame with a title block is the paper sheet, not the board: ignore it.

## 6. Place the chips

- Drag a chip, or hover it and press **M** (move). **R** rotates by 90°.
- Group chips by function (e.g. counters together, oscillator near what it clocks) and turn them so that the ratsnest lines are short and cross as little as possible.
- Keep roughly 5 mm between chips: tracks need room.

## 7. Route the tracks

Two copper Layers: **F.Cu** (front, red) and **B.Cu** (back, blue). A common habit: mostly horizontal tracks on one, mostly vertical on the other.

By hand:

1. Pick the Layer: **Page Up** = F.Cu, **Page Down** = B.Cu.
2. Hover a pad, press **X** (route track), click to make bends, click on the destination pad to finish. KiCad pushes or hugs other tracks automatically.
3. While routing, **V** places a via (a hole joining the two Layers) and continues on the other Layer.
4. **Delete** removes a selected track segment. **Ctrl/Cmd-Z** undoes.

The ratsnest line disappears once a connection is made. GND and VCC have many pins: route them like any other net.

Auto-routing with Freerouting (optional, all at once):

1. Freerouting needs Java 25 or later: `brew install --cask temurin` (asks for your password), then quit and reopen KiCad.
2. **Plugin and Content Manager** (project window) → **Plugins** → install **Freerouting**, then restart the PCB editor.
3. **Tools → External Plugins → Freerouting**. Wait, then accept the result.
4. If the plugin doesn't work: **File → Export → Specctra DSN…**, open the `.dsn` in the Freerouting app ([freerouting.app](https://freerouting.app)), let it route, save the `.ses`, then **File → Import → Specctra Session…**.

Mixing is fine: route the tricky nets by hand, then let Freerouting do the rest.

## 8. Check and save

1. **Inspect → Design Rules Checker → Run DRC**. Fix until there are **0 errors** and **0 unconnected items**. Silkscreen warnings don't matter.
2. Save (**Cmd-S**).
3. Check it against the netlist:

```sh
./build/debug/src/cli/bw check-board netlists/counter.bwn boards/counter/counter.kicad_pcb
```

`ok` means the copper connects exactly what the netlist says. Otherwise it lists:

- `open NET: {pads} | {pads}`: the net is in pieces; join them.
- `short: NET1 (pad), NET2 (pad)`: copper joins two nets that must be separate.
- `U3.7: copper on an unconnected pad`: a track touches a pin that must stay free.

## 9. If the netlist changes

Export again (step 2), then **File → Import → Netlist…** with the same options. Placement and existing tracks are kept; new connections appear as ratsnest lines.

## Rules

- Only F.Cu and B.Cu (the default for a new project).
- No filled zones (**Add Filled Zone**), no text or drawings on F.Cu/B.Cu.
- Don't add footprints by hand: only those from the netlist.

## Commit

```sh
git add boards/counter/counter.kicad_pro boards/counter/counter.kicad_pcb
```

Everything else in `boards/counter/` is ignored by git.

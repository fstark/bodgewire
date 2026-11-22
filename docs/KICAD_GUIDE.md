# KiCad Guide — Routing a Board

For someone who has never used KiCad. The example is the Counter: `machines/010-counter/counter.bwn`, its Board in `machines/010-counter/board/`. For another Machine, replace the directory and `counter` with its own.

What you do in KiCad: draw the copper tracks, and move parts if you want. What you never do: change the circuit. If the circuit is wrong, the netlist (`.bwn`) is fixed and the Board brought up to date (step 6).

## 1. Install KiCad (once)

KiCad 10, from [kicad.org](https://www.kicad.org/download/macos/), or `brew install --cask kicad` (the Boards are written in its format). The standard footprint libraries come with it.

## 2. Open the Board

The agent writes the Board with every part placed, the outline drawn and no tracks: `bw place` ([ADD_MACHINE.md](recipes/ADD_MACHINE.md) §3) gives `counter.kicad_pcb` and, if there is none, `counter.kicad_pro`.

1. Open KiCad, **File → Open Project…**, pick `counter.kicad_pro`.
2. Double-click `counter.kicad_pcb` to open the PCB editor.
3. First time only: if asked about the footprint library table, choose **"Copy default global footprint library table"**.

Each chip is a footprint: its outline and its pads (holes with copper rings). The square pad is pin 1. Thin straight lines between pads (the "ratsnest") show what must be connected; they are not copper yet. The big pink frame with a title block is the paper sheet, not the Board: ignore it.

Moving parts is allowed (drag, or hover and press **M**; **R** rotates by 90°): group them by function and keep roughly 5 mm between chips, tracks need room. Digits read upright only turned 90° (`bw place` places them at 0°).

## 3. Route the tracks

Two copper Layers: **F.Cu** (front, red) and **B.Cu** (back, blue). A common habit: mostly horizontal tracks on one, mostly vertical on the other.

Auto-routing with Freerouting (the usual way, all at once):

1. Freerouting needs Java 25 or later: `brew install --cask temurin` (asks for your password), then quit and reopen KiCad.
2. **Plugin and Content Manager** (project window) → **Plugins** → install **Freerouting**, then restart the PCB editor.
3. **Tools → External Plugins → Freerouting**. Wait, then accept the result.
4. If the plugin doesn't work: **File → Export → Specctra DSN…**, open the `.dsn` in the Freerouting app ([freerouting.app](https://freerouting.app)), let it route, save the `.ses`, then **File → Import → Specctra Session…**.

By hand:

1. Pick the Layer: **Page Up** = F.Cu, **Page Down** = B.Cu.
2. Hover a pad, press **X** (route track), click to make bends, click on the destination pad to finish. KiCad pushes or hugs other tracks automatically.
3. While routing, **V** places a via (a hole joining the two Layers) and continues on the other Layer.
4. **Delete** removes a selected track segment. **Ctrl/Cmd-Z** undoes.

The ratsnest line disappears once a connection is made. GND and VCC have many pins: route them like any other net. Mixing is fine: route the tricky nets by hand, then let Freerouting do the rest. A Puzzle that cracks a Trace may need a route order (a pad at an end of its Trace): the agent says so when it hands the Board over.

## 4. Check and save

1. **Inspect → Design Rules Checker → Run DRC**. Fix until there are **0 errors** and **0 unconnected items**. Silkscreen warnings don't matter. One connection Freerouting gave up on can be left to the agent (`tools/route_missing.py`).
2. Save (**Cmd-S**), then close the Board: KiCad does not reload a file the agent changes, and its next save (autosave included) would undo the change.
3. The agent checks it against the netlist:

```sh
./build/release/src/cli/bw check-board machines/010-counter/counter.bwn machines/010-counter/board/counter.kicad_pcb
```

`ok` means the copper connects exactly what the netlist says. Otherwise it lists:

- `open NET: {pads} | {pads}`: the net is in pieces; join them.
- `short: NET1 (pad), NET2 (pad)`: copper joins two nets that must be separate.
- `U3.7: copper on an unconnected pad`: a track touches a pin that must stay free.

## 5. Rules

- Only F.Cu and B.Cu (the default for a new project).
- No filled zones (**Add Filled Zone**), no text or drawings on F.Cu/B.Cu.
- Don't add footprints by hand: only those from the netlist.

## 6. If the netlist changes

- **Nets changed, no part added**: the agent strips the tracks and puts each pad on its new Net (`bw place --force`, or `tools/renet_board.py` for a Board laid out by hand); you route it again whole (step 3).
- **Parts added**: the agent exports the netlist (`bw export-kicad … -o counter.net`); **File → Import → Netlist…**, the replace and delete options off, **Update PCB**. Placement and existing tracks are kept; the new parts follow the mouse (click to drop them), and their connections appear as ratsnest lines.

If an import says `footprint '...' not found`: **Preferences → Manage Footprint Libraries… → Global Libraries**; the `KiCad` row must point to `/Applications/KiCad/KiCad.app/Contents/SharedSupport/template/fp-lib-table` (a fresh install may point to a non-existent `~/Applications/...` copy).

## Commit

Only `<m>.place`, `<m>.kicad_pro` and `<m>.kicad_pcb`; KiCad's other files in `board/` (`.kicad_prl`, `.net`, `.dsn`, backups) are ignored by git.

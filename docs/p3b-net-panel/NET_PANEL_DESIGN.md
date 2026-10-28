# Net Panel — Detailed Design (P3b)

Implements the specification in [NET_PANEL.md](NET_PANEL.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/netlist/
  net_pins.hpp / .cpp     NEW: Drivers and Loads of a design Net (headless)
  builder.hpp / .cpp      IsRail() made public
src/ui/
  net_panel.hpp / .cpp    NEW: the `Net` section (ImGui only)
  board_view.hpp / .cpp   selected Net, route() selects
  side_panel.hpp / .cpp   draws the `Net` section
  main.cpp                Board clicks and --probe select the Net
tests/netlist/
  net_pins_test.cpp       NEW
tests/machines/
  net_panel_test.cpp      NEW: no-leak checks on Puzzles
```

The listing only needs the design `LoadedCircuit` and the Parts catalogue, both already in `bw_netlist`, so it lives there and is tested without a Board. Mapping entries to pads is `BoardScene::componentIndex`/`padIndex`, already in `bw_view`. The UI only draws.

## 2. Public Interface

```cpp
// src/netlist/net_pins.hpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "netlist/builder.hpp"

namespace bw {

enum class PinRole : std::uint8_t { Output, PullUp, PullDown, Supply, Load };

struct NetPin {
    std::string reference;  // "U6"
    PinIndex pin;           // package pin number - 1
    std::string name;       // datasheet name "1Y"; "" for resistors
    PinRole role;
    std::string value;      // resistors: "4k7", else ""
    bool operator==(const NetPin&) const = default;
};

struct NetPins {
    std::string net;
    bool rail = false;            // GND or VCC
    std::vector<NetPin> drivers;  // Output, PullUp, PullDown, Supply
    std::vector<NetPin> loads;    // everything else
};

// Drivers and Loads of a design Net, each sorted by ReferenceLess then pin. Unknown net → empty.
NetPins DescribeNet(const LoadedCircuit& design, std::string_view net);

// Natural order: "U2" < "U10" < "X1"; letters first, then the number, then the whole text.
bool ReferenceLess(std::string_view a, std::string_view b);

// "U6 pin 6 1Y", "R1 pin 2 · 4k7 pull-up", "J1 pin 1 VCC · supply" (NET_PANEL.md S3).
std::string FormatNetPin(const NetPin& pin);

}  // namespace bw
```

```cpp
// src/netlist/builder.hpp, next to ConnectorNet
bool IsRail(std::string_view net);  // "GND" or "VCC"
```

## 3. Integration Points

### 3.1 `IsRail` public (`src/netlist/builder.cpp`)

Move `IsRail` (line 33, anonymous namespace) out to namespace `bw`; declare it in `builder.hpp`. `tieResistors()` keeps calling it. No behaviour change.

### 3.2 Selected Net in `BoardView` (`src/ui/board_view.hpp/.cpp`)

```cpp
// public
std::optional<NetId> net() const { return net_; }
void selectNet(std::optional<NetId> net) { net_ = net; }
// private
std::optional<NetId> net_;  // the Net panel's Net; drawn when the hover highlights none
```

In `BoardView::draw`, replace `const std::optional<NetId> lit = highlight_;` with:

```cpp
const std::optional<NetId> lit = highlight_ ? highlight_ : net_;  // S2
```

`BoardView::route` (S1): a clicked target also selects.

```cpp
void BoardView::route(const TargetHits& hits) {
    if (hits.hovered) {
        hoverFromOutside(hits.hovered);
    }
    if (hits.clicked) {
        focus(*hits.clicked);
        select(hits.clicked->component);
        if (hits.clicked->pad && scene_) {
            if (const auto net = scene_->padNet(hits.clicked->component, *hits.clicked->pad)) {
                net_ = net;
            }
        }
    }
}
```

This one change covers pinout clicks (`ChipPanel`), manual links (`ManualView`, `ManualWindows`) and Net panel entries, since all go through `route`. `Zoom to` and `` `U6` `` links select the Component only. Cost: none per frame.

### 3.3 Board clicks (`src/ui/main.cpp`, `RunWindow`)

In the `takeClick()` block, after `boardView.select(*click->component)`:

```cpp
if (pick && boardView.scene()) {
    if (const auto net = boardView.scene()->netOf(pick->hit)) {
        boardView.selectNet(*net);  // any tool, S1
    }
}
```

(`pick` is declared just below today; move `const auto& pick = click->pick;` up.) `netOf` gives nullopt for a Component hit, so a body click keeps the Net.

### 3.4 `--probe REF.PIN` (`src/ui/main.cpp`, `RunWindow`)

Where `--probe` already selects the Component, also select the pad's Net:

```cpp
const std::string pin = options.probe->substr(options.probe->find('.') + 1);
if (const auto pad = scene->padIndex(*component, pin)) {
    boardView.selectNet(scene->padNet(*component, *pad));
}
```

### 3.5 Side panel (`src/ui/side_panel.hpp/.cpp`)

New member `NetPanel net_;`. New section between `Probe` and `Chip`:

```cpp
if (scene && ImGui::CollapsingHeader("Net", open)) {
    if (const auto net = board.net()) {
        const NetPanelHits hits = net_.draw(*scene, *net, design, loaded.manuals.manuals);
        board.route(hits.targets);
        if (hits.clear) {
            board.selectNet(std::nullopt);
        }
    } else {
        ImGui::TextDisabled("click a pin or trace");
    }
}
```

Update the class comment to list the `Net` section.

## 4. Internal State

```cpp
// src/ui/net_panel.hpp
struct NetPanelHits {
    TargetHits targets;  // entry hovered / clicked
    bool clear = false;  // `Clear` pressed
};

// The Net section: header, Drivers, Loads, each entry a link to its pad.
class NetPanel {
public:
    NetPanelHits draw(const BoardScene& scene, NetId net, const LoadedCircuit& design,
                      const Manuals& manuals);

private:
    std::optional<NetId> cached_;  // design is fixed for the run
    NetPins pins_;
};
```

The listing is recomputed only when the selected Net changes (`cached_ != net`), using `DescribeNet(design, scene.netName(net))`.

## 5. Key Algorithms

### 5.1 `DescribeNet`

```
result.net = net; result.rail = IsRail(net)
for c in design.connections where c.net == net:
    part = design.parts entry with reference == c.reference   (LoadedPart: info, value)
    if part.info->kind == Resistor:
        other = connection of (c.reference, 1 - c.pin)
        if result.rail:            role = Load
        else if other.net == VCC:  role = PullUp
        else:                      role = PullDown        (builder only accepts pull-ups/-downs)
        add {ref, pin, "", role, part.value}
    else:
        spec = design.components[c.reference]->pins()[c.pin]
        role = IsDriver(spec.kind)                                  ? Output
             : spec.kind == Power && part.info->name == "POWER"   ? Supply
             :                                                       Load
        add {ref, pin, spec.name, role, ""}
sort drivers and loads by (ReferenceLess(reference), pin)
```

`connections` holds every netlist pin plus implicit power pins (builder `addImplicitPower`), each once, so every pin of the Net appears exactly once. `connections` scan is linear (~400 entries on the test pattern), run once per selection.

### 5.2 `ReferenceLess`

```
split each into letters prefix + digits suffix ("U10" → "U", 10; no digits → whole text, none)
compare prefixes; then numbers (none < any); then the whole strings
```

### 5.3 `FormatNetPin`

```
base = "<ref> pin <pin+1>" + (name.empty() ? "" : " <name>")
PullUp   → base + " · <value> pull-up"
PullDown → base + " · <value> pull-down"
Supply   → base + " · supply"
else     → base
```

### 5.4 `NetPanel::draw` (S3)

```
header: TextWrapped("<net> · <drivers+loads> pins"), SameLine, SmallButton("Clear") → clear
Text("Driven by"); entries(drivers) or TextDisabled("nothing")
loads label: rail ? TreeNode("Feeds (N pins)") closed : TreeNodeEx("Feeds", DefaultOpen)
    entries(loads) or TextDisabled("nothing")

entry(pin):
    component = scene.componentIndex(pin.reference)
    pad = component ? scene.padIndex(*component, to_string(pin.pin + 1)) : nullopt
    if !pad: TextUnformatted(label); continue
    PushID(label); Selectable(Displayable(label)) → targets.clicked = {component, pad}
    hovered → targets.hovered = {component, pad};
              tooltip: partName + (service role ? " · " + role.name : "")
```

`·` (U+00B7) is in the default ImGui font (the Chip panel header already uses it); text still goes through `Displayable` for dashes in roles.

## 6. Reused Infrastructure

- `LoadedCircuit::connections`, `parts`, `components` (`src/netlist/builder.hpp`) — the design Netlist; no new connectivity data.
- `PinSpec::kind`, `IsDriver` (`src/sim/logic.hpp`), `PartInfo::kind` (`src/parts/catalogue.hpp`) — roles.
- `IsRail` (`builder.cpp`) — made public, not duplicated.
- `BoardScene::padNet`, `netOf`, `netName`, `componentIndex`, `padIndex`, `partName` — selection and entry → pad mapping. Net ids come from the design (`BoardScene` labels from fault-free Copper), so no Fault reaches the panel.
- `TargetHits`, `BoardView::route`, `focus`, `hoverFromOutside` — links, framing, flash, pad hover ring, exactly as the Chip panel and manuals.
- `ComponentRole` (`src/manual/manual.hpp`), `Displayable` (`src/ui/manual_view.hpp`) — tooltip.
- `Machine::designCircuit()`, `DesignOf(session)` — the design for Puzzles.

## 7. Build Integration

```cmake
# src/netlist/CMakeLists.txt
add_library(bw_netlist STATIC
  builder.cpp
  diagnostic.cpp
  net_pins.cpp
  parser.cpp
  units.cpp
)

# src/ui/CMakeLists.txt: add net_panel.cpp to bw_ui sources

# tests/CMakeLists.txt: add machines/net_panel_test.cpp, netlist/net_pins_test.cpp
```

No new library dependencies.

## 8. Dependency Diagram

```
bw_ui (net_panel, side_panel, board_view, main)
  ├─> bw_view (BoardScene) ─> bw_board ─> bw_netlist
  ├─> bw_manual (ComponentRole) ─> bw_netlist
  └─> bw_netlist (net_pins) ─> bw_parts ─> bw_sim
```

## 9. Testing

Catch2, tag `[netlist]` / `[machines]`.

`tests/netlist/net_pins_test.cpp` (netlists via `BW_NETLIST_DIR`):

- `net pins: counter CLK` — drivers `{X1, 7, "OUT", Output}`; loads `U1 pin 2 CLK`, `U2 pin 2 CLK`.
- `net pins: pull-up` — counter `RESET_N`: driver `R1 pin 2 · 4k7 pull-up`; loads `U1 pin 1 CLR_N`, `U2 pin 1 CLR_N`.
- `net pins: rail` — counter `VCC`: `rail`, sole driver `J1 pin 1 VCC · supply`; loads contain `R1 pin 1` (role Load) and `U1 pin 16 VCC`.
- `net pins: test pattern LINE_END_N` — driver `U6 pin 6 1Y`; loads `U2 pin 9 LOAD_N`, `U9 pin 5 3A`, in that order.
- `net pins: natural order` — test pattern `CLK` loads: `U5` entries before `U14`; `ReferenceLess("U2","U10")`, `!ReferenceLess("U10","U2")`, `ReferenceLess("R1","U1")`.
- `net pins: every pin once` — test pattern: summed over all `design.nets`, drivers + loads == `connections.size()`; no `(reference, pin)` twice.
- `net pins: unknown net` — empty lists, `rail` false.
- `net pins: FormatNetPin` — the four forms of 5.3.

`tests/machines/net_panel_test.cpp` (Puzzles via `BW_PUZZLE_DIR`, `LoadPuzzleFile`):

- `net panel: crack does not split H0` — `broken_trace` `designCircuit()`: `H0` driver `U1 pin 14 QA`, loads `U13 pin 6 2B`; and a `BoardScene` of the Puzzle gives the same `padNet` for both pads.
- `net panel: stuck output is still an output` — `stuck_gate`: `GRID` driver `U12 pin 11 4Y` (Output), load `U11 pin 10 3B`.

UI (S4, by the agent): `bw_ui puzzles/broken_trace.bwp --probe U6.6 --screenshot /tmp/net.png --after 1s` and `--probe U1.16`; read the PNGs.

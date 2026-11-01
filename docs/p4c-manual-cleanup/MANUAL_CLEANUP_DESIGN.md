# P4c Manual Cleanup — Detailed Design

Implements the specification in [MANUAL_CLEANUP.md](MANUAL_CLEANUP.md).

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

Most of this step is deletion. The only new code is the Driver pads of a Net in `BoardScene` and the marks drawn on them.

---

## 1. Directory Layout

```
manuals/{counter,chaser,test_pattern}/user.md     rewritten: title + What it does
manuals/{counter,chaser,test_pattern}/service.md  cut: title + Components
src/manual/manual.cpp                             CheckUserManual, CheckServiceManual shapes
src/view/board_scene.{hpp,cpp}                    + ScenePad, drivers(NetId)
src/ui/board_view.{hpp,cpp}                       − Net selection; + Driver marks
src/ui/manual_view.{hpp,cpp}                      − Service Manual window, − easy-mode label
src/ui/side_panel.{hpp,cpp}                       − Net section, − Puzzle "When it works"
src/ui/toolbar.{hpp,cpp}                          − Service Manual (H)
src/ui/main.cpp                                   − --manual service, − selectNet; Tooltip
src/ui/net_panel.{hpp,cpp}                        deleted
src/ui/CMakeLists.txt                             − net_panel.cpp
tests/manual/manual_test.cpp                      new shapes
tests/machines/net_panel_test.cpp → net_drivers_test.cpp   + BoardScene::drivers
tests/CMakeLists.txt                              renamed test file
```

`DescribeNet`, `FormatNetPin` and `ReferenceLess` stay in `bw_netlist` with their tests ([net_pins_test.cpp](../../tests/netlist/net_pins_test.cpp)).

## 2. Manuals

### 2.1 Checks (spec: User Manual, Service Manual)

In [manual.cpp](../../src/manual/manual.cpp), `CheckShape` already rejects any heading list other than the one given. Only the section lists change:

```cpp
// CheckUserManual
constexpr std::string_view kSections[] = {"What it does"};
// CheckServiceManual
constexpr std::string_view kSections[] = {"Components"};  // no optional sections
CheckShape(doc, problems, kSections);
```

`CheckReferences`, the Components bullet checks (every Component exactly once, each bullet starts with a `` `REF` ``), `ComponentRole`, `LinkAt`, `PinBullet` and `LoadManuals` are unchanged. `Manuals::service` stays: it now holds only the roles.

### 2.2 Content

`user.md`, one paragraph each:

```markdown
# Counter

## What it does

Connect +5 V to `J1`. After power-up, the red LED `D1` blinks about once a second.
```

```markdown
# LED Chaser

## What it does

Connect +5 V to `J1`. After power-up, one light runs along the eight LEDs `D1` to
`D8`, about 15 steps a second, and starts again at `D1`. For up to half a second
after power-up a few LEDs may light together.
```

```markdown
# Test Pattern Generator

## What it does

Connect +5 V to `J1` and a TTL monitor to `J2`. After power-up, the monitor shows a
steady white grid on black, 6 lines across and 18 down, in the top-left part of the
screen. It locks within half a second and never rolls.
```

`service.md`: the existing title and `## Components` list stay word for word. `## How it works` and `## Troubleshooting` are deleted.

## 3. Driver Pads in BoardScene

### 3.1 Interface

[board_scene.hpp](../../src/view/board_scene.hpp):

```cpp
struct ScenePad {
    std::size_t component;  // index in board().components
    std::size_t pad;        // pad within the Component
    bool operator==(const ScenePad&) const = default;
};

class BoardScene {
public:
    // Pads of the design Net's Drivers (outputs, pull resistors, supply), in DescribeNet
    // order; empty for an unknown Net.
    std::span<const ScenePad> drivers(NetId net) const;
private:
    std::vector<std::vector<ScenePad>> driverPads_;  // [NetId]
};
```

### 3.2 Construction

At the end of the `BoardScene` constructor, after `info_` and `padNets_` exist:

```
driverPads_.resize(netNames_.size())
for (name, id) in design.nets:
    for pin in DescribeNet(design, name).drivers:
        k = componentIndex(pin.reference)
        p = k ? padIndex(*k, to_string(pin.pin + 1)) : nullopt
        if p: driverPads_[id].push_back({*k, *p})
```

`DescribeNet` scans every Connection per Net: about 100 Nets × 400 Connections on the test pattern. That runs once per scene, well under a millisecond. Built from the design netlist only, so a crack, a bridge or a stuck output never changes the marks (ARCHITECTURE: the UI must not leak faults). `bw_view` already reaches `bw_netlist` through `bw_board` (PUBLIC), so no CMake change is needed.

## 4. UI

### 4.1 Driver marks (spec: Board hover)

In `BoardView::draw` ([board_view.cpp](../../src/ui/board_view.cpp#L371)), `lit` becomes `highlight_` alone. After the hovered-pad ring (line 439):

```cpp
constexpr ImU32 kDriverMark = IM_COL32(255, 255, 255, 255);
constexpr float kDriverMarkGap = 5.0f;  // px outside the pad, outside the hover ring

if (highlight_) {
    for (const ScenePad& driver : scene_->drivers(*highlight_)) {
        const Pad& pad = board.components[driver.component].pads[driver.pad];
        const ImVec2 half = PadHalf(map, pad);
        list.AddCircle(map(pad.position), std::max(half.x, half.y) + kDriverMarkGap,
                       kDriverMark, 0, 2.0f);
    }
}
```

`highlight_` is set by Board hover (`input`) and by `hoverFromOutside` (Chip panel pin rows, manual links to a pin), so both show the marks.

### 4.2 Net selection removed (spec: Removed)

- [board_view.hpp](../../src/ui/board_view.hpp#L94): delete `net()`, `selectNet()`, `net_`.
- `BoardView::route`: delete the `net_ = net` block; `focus` and `select` stay.
- [main.cpp](../../src/ui/main.cpp#L292): `--probe` keeps `boardView.select(*component)`, drops `selectNet`.
- [main.cpp](../../src/ui/main.cpp#L363): the click handler drops the `selectNet` block.

### 4.3 Net panel removed

- Delete `src/ui/net_panel.{hpp,cpp}`; drop `net_panel.cpp` from `bw_ui` in [src/ui/CMakeLists.txt](../../src/ui/CMakeLists.txt).
- [side_panel.hpp](../../src/ui/side_panel.hpp): drop the include, `NetPanel net_` and "Net" from the class comment.
- [side_panel.cpp](../../src/ui/side_panel.cpp#L88): delete the `Net` section.

### 4.4 Puzzle section (spec: User Manual)

[side_panel.cpp](../../src/ui/side_panel.cpp#L64): delete the "When it works" tree node; drop `ManualView puzzle_` from `SidePanel`. The section shows the title and brief.

### 4.5 Service Manual window removed

- [manual_view.hpp](../../src/ui/manual_view.hpp): `ManualWindows` keeps one window.

  ```cpp
  // The floating User Manual window.
  class ManualWindows {
  public:
      void draw(bool& open, const Manuals& manuals, BoardView& board,
                const LoadedCircuit& design);
  private:
      ManualView user_;
  };
  ```

- [manual_view.cpp](../../src/ui/manual_view.cpp#L18): delete `kTroubleshooting` and the "(easy mode)" label; every heading is `ImGuiTreeNodeFlags_DefaultOpen`.
- [toolbar.cpp](../../src/ui/toolbar.cpp#L218): delete the "Service Manual (H)" button and the `H` key; `serviceManualOpen` goes from [toolbar.hpp](../../src/ui/toolbar.hpp).
- [main.cpp](../../src/ui/main.cpp): `Options::serviceManual` goes; `--manual` accepts only `user` (anything else is a usage error); usage string `[--manual user]`; `manualWindows.draw(toolbar.userManualOpen, ...)`.

### 4.6 Component hover text (spec: Board hover)

`Tooltip` in [main.cpp](../../src/ui/main.cpp#L237) replaces the role with the catalogue description, the same text as the datasheet title (`PartInfo::description`):

```cpp
if (hit.kind == HitKind::Component) {
    if (const PartInfo* part = FindPart(scene.partName(hit.component))) {
        text += " \u00b7 " + std::string(part->description);
    }
}
```

`"U6 74LS20 · dual 4-input NAND"`, `"R1 4k7 · resistor"`. The Chip panel role line ([chip_panel.cpp](../../src/ui/chip_panel.cpp#L80)) is unchanged and is the only place roles appear.

## 5. Docs

| File | Change |
|---|---|
| GLOSSARY | User Manual: "what the Machine does when it works, seen from outside, and how to power it". Service Manual: "the role of each Component on this Board; shown in the Chip panel". No hard mode. |
| GAME_DESIGN | Tools: Board view "with the chip pinouts; hovering a net marks what drives it"; manuals bullet "User Manual (what it does), Component roles, simplified datasheets". Presentation: "the hovered net highlighted on all layers"; Schematic "the board, pinouts and datasheets do its job". |
| ARCHITECTURE | Design netlist row: "Net names, pinouts, Driver marks and the manual checks". No-leak rule: "pinouts and Driver marks". No-schematic note: "Net highlighting with Driver marks, the Chip pinout and the datasheets". P4c row: "done" at the end. |
| COUNTER_DESIGN, LED_CHASER_DESIGN | `Manuals`: "User: What it does (one paragraph). Service: Components (every reference)." |

p3a and p3b docs stay as they are.

## 6. Reused Infrastructure

- `CheckShape`, `CheckReferences`, the Components checks, `ComponentRole`, `LinkAt`: unchanged.
- `DescribeNet` (P3b): the Driver list, including resistor pull-ups and the POWER connector's supply pins.
- `BoardScene::componentIndex`, `padIndex`: map Driver pins to pads.
- `hoverFromOutside` / `highlight_`: the existing hover path drives the marks.
- `PartInfo::description`: the hover text; no datasheet parsing.

## 7. Dependency Diagram

```
bw_ui ──► bw_view ──► bw_board ──► bw_netlist (DescribeNet)
  └─────► bw_manual ─► bw_netlist
```

No new edges.

## 8. Testing

- [manual_test.cpp](../../tests/manual/manual_test.cpp) `manual: checks`: `kGoodService` loses "How it works" and "Troubleshooting" (its line numbers in expected messages shift). Add: a service manual with `## How it works` is rejected ("sections must be: Components"); a user manual with `## Connections` is rejected ("sections must be: What it does"); `# Counter\n## What it does\nConnect +5 V to `J1`.\n` passes.
- [manuals_test.cpp](../../tests/machines/manuals_test.cpp): unchanged; it checks the rewritten files of all three Machines and a role for every Component.
- `tests/machines/net_drivers_test.cpp` (renamed from net_panel_test.cpp, tag `[machines]`): the two DescribeNet cases stay. Add:
  - broken_trace: `scene.drivers(H0)` is exactly U1 pad 14, although H0 is cracked.
  - stuck_gate: `scene.drivers(GRID)` is exactly U12 pad 11.
  - counter netlist Machine: `drivers(RESET_N)` is R1's pull-up pad; `drivers(GND)` is J1's GND pad.
- UI (no unit tests): release screenshot of a Puzzle with `--manual user` (title and brief only in Puzzle, User Manual window open, no Net section); user check of hover text and Driver marks.

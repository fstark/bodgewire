# Net Panel — Implementation Plan (P3b)

Design: [NET_PANEL_DESIGN.md](NET_PANEL_DESIGN.md)
Spec: [NET_PANEL.md](NET_PANEL.md)

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Headless Drivers/Loads listing (`net_pins`) with tests, incl. no-leak Puzzle tests | |
| 2 | Selected Net in `BoardView`: Board clicks, `route`, `--probe`, highlight | |
| 3 | `Net` section in the right column (`NetPanel`) + screenshots | |
| 4 | Release check, user check, docs | |

Build gate: `cmake --preset debug && cmake --build --preset debug`
Test gate: `ctest --preset debug -j 10`

All C++ clang-format clean (never CMakeLists). Visual checks: `build/debug/src/ui/bw_ui FILE --screenshot /tmp/x.png --after 1s`, then view the PNG.

---

## Phase 1 — Headless listing

Everything the panel shows, without UI, so it is tested first (design §2, §5.1–5.3, §9).

### 1.1 — `IsRail` public

`src/netlist/builder.cpp`: move `bool IsRail(std::string_view net)` from the anonymous namespace (line 33) into namespace `bw` after it. `src/netlist/builder.hpp`: declare it after `ConnectorNet`:

```cpp
bool IsRail(std::string_view net);  // "GND" or "VCC"
```

### 1.2 — `net_pins`

Create `src/netlist/net_pins.hpp` exactly as design §2 (`PinRole`, `NetPin`, `NetPins`, `DescribeNet`, `ReferenceLess`, `FormatNetPin`). Includes: `<cstdint>`, `<string>`, `<string_view>`, `<vector>`, `"netlist/builder.hpp"`.

Create `src/netlist/net_pins.cpp`:

- `ReferenceLess` (§5.2): split at the first digit of a trailing digit run; prefixes compared as strings; numbers parsed with `std::from_chars` (no digits sorts before any number); then `a < b`.
- `DescribeNet` (§5.1): scan `design.connections` for `c.net == net`; find the `LoadedPart` by reference in `design.parts`; resistor (`info->kind == PartKind::Resistor`): find the other connection `(c.reference, 1 - c.pin)`; role Load on a rail, else PullUp if the other Net is `VCC`, else PullDown; `value` = `LoadedPart::value`. Otherwise `spec = design.components.at(c.reference)->pins()[c.pin]`: `IsDriver(spec.kind)` → Output; `spec.kind == PinKind::Power && info->name == "POWER"` → Supply (the video `GND` is a Load); else Load. Sort both vectors by `ReferenceLess(reference)` then `pin` (`std::ranges::sort` with a lambda).
- `FormatNetPin` (§5.3) with `std::format`; the separator is `" \u00b7 "`.

`src/netlist/CMakeLists.txt`: add `net_pins.cpp` to `bw_netlist`.

### 1.3 — Unit tests

Create `tests/netlist/net_pins_test.cpp` (tag `[netlist]`), loading with `LoadNetlistFile(std::string(BW_NETLIST_DIR) + "/<name>.bwn")`. Cases from design §9: `net pins: counter CLK`, `net pins: pull-up`, `net pins: rail`, `net pins: test pattern LINE_END_N`, `net pins: natural order`, `net pins: every pin once`, `net pins: unknown net`, `net pins: FormatNetPin`. Compare entries with `FormatNetPin` strings where that reads better (e.g. `{"U2 pin 9 LOAD_N", "U9 pin 5 3A"}`), and check `role` where the case is about roles.

### 1.4 — No-leak tests

Create `tests/machines/net_panel_test.cpp` (tag `[machines]`), using `LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / "<name>.bwp")`:

- `net panel: crack does not split H0` — `broken_trace`: `DescribeNet(machine->designCircuit(), "H0")` drivers `{"U1 pin 14 QA"}`, loads `{"U13 pin 6 2B"}`; `BoardScene scene(machine->board(), machine->pristineBoard(), machine->designCircuit())`: `padNet` of `U1` pad `14` equals that of `U13` pad `6` (via `componentIndex`/`padIndex`).
- `net panel: stuck output is still an output` — `stuck_gate`: `GRID` drivers `{"U12 pin 11 4Y"}` with role Output, loads `{"U11 pin 10 3B"}`.

`tests/CMakeLists.txt`: add `machines/net_panel_test.cpp` and `netlist/net_pins_test.cpp` (alphabetical in their groups).

### Fence

- [ ] `src/netlist/net_pins.{hpp,cpp}` exist; `IsRail` declared in `builder.hpp`
- [ ] `ctest --preset debug -j 10` passes, incl. the new `[netlist]` and `[machines]` cases
- [ ] clang-format clean
- [ ] Commit: `"net panel: phase 1 — Drivers and Loads of a design Net"`

---

## Phase 2 — Selected Net

The Board remembers one Net and shows it; every existing pin/Copper click selects it (spec S1, S2; design §3.2–3.4). No panel yet: the highlight is the visible result.

### 2.1 — `BoardView` state and highlight

`src/ui/board_view.hpp`: add public `std::optional<NetId> net() const { return net_; }` and `void selectNet(std::optional<NetId> net) { net_ = net; }`; private `std::optional<NetId> net_;  // the Net panel's Net; drawn when the hover highlights none`.

`src/ui/board_view.cpp`, `BoardView::draw`: `const std::optional<NetId> lit = highlight_ ? highlight_ : net_;`.

### 2.2 — `route` selects

`BoardView::route`: after `focus(*hits.clicked)`, `select(hits.clicked->component)` and, when the target has a pad and `scene_->padNet(...)` has a value, `net_ = *that`. Code in design §3.2.

### 2.3 — Board clicks and `--probe`

`src/ui/main.cpp`, `RunWindow`:

- In the `takeClick()` block, move `const auto& pick = click->pick;` above the `select` and add, after it, `if (pick && boardView.scene())` → `netOf(pick->hit)` → `boardView.selectNet(*net)` (design §3.3).
- Where `--probe` selects the Component, also select the pad's Net via `padIndex` on the text after the `.` (design §3.4).

### 2.4 — Visual check

`bw_ui puzzles/broken_trace.bwp --probe U6.6 --screenshot /tmp/p3b-2.png --after 1s`: `LINE_END_N` (U6 pin 6, U2 pin 9, U9 pin 5 and the Traces between) drawn in the highlight colour with the cursor off the Board.

### Fence

- [ ] Build and tests pass
- [ ] Screenshot shows the selected Net highlighted
- [ ] clang-format clean
- [ ] Commit: `"net panel: phase 2 — selected Net on the Board"`

---

## Phase 3 — `Net` section

The panel itself (spec S3; design §3.5, §4, §5.4).

### 3.1 — `NetPanel`

Create `src/ui/net_panel.hpp` with `NetPanelHits` and `NetPanel` exactly as design §4 (includes: `<optional>`, `"manual/manual.hpp"`, `"netlist/net_pins.hpp"`, `"ui/board_view.hpp"`).

Create `src/ui/net_panel.cpp` implementing §5.4:

- recompute `pins_ = DescribeNet(design, scene.netName(net))` when `cached_ != net`;
- header `std::format("{} \u00b7 {} pins", ...)`, `ImGui::SameLine()`, `SmallButton("Clear")`;
- `Driven by` then entries or `TextDisabled("nothing")`;
- `Feeds`: rail → `ImGui::TreeNode` labelled `std::format("Feeds ({} pins)", n)` (closed by default); else `TreeNodeEx("Feeds", ImGuiTreeNodeFlags_DefaultOpen)`; `TreePop` when open;
- entry helper in the anonymous namespace: `void Entry(const NetPin& pin, const BoardScene& scene, const Manuals& manuals, TargetHits& hits)`; pad via `componentIndex`/`padIndex(…, std::to_string(pin.pin + 1))`, plain `TextUnformatted` if none; `PushID` on `FormatNetPin` text; `Selectable`; hover tooltip `partName` + `" \u00b7 " + role->name` when `ComponentRole` finds one; text through `Displayable`.

`src/ui/CMakeLists.txt`: add `net_panel.cpp` to `bw_ui`.

### 3.2 — Side panel

`src/ui/side_panel.hpp`: include `"ui/net_panel.hpp"`, add member `NetPanel net_;`, class comment lists `Net`. `src/ui/side_panel.cpp`: insert the `Net` section between `Probe` and `Chip` as design §3.5.

### 3.3 — Visual check

- `bw_ui puzzles/broken_trace.bwp --probe U6.6 --screenshot /tmp/p3b-3a.png --after 1s`: section `Net` with `LINE_END_N · 3 pins`, `Clear`, `Driven by U6 pin 6 1Y`, `Feeds U2 pin 9 LOAD_N`, `U9 pin 5 3A`; Chip section still visible below or reachable by scrolling.
- `--probe U1.16 --screenshot /tmp/p3b-3b.png`: `VCC · N pins`, `Driven by J1 pin 1 VCC · supply`, `Feeds (N pins)` folded.
- `--probe U1.CLK` on `dead_counter.bwp` (test pattern): `X1 pin 8 OUT` and the seven `CLK` loads, `U5` before `U14`.

If the right column overflows so that `Chip` is pushed off-screen at 1280 × 800 with the TV open, note it in this plan's implementation notes for the user check; do not redesign the layout here.

### Fence

- [ ] Build and tests pass
- [ ] Screenshots read and match S3
- [ ] clang-format clean
- [ ] Commit: `"net panel: phase 3 — Net section"`

---

## Phase 4 — Release, user check, docs

### 4.1 — Release

`cmake --preset release && cmake --build --preset release && ctest --preset release -j 10`. `build/release/src/ui/bw_ui puzzles/broken_trace.bwp --probe U1.CLK --screenshot /tmp/p3b-rel.png --after 3s`: ratio ≈ 1.00×, `ui` ms within noise of P3a (0.4–0.7 ms).

### 4.2 — User check

User plays the three Puzzles following Nets with the panel (spec "Done when"). Record feedback under "Later" in this plan.

### 4.3 — Docs

- `docs/ARCHITECTURE.md` roadmap P3b row: mark done with the commit range.
- This plan: status column, a short summary line at the top, implementation notes (differences from the design).

### Fence

- [ ] Release tests pass; ratio ≈ 1.00×
- [ ] User confirmed
- [ ] Commit: `"docs: P3b net panel done"`

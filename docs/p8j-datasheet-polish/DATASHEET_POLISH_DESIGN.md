# P8j — Datasheet polish — Design

How [DATASHEET_POLISH.md](DATASHEET_POLISH.md) is built. Plan: [DATASHEET_POLISH_PLAN.md](DATASHEET_POLISH_PLAN.md). Numbers in parentheses (§3) are the spec's items.

The kernel, the Parts, the instruments, Puzzles and the Launcher are untouched. New: pure pin looks and groups (`src/view/pin_marks`), the binder's history (`src/view/binder`), a chip scope for a sheet's links (`src/manual`). Changed: the chip page (`src/ui/chip_panel`, `src/ui/manual_window`), the Board's pin plates and tooltips (`src/ui/board_view`, `src/view/board_scene`), four netlists and three KiCad netlist exports, the four KiCad Boards' pad Net names, the manuals' check, the sheets in `datasheets/`.

## Files

| File | Change |
|---|---|
| `src/view/pin_marks.hpp`, `.cpp` (new), `src/view/CMakeLists.txt` | `PinMark`, `PinLook`, `LookOf`, `WorksWith` (§1) |
| `src/view/binder.hpp`, `.cpp` | `Place`, the history, scroll per place; `openPart`/`closePart`/`part()` gone (§2) |
| `src/view/board_scene.hpp`, `.cpp` | `BoardTarget::pads`; `pinNames`, `pinKinds`, `partKind`, `alone`; `hoverText` with ` · ` (§5) |
| `src/manual/manual.hpp`, `.cpp` | `LinkScope::Chip` for `Parts`, `ManualLink::pins`, `LinkAt(…, chip)`, `ExpandRange`; `ComponentRoleSpans` for `ComponentRole`; the one-pin Net warning in `LoadManuals` (§3, §6) |
| `src/ui/manual_view.hpp`, `.cpp` | `FlowSpans` (public `DrawSpans`), the chip scope's dotted pin links, targets for a range (§4) |
| `src/ui/chip_panel.hpp`, `.cpp` | title, role, no Zoom to, no `TreeNodeEx`, symbols, looks, group tint, tooltip; Pins' hits merged (§4) |
| `src/ui/manual_window.hpp`, `.cpp` | the list without sheets; Back and All chips; per-chip page ids; the history; X1 on the desktop (§4) |
| `src/ui/contents_page.cpp` | its view's child id per chip; `BoardTarget{…}` init (§4) |
| `src/ui/board_view.cpp` | rings and flash for `pads`; `DrawPinNames` draws the group's marks; `BoardTarget{…}` inits (§5) |
| `src/ui/main.cpp` | `Tooltip`: ` · ` (§5) |
| `machines/010-counter/counter.bwn`, `020-led-chaser/chaser.bwn`, `070-tennis/tennis.bwn`, `130-bw2001/bw2001.bwn`; their `board/*.kicad_pcb`; `board/*.net` (Counter, Chaser, Tennis: the BW-2001 has none) | one-pin Nets removed (§6) |
| `tests/view/pin_marks_test.cpp` (new), `tests/CMakeLists.txt` | §1 |
| `tests/view/binder_test.cpp` | `binder: back …`; the "Part opened from the index" case removed (§2) |
| `tests/manual/manual_test.cpp` | Chip scope, ranges, role spans, one-pin Nets; the `C3`/`C1` examples moved to remaining Nets (§3, §6) |
| `tests/machines/manuals_test.cpp` | `ComponentRole` → `ComponentRoleSpans` (§3) |
| `tests/netlist/sample_test.cpp` | the Counter's count read from `U1`–`U5`'s `QA`–`QD` pins, not the `C0`–`C19` Nets; `ApiCounter` without them (§6) |
| `tests/view/board_scene_test.cpp`, `tests/view/focus_test.cpp` | hover text; `BoardTarget{…}` inits (§5) |
| `datasheets/*.md` | ranges for generic names; netlist syntax out (§7) |
| `docs/recipes/ADD_PART.md`, `ADD_MACHINE.md`, `docs/GLOSSARY.md`, `docs/ARCHITECTURE.md`, `AGENTS.md` | §8 |

The build is `-Wall -Wextra -Werror` (`CMakeLists.txt:14`): a field added to an aggregate breaks every positional brace-init that omits it (`-Wmissing-field-initializers`). The inits to complete: `BoardTarget{…}` in `board_view.cpp` (1466, 1499), `chip_panel.cpp` (59, 66, 69, 100, 297), `manual_window.cpp` (467, 470), `contents_page.cpp` (155), `focus_test.cpp` (123–126); `ManualLink{…}` in `manual.cpp` (417, 433, 438). New fields go last.

## 1. Pin looks and groups (§6, §9, §13, §14)

### `src/view/pin_marks.hpp`

```cpp
// A pin's symbol on the pinout and beside its plate on the Board (P8j).
enum class PinMark : std::uint8_t { In, Out, OpenCollector, Both, Power, NoConnect, Passive };

struct PinLook {
    PinMark mark = PinMark::Passive;
    bool activeLow = false;  // the name ends in _N: a bubble at the body
    bool clock = false;      // CLK, CLK+digit (CLK1), …CLK (1CLK), CKA/CKB, PHI, PHI+digit (PHI2)
};

// A chip's pin (`chip`: its PartKind is Chip) by its kind; anything else's (a resistor, LED,
// digit, switch, connector) Passive but power. `kind` nullopt: Passive.
PinLook LookOf(std::string_view name, std::optional<PinKind> kind, bool chip);

// The pins (indices into `names`, the Part's pin names in pin order) that pin `pin` works with,
// itself included, sorted; empty when the Part has no sections, or `pin` is power, NC or has no
// kind. A section's pin: its name starts with digits then a letter ("3A", "1CLK", "1A1"); a Part
// with sections has at least one. A section pin: its section's pins and the shared ones (no
// leading digit, not power, not NC); a shared pin: every section's pins and itself.
std::vector<std::size_t> WorksWith(std::span<const std::string> names,
                                   std::span<const std::optional<PinKind>> kinds, std::size_t pin);
```

- `LookOf` for a chip: `Input` → In; `Output` → Out; `OpenCollector` → OpenCollector; `Bidirectional`, `PullUp` (the 6520/6522/6530 ports, the MC3446's bus: ADD_PART.md "bus pins are `PullUp` Pins") → Both; `Power` → Power; `NoConnect` → NoConnect; `Switched`, `Joined` → Passive. Not a chip: Passive whatever its kind (the switches' pins are `Output`, the connectors' `Input`: `switch.cpp:41`, `connector.cpp:53`), but Power when its kind is `Power` or its name is a power name (`PowerPinVolts`: a connector's `VCC`/`GND`). `activeLow`, `clock` by name.
- Only `sim/logic.hpp`'s `PinKind` and `parts/catalogue.hpp`'s `PowerPinVolts` are needed: pure, in `src/view/` beside `bench_places` and `binder`.

### Tests (`tests/view/pin_marks_test.cpp`, cases `pin marks: …`, `[view]`)

Pin names and kinds are written in the test (the Parts' `PinSpec`s read once from `src/parts/`): no helper in `catalogue`.

- `pin marks: looks`: each `PinKind` on a chip; `PullUp` → Both; a switch's `Output` → Passive; a connector's `GND` → Power; `CLR_N` active low; `CLK`, `CLK1`, `1CLK`, `CKA`, `PHI`, `PHI2` clocks; `CA1`, `LE`, `OUT`, `PHIX` not.
- `pin marks: gates`: 74LS00: `1Y` → `1A 1B 1Y`; `VCC` → empty.
- `pin marks: shared pins`: 74LS273 `3Q` → `3D 3Q CLK CLR_N`; `CLK` → every `nD`, `nQ`, `CLK`; 74LS157 `2Y` → `2A 2B 2Y S G_N`; 74LS74 `1Q` → `1D 1CLK 1PRE_N 1CLR_N 1Q 1Q_N`; 74LS244 `1Y2` → section 1 with `1OE_N`; 74LS20 `NC3` → empty.
- `pin marks: no sections`: 74LS161 `QA`; a resistor's `1`, `2` (no kinds): empty.

## 2. The binder's places and history (§2, §3, §12)

### `src/view/binder.hpp`

```cpp
// Where the binder is open: a divider and, on Datasheets, the selected chip (nullopt: the list,
// nothing selected); on Contents, its chip.
struct Place {
    Divider divider = Divider::User;
    std::optional<std::size_t> chip;
    bool operator==(const Place& o) const { return divider == o.divider && chip == o.chip; }
};

class Binder {
public:
    // As now, and: a new selection while out turns to Datasheets (as now) and records the place
    // it leaves, also when already on Datasheets (U5 → U7 is a move).
    void frame(bool out, std::optional<std::size_t> selected, bool hasContents = false);
    void turn(Divider divider);  // a tab clicked: records the place it leaves, if another
    Place here() const;          // {divider, selected on Datasheets, contents() on Contents}
    // The page drawn this frame and its scroll, kept per place for back(): only the page drawn
    // updates its place (a hovered chip's page never writes the selected one's).
    void scrolled(const Place& page, float y);
    bool canGoBack() const { return !history_.empty(); }
    // The place to return to, off the history, with its scroll. The Binder becomes it (divider;
    // selected_ = its chip on Datasheets, so the next frame() sees no new selection; contents_ on
    // Contents) and records nothing; the caller matches the Board's selection (BoardView::select
    // or deselect, never focus) and scrolls the page once.
    struct Back {
        Place place;
        float scroll = 0;
    };
    std::optional<Back> back();
    void showList();  // ☰ All chips: recorded as a move; the caller deselects the Board
private:
    void record(const Place& from);  // a repeat of the last dropped; at most kHistory (32)
    std::vector<Place> history_;
    std::map<std::pair<Divider, std::optional<std::size_t>>, float> scrolls_;
    // as now: divider_, shown_, selected_, contents_, out_ (part_ gone)
};
```

- Every change of `here()` by `turn`, `frame`'s selection or `showList` records the place left; `back()` and coming out (`frame`'s `out && !out_`) record nothing. The history survives putting the binder away.
- `openPart`, `closePart`, `part()` go with the generic sheet (phase 2), and binder_test's "a Part opened from the index" case with them.
- Tests (`tests/view/binder_test.cpp`, `binder: back …`): a tab then back; a selection then back (the selection restored, no turn at the next `frame()` with it); U5 → U7 on Datasheets then back; back twice; nothing to go back to; a hover (no call) records nothing; the same place twice recorded once; 33 moves keep 32; the scroll per place, only from `scrolled`; back to Contents stays on Contents.

## 3. Links in a chip's sheet, the role sentence (§5, §10)

### `src/manual/manual.hpp`

```cpp
// Chip: a chip's sheet (P8j): only that chip's pin names and ranges of them (a sheet is shared by
// every Machine: its `D7` is a pin, never the LED D7). Machine: a Machine's own manual.
enum class LinkScope : std::uint8_t { Chip, Machine };

struct ManualLink {
    // as now, and last:
    std::vector<std::string> pins;  // Chip scope: pin numbers, one or a range's; references = {chip}
};

// `chip`: the reference whose pins a Chip scope links ("U5"); unused for Machine.
std::optional<ManualLink> LinkAt(std::span<const Span> spans, std::size_t i,
                                 const LoadedCircuit& design, LinkScope scope,
                                 std::string_view chip = {});

// The pin names `a`–`b` stands for, both included: equal-length names differing in one upper-
// case letter or digit (`1A`–`4A`, `A`–`D`), or one prefix and two numbers (`A0`–`A15`);
// {a, b} otherwise. Shared by BulletPins and the Chip scope.
std::vector<std::string> ExpandRange(std::string_view a, std::string_view b);

// The Components bullet of `reference` after its leading code span (the space after it
// dropped), its first letter upper-cased: the role sentence as spans. nullopt if absent.
std::optional<std::vector<Span>> ComponentRoleSpans(const Markdown& service,
                                                    std::string_view reference);
```

- Chip scope at `spans[i]` (Code): a range when `spans[i+1]` is Text trimming to "–" or "-", `spans[i+2]` is Code, and every name of `ExpandRange` is a pin of `chip` (`PinNumber(design, chip, name)`): `spansUsed = 3`. Else `spans[i]` alone when a pin of `chip`. Else nullopt.
- `LinkScope::Parts` goes in phase 2 with its users (the chip panel's Function and Pins, the generic sheet: removed in the same phase).
- `ComponentRole` goes; its users (`chip_panel.cpp`, `tests/machines/manuals_test.cpp`) take `ComponentRoleSpans`.

### Tests (`tests/manual/manual_test.cpp`, `[manual]`)

- `manual: chip pins`: the Counter, U5: `CLK` → pin 2; `` `A`–`D` `` → pins 3–6, `spansUsed == 3`; `QE`, `U4`, `FFFC` plain.
- `manual: ranges`: `ExpandRange("A0", "A15")` → 16 names; `("1A", "4A")`; `("A", "D")`; `("QA", "U9")` → the two.
- `manual: a sheet never links a reference`: the LED computer: `D7` in U1's (the 6502's) scope → U1's `D7`, not the LED; `U2` plain.
- `manual: role spans`: the Counter's U5 → "Counter bits 16–19: counts when ", `U4`, …; absent → nullopt.
- The `LinkScope::Parts` checks (lines 101–130) become Chip checks.

## 4. The chip page (§1–§12)

### `src/ui/manual_view.hpp`, `.cpp`

- `TargetHits FlowSpans(std::span<const Span>, const BoardScene*, const LoadedCircuit&, LinkScope, std::string_view chip = {})`: today's `DrawSpans`, public.
- `ManualView::draw(blocks, scene, design, scope, std::string_view chip = {})` passes `chip` on.
- Chip scope: a link in `kPageCode`, the mono face, a **dotted** underline (`TextFlow::text(raw, colour, underline, bool dotted = false)`: 2 px dashes, 2 px gaps). Its target: one pin `{{chip}, its pad, nullopt, {}}`; a range `{{chip}, nullopt, nullopt, its pads}`.

### `src/ui/chip_panel.cpp`

`ChipPanel::draw(scene, component, manuals, design)` keeps its signature and draws, in order:

1. **The title** (§4): `PushPageFont(PageFace::Bold, 1.3f)`; a `TextFlow` of the reference as a link (`kPageLink`, underlined; hovered → `hits.hovered = {{component}}`, clicked → `hits.clicked`), ` · `, the Part's name, ` · `, its description; `ImGui::Separator()`.
2. **The role** (§5): `ComponentRoleSpans` through `FlowSpans(…, LinkScope::Machine)`.
3. **The pinout** (§6–§8): the DIP table as now with two more fixed columns, the stubs (`kStub` 20 px) either side of the body; on them `DrawPinSymbol(list, bodyEdge, y, side, PinLook, bool alone)` in `kSymbol` (`IM_COL32(138, 79, 216, 255)`): arrowheads 5.5 px, the bubble r 3.6 at the body, the pulse 12 × 6 over the line, a dashed line when `alone`, a hollow head for OpenCollector, a grey plain line for Power, nothing for NoConnect (its number faint). The digit's pins (`DigitPinout`) and the list's (`PinSide::List`) draw the same symbol before their name, horizontally. `PinRow` gains `look` (`LookOf(name, scene.pinKinds(c)[p], scene.partKind(c) == PartKind::Chip)`) and `alone` (`scene.alone(c, p)`); with `alone`, `net` is the design Net's name when it has one (the nine kept, §6) else empty, and the name is drawn faint (`IM_COL32(168, 162, 148, 255)`). The name in `PageFace::Mono`, the Net in `kPageLink` regular.
4. **The group tint** (§9): `group_` (pads, from `WorksWith(scene.pinNames(c), scene.pinKinds(c), pin)`) for the pin hovered **last frame** (a row's hover, or `hits.hovered->pad` from Function or Pins); each row in it on a band `IM_COL32(236, 226, 250, 255)`, the hovered one `IM_COL32(220, 202, 246, 255)`. One frame late: the text's hover is known after the rows are drawn.
5. **Function, then Pins** (§10): a bold line each (`PushPageFont(PageFace::Bold)`), then `function_.draw(Section(*datasheet, "Function"), &scene, design, LinkScope::Chip, reference)`; the same for Pins. The two `TreeNodeEx` go. **Both draws' hits are merged** into `hits` (today Pins' are dropped, `chip_panel.cpp:362`).
6. **The tooltip** (§11): `std::format("pin {} · {} · {}", number, name, net)` (without ` · net` when there is none), then the Pins line.

"Zoom to" goes (the title's click does it).

### `src/ui/manual_window.cpp`

- `datasheets()`:
  - **the keys** (§2, §3): `◀ Back` (`ImGui::BeginDisabled(!binder_.canGoBack())`, its triangle drawn) and, on a chip's page, `☰ All chips` (three lines with a dot each, drawn); `ControlHeight()` high, at the reading size (the page's controls, as the Give up page's button).
  - **the chip shown**: `board.pointedComponent()` as now (a hover shows a chip's page; ☰ deselects only).
  - **the list** (§1): Part names in `kPageCode`, not links; references as now.
  - ☰: `binder_.showList()`, `board.deselect()`.
- **Per-chip page ids** (§12): the page's child id is `PageId(divider)` and, on Datasheets with a chip shown, its reference (`"page datasheets U5"`): ImGui keeps each child's scroll. Inside the child, after the page: `binder_.scrolled({divider, chip shown}, ImGui::GetScrollY())`.
- **Contents** (`contents_page.cpp`): the inner view's child id `"view " + reference`: each chip keeps its place (the outer page does not scroll). A Contents place restores the divider and the chip, no scroll.
- **Back** (§12): the key, or on the desktop only (`#ifndef __EMSCRIPTEN__`: in a browser X1 is the browser's back) `ImGui::IsMouseClicked(3)` while the binder's window is hovered: `binder_.back()`; then `board.select(*chip)` or `board.deselect()` (Datasheets); `scrollOnce_` applied by `ImGui::SetNextWindowScroll({0, scroll})` before the page's `BeginChild` the next frame.
- Tabs call `binder_.turn` as now, which now records.
- `sheet_` and the list's Part link go.

## 5. The Board (§14)

### `src/view/board_scene.hpp`, `.cpp`

```cpp
struct BoardTarget {
    // as now, and last:
    std::vector<std::size_t> pads;  // a range of one part's pins (P8j): ringed, no Net lit
};
std::span<const std::string> pinNames(std::size_t component) const;  // pad order (info_[k].pinNames)
std::span<const std::optional<PinKind>> pinKinds(std::size_t component) const;  // pad order
PartKind partKind(std::size_t component) const;
bool alone(std::size_t component, std::size_t pad) const;  // its Net reaches no other pad, or none
```

- `pinKinds` filled in the constructor beside `info_[k].pinNames`, from `design.components.at(reference)->pins()` (nullopt without a Component).
- `alone` from `netPads_` (`board_scene.hpp:144`), as `wiring()`'s `used` reads it (`board_scene.cpp:458`): no new count.
- `hoverText`: a pad `"U3 · pin 3 · 1Y · SIX_N"`; a chip `"U5 · 74LS161"` (`main.cpp`'s `Tooltip` adds ` · description`). The shelf's probe tip (`shelf.cpp:126`) shows the same text. `board_scene_test.cpp`'s "view: hover text" updated.

### `src/ui/board_view.cpp`

- `DrawPinNames(list, map, scene, component, std::optional<std::size_t> hoveredPad)`: `hoveredPad` is `hoverTarget_->pad` when the hover target is one pad of `component`. For each pad of `WorksWith(…, *hoveredPad)`, beside its plate on the side away from the pad, `DrawPlateMark(list, plate, towardChip, PinLook)`: an arrow (towards the courtyard's centre for In, away for Out, both for Both, a hollow head for OpenCollector), the pulse for a clock; `kSymbol`, about 10 px. Only where names are drawn (the function returns early below `kPinNameMinPadPx`).
- `BoardTarget::pads`: the hover ring and the click's flash ring only those pads (`board_view.cpp:944–951` rings every pad when `!target.pad`); `highlight_` not set for them.
- A finger held on a pad: the pick path already sets `hoverTarget_` (P8f's loupe): nothing more; checked by a `--touch --hold` shot.

### `src/ui/main.cpp`

`Tooltip`: the component's description after ` · ` (as now); the pin's line after ` · ` (was ` - `).

## 6. Netlists, Boards, the manuals' check (§15)

- **The Nets removed** (the agent's and the review's sweeps, 2026-11-27; named by no player's manual, Puzzle, Vetting Script or `bench/` file):
  - `machines/010-counter/counter.bwn`: `C0`–`C18` (`C19` stays: `U5.QD D1.K`);
  - `machines/020-led-chaser/chaser.bwn`: its fifteen;
  - `machines/070-tennis/tennis.bwn`: `LP1`, `RP1`, `ST1`, `ST2`;
  - `machines/130-bw2001/bw2001.bwn`: `CHASSIS`, `SEL3_N`, `SEL4_N`, `SEL5_N`, `SEL6_N`, `SEL10_N`.
  Comments naming them adjusted (the Counter's header).
- **Their test users**: `tests/netlist/sample_test.cpp` (`Count()` reads `nets.at("C" + bit)`: it reads the five counters' `QA`–`QD` pins' Nets in the built Circuit instead; `ApiCounter` stops making `C0`–`C18`); `tests/manual/manual_test.cpp` (line 135's `C3` → `CARRY1`; line 142's `` `C3`–`C1` ``, `` `R1`–`C3` `` → `` `CARRY3`–`CARRY1` ``, `` `R1`–`CARRY3` ``, the expected messages adjusted).
- **The Boards**: the game reads no Net names from a `.kicad_pcb` (`BoardScene` labels pads from the netlist; `CheckBoard` compares copper islands), so only KiCad cares: a one-off script deletes the `(net "NAME")` line of each pad whose Net was removed (no copper touches them); `bw check-board` on each before and after. The Counter's, Chaser's and Tennis's `board/*.net` regenerated by `bw export-kicad` (the BW-2001 has none). ADD_MACHINE.md §3 gains this case (a routed Board's Nets changed, no part moved).
- **The check**: in `LoadManuals`, a Warning per design Net reaching exactly one pin (a count over `design.connections`) whose name is no code span of either manual: `"net C16 reaches one pin and no manual names it"`, its file `"netlist"`, line 0 (a `LoadedCircuit` has no file name). Rails reach many pins: unaffected. `tests/machines/manuals_test.cpp` (every Machine clean) enforces it; `manual: one-pin Nets` in `manual_test.cpp`: a tiny netlist with `net X U1.QA` warned, and not once a manual names `X`.

## 7. Sheets (§16)

- Generic names to ranges, sheet by sheet (the spec's table): 74LS107, 74LS175, 74LS273, 74LS373, 74LS74 (`D`, `Q`, `Q_N`, `J`, `K`, `CLR_N` for each section's), 74125, 74LS139, 74LS157, 74LS244 (`A`, `B`, `Y`, `OE_N`), 6520 (`IRQ_N` for `IRQA_N`/`IRQB_N`), MC3446. Plain as they are: the 74LS47/48's `a`–`g` and DIGIT's `C`, `E`, `F` (segments), the Z80's `R` (a register), the 6530's `00`–`11` (values).
- Netlist syntax out: 2316B, 2332, 2513, 2716 (`contents=`, `offset=`, `.rom`, `.bin`, `select=`: the sheet keeps "each chip select's level is set when the chip is made"), R, C (`value=`). Their facts go to ADD_PART.md or ADD_MACHINE.md where missing (`grep -n "contents=\|value=\|select=" docs/recipes/*.md`).
- `CheckDatasheet` unchanged; `manuals_test` checks every sheet.

## 8. Docs

- GLOSSARY: **Pin mark**, **Section** (a Part's pins by leading digit), **goes nowhere**.
- ADD_PART.md: a sheet names pins by name or range, never generically; no netlist syntax in a sheet; a clock's name gives its pulse.
- ADD_MACHINE.md: no Net reaching one pin unless a player's manual names it; §3 a routed Board's Nets removed.
- ARCHITECTURE: the P8j row done; AGENTS.md shared helpers: `LookOf`, `WorksWith` (`src/view/pin_marks.hpp`), `ExpandRange` (`src/manual/manual.hpp`).

## Checks

Test names use spaces (`"board scene: …"`, `"view: hover text"`, `"bench options: …"`, `"fault draw: …"`, `"sample: …"`).

- Logic: `ctest --preset release -R "pin marks|binder|manual|board scene|view: hover"`; debug the same.
- Netlists: `bw check-board` on the four; `ctest --preset release -R "sample|manual|counter|chaser|tennis|bw2001|drc|procedural|fault draw|board"` (a `drc_*` case hung at 0 % CPU on kicad-cli is killed and rerun alone).
- Screenshots (release, viewed): the spec's list. A chip is selected by a scripted tap on its body or its reference in the list; a pointer rests where a `--tap` left it for 2.5 s (`kLingerTime`, `fingers.hpp:24`), so `--after` falls inside.
- Speed: the BW-2001 in `bw_ui` with the binder out on a chip's page, ratio and UI ms before and after.
- The full release suite and the web build at the end.

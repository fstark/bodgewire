# P6i — Manuals that explain — Design

Spec: [MANUALS.md](MANUALS.md). Plan: [MANUALS_PLAN.md](MANUALS_PLAN.md). The manuals' sections follow [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) §2 (its `user.md` and `service.md` bullets), which this step extends (phase 4). No kernel, Part, builder, device or Session change.

## Files

| File | Change |
|---|---|
| `src/manual/markdown.{hpp,cpp}`, `tests/manual/markdown_test.cpp` | `BlockKind::Table`, its cells |
| `src/manual/manual.{hpp,cpp}`, `tests/manual/manual_test.cpp` | `LinkAt` (runs, Nets, pins by name, a scope), `CheckReferences` (the same, table cells), the optional sections; the existing `manual: links` (`link->reference`, the 3-argument `LinkAt`, `` `CLK` `` now a Net link in Machine scope) and `manual: checks` (the section messages) cases updated |
| `tests/machines/manuals_test.cpp` | an assertion that the Counter's manuals have their four new sections |
| `src/ui/board_view.{hpp,cpp}` | `BoardTarget` a part, a run or a Net; hover, frame and flash for each |
| `src/view/board_scene.{hpp,cpp}` | unchanged (a Net link's `NetId` comes from `design.nets`) |
| `src/ui/manual_view.{hpp,cpp}` | tables drawn; links of every kind; the scope per page |
| `src/ui/manual_window.cpp`, `src/ui/chip_panel.cpp` | `BoardTarget`'s new shape; datasheets drawn with the part-only scope |
| `machines/010-counter/manuals/{user,service}.md`, `machines/010-counter/docs/COUNTER_DESIGN.md` (§Manuals) | the new text (phase 3) |
| `docs/STYLE.md`, `docs/recipes/ADD_MACHINE.md`, `docs/GLOSSARY.md`, `docs/ARCHITECTURE.md` | phase 4 |

## Tables (`src/manual/markdown`)

```cpp
enum class BlockKind : std::uint8_t { Title, Heading, Paragraph, Bullet, Table };

struct Block {
    BlockKind kind;
    std::uint32_t line;
    std::vector<Span> spans;                    // empty for a Table
    std::vector<std::vector<std::vector<Span>>> rows;  // Table: rows of cells, the header first
};
```

- A table starts at a line beginning with `|` whose next line is a separator (`|`, `-`, `:`, spaces only, at least one `-`). Each following line beginning with `|` is a row; any other line (blank or not) ends it, then is parsed as usual. Without a separator the line is a paragraph line, as today.
- A row's cells: the line trimmed, the outer `|` dropped, split at `|` outside code spans (a `` `a|b` `` cell stays whole), each cell trimmed and parsed by `Inline`. The separator row is dropped. Rows keep their own cell counts; the drawing pads short rows.
- `PlainText` of a Table's `spans` is empty: `CheckShape` and `Section` are unchanged. The `Builder` closes the open block before a table and pushes the table whole.
- Test cases `markdown: table …` `[manual]`, as the spec lists.

## Links (`src/manual/manual`)

```cpp
enum class LinkScope : std::uint8_t { Parts, Machine };  // a datasheet; a Machine's manual

// A link starting at spans[i]: a part (with " pin N" after it), a run `U1`–`U5`, a pin by name
// `U2.ENT`, or (Machine scope) a Net.
struct ManualLink {
    std::vector<std::string> references;  // one part, or a run's parts in netlist order
    std::optional<std::string> pin;       // a pin number, with one reference
    std::optional<std::string> net;       // a Net link: no references
    std::size_t spansUsed = 1;            // a run: 3 (`U1`, "–", `U5`)
    std::size_t pinChars = 0;             // as today: leading chars of the next Text span
};
std::optional<ManualLink> LinkAt(std::span<const Span> spans, std::size_t i,
                                 const LoadedCircuit& design, LinkScope scope);  // no default: every caller says
```

In order, at a Code span `spans[i]`:

1. **A run** (Machine scope only): `spans[i]` a part, `spans[i + 1]` a Text span exactly `–` (U+2013), `spans[i + 2]` a Code span naming a part; both split as letters then digits (`LooksLikeReference`), the same letters, the first number smaller. `references`: every part of the design with those letters and a number in that range, in netlist order. `spansUsed = 3`. Datasheets are full of pin ranges (`` `D0` ``–`` `D7` ``) that could match a Machine's references: never a run there.
2. **A part**: as today, `pinChars` and `pin` from " pin N".
3. **A pin by name** (Machine scope only): `REF.PIN`, `REF` a part, `PIN` one of its pin names from `design.components.at(REF)->pins()` (not `PartPinNames`, which builds a Circuit per call; `LinkAt` runs per span per frame); `pin` is its number as text (the pin index + 1, as `padIndex` takes it). A resistor is not in `design.components` and has no pin names: `` `R1` pin 2 `` names its pins.
4. **A Net** (Machine scope only): `design.nets` contains the text.
5. Otherwise none: the span is drawn as code.

A run whose ends are both Nets (`` `A8`–`A15` ``) is two Net links and an en dash.

`CheckReferences(doc, problems, design)` (Machine scope: only the User and Service Manuals call it) walks each block's spans and each table cell's spans with the same rules, and reports:

- a reference-looking span (`LooksLikeReference`) that is neither a part nor a Net, as today: `unknown reference 'U9'`;
- two **parts** joined by `–` whose letters differ or whose numbers do not increase: `bad run U5–U1`. Only when both ends are parts: the Typewriter's `` `R1` ``–`` `R3` `` (a resistor and a Net) and the Message's `` `F5` ``–`` `F1` `` (Nets) are not runs and stay unreported;
- `REF.PIN` whose right side looks like a pin name (upper case letters, digits, `_`): `REF` not a part: `unknown reference 'U9'`; no such pin: `U2 has no pin ENX`. Any other `.` span is not checked: `` `0.5` ``, the BW-2001's `` `UH1.bin` ``;
- " pin N" beyond the part's pins, as today.

`CheckDatasheet` keeps its own checks; datasheets do not call `CheckReferences` today, unchanged.

### Sections

`CheckShape(doc, problems, required, optional = {})`: the headings must be the required ones in order, each optional one present at most once, after the required ones, in the given order. Datasheets pass no optional ones.

- User Manual: required `What it does`; optional `Using it`, `How it works`.
- Service Manual: required `Components`; optional `Test points`, `Troubleshooting`.
- The message lists them: `sections must be: What it does[, Using it][, How it works]`; with no optional sections it reads as today (`sections must be: Name, Pins, …`).

## The Board (`src/ui/board_view`)

```cpp
struct BoardTarget {
    std::vector<std::size_t> components;  // one part, or a run's parts; empty for a Net
    std::optional<std::size_t> pad;       // with one part
    std::optional<NetId> net;             // a Net link
};
```

Every place building a `BoardTarget` builds `{{component}, pad, std::nullopt}`: `chip_panel.cpp:54`, `:57`, `:208`; `manual_window.cpp:290`, `:293`; `manual_view.cpp`; `board_view.cpp:1220` (its own hover at the pick). The readers (`board_view.cpp` at the outside hover, the pad ring, the flash, `focus`, `route`) read `components.front()` only after checking `!components.empty()`: a Net target has none.

- **Hover from outside** (`hoverFromOutside`, drawn at the next frame): a Net: `highlight_` that Net (the copper lit and its drivers marked, as a Board hover); a pad: as today; one part or a run: each part outlined with `DrawUseSelection(…, kHighlight)` (real look) or its courtyard drawn lit (plain look). Today a part with no pad shows nothing (`hoverTarget_` is read only with a pad).
- **Focus** (a click): the frame box is the union of the parts' courtyards (a run), the pad's box (a pad), or the Net's copper (its pads, traces and vias, `scene_->padNet` / `traceNet` / `viaNet` equal to the Net); `share` 0.5 as for a part, no `minPixelsPerMm` for a Net or a run. `Flash` keeps the target: the flash circles every pad of every part of the run, or every pad of the Net; while the flash lasts a Net stays lit (`highlight_` set from the flash when nothing else lights a Net).
- **`route`**: a click selects only a single part (`components.size() == 1`), as today; a run or a Net selects nothing.
- A Net link's `NetId`: `design.nets.find(name)`; `BoardScene` numbers its Nets from `design.nets` (`board_scene.cpp`), so the ids agree. No new `BoardScene` API.

## The binder (`src/ui/manual_view`, `manual_window`)

- `ManualView::draw(blocks, scene, design, LinkScope scope)`: the User and Service Manuals with `Machine`, datasheets (`sheet_`, the chip panel) with `Parts`.
- `DrawSpans` uses `LinkAt` with the scope and advances by `spansUsed`; a run draws its three spans as one underlined link; a Net link's target is `{{}, std::nullopt, design.nets.at(*link->net)}`; a pin by name a pad as " pin N" is.
- A Table: `ImGui::PushID(block.line)` (several tables on a page), `ImGui::BeginTable("table", …)` with as many columns as the widest row, `ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp`; the header row set apart by `ImGuiTableRowFlags_Headers` with a paper-tinted header background (`ImGuiCol_TableHeaderBg`) and a rule under it (no bold font is loaded: `kBold` is only a colour); each cell `DrawSpans` (TextFlow wraps at the cell's right edge: its `right_` is the content region's, which a table column sets). Drawn only while its section is open, as paragraphs.

## The Counter's manuals

Written to STYLE's Manuals rules (phase 4 writes them; phase 3 follows the draft the user approved on 2026-11-18):

- `user.md`: `What it does` (welcome: what the board is, the five counters dividing 1 MHz by 1 048 576, the LED about once a second); `How it works` (the goal, the clock, one chip counting 0–15, the odometer, the LED, the unused pins in a table).
- `service.md`: `Components` unchanged; `Test points` (a table: probe point, what a working Board shows on the logic probe), each row checked with `bw run machines/010-counter/counter.bwn --time T --probe …` at 1 s, 1.5 s and 2 s before it is written (a slow Net: "high for about half a second, then low"); `Troubleshooting` (by symptom: the LED never changes, blinks too fast, never lights: ordered checks from the clock up), written from the working Board.

## Docs

- `docs/STYLE.md`: `## Manuals`: one idea per sentence; short sentences; the goal first; a familiar picture before the mechanism; no formulas; leave out what the reader does not need to repair this Machine; unused features in a table; end a mechanism with what the probe shows; links (a part, `REF.PIN`, a run, a Net).
- `docs/recipes/ADD_MACHINE.md` §2: the user.md and service.md bullets name the new sections and point to STYLE's Manuals.
- `docs/GLOSSARY.md`: **Test Point** (a place on the Board the Service Manual names, with what a working Board reads there).
- `docs/ARCHITECTURE.md`: the P6i row done (its Service section named `Test points`, not "What a working board shows"); a row for the other Machines' manuals.
- `machines/010-counter/docs/COUNTER_DESIGN.md` §Manuals: the four sections, replacing "one paragraph: power `J1`".

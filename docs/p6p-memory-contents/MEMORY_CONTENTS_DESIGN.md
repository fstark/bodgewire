# P6p — Memory contents — Design

How [MEMORY_CONTENTS.md](MEMORY_CONTENTS.md) is built. Plan: [MEMORY_CONTENTS_PLAN.md](MEMORY_CONTENTS_PLAN.md). Numbers in parentheses (§7) are the spec's "What the player sees" items.

The kernel, the builder, devices and Puzzles are untouched. Parts gain a read of their cells (no behaviour change). The rest is `bw_ui` (`src/ui`), its ImGui-free logic (`src/view`), the disassemblers (`src/asm`), one Machine method, six data files and the docs.

## Files

| File | Change |
|---|---|
| `src/parts/memory_cells.hpp` | `read`, `wordBits` |
| `src/parts/rom.*`, `mos6530.*`, `ram2114.*`, `ram6102.*` | their `read`, `wordBits` |
| `src/machine/machine.hpp`, `.cpp` | `ChipContents`, `Machine::contents` |
| `src/asm/z80_disassembler.hpp`, `.cpp` (moved from `tests/asm/`) | `DisassembleZ80`, unchanged; `Z80DocumentedOpcodes` stays in the tests (`tests/asm/z80_opcodes.{hpp,cpp}`) |
| `src/asm/disassembled.hpp` (new), `src/asm/mos6502_disassembler.hpp`, `.cpp` (new) | `Disassembled` (the Z80's `Z80Text` renamed); `Disassemble6502` |
| `src/parts/mos6502_core.hpp`, `.cpp`, `src/asm/asm6502.cpp` | `Mos6502OperandBytes` (moved from `asm6502.cpp`'s `OperandBytes`) |
| `src/asm/CMakeLists.txt`, `tests/CMakeLists.txt` | the moved and new files |
| `src/view/chip_body.hpp`, `.cpp` (new) | `PadCentres`, `ChipBody`, `BodyOf` moved from `board_look.cpp`, its `Box` as `BodyBox`; `MemoryLook`, `MemoryLookOf`, `MemoryWindowOf`, `OnMemoryWindow` |
| `src/view/memory_contents.hpp`, `.cpp` (new) | the views' logic: settings, defaults, character sets, hex rows, code lines, glyph dots |
| `src/view/contents_defaults.hpp`, `.cpp` (new) | `ContentsDefaults`, `ParseContentsDefaults` |
| `src/view/binder.hpp`, `.cpp` | `Divider::Contents`, `openContents`, `contents()` |
| `src/view/CMakeLists.txt` | the new files; `bw_asm` linked |
| `src/ui/board_look.cpp` | uses `chip_body.hpp`; `MemoryOf`, `WindowRadius` replaced by `MemoryLookOf`, `MemoryWindowOf` |
| `src/ui/board_view.hpp`, `.cpp` | the window's press and click, `takeContentsClick`, its tooltip |
| `src/ui/contents_page.hpp`, `.cpp` (new), `src/ui/CMakeLists.txt` | the page: head, view buttons, settings, the three views |
| `src/ui/manual_window.hpp`, `.cpp` | the Contents tab and page; `openContents`, `setContentsDefaults`; `PushPaper`'s widget colours |
| `src/ui/main.cpp` | `roms/contents.txt` read; the window click; `--contents` |
| `machines/{080-message,090-typewriter,110-led-computer,120-bw1,130-bw2001,140-bw80}/roms/contents.txt` (new) | the defaults |
| `machines/…/manuals/user.md` (the same six) | one sentence: the window, the Contents divider |
| `tests/parts/rom_test.cpp`, `mos6530_test.cpp`, `ram2114_test.cpp`, `ram6102_test.cpp`, `tests/machine/machine_test.cpp`, `tests/view/*`, `tests/asm/*` (below) | the tests |
| `docs/GLOSSARY.md`, `docs/recipes/ADD_MACHINE.md`, `ADD_PUZZLE.md`, `docs/ARCHITECTURE.md` | Contents, the file, the Puzzle rule, the row |

## 1. The read (§11)

### `MemoryCells`

```cpp
// The word as the chip holds it now, a stuck cell included (bit 0 the chip's first data Pin).
// Not while the Circuit runs for a RAM: the Runner's thread writes it.
virtual std::uint8_t read(std::size_t memory, std::size_t word) const = 0;
// Bits in a word of `memory`: a 2716's 8, a 2513's 5, a 2114's 4, a 6102's 1.
virtual unsigned wordBits(std::size_t memory) const = 0;
```

`Rom`: `block_[word]` and `spec_.data.size()`; `Mos6530`: `rom_[word]` or `ram_[word]`, 8; `Ram2114`: `cells_[word]`, 4; `Ram6102`: `cells_[word]`, 1. Each `Check`s the memory and word as `stick` does. `Rom::word` stays (its tests).

**Tests** (case names starting `rom:`, `6530:`, `2114:`, `6102:` as their files' do): `read` after a `stick`, `wordBits`.

### `Machine::contents`

```cpp
// machine.hpp, namespace bw. A ROM chip's words (P6p).
struct ChipContents {
    std::vector<std::uint8_t> words;
    unsigned bits;  // per word
};
// Machine: the ROM chip's words as it holds them now: the design's, through the current stuck
// cells and columns of memory 0 (a swap clears them at once, before the next build). nullopt: no
// such chip, or not a ROM (a RAM's cells belong to the running Machine).
std::optional<ChipContents> contents(std::string_view reference) const;
```

From `designCircuit_->components` (never run: no thread), only a `Rom` or a `Mos6530` (`dynamic_cast`), `MemoryCells::read(0, w)` for every word, then each `cells_` entry of that reference with `memory == 0`: `dataBit(pin)` set or cleared in its word, or in every word for a column (the builder's `stickCells` rule, `builder.cpp:165`). Memory 0 is the ROM on both (a `Rom`'s one memory, the 6530's `rom`).

No `Session` method: the page reads `session.machine()->contents(...)`. Without a Machine there is no Board, so no window and no page.

**Tests** (`machine_test.cpp`, `machine: contents …`): Odd E's UA2 word `$2A` reads `$44` (bit 2 over the design's `$40`); after `swap("UA2")` `$40`; a column on a test netlist; a 2114 and an unknown reference nullopt.

## 2. The defaults (spec decisions)

### `machines/NNN-name/roms/contents.txt`

```
# BW-80: what the binder's Contents page opens each chip at (P6p).
cpu z80
charset zx80
chip IC2 address $0000 glyphs $0E00
```

Lines: `#` comments and blank lines; `cpu 6502|z80` (at most once); `charset ascii|petscii|zx80` (at most once; default ASCII); `chip REF` followed by `address N` and/or `glyphs N` in any order (`$` hex or decimal, as `.bwp` words), each chip at most once. The six files:

| Machine | Lines |
|---|---|
| 080 Message | `chip U15 glyphs 0` (U14 needs none: Hex ASCII by default) |
| 090 Typewriter | `chip U23 glyphs 0` |
| 110 LED Computer | `cpu 6502`, `chip U2 address $F800` |
| 120 BW-1 | `cpu 6502`, `chip U2 address $1C00`, `chip U3 address $1800` |
| 130 BW-2001 | `cpu 6502`, `charset petscii`, `chip UH1 address $C000` … `chip UH7 address $F800` (the netlist's comments, `bw2001.bwn:106`), `chip UA2 glyphs 0` |
| 140 BW-80 | as above |

The address is the one the program is assembled at (`.rom`/`.org` in each `.asm`; the BW-1's `bw1_002.asm:19`), the first of a chip's mirrors.

### `src/view/contents_defaults.hpp`

```cpp
enum class Cpu : std::uint8_t { Mos6502, Z80 };
enum class CharacterSet : std::uint8_t { Ascii, Petscii, Zx80 };

struct ChipDefaults {
    std::optional<std::uint16_t> address;
    std::optional<std::size_t> glyphs;  // the word where its character set starts
};
// A Machine's roms/contents.txt (P6p): never more than defaults.
struct ContentsDefaults {
    std::optional<Cpu> cpu;
    CharacterSet charset = CharacterSet::Ascii;
    std::map<std::string, ChipDefaults, std::less<>> chips;
};
// Errors: "<file> line N: <what>" (unknown keyword, bad number, a second cpu, a chip twice,
// address over $FFFF). The chips are not checked against a netlist here (the test does).
std::expected<ContentsDefaults, std::string> ParseContentsDefaults(std::string_view text,
                                                                  std::string_view file);
```

`main.cpp` reads `<dir>/roms/contents.txt` beside the manuals (`LoadManuals`); a missing file is no defaults; an error is printed to stderr as a manual problem is, and the file ignored.

**Test** (`tests/view/contents_defaults_test.cpp`, cases `contents defaults: …`): each keyword, `$` and decimal, each error with its line. And for every Machine with a `roms/contents.txt`: it parses; each `chip` is a ROM chip of the Machine's netlist (its Part `FindRomSpec`, or `6530`: `MemoryLookOf` comes in phase 3); `glyphs` is under its word count; and every Machine with a CPU Part (6502, Z80) and a ROM chip has `cpu`.

## 3. The window (§1–3)

### `src/view/chip_body.hpp`

`PadCentres`, `ChipBody`, `BodyOf` move unchanged from `board_look.cpp` (anonymous namespace there) so the drawing and the click share the body; `board_look.cpp`'s `Box` moves as `BodyBox` (a public `Box` could clash with `placer.cpp`'s and `footprint.cpp`'s anonymous ones). `BoardScene::bodyAt` (courtyard) stays the pick's body; `ChipBody` is the drawn epoxy. Added:

```cpp
// A chip holding a program or a character set (P6p): an EPROM shows its quartz window, a mask
// ROM only while x-rayed. None: no window, no contents.
enum class MemoryLook : std::uint8_t { None, Eprom, Mask };
MemoryLook MemoryLookOf(std::string_view part);  // 2716 Eprom; 2332, 2316B, 2513, 6530 Mask

// The window at the body's centre: the quartz's radius 30 % of the body's width, the seal's
// 23/20 of it (board_look.cpp's numbers). nullopt: not a memory chip, or no body.
struct MemoryWindow {
    Point centre;
    std::int64_t quartz, seal;  // radii, nm
    bool horizontal;            // the body's
};
std::optional<MemoryWindow> MemoryWindowOf(const BoardComponent& component, std::string_view part);
// The chip whose window (its seal included) covers `at`, if any.
std::optional<std::size_t> MemoryWindowAt(const BoardScene& scene, Point at);
```

`board_look.cpp`: `DrawMemoryWindow` takes the window from `MemoryWindowOf`, `DrawChip`'s part number space from its `seal`. The look is unchanged (checked on a screenshot).

**Test** (`tests/view/chip_body_test.cpp`, cases `chip body: …`): `MemoryLookOf` for the five Parts and a 74LS00; on the BW-80's Board, IC2's window centred in its body, the seal 23/20 of the quartz, `MemoryWindowAt` true at the centre and at the seal's edge, false just outside it and on a pad, none on a 74LS chip.

### The click, `BoardView`

`BoardView` gains `std::optional<std::size_t> takeContentsClick()` and `windowPressed_` (the flip corner's pattern, `board_view.cpp:1156`): at the press, `windowPressed_ = plainLook_ ? nullopt : MemoryWindowAt(*scene_, at)` (after the flip corner, which wins); at the release without a drag, if the same window is under the mouse, `contentsClick_` is set and no `click_` is made, so no tool, pencil or selection sees it (§1). The double-click fit is not on a window (`onWindow` beside `onCorner`). A right-click is unchanged: what it does on the body (`rightClick_`, a pencil mark's erasing).

Hover (§2): when the mouse is on a window and nothing is dragged, `hovered_` is the chip (the x-ray, as today) and the tooltip is `"Read " + reference + "'s contents"`, in place of `tooltip_`'s.

`main.cpp`: after `takeClick`, `if (const auto k = boardView.takeContentsClick())`: `shelf.manualOut = true`, `manualWindows.openContents(*k)`, `BringToFront(kManualWindowName)` (or its popped-out window, as the shelf's click does).

The window's click wins over a Trace or wire under it, whatever the tool (the user's "always opens").

## 4. The binder (§4–5)

### `Binder`

`Divider` gains `Contents`, after `Repairs`. `Binder` gains:

```cpp
// A memory chip's window clicked (P6p): the Contents divider shows it, now if out, else when the
// binder comes out (the click brings it out).
void openContents(std::size_t component);
std::optional<std::size_t> contents() const { return contents_; }  // the tab shows once set
```

`openContents` sets `contents_`; out (`out_`, the last `frame`'s), `divider_ = Contents`; away, `shown_ = Contents` (the next coming out), so a later coming out from the shelf is at the User Manual again. `frame` is unchanged: a different selection while out still turns to Datasheets (§4). **Test** (`binder_test.cpp`, `binder: …`): no contents at start; opened while away, it comes out at Contents; opened while out, it turns at once, and put away and out again it is at the User Manual; a later selection turns to Datasheets; `contents()` kept after.

### `ManualWindows`

`kTabs` becomes `std::array<Tab, 5>` with `{Divider::Contents, "Contents", IM_COL32(200, 185, 235, 255)}` (a fifth colour, lilac); `tabs()` skips it while `binder_.contents()` is empty, so four tabs show until then. The fifth tab spans y 230–274: the binder's minimum height (about 292) leaves 6 px. `PageId` and `draw`'s switch gain `Contents` (`-Werror`). `draw` takes `const Machine*` (`session.machine()`) and calls `contents_.draw(...)` (below) for the divider. New, for `main.cpp`:

```cpp
// A memory chip's window clicked, or --contents: the Contents divider at that chip, at `view`
// the first time (else its default or the player's last).
void openContents(std::size_t component, std::optional<ContentsView> view = std::nullopt);
void setContentsDefaults(ContentsDefaults defaults);  // the Machine's roms/contents.txt
```

The `--manual` flag is unchanged.

**The paper's widgets**: `PushPaper` (`manual_window.cpp:69`) pushes text colours only, and no page had a field before. It gains the frame (`FrameBg`, `FrameBgHovered`, `FrameBgActive`: a lighter paper), `SliderGrab`, `SliderGrabActive` (the tan of the buttons, darker), `CheckMark` and `TextSelectedBg` (a pale blue), so a field reads as ink on paper; `kPaperColours` follows. `PopupBg` (the paper) is pushed around each combo only: `PageTooltip`'s light text needs the tooltips' dark background.

## 5. The page, `src/ui/contents_page.{hpp,cpp}`

```cpp
// The binder's Contents page (P6p): one memory chip's words in the view the player picked.
class ContentsPage {
public:
    void setDefaults(ContentsDefaults defaults);
    // The head's link hovered (to highlight); a click frames the chip itself (board.focus) without
    // selecting it, so the page stays (BoardView::route would select it and turn to Datasheets).
    TargetHits draw(BoardView& board, std::size_t component, const Machine& machine);
    void open(std::string_view reference, std::optional<ContentsView> view);
private:
    ContentsDefaults defaults_;
    std::map<std::string, ContentsSettings, std::less<>> settings_;  // per chip, for the run
    // The code view's lines, kept until the chip, its settings or its words change.
    struct CodeCache { std::string reference; ContentsSettings settings; std::size_t hash; std::vector<CodeLine> lines; };
    std::optional<CodeCache> code_;
    std::optional<std::uint32_t> goTo_;  // the address to scroll to on the next draw
};
```

Each frame: `machine.contents(reference)` (a copy of at most 4K words: cheap at the UI's rate; the code cache compares a hash of the words, so a swap re-disassembles); the settings for the chip, made by `DefaultSettings` on first use (§6). Layout (§5):

1. Head: the reference as a `TextFlow` link (`kPageLink`; hover returned as the index's, `manual_window.cpp:index`; a click calls `board.focus` itself), then `" " + part + "  " + SizeText(words, bits)`, then the address range when the chip has an address (`$C000-$C7FF`, a plain hyphen: the UI font has no en dash). `Title` scale for the reference line.
2. Three `SmallButton`s, the current one drawn as pressed (its `ButtonActive` colour).
3. The settings line: `Address` (`$` field, 4 hex digits, `InputText` with `CharsHexadecimal`), then the view's own: Hex `Characters` (`Combo`); Code `CPU` (`Combo`), `From`, `Length`; Glyphs `From`, `Rows` (`SliderInt` 1–16), `Bits` (`SliderInt` 1–8). Hex and Code end with `Go to`. The fields are narrow (`SetNextItemWidth`), wrapping with `SameLine` only while they fit.
4. The view in a child (`BeginChild("view")`), `ImGuiListClipper` over its lines; the page's fixed-width look: the UI font, columns placed by measured `CalcTextSize("0")` (the default font is monospaced).

**Hex** (§7): words per row `HexColumns(width / CalcTextSize("0").x)`: 8 at the default width (the page about 448 px, 62 characters; a row of 16 needs 71), 16 when widened; a line `AAAA  WW WW …  text`, the text column's `£` drawn only when `ImGui::GetFont()` has U+00A3 (`IsGlyphInFont`), else `.`. Go to: the row of `address`, scrolled to with `SetScrollY(row * lineHeight)`, its word outlined (`AddRect`) until another Go to; an address outside the chip is ignored.
**Code** (§8): the cached lines, `AAAA  BB BB BB  TEXT`, bytes padded to the longest (3 for the 6502, 4 for the Z80). Go to: the line whose range covers the address.
**Glyphs** (§9): `GlyphLayout(width, bits)`: 16, 8 or 4 glyphs a row, the most whose dot is at least 3 px, the dot then `floor(width / (columns × (bits + 2)))` (an 8-bit set at the default width: 8 a row, 5 px dots); each glyph a dark square with lit dots (`IM_COL32(255, 200, 80, 255)` on `IM_COL32(30, 28, 26, 255)`: a lit phosphor on the paper), its index below in small print. Hover: `PageTooltip("$05: words $028-$02F")`, the words the chip's own (the CPU address would mislead on a character ROM).

The Contents link is the page's only Board target; its hits go to `board.route` as the other pages'.

## 6. The views' logic, `src/view/memory_contents.{hpp,cpp}`

```cpp
enum class ContentsView : std::uint8_t { Hex, Code, Glyphs };

struct ContentsSettings {
    ContentsView view = ContentsView::Hex;
    std::uint16_t address = 0;  // of word 0 (§6)
    CharacterSet charset = CharacterSet::Ascii;
    Cpu cpu = Cpu::Mos6502;
    std::size_t codeFrom = 0, codeLength = 0;  // words; length 0: to the end
    std::size_t glyphFrom = 0;
    unsigned glyphRows = 8, glyphBits = 8;
    bool operator==(const ContentsSettings&) const = default;
};
// §10's first view and §6–9's defaults, from the Machine's file and the chip's size.
ContentsSettings DefaultSettings(const ContentsDefaults& defaults, std::string_view reference,
                                 unsigned bits);
std::string SizeText(std::size_t words, unsigned bits);  // "2K x 8", "512 x 5"
// UTF-8: one ASCII character, "£", or "." when none (§7).
std::string_view CharacterOf(CharacterSet set, std::uint8_t code);
int HexColumns(float characters);                         // 16, 8 or 4 that fit a row
struct GlyphGrid { int columns; float dot; };              // dot in px
GlyphGrid GlyphLayout(float width, unsigned bits);
std::string HexLine(std::span<const std::uint8_t> words, std::uint32_t address, std::size_t from,
                    int columns, CharacterSet set);       // one row
struct CodeLine {
    std::uint32_t address;  // the CPU's: settings.address + offset, on 16 bits
    std::size_t offset, length;
    std::string text;       // "LDA #$00", ".byte $FF"
};
std::vector<CodeLine> Disassembly(std::span<const std::uint8_t> words, const ContentsSettings& s);
// Lit dot of a glyph: row `row` (a word) of glyph `glyph` from glyphFrom, column 0 the leftmost
// (bit glyphBits − 1). False past the chip's end.
bool GlyphDot(std::span<const std::uint8_t> words, const ContentsSettings& s, std::size_t glyph,
              unsigned row, unsigned column);
std::size_t GlyphCount(std::size_t words, const ContentsSettings& s);
```

`DefaultSettings`: `address` the chip's (else 0); `charset` the Machine's; `cpu` the Machine's (else 6502); `glyphFrom` the chip's `glyphs` (else 0), `glyphRows` 8, `glyphBits` the word width; `view` Code when the chip has an address and the Machine a CPU, else Glyphs when it has `glyphs`, else Hex (§10).

`Disassembly`: from `codeFrom` to `codeFrom + codeLength` (or the end), each instruction from `Disassemble6502` or `DisassembleZ80` over the words left in the range, at `address + offset`; nullopt (undocumented, or past the range) gives `.byte $XX` for one word.

**Test** (`tests/view/memory_contents_test.cpp`, cases `contents: …`): `DefaultSettings` for the six Machines' chips (§ table: IC2 Code Z80 `$0000` glyphs `$0E00`; UA2 Glyphs 8 × 8; U15 Glyphs 8 × 5; U14 Hex); `CharacterOf` for each set's edges (§7's list); `HexColumns(62)` 8, `HexColumns(71)` 16; `GlyphLayout(434, 8)` 8 columns of 5 px; `HexLine` text; `Disassembly` over a few bytes (a branch's target, `.byte` at the range's end, `$FF` as `.byte`); `GlyphDot` on the real files: the 2513's `@` (word 1 `.###.`), UA2's `E` (screen code 5, the cell Odd E sticks), IC2's `K` at `$0E00 + $30 × 8`.

## 7. Disassembly, `src/asm`

`git mv tests/asm/z80_disassembler.{hpp,cpp} src/asm/`; its comment loses "test-only" but keeps why it decodes by fields (apart from the assembler's table, so the two check each other). `Z80DocumentedOpcodes` moves to `tests/asm/z80_opcodes.{hpp,cpp}` (test data, unchanged). `asmz80_test.cpp` includes both.

`src/asm/mos6502_disassembler.hpp`:

```cpp
// src/asm/disassembled.hpp, shared with DisassembleZ80 (its Z80Text renamed).
struct Disassembled { std::string text; std::size_t length; };
// mos6502_disassembler.hpp. The documented instruction at the start of `bytes` at address `at`: MOS syntax, upper case,
// $hex ("LDA ($FA),Y", "BNE $1C4F"), an absolute operand always 4 digits ("LDA $0034": real code
// uses absolute on zero page); nullopt for an undocumented opcode or too few bytes.
std::optional<Disassembled> Disassemble6502(std::span<const std::uint8_t> bytes, std::uint16_t at);
```

From `Mos6502Decode` (`documented`, the form) and the form's operand bytes (`asm6502.cpp:49`'s `OperandBytes`, moved beside `Mos6502Decode` as `Mos6502OperandBytes` in `mos6502_core.hpp` so both use it). The Z80 disassembler is unchanged but the type's name: its displacements stay decimal, as Zilog writes them (`(IX-3)`).

**Test** (`tests/asm/mos6502_disassembler_test.cpp`, cases `disassembler 6502: …`; the Z80 cases keep their `asm z80:` names in `asmz80_test.cpp`): for every documented opcode with operands `$34`/`$1234`, the text assembled back by `bw_asm` (`Assemble` on `.org $0200` plus the line) gives the same bytes; an undocumented one and a short buffer nullopt; a branch backwards. The Z80 tests run as today.

## 8. Flags (§ Tests)

`--contents REF[:hex|code|glyphs]`: the binder out at Contents on the chip `REF` (its component index from the scene), at that view (else its default), through `ManualWindows::openContents`. Refused (usage) when `REF` is not a memory chip on the Board. Settings beyond the view come from the defaults: the screenshots need no more (the BW-80's `$0038` is on the Code view's first screen).

## 9. Docs

- **User Manuals** (the six Machines): one sentence at the end of **Using it** (none mentions the binder today): "Click a memory chip's window to read what it holds: the binder opens at Contents, in hex, as code or as glyphs."
- **GLOSSARY**: Contents (the divider, the three views, what holds means); Equipment's binder sentence ("four dividers" → "four dividers, and Contents once a memory chip's window is clicked").
- **ADD_MACHINE.md**: a Machine with memory chips writes `roms/contents.txt` (its CPU, character set, each chip's address and glyphs); the test checks it.
- **ADD_PUZZLE.md**, under the memory Fault rules: "The binder's Contents page shows a ROM's stuck cell (P6p): a fault the views give away at a glance needs a symptom that already points there (BW-2001 `odd_e`, BW-1 `rom_test_fails`); a pin Fault does not show there."
- **ARCHITECTURE**: the P6p row done.

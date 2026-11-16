# P5x BW-80 (ZX80) — Detailed Design

Implements [BW80_MACHINE.md](BW80_MACHINE.md): the `MEMBRANEKB` keyboard and the code it shares with the BW-2001's, the `EXPANSION` connector, the Session, CLI and window changes, `bw place`'s `at` rule, and Machine 140, whose own design (netlist, Board, manuals, tests) is [BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md).

Recipes: [ADD_CONNECTOR_DEVICE.md](../recipes/ADD_CONNECTOR_DEVICE.md) (the chain from Part to window; a key matrix; passive connectors; typing into a slow Machine), [ADD_PART.md](../recipes/ADD_PART.md) (catalogue row, datasheet, footprint, `PartPinNames`), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md) (netlist, manuals, Board, tests). All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

Sources, by short name:

- **ZX80**: Grant Searle's ZX80 page (searle.x10host.com/zx80/zx80.html, plain http: its certificate does not cover the host), his redrawn issue 2 schematic (`zx80.pdf`, P5v's source), his "Circuit operation" (I/O decoder, keyboard), his issue 2 foil layout (`PCBFoils.zip`: the silk layout drawing `ZX80Layout_L.gif` and `drill.txt`, hole coordinates in mm from the board's bottom left) and his keyboard overlay (`Keyboards.gif`).
- **WIKI**: Sinclair Wiki, "ZX80/81 edge connector" (sinclair.wiki.zxnet.co.uk): the 2 × 23 pinout, ROMCS absent on the ZX80.
- **LISTING**: the ZX80 ROM listing (`external/roms/zx80/zx80_rom_source.txt`, P5v): `KEYBOARD` ($013F), the `MAIN-KEYS` table ($006C: 39 unshifted codes, 39 shifted), `TKN-TABLE` ($00BA), the editor's K-mode rule (the key handling just before `EDC-2`, $0344–$034B: a letter's code + $C0 is a keyword from $E6 up), `KEYBOARD`'s debounce (the shift counter in `RESULT_lo`, $0183–$0193), the keyboard drawing.
- **P5q**: [BW2001_MACHINE_DESIGN.md](../p5q-bw2001/BW2001_MACHINE_DESIGN.md) §6 (`MATRIXKB`: Part, Session, window, capture), §7 (`grid pet`).

---

## 1. The ZX80's keyboard, as the circuit sees it (ZX80, LISTING)

Forty keys in eight rows of five. A row is one of `A8`–`A15`; a column is one of `KB0`–`KB4`, pulled up by R13–R17 (47k) and buffered onto `D0`–`D4` by IC10 while `KBD_N` is low (an `IN` from an even port). Each row reaches the keys through a diode (D3–D10, cathode on the address line), so that two keys on one column (SHIFT and a letter) never join two address lines. `KEYBOARD` reads port `$FEFE`, then rotates the high byte (`$FD`, `$FB`, … `$7F`): one address line low at a time, the others high; a held key on that row reads 0 on its column. It takes a key when two consecutive frames read the same one, and accepts a new key only after five frames that read none (`RESULT_lo`, a shift counter halved on each no-key frame and set back to $0F while a key is down), so a typed press is held three frames and released six.

| Row | Line | `KB0` | `KB1` | `KB2` | `KB3` | `KB4` |
|---|---|---|---|---|---|---|
| 0 | `A8` | SHIFT | Z | X | C | V |
| 1 | `A9` | A | S | D | F | G |
| 2 | `A10` | Q | W | E | R | T |
| 3 | `A11` | 1 | 2 | 3 | 4 | 5 |
| 4 | `A12` | 0 | 9 | 8 | 7 | 6 |
| 5 | `A13` | P | O | I | U | Y |
| 6 | `A14` | NEW LINE | L | K | J | H |
| 7 | `A15` | SPACE | . | M | N | B |

(LISTING's `MAIN-KEYS` order, row by row, column 0 first; the drawing on the overlay runs the right half of each row the other way, 6–0 and Y–P.)

Each key's three legends (the overlay): the letter or digit; the shifted legend (yellow: a symbol, a graphic, THEN, TO, AND, OR, NOT, `**`, the four cursor arrows, HOME, RUBOUT, EDIT on NEW LINE, £ on SPACE); and for most letters a keyword above the key (white), typed by that key alone when the cursor is `K`: the letter's code + $C0 in `TKN-TABLE`. A: LIST, B: RETURN, C: CLS, D: DIM, E: SAVE, F: FOR, G: GO TO, H: POKE, I: INPUT, J: RANDOMISE, K: LET, N: NEXT, O: PRINT, Q: NEW, R: RUN, S: STOP, T: CONTINUE, U: IF, V: GO SUB, W: LOAD, X: CLEAR, Y: REM; L, M, P and Z have none. BREAK, in the keyword position above SPACE, is the SPACE key itself: a running program reads row `A15` and tests bit 0 without looking at SHIFT. Shifted symbols: Z `:`, X `;`, C `?`, V `/`, B OR, N `<`, M `>`, `.` `,`, SPACE £, H `**`, J `-`, K `+`, L `=`, NEW LINE EDIT, Y `"`, U `$`, I `(`, O `)`, P `*`, 1 NOT, 2 AND, 3 THEN, 4 TO, 5 ←, 6 ↓, 7 ↑, 8 →, 9 HOME, 0 RUBOUT; A, S, D, F, G, Q, W, E, R, T graphics. Functions (ABS, PEEK, CHR$ …) have no key on the ZX80: they are typed letter by letter and read at the syntax check.

The cursor is `K` where the ROM expects a command: at the start of a line (a line number typed there keeps it `K`) and after THEN; `L` otherwise.

## 2. `MEMBRANEKB` — the BW-80 keyboard

### 2.1 Shared code — `src/parts/key_matrix.{hpp,cpp}`, `src/parts/press_queue.{hpp,cpp}` (new), `src/parts/matrix_keyboard.{hpp,cpp}` (refactored)

- `struct MatrixPress { std::size_t key; bool shift; }` and the private-use characters for named keys move from `matrix_keyboard.hpp` to `key_matrix.hpp`; the PET's (`kKeyHome` … `kKeyRvs`) stay, and the ZX80's are added: `kKeyEdit`, `kKeyBreak`, `kKeyRubout` = `kKeyDel`, `kKeyNewLine` = `'\r'` (the four arrows and HOME are the PET's `kKeyUp`, `kKeyDown`, `kKeyLeft`, `kKeyRight`, `kKeyHome`).
- `class KeyMatrix`, an abstract interface a keyboard Part implements beside `Component`:

```cpp
enum class KeyMatrixKind : std::uint8_t { Bw2001, Bw80 };

// A keyboard of keys in a matrix, held from the UI or pressed from a queue in simulation time:
// the BW-2001's MATRIXKB (P5q), the BW-80's MEMBRANEKB (P5x). Keys are indexed as the Part's
// key table lists them.
class KeyMatrix {
public:
    virtual ~KeyMatrix() = default;
    virtual KeyMatrixKind kind() const = 0;          // Bw2001, Bw80: the look and the key map
    virtual std::size_t keyCount() const = 0;
    virtual std::size_t shiftKey() const = 0;        // held with a shifted press
    virtual Component& component() = 0;              // for wakes
    // Any thread; follow with a wake.
    virtual void setHeld(std::size_t key, bool held) = 0;
    virtual bool held(std::size_t key) const = 0;
    // One producer thread. The presses that type `text` (see presses), queued; false when
    // nothing was queued (no key for a character, the queue full). Follow with a wake.
    virtual bool type(std::u32string_view text) = 0;
    virtual void dropWaiting() = 0;
    // Any thread: the scripted press held now includes `key`, for drawing.
    virtual bool scripted(std::size_t key) const = 0;
};
```

  `type` takes text, not one character: the BW-80's tokenizer needs the words. `MatrixKeyboard::type(char32_t)` stays for its tests and `bw2001_rig`; its text form queues each character through `MatrixKeyFor`.
- `PressQueue` is `MatrixKeyboard`'s queue and phases, moved: `PressQueue(Time hold, Time gap)`; `bool push(MatrixPress)` (producer; false when `kMatrixWaiting` = 4096 wait); `dropWaiting()`; `void step(EvalContext&)` (the Hold/Gap timing, `nextAt_`, taking the next press when idle, `wakeAfter`); `std::optional<MatrixPress> current() const` (simulation thread: the press held now); `bool scripted(std::size_t key, std::size_t shift) const` (any thread, the atomic `showing_`); `void reset()` at power-on (the queue kept). `MatrixKeyboard` keeps its behaviour and its tests: 40 ms and 40 ms, `kMatrixHold`, `kMatrixGap`, `kMatrixWaiting` now in `press_queue.hpp`.
- Builder: `KeyMatrix* KeyMatrixOf(const LoadedCircuit&, std::string_view reference)` (`builder.hpp`, which includes `key_matrix.hpp`; beside `MatrixKeyboardOf`, which stays for `wireSwitches` and the rig): the Part whose name is `MATRIXKB` or `MEMBRANEKB`, cast to its class and up to `KeyMatrix`.
- Sources: `press_queue.cpp` in `src/parts/CMakeLists.txt` (`key_matrix.hpp` is all inline); `tests/parts/press_queue_test.cpp` in `tests/CMakeLists.txt`. `tests/machine/typing_test.cpp`'s refusal of `{` moves with the check (§4.2): the parser keeps the text and notes the character.
- `PressQueue::step` takes the time and returns the delay to wake after (no stub `EvalContext` is needed: its constructor is the Simulator's); the owner calls `ctx.wakeAfter`.

### 2.2 Key table and character map — `src/parts/zx80_keys.{hpp,cpp}` (new)

```cpp
// One key of the BW-80 keyboard: where it is in the matrix and what its cap says.
struct Zx80Key {
    std::uint8_t row;          // 0-7: A8-A15
    std::uint8_t column;       // 0-4: KB0-KB4
    std::string_view legend;   // "A", "NEW LINE", "SPACE", "SHIFT"
    std::string_view shifted;  // "" or the yellow legend: ":", "THEN", "\xe2\x86\x90" (arrows), "EDIT", "RUBOUT", "GRAPHIC"
    std::string_view keyword;  // "" or the white keyword above: "LIST"
};
std::span<const Zx80Key> Zx80Keys();                        // §1's table, row 0 column 0 first
std::optional<std::size_t> Zx80KeyAt(int row, int column);
inline constexpr std::size_t kZx80KeyCount = 40;

// The press that types `c`: letters (either case) and digits by their key; ' ', '.', '\n' or
// '\r' (NEW LINE); the shifted symbols with SHIFT (" $ ( ) * - + = : ; ? / < > , and the pound
// sign U+00A3); kKeyUp, Down, Left, Right, Home, Del (RUBOUT) and Edit with SHIFT; kKeyBreak the
// SPACE key alone; nullopt for anything else.
std::optional<MatrixPress> Zx80KeyFor(char32_t c);

// The presses that type `text` as the ZX80's editor reads it: a keyword where the cursor is K
// (the start of a line, after its number; after THEN) becomes its key alone; THEN, TO, AND, OR,
// NOT and ** become their SHIFT keys anywhere; every other character goes through Zx80KeyFor,
// characters with no key skipped. "GO TO" and "GOTO", "GO SUB" and "GOSUB", "RAND" and
// "RANDOMISE", "CONT" and "CONTINUE" are the same keyword.
std::vector<MatrixPress> Zx80Presses(std::u32string_view text);
```

The tokenizer is a small state machine over the text: `k = true` at the start and after each NEW LINE; inside a string (between quotes) every character is typed as it is; outside one a space is typed only between two plain words (never in `K` mode, after a keyword or SHIFT word, or before a SHIFT word: the editor spaces keywords itself); digits typed and `k` kept; a run of letters (in `K` mode with one inner space for `GO TO`, `GO SUB`) compared, when `k`, against the keywords: a match is one press and `k = false`; no match (a listing we do not understand): its letters typed, `k = false`; when not `k`, the run compared against the five SHIFT words, else typed letter by letter; after THEN, `k = true`. Everything else by `Zx80KeyFor`. It is pure and tested on its own.

### 2.3 Part — `src/parts/membrane_keyboard.{hpp,cpp}` (new)

`class MembraneKeyboard final : public Component, public KeyMatrix`. Pins (pin index = pad − 1): `A8`–`A15` (pads 1–8) `PinKind::Input`, `Family::Mos`, not sampled: a change wakes the Part, which must answer inside an `IN` (the row pattern is on `A8`–`A15` for the I/O cycle, about 1.2 µs; the Part answers in `kMembraneDelay` = 5 ns, a diode and a membrane contact); `KB0`–`KB4` (pads 9–13) `PinKind::OpenCollector`, `Family::Ttl`: a diode conducting pulls the column low, otherwise the pin drives nothing and the Board's pull-up holds it high (`DriveOf` for `OpenCollector`: `Zero` drives, anything else is `Z`).

`evaluate`: `queue_.step(ctx)`; then for each column c: low when some key k with `column == c` is held (`held_[k]`, or the scripted press and its SHIFT) and the input of its row reads `Zero` (an `X` or floating line does nothing: the diode needs a low); `ctx.drive(kColumn0 + c, low ? Zero : Z, kMembraneDelay)`. Cost: no key held, nothing is driven, so the only work is the evaluation itself at each change of `A8`–`A15` (a few per instruction). `powerOn`: `queue_.reset()`, outputs Z.

`type(text)`: `Zx80Presses(text)` pushed one by one, false when none was. Hold 60 ms, gap 120 ms (`kMembraneHold`, `kMembraneGap`: §1's debounce, three frames down, six up). `shiftKey()` = `*Zx80KeyAt(0, 0)`.

Catalogue row `PartInfo{"MEMBRANEKB", "BW-80 keyboard (8 x 5 membrane matrix)", "HDR-1x13", Connector, &CreateMembraneKeyboard}` (sorted after `MC3446`: `MATRIXKB`, `MC3446`, `MEMBRANEKB`, `MONITOR`), `CreateMembraneKeyboard` refusing parameters as `CreateMatrixKeyboard` does. `ConnectorPins("MEMBRANEKB")` gives the 13 pins (electrical netlist, `PartPinNames`, `RailVolts`, `FindPower`: no power pin, `ConnectorPowersDevice` false). Footprint `{"HDR-1x13", "Connector_PinHeader_2.54mm:PinHeader_1x13_P2.54mm_Vertical"}` in `kFootprints` (22 → 24 with §3), the file copied unchanged from KiCad 10's library into `boards/footprints/`. `datasheets/MEMBRANEKB.md`: the function (§1), the matrix table with the legends, the pins (a range expands only between names differing in one character: `A8`, `A9`, `A10`–`A15`). Sources: `zx80_keys.cpp`, `membrane_keyboard.cpp` in `src/parts/CMakeLists.txt`; the tests in `tests/CMakeLists.txt`.

Tests, `tests/parts/membrane_keyboard_test.cpp`: the row (kind, package, no power, 13 pins, `A8` pad 1 and `KB0` pad 9, `A8` not sampled, `KB0` open collector); a bench netlist of `J1 POWER`, `J3 MEMBRANEKB`, the five 47k pull-ups, and the eight rows on ties (`VCC` or `GND` from the case): a held key reads its column low with its row low, high with the row high and after release; SHIFT (row 0) and A (row 1) held with rows 0 low and 1 high: `KB0` low, `KB1` high, and the two row Nets keep their own values (the diodes: the bench's rows are rails, a join would be contention); a row left unconnected: a key on it does nothing; `type(U"A")` holds the key 60 ms then releases 60 ms, `type(U"\"")` holds SHIFT with Y; `dropWaiting`; `scripted`. `tests/parts/zx80_keys_test.cpp`: `Zx80Keys` (40, each row and column once, `Zx80KeyAt(6, 0)` NEW LINE), `Zx80KeyFor` on every key's characters and the named keys, `Zx80Presses` on `10 PRINT "HI"\n20 GO TO 10\n`, `IF A=1 THEN PRINT A**2\n`, `LET A=PEEK(16396)\n` (PEEK letter by letter), `GOTO`, lower case, a string keeping its spaces, `REM A B`, a word that is no keyword at a line start (typed as letters).

`tests/parts/matrix_keyboard_test.cpp`: unchanged but for the moved constants' header; `tests/parts/press_queue_test.cpp` for the queue alone (push, the phases on a stub `EvalContext`, drop, the full queue).

## 3. `EXPANSION` — the ZX80's edge connector

A passive connector as P5q's `USERPORT` (`connector.cpp`, its own table `kExpansion` since four pins are `NoConnect`): catalogue `PartInfo{"EXPANSION", "expansion bus edge connector (2 x 23)", "HDR-2x23", Connector, &CreateConnector<"EXPANSION">}` (sorted between `DIGITCC` and `IEEE488`), footprint `{"HDR-2x23", "Connector_PinHeader_2.54mm:PinHeader_2x23_P2.54mm_Vertical"}`. On the 2 × 23 header the ZX80's upper contact n is pad 2n − 1 and its lower contact n is pad 2n, so each pair faces as on the edge. Pin names, every one a sampled MOS `Input` but the unconnected ones `NoConnect` (WIKI, ZX80):

| n | upper (pad 2n − 1) | lower (pad 2n) |
|---|---|---|
| 1 | `D7` | `VCC5` |
| 2 | `RAMCS_N` | `NC9V` |
| 3 | `SLOTA` (NoConnect) | `SLOTB` (NoConnect) |
| 4 | `D0` | `GND1` |
| 5 | `D1` | `GND2` |
| 6 | `D2` | `PHI` |
| 7 | `D6` | `A0` |
| 8 | `D5` | `A1` |
| 9 | `D3` | `A2` |
| 10 | `D4` | `A3` |
| 11 | `INT_N` | `A15` |
| 12 | `NMI_N` | `A14` |
| 13 | `HALT_N` | `A13` |
| 14 | `MREQ_N` | `A12` |
| 15 | `IORQ_N` | `A11` |
| 16 | `RD_N` | `A10` |
| 17 | `WR_N` | `A9` |
| 18 | `BUSAK_N` | `A8` |
| 19 | `WAIT_N` | `A7` |
| 20 | `BUSRQ_N` | `A6` |
| 21 | `RESET_N` | `A5` |
| 22 | `M1_N` | `A4` |
| 23 | `RFSH_N` | `NC23` (NoConnect: the ZX81's ROMCS) |

`D0`–`D7` are the memory side (`MD0`–`MD7` in the netlist, the ZX80's D0'–D7'); `RAMCS_N` is `RAM_CS_N`, the far side of R19 (an expansion pulls it high to disable the inner RAM); `PHI` is the netlist's `PHI_N` (§ Spec, decided); `NC9V` the unregulated supply, which the Board does not have. `VCC5` and the grounds are plain Inputs on `VCC` and `GND`, as the PET connectors' (a `Power` pin needs `PowerPinRail` rows and breaks the builder: P5q's notes). `datasheets/EXPANSION.md` with the table and the ZX80's convention (contacts numbered from the right, looking into the back); its address range written `A4`–`A9`, `A10` … `A15` (the checker expands ascending one-character ranges only).

Tests: `tests/parts/connector_test.cpp` gains a case as `USERPORT`'s (46 pins, `D7` pad 1, `VCC5` pad 2, `A0` pad 14, `RFSH_N` pad 45, the four NoConnects, no power pin, sampled, `!ConnectorPowersDevice`).

## 4. Session, CLI, window

### 4.1 Session — `src/session/session.{hpp,cpp}`

- `matrixKeyboards()` lists every `MATRIXKB` and `MEMBRANEKB` Part, netlist order; `KeyMatrixKind matrixKind(reference)` (the UI's look and key map).
- `type(reference, char32_t)` on a key matrix becomes `matrix->type(std::u32string_view{&c, 1})`; new `bool typeText(reference, std::u32string_view text)`: a key matrix takes the whole text (the tokenizer sees the words); a `KEYBOARD` gets it character by character through `typeCode` into its own queue (32 keys: a paste longer than that is cut, as the ASCII window's is). The matrix capture's paste and `--type` on a key matrix use `typeText`; `--type` on a `KEYBOARD` keeps its per-key edges (`Typing::keys`, `kTypingInterval`).
- `setMatrixKeyHeld`, `matrixKeyHeld`, `matrixKeyDown`, `powerOff` work on `KeyMatrixOf` (`keyCount()` for the bound, `component().id()` for the wake, `scripted`).

### 4.2 CLI — `src/machine/typing.{hpp,cpp}`, `src/cli/main.cpp`, `src/ui/main.cpp`

`Typing` keeps the text it was parsed from (`std::u32string text`, the escapes applied) beside `keys`, and `untypable`, the first character with no `KeyboardCode` (which `keys` skips); the refusal moves to where a `KEYBOARD` is the target: `bw run` and `bw_ui` say "cannot type the text on 'J1', a keyboard". `bw run`: a `KeyMatrixOf` target queues `typing.text` at `typing.start` in one edge (as the `MATRIXKB` branch did); a `KEYBOARD` keeps one edge a key. `bw_ui`: `session.typeText` once at `typing.start` for a matrix. The reference check accepts any `matrixKeyboards()` entry (already).

### 4.3 Capture — `src/ui/matrix_capture.{hpp,cpp}`

`MatrixCapture` gains `KeyMatrixKind kind` (set with `reference` from `session.matrixKind`). `send` maps a character to a press by kind (`MatrixKeyFor` or `Zx80KeyFor`) for the drawn cap. `handle`'s special keys by kind: Bw2001 as today; Bw80: Return `'\r'`, Shift+Return `kKeyEdit`, Backspace `kKeyDel` (RUBOUT), Escape `kKeyBreak` (SPACE), Home `kKeyHome`, the arrows the cursor keys. Text input goes through `session.type` as today (one character); paste (`Cmd`/`Ctrl+V`) decodes the clipboard's UTF-8 into a `std::u32string` (so £ types) and calls `session.typeText` once.

### 4.4 Window — `src/view/matrix_keyboard_layout.{hpp,cpp}`, `src/ui/matrix_keyboard_window.{hpp,cpp}`

- Layout: `std::span<const MatrixCap> Zx80Caps()`: four rows of ten keys, each 1u, at the ZX80 overlay's offsets (row 0 the digits at x = 0, row 1 Q–P at 0.25, row 2 A–NEW LINE at 0.5, row 3 SHIFT–SPACE at 0.15: the overlay's stagger), 1.1u between rows to leave room for the keywords; `kZx80KeyboardWidth` = 10.5u, `kZx80KeyboardRows` = 4. The test (`matrix_keyboard_layout_test.cpp`): every key once, inside the box, no overlap, for both layouts.
- Window: `draw` picks the look from `capture.kind`. Bw80: the ZX80's membrane (ZX80's overlay, memory: a drawn device, tuned on screenshots): a black panel with a thin light line along its top, blue caps with rounded corners (`#1D1DEA`-like blue, darker at the bottom), the legend in white at the cap's lower left, the shifted legend in yellow at the upper right (graphics as a small 2 × 2 mosaic), the keyword in white centred above the cap (yellow for NOT, AND, THEN, TO, which are shifted; EDIT above NEW LINE and BREAK above SPACE in yellow and blue as the overlay), "BW-80" engraved at the left; a held cap drawn darker and flat (a membrane does not travel). The caps come from `Zx80Caps()` and `Zx80Keys()`; the click, right-click latch and `session.setMatrixKeyHeld` handling is the same loop as today, over the kind's caps and keys. The window's title stays "Keyboard J3"; F2 focuses it as for the BW-2001.
- `src/ui/main.cpp`: nothing new beyond the kind (`matrixCapture.kind = session.matrixKind(reference)`).

## 5. `bw place`: `size W H`, `at REF X Y` — `src/board/place_rules.{hpp,cpp}`, `placer.cpp`

- Rules: `size W H`, the Board's outline in millimetres (width across, depth from the back edge to the front), and `at REF X Y`, a part's courtyard centre in millimetres from the Board's back-left corner, y toward the front (KiCad's footprint origin is pad 1, so the placer puts the origin at the target minus the courtyard's midpoint, as `putInCell` centres a chip); any package; the part upright (0°: a DIP standing, pin 1 at the top left, as the chip grid places chips). `std::optional<std::pair<double, double>> size` and `struct AtRule { std::string reference; double x, y; }` (in the `PlaceRule` variant) on `PlaceRules`; errors: a bad number, `at` without `size`, `size` with `grid pet` or given twice, a reference given twice (with any rule).
- Placer: a rules file with `size` takes the fixed-layout path (`runPet` generalised to `runFixed`: `petParts_` become `fixedParts_` with a position each, from the PET cell or from the rule). Under `size` the outline is the size given, from `kOrigin`; the edge groups are placed inside it with `placeGroup` as today (no widening: a group that does not fit is an error); `next` beside a fixed part (`placeDeferred`), free parts to the nearest empty place by their Nets (`placeFree`, which today looks for empty PET cells: under `size` it searches a 2.54 mm grid of the outline, inside the bare margin, for the nearest spot whose courtyard overlaps nothing, to the centroid of the parts sharing the part's Nets); a courtyard leaving the outline is an error naming the part; `checkOverlaps` as now. A pin header on the back or front edge lies along it (today's `grid pet` rule, now for both fixed layouts). `disc` stays a package that needs a rule (ADD_PART), as today.
- Tests (`tests/board/place_rules_test.cpp`, `placer_test.cpp`): parse (`size 155.5 142`, `at IC1 64.0 66.2`; a bad number; twice; `at` without `size`); a DIP's courtyard centre at its place (within 0.01 mm), the outline the size given, `next` beside it, a free resistor near its chip, a header on the back edge lying along it, two `at` parts overlapping refused, a part off the Board refused, `size` with `grid pet` refused.
- `BOARD_PLACEMENT.md`'s grammar list gets the rules (a P4k doc: history, so one line pointing here) and ADD_MACHINE §3 the use (a real board's layout from its hole coordinates).

## 6. Machine 140 BW-80

[BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md): the netlist's additions (`J3 MEMBRANEKB` on `A8`–`A15` and `KB0`–`KB4`, `J4 EXPANSION`, `IC1.BUSAK_N` joined), the Board's rules file (every chip `at` Searle's measured position), the manuals, the tests and the Session case. The ZX80's I/O decoder and keyboard buffer are P5v's already.

## 7. Tests at a glance

| File | Cases |
|---|---|
| `tests/parts/press_queue_test.cpp` ("press queue: …") | push, phases, drop, full |
| `tests/parts/matrix_keyboard_test.cpp` | unchanged |
| `tests/parts/zx80_keys_test.cpp` ("zx80 keys: …") | table, map, tokenizer |
| `tests/parts/membrane_keyboard_test.cpp` ("membrane keyboard: …") | row, bench, diodes, typing |
| `tests/parts/connector_test.cpp` | `EXPANSION` |
| `tests/view/matrix_keyboard_layout_test.cpp` | `Zx80Caps` |
| `tests/board/place_rules_test.cpp`, `placer_test.cpp` | `size`, `at` |
| `tests/session/session_test.cpp` | `session: bw80 with every instrument`; `session: typeText on a MEMBRANEKB` (a bench: the presses queued, drawn, dropped at power-off) |
| `tests/machines/bw80_test.cpp`, `bw80_screen.cpp` (registered in `tests/CMakeLists.txt` as `bw2001_screen.cpp` is) | the Machine ([BW80_DESIGN.md](../machines/140-bw80/BW80_DESIGN.md#tests)) |
| `tests/machines/manuals_test.cpp`, `tests/board/boards_test.cpp`, `tests/machine/electrical_test.cpp`, `tests/view/board_scene_test.cpp` | `"bw80"` in the per-Machine loops |

Shared code (the kernel's drive kinds untouched; the Session, CLI, window, placer): the full debug suite at the end. ctest names are the Catch2 case names: the fences match on their words ("press queue", "zx80 keys", "membrane keyboard").

## 8. Docs and release

- Recipes: ADD_CONNECTOR_DEVICE (a key matrix that drives its columns; `KeyMatrix` and `PressQueue` for a new keyboard; a paste tokenizer; the window's look by kind); ADD_MACHINE §3 (`size` and `at` for a real board's layout, from a hole table; a 2 × N header's pads for a two-sided edge); ADD_PART (a datasheet's pin ranges expand only between names one character apart).
- GLOSSARY: **Membrane Keyboard**, **Key Matrix** (if missing), **Keyword Entry**.
- GAME_DESIGN: ladder row 140 (BW-80: TV, ZX80 keyboard floating, the Z80 making the picture).
- ARCHITECTURE: the P5x row done with the date, the measured ratios and the links; the Machine list if any.
- Release: `bw run netlists/bw80.bwn --time 2s` (median of 3; P5v: 0.245×); `bw_ui netlists/bw80.bwn` on the Board with every instrument (32 Clips on `A0`–`A15`, `D0`–`D7`, the strobes; 4 Hooks; probe; meter): ratio and UI ms; `--type J3:1s:"PRINT 2+2\n"` screenshots at 1 s and 3 s viewed (the `K` cursor, then `4` and `0/-2`), the keyboard window viewed and tuned.

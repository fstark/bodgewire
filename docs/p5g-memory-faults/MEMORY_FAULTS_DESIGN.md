# P5g Memory Faults — Detailed Design

Implements [MEMORY_FAULTS.md](MEMORY_FAULTS.md): stuck cells and stuck columns in the 2114 and the ROMs, two Puzzle lines, tests on the 110 LED computer and the 080 message display. No new Machine, no Puzzle file, no UI change.

Recipes: [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md) (Fault lines and tests, extended in §7). All code follows [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Memory cells — `src/parts/memory_cells.hpp` (new)

```cpp
// A memory Part whose cells a Fault can stick (ARCHITECTURE, Faults: internal faults): the 2114
// and the ROMs. Words are the chip's own addresses (its A pins, bit 0 first); bits are its data
// Pins, bit 0 first.
class MemoryCells {
public:
    virtual ~MemoryCells() = default;
    virtual std::size_t wordCount() const = 0;
    // Bit of a word stored for data Pin `pin`; nullopt if `pin` is not a data Pin.
    virtual std::optional<unsigned> dataBit(PinIndex pin) const = 0;
    // From now on bit `bit` of word `word` reads `one`, whatever is written or powered on.
    virtual void stick(std::size_t word, unsigned bit, bool one) = 0;
};
```

Header only (`<cstddef>`, `<optional>`, `sim/ids.hpp` for `PinIndex`). It sits in `parts`, not in the kernel: the Simulator never sees it. A column is `stick` on every word: from outside it cannot be told apart from a stuck sense amplifier, because every read of that bit gives the stuck value and nothing else reads the cells.

## 2. 2114 — `src/parts/ram2114.{hpp,cpp}`

`class Ram2114 final : public Component, public MemoryCells`.

```cpp
std::size_t wordCount() const override { return words_.size(); }
std::optional<unsigned> dataBit(PinIndex pin) const override;  // index in kData
void stick(std::size_t word, unsigned bit, bool one) override;  // Check()s word < 1024, bit < 4
private:
    void store(std::size_t address, unsigned data);  // words_[a] = data with the stuck bits forced
    std::array<std::uint8_t, 1024> words_{}, stuckMask_{}, stuckValue_{};
```

- `store`: `words_[a] = static_cast<std::uint8_t>(((data & ~stuckMask_[a]) | stuckValue_[a]) & 0xF)` (parenthesised: `-Wbitwise-op-parentheses` is an error). Both write paths in `evaluate` (the held write and the write in progress) go through it.
- `powerOn`: random contents as today, then `store(a, words_[a])` for every word. The random draws are unchanged, so a fault-free run keeps its seed behaviour.
- `stick`: sets the mask bit and the value bit, and forces `words_[word]` at once, so `word()` shows it before power-on too.
- Cost: two byte loads per write; reads are unchanged.

## 3. ROM — `src/parts/rom.{hpp,cpp}`

`class Rom final : public Component, public MemoryCells`.

- `wordCount()`: `words_.size()`.
- `dataBit(pin)`: index of `pin` in `spec_.data`.
- `stick`: `Check`s, then sets or clears the bit in `words_[word]`. The specialised `EvaluateRom` reads `words_` as before, so its X handling (`KnownBits`) sees the faulty contents. No runtime cost.
- `CreateRom` reads the file at every build into a fresh vector (catalogue.cpp), so sticking one build's ROM never touches another build.

## 4. Builder — `src/netlist/builder.{hpp,cpp}`

```cpp
// Stuck cell Fault inside a memory chip (MemoryCells): bit `pin` (a data Pin) of word `word`, or
// of every word (a stuck column), reads `value` (Zero or One).
struct StuckCell {
    std::string reference;
    PinIndex pin;
    std::optional<std::size_t> word;  // nullopt: every word
    LogicValue value;
};

LoadResult BuildCircuit(const Netlist& netlist, std::span<const StuckPin> stuck = {},
                        std::span<const std::string> unpowered = {},
                        std::span<const StuckCell> cells = {});
```

`Builder` keeps `cells_`. In `build()`, after the stuck-pin `Check` loop (errors already returned), for each cell:

```cpp
const auto it = loaded_->components.find(c.reference);
auto* memory = it == loaded_->components.end() ? nullptr : dynamic_cast<MemoryCells*>(it->second);
const auto bit = memory ? memory->dataBit(c.pin) : std::nullopt;
Check(bit && (!c.word || *c.word < memory->wordCount()) &&
          (c.value == LogicValue::Zero || c.value == LogicValue::One),
      "builder: stuck cell must be a data bit of a memory word");
for (word in c.word ? {*c.word} : [0, wordCount())) memory->stick(word, *bit, c.value == One);
```

The Machine validates first (§6), so the `Check` only guards misuse. An Unpowered memory is stuck too: harmless, it drives nothing.

## 5. Puzzle syntax — `src/machine/puzzle.{hpp,cpp}`

```cpp
struct CellDecl {
    std::uint32_t line;
    PinRef pin;                         // the data pin, by number or name: U3.IO2, U3.11
    std::optional<std::uint32_t> word;  // nullopt: `fault column`
    LogicValue value;                   // Zero or One
};
// Puzzle:
    std::vector<CellDecl> cells;  // stuck cells and columns, in file order
```

- `fault cell REF.PIN WORD 0|1`: exactly 5 tokens, `ParsePin`, value `0`/`1`, else `"expected: fault cell REF.PIN WORD 0|1"`. WORD: `$` followed by 1–8 hex digits (either case), or 1–9 decimal digits; else `"bad word 'WORD'"`.
- `fault column REF.PIN 0|1`: exactly 4 tokens, else `"expected: fault column REF.PIN 0|1"`.
- The catch-all becomes `"expected: fault stuck|crack|bridge|joint|cell|column ..."` (the existing `puzzle_test` strings change with it).

## 6. Machine — `src/machine/machine.{hpp,cpp}`

New private member `std::vector<StuckCell> cells_;`.

`LoadMachine`, after the joints, for each `CellDecl`. Here `written` is `"cell U3.IO2 $1FF"` or `"column U3.IO2"`, with the pin and word as given:

- `known(line, pin)` (unknown reference, no such pin: its existing messages);
- `designCircuit_->components.find` (a resistor passes `known()` but has no Component), then `dynamic_cast<const MemoryCells*>`; either missing → `written + ": RAM and ROM chips only"`;
- `FindPin` then `dataBit`; nullopt → `written + ": not a data pin"`;
- `word >= wordCount()` → `written + ": U3 (2114) has 1024 words"`;
- stored as `{reference, pin index, word, value}`.

`swap(reference)` also erases that chip's `cells_`. `build()` passes `cells_` as the fourth argument. A cell Fault is not shown anywhere: `board()`, `probeNet` and `connected` do not change.

Internal address line: `fault stuck U3.A0 0` already loads and builds (P4h stuck input). Nothing changes.

## 7. Docs

- [ADD_PUZZLE.md](../recipes/ADD_PUZZLE.md), Faults list: `fault cell U3.IO2 $1FF 0` (the chip's own word address) and `fault column U15.O1 0`, repaired by swapping; an internal address line written as a stuck input (`fault stuck U3.A0 0`). Design rules: a memory Fault belongs where the Machine shows the memory (character or message ROM, video RAM); in program memory it mostly crashes the program, which only tells the player "swap".
- ADD_PUZZLE.md, Tests: a Fault can be tested before its Puzzle file exists with `InlinePuzzle("machine …\nfault …")`.
- [GLOSSARY.md](../GLOSSARY.md), after Stuck-at Fault: **Stuck Cell** (one bit of one word of a memory chip always reads 0 or 1, whatever is written; a **Stuck Column** is that bit in every word; repaired by a swap).
- [GAME_DESIGN.md](../GAME_DESIGN.md): under Puzzles, flakiness as a Symptom class with its causes (intermittent contacts, thermal chips, timing margins, floating or contended lines, flaky memory cells); under Open questions, the cost of a swap (soldered or socketed chips, spares, time).
- [ARCHITECTURE.md](../ARCHITECTURE.md): the P5g row at the end; the P5h row's memory candidates become "a stuck cell or column, an internal address line" (the wrong IC dropped); a new last row **P5n Flaky machines**: weak and flaky cells, coupling, read disturb, intermittent contacts, thermal chips, RAM test ROMs; after the computers, so video RAM can show them.
- The 110 and 080 Machine docs list the memory candidates in their Puzzle notes.

## 8. Tests

`tests/parts/ram2114_test.cpp`:

- "2114: a stuck cell holds its value through writes and power-on": stick `$123` bit 2 at 1, then for seeds 1–4 power on and read `$123`. Bit 2 is 1. Write `0x0` through the pins, read back `0x4`. Word `$124` reads back what was written.
- "2114: a stuck column sticks every word": stick bit 0 at 0 on every word, write `0xF` to two words, read `0xE` from both.

`tests/parts/rom_test.cpp`:

- "rom: stuck cell and column": a 2716 from a test contents vector, stick word 5 bit 3 at 0, drive address 5, read `O3` = 0 and the other outputs as the contents; `dataBit` of `O3` is 3, of `A0` nullopt; `wordCount` 2048 (`size()`). A 2513 with `O1` stuck at 0 in every word: `dataBit(O1) == 0`, two addresses read `O1` = 0.

`tests/netlist/builder_test.cpp`:

- "builder: stuck cells reach the memory": a small netlist with a 2114, `StuckCell{"U1", IO1 index, 7, One}` and a column; `static_cast<Ram2114&>` `word(7)` has bit 0 set, every word the column bit.

`tests/machine/puzzle_test.cpp`:

- "puzzle: cell and column": `fault cell U3.IO2 $1FF 0`, `fault cell U3.11 511 1`, `fault column U15.O1 0` parsed (pin, word `0x1FF`, 511, nullopt, values). Errors: missing value, `$`, `$1G`, `12a`, `$123456789` (too long), a column with a word.

`tests/machine/machine_test.cpp` (on `led_computer`):

- "machine: memory fault errors": `U99`, `U3.XX`, `U5.A` (74LS138: `"cell U5.A $0: RAM and ROM chips only"`), `U3.A0` (`"column U3.A0: not a data pin"`), `U3.IO1 $400` (`"cell U3.IO1 $400: U3 (2114) has 1024 words"`).
- "machine: a swap removes the chip's memory Faults": cells on `U3` and `U4`; after `swap("U3")` a build's `U3` word is fault-free and `U4`'s still stuck.

Inline Puzzles: `tests/machine/test_points.{hpp,cpp}` gains `Machine InlinePuzzle(std::string_view text)` (`ParsePuzzle` + `LoadMachine` with the repository root, `REQUIRE`d), used by both Machine test files; no Puzzle file is needed.

`tests/machines/led_computer_test.cpp`: the `Run` helper takes a built `LoadedCircuit` (default: the fault-free Machine's):

- "led_computer: a stuck stack cell breaks the return" `[slow]`: `fault cell U4.IO4 $1FF 0` (bit 7 of the high byte of `JSR`'s return address at `$F811`: `$F8` → `$78`). Nothing answers at `$7814`; the undriven data bus reads `$FF` (TTL inputs of `U7` on it), the 6502 runs `$FF` (ISC abs,X, its writes to `$FFFF`) up to `$E000`, the ROM mirror of `$F800`, and the program restarts about 62 ms later. For seeds 1–3: every latch write before `kPowerOnReset + 2 * kStep` shows `0x01`. After `swap("U4")`, the first three writes show `0x01 0x02 0x04`.
- "led_computer: an internal address line darkens the LEDs" `[slow]`: `fault stuck U3.A0 0`. `sta dir` writes `U3` word 0, clearing `light`; `JSR`'s `$F8` and `$13` share `U3` word `$1FE`, so `RTS` returns to `$F314`, erased ROM (`$FF`), which runs to `$F800`. For seeds 1–3: the first three latch writes show `0x00`. After `swap("U3")`, the first three show `0x01 0x02 0x04`.

`tests/machines/message_test.cpp`: `PageFault` gains a font override, `int fontWord = -1; unsigned fontAnd = 0x1F;` applied to the 2513 word `fontWord` (`code * 8 + glyphLine`):

- "message: a stuck column in the character ROM" `[slow]`: `fault column U15.O1 0` → `CheckFaultyPage(machine, {.dotsMask = 0x1E})` (the `thin_letters` page, made inside the ROM); `swap("U15")` → `CheckRepaired`.
- "message: a stuck cell in the character ROM" `[slow]`: the top bar of `E` (code 5, glyph line 1: line 0 is blank in every glyph) is word `$29` = `#####`; `fault cell U15.O3 $29 0` takes out its middle dot → `CheckFaultyPage` with `{.fontWord = 0x29, .fontAnd = 0x1B}`; `swap("U15")` → `CheckRepaired`.

## 9. Release check

- `bw run` headless ratio of `netlists/led_computer.bwn` and `netlists/bus_load.bwn` (2114 writes) before and after phase 1: unchanged within noise.
- `bw_ui` release: an uncommitted `puzzles/p5g_check.bwp` (message display, the stuck cell of §8). Screenshot after 1 s, viewed: the letter lacks its dot everywhere it appears. The file is deleted after the check.

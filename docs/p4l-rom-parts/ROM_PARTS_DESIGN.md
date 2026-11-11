# P4l ROM Parts — Detailed Design

Implements [ROM_PARTS.md](ROM_PARTS.md): the ROM contents format, the 2716 and 2513 ROM Parts, the 74LS165, the DIP-24 package, contents paths, and the tests of the message display. The Machine's netlist, ROM files, Board and manuals are in [MESSAGE_DESIGN.md](../machines/080-message/MESSAGE_DESIGN.md).

Recipes followed: [ADD_PART.md](../recipes/ADD_PART.md) (74LS165; the ROMs, which are a new kind of Part family, §3), [ADD_MACHINE.md](../recipes/ADD_MACHINE.md). This design gives only what those recipes leave open.

All code must follow [STYLE.md](../STYLE.md) and [NAMING.md](../NAMING.md).

---

## 1. Directory Layout

```
src/parts/rom_contents.hpp, rom_contents.cpp   contents format: parse a .rom file
src/parts/rom.hpp, rom.cpp                     ROM Component and its specs (2716, 2513)
src/parts/ls165.hpp, ls165.cpp                 74LS165 Component
datasheets/2716.md, 2513.md, 74LS165.md
boards/footprints/DIP-24_W15.24mm.kicad_mod    vendored, KiCad 10.0.7, unchanged
roms/message.rom, roms/2513.rom                Machine contents (Machine design)
tests/parts/rom_contents_test.cpp, rom_test.cpp, ls165_test.cpp
tests/machines/message_test.cpp
netlists/message.bwn, manuals/message/, boards/message/ (Machine design)
```

Everything else is small changes to existing files (§7).

## 2. Contents format

```cpp
// src/parts/rom_contents.hpp
// The words of a ROM of `size` words of `width` bits (1-8), from a contents file's text
// (ROM_PARTS.md, New code 1). Unwritten words are all ones. Errors: "<file> line N: <what>".
std::expected<std::vector<std::uint8_t>, std::string>
ParseRomContents(std::string_view text, std::string_view file, std::size_t size, int width);

// The same from a file; "cannot read '<path>'" if it cannot be opened. Errors name the file's
// filename only.
std::expected<std::vector<std::uint8_t>, std::string>
ReadRomContents(const std::filesystem::path& path, std::size_t size, int width);
```

- Line by line; `;` outside a string starts a comment to the end of the line. Tokens are separated by spaces and tabs, except inside a string.
- Token kinds, by their first character: `@` address (1–4 hex digits after it); `"` string up to the next unescaped `"` on the same line (`\"`, `\\`; any other `\` an error; characters 0x20–0x7E only); `.` or `#` a dot row (only `.` and `#`, at most `width` long, value MSB first); otherwise a hex word, 1–2 digits, optionally `*N` with N decimal ≥ 1.
- Error texts (each prefixed "`<file>` line N: "): "unknown token 'xyz'", "bad address 'xyz'", "word 0xNN is wider than W bits" (also for string characters and dot rows longer than `width`: "dot row 'xyz' is wider than W bits"), "address 0xNNN is past the end (size 0xNNN)", "address 0xNNN already written at line M", "unterminated string", "bad escape '\\x'".
- Written-twice tracking uses a `std::vector<std::uint32_t>` of lines per word during parsing only.

## 3. ROM Parts

A family of Parts sharing one class, as the gate packages share `GatePackage` ([gate_package.hpp](../../src/parts/gate_package.hpp)): a `RomSpec` table row per Part.

```cpp
// src/parts/rom.hpp
struct RomEnable {
    PinIndex pin;  // active low: every enable low drives the outputs, else they float
    Time delay;    // enable to output, ps
};

struct RomSpec {
    std::string_view part;                     // "2716"
    std::span<const std::string_view> names;   // index = pin - 1
    std::span<const PinIndex> address;         // bit 0 first
    std::span<const Time> addressDelay;        // per address bit, ps
    std::span<const PinIndex> data;            // bit 0 first, at most 8
    std::span<const RomEnable> enables;
    std::span<const PinIndex> otherInputs;     // inputs with no function in a read ("VPP")
    Time offDelay;                             // enable high to outputs off, ps
};

const RomSpec* FindRomSpec(std::string_view part);  // nullptr unless a ROM Part
const RomSpec& RomSpecOf(std::string_view part);     // Check()s

// Read-only memory: the word at the address on the data Pins while enabled (ROM_PARTS.md).
class Rom final : public Component {
public:
    Rom(std::string reference, const RomSpec& spec, std::vector<std::uint8_t> words);
    void evaluate(EvalContext& ctx) override;
    std::size_t size() const { return words_.size(); }  // 2^address bits

private:
    const RomSpec& spec_;
    std::vector<std::uint8_t> words_;
};
```

- Pin kinds from the spec: `VCC` and `GND` `Power`; address, enable and other inputs `Input` (TTL); data `Output` (TTL; driving `Z` floats them, `DriveOf`); every other name `NoConnect` (`VGG`, `VDD`, `NCn`). All TTL: both chips have TTL-compatible inputs and outputs.
- `evaluate` (no allocation):
  - Enabled: every enable reads 0; any 1 → off; else (an X) unknown.
  - Address: the bits read now; with k X bits (k ≤ 8) every value they could have is looked up and a data bit is known where all words agree, X otherwise; k > 8 gives every data bit X.
  - Output per data bit: off → `Z`; unknown enable → X; else the bit (or X).
  - Delay: the largest `addressDelay` of the address pins that changed now (`read` ≠ `readBefore`); else, if an enable changed, `offDelay` when going off, else the largest delay of the enables that changed; else (first evaluation) the largest address delay. Every data pin is driven each evaluation (inertial: an unchanged value schedules nothing). Known simplification: the delay counts from the latest change, so a row bit changing just after a character bit shows the new word 450 ns after the row change, earlier than 500 ns after the character change; harmless on 080, where the row changes at a line end, never just after a character.
- No `powerOn`: no state; the first evaluation drives every output.
- Specs (`src/parts/rom.cpp`), delays in `src/parts/delays.hpp` as `Time` constants:

| Part | Pins (1–24) | Address | Data | Enables | Delays (ns) |
|---|---|---|---|---|---|
| `2716` | `A7` `A6` `A5` `A4` `A3` `A2` `A1` `A0` `O0` `O1` `O2` `GND` `O3` `O4` `O5` `O6` `O7` `CE_N` `A10` `OE_N` `VPP` `A9` `A8` `VCC` | `A0`–`A10` | `O0`–`O7` | `CE_N` 450, `OE_N` 120 | address 450 (`k2716Access`), off 100 (`k2716Float`) |
| `2513` | `VGG` `NC2` `NC3` `O1` `O2` `O3` `O4` `O5` `NC9` `GND` `CE_N` `VDD` `NC13` `A1` `A2` `A3` `A4` `A5` `A6` `A7` `A8` `A9` `NC23` `VCC` | `A1`–`A9` | `O1`–`O5` | `CE_N` 150 | `A1`–`A3` 450 (`k2513RowAccess`), `A4`–`A9` 500 (`k2513CharacterAccess`), off 150 |

  `2716` `otherInputs` = `VPP`. Sources: Intel 2716 (maximums, no typicals published; known values, the PDF not fetched); Signetics 2513, 1972 MOS data book pp. 65–67 (typicals; `t_CE` 150 typical used for off too).
- Catalogue (`src/parts/catalogue.cpp`): a factory template

  ```cpp
  template <PartName Name>
  std::expected<Component*, std::string> CreateRom(Circuit& circuit, std::string reference,
                                                   std::span<const PartParam> params);
  ```

  Only `contents` is accepted ("2716: unknown parameter 'x'"); missing: "2716 needs contents=<file>"; the words come from `ReadRomContents(param.value, 1 << address bits, data bits)`, its error prefixed "2716: ". Rows, sorted by name (digits sort before `74LS`): `PartInfo{"2513", "64-character 5 x 7 character generator ROM", "DIP-24", Chip, &CreateRom<"2513">}`, `PartInfo{"2716", "2K x 8 EPROM", "DIP-24", Chip, &CreateRom<"2716">}`.
- Datasheets, in the shape of `74LS157.md`:
  - Pin names for the manual checks: `PartPinNames` ([manual.cpp](../../src/manual/manual.cpp)) creates each Part without parameters, which a ROM refuses; it takes a ROM's names from `FindRomSpec(part)->names` instead, so `CheckDatasheet` covers them.
  - `2716.md`: Function (2048 bytes kept with the power off; the byte at `A0`–`A10` appears on `O0`–`O7` about 450 ns after the address settles, while `CE_N` and `OE_N` are both low; otherwise the outputs are off (floating), so several memories can share a bus; `VPP` is the programming voltage, tied to `VCC` to read). Pins: `` `A0`–`A9` ``, `` `A10` `` on its own (a range expands only between names of one length), `` `O0`–`O7` ``, `CE_N`, `OE_N`, `VPP`, `VCC`, `GND`.
  - `2513.md`: Function (a character generator: 64 characters, each 8 lines of 5 dots; `A4`–`A9` pick the character (the low 6 bits of its ASCII code, upper case), `A1`–`A3` the line, line 0 blank; `O5` is the leftmost dot, high for a lit dot; about 450 ns after a line change, 500 ns after a character change; `CE_N` low to drive the outputs, else they float. The real chip also needs −5 V (`VDD`) and −12 V (`VGG`); on these Boards those supplies are not modelled and the pins are left unconnected). Pins.

## 4. 74LS165

```cpp
// src/parts/ls165.hpp
// 8-bit parallel-in, serial-out shift register. Pins per datasheet: 1 SH_LD_N, 2 CLK, 3 E, 4 F,
// 5 G, 6 H, 7 QH_N, 8 GND, 9 QH, 10 SER, 11 A, 12 B, 13 C, 14 D, 15 CLK_INH, 16 VCC.
// SH_LD_N low loads A-H at once; while high, each rise of CLK OR CLK_INH shifts SER -> QA ... QH.
class Ls165 final : public Component {
public:
    explicit Ls165(std::string reference);
    static constexpr PinIndex kShLdN = 0, kClk = 1, kE = 2, kF = 3, kG = 4, kH = 5, kQHN = 6,
                              kQH = 8, kSer = 9, kA = 10, kB = 11, kC = 12, kD = 13,
                              kClkInh = 14;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    std::array<LogicValue, 8> stages_{};  // QA..QH
};
```

- Pins: `CLK` is `risingEdgeOnly` (a fall of `CLK` never shifts: with `CLK_INH` high the OR stays high, with it low the OR falls); as for every edge-only clock in the catalogue, X on `CLK` is not modelled (the kernel evaluates such a pin only on 0 → 1). `CLK_INH` is an ordinary input (its rise while `CLK` is low shifts).
- `evaluate`:
  - `SH_LD_N` 0: `stages_` = `A`–`H` read now (QA = `A`). X: each stage keeps its value where it equals its input, else X.
  - `SH_LD_N` 1, and it was 1 before this timestamp: an edge of the gated clock (OR of `CLK` and `CLK_INH` was 0 before and is 1 now) shifts in `SER` read before; `CLK_INH` going from 0 to X while `CLK` is low makes X each stage that a shift would change. A `SH_LD_N` rise at the same time as a clock edge does not shift (load wins).
  - `QH` = `stages_[7]`, `QH_N` its inverse; delay `kLs165Load` when `SH_LD_N` fell now, `kLs165Clock` after a shift, else `kLs165Data` (`H` to `QH` while loading).
- `powerOn`: each stage random, `setOutput` on `QH` and `QH_N`.
- Delays (`delays.hpp`, TI typicals, ps; the PDF cannot be fetched, known values): `kLs165Clock{21'000, 24'000}`, `kLs165Load{22'000, 27'000}`, `kLs165Data{14'000, 21'000}`.
- Catalogue: `PartInfo{"74LS165", "8-bit parallel-in shift register", "DIP-16", Chip, &CreateChip<Ls165, "74LS165">}` after `74LS164`.
- Datasheet `74LS165.md`: Function (eight stages loaded at once from `A`–`H` while `SH_LD_N` is low, `QH` showing `H`; while it is high each rising clock edge moves every stage one place towards `QH`, `SER` entering at `A`; `CLK_INH` high stops the clock (the two are ORed)). Pins.

## 5. DIP-24 package

- `src/board/kicad_netlist.cpp` `kFootprints` (size 10): `{"DIP-24", "Package_DIP:DIP-24_W15.24mm"}`.
- `boards/footprints/DIP-24_W15.24mm.kicad_mod`: copied unchanged from `KiCad.app/Contents/SharedSupport/footprints/Package_DIP.pretty/` (KiCad 10.0.7, as the README says).
- `footprint: every catalogue package loads` covers it with no change; `bw place` already refuses a part whose package is not a grid package without a rule ("needs a placement rule") and places any footprint by an `edge`/`row` rule; no placer change.
- Chip panel: `info->package.starts_with("DIP")` already draws 24 pins left and right.

## 6. Contents paths

```cpp
// src/netlist/builder.hpp
// Makes every relative `contents=` parameter relative to `directory` (the netlist file's).
void ResolveContentsPaths(Netlist& netlist, const std::filesystem::path& directory);
```

- `(directory / value).lexically_normal()` for a relative value; absolute values unchanged.
- Called by `LoadNetlistFile` (after parsing; `LoadNetlist` is split so the parse result can be adjusted: `LoadNetlistFile` parses, resolves, then `BuildCircuit`) and by `LoadMachine` in [machine.cpp](../../src/machine/machine.cpp) (after `ParseNetlist`, `netlistPath.parent_path()`). `ElectricalNetlist` copies the parameters, so electrical builds read the same files.
- Every build reads the files again (2 KB and 512 bytes; builds happen at power-on and after Repairs, not in the run loop).
- A netlist parsed from text (tests) keeps its values: relative to the working directory.

## 7. Other changes

| File | Change |
|---|---|
| `src/parts/catalogue.cpp` | includes, `CreateRom`, rows `2513`, `2716`, `74LS165` |
| `src/parts/CMakeLists.txt` | `ls165.cpp`, `rom.cpp`, `rom_contents.cpp` (sorted) |
| `src/parts/delays.hpp` | `kLs165Clock`, `kLs165Load`, `kLs165Data`; `k2716Access`, `k2716ChipEnable`, `k2716OutputEnable`, `k2716Float`, `k2513RowAccess`, `k2513CharacterAccess`, `k2513ChipEnable` (`Time`) |
| `src/board/kicad_netlist.cpp` | `DIP-24` (§5) |
| `src/netlist/builder.{hpp,cpp}` | `ResolveContentsPaths` (§6) |
| `src/machine/machine.cpp` | calls it (§6) |
| `tests/CMakeLists.txt` | the new test files |
| `tests/parts/catalogue_test.cpp` | `2513` in `kOffCornerPower`; the pin count from the package (`DIP-14`, `DIP-16`, `DIP-24`) instead of 14 or 16; ROM Parts created with `contents=` the test file below |
| `tests/data/empty.rom` | a comment only: a valid contents file for any ROM (every word all ones), used by the catalogue and placer tests before `roms/` exists, by absolute path (`BW_ROOT_DIR`) |
| `src/manual/manual.cpp` | `PartPinNames` takes a ROM's names from `FindRomSpec` (§3) |
| `tests/machine/electrical_test.cpp` | `LoadDesign` (`ParseNetlist` + `BuildCircuit`) calls `ResolveContentsPaths(netlist, BW_NETLIST_DIR)`, so `message` loads there (phase 7) |
| `tests/board/kicad_netlist_test.cpp` | `KicadFootprint("DIP-24")` |
| `tests/board/placer_test.cpp` | a 2716 (`contents=` `tests/data/empty.rom`, absolute) without a rule refused; in a `row` rule placed against its edge |
| `tests/netlist/builder_test.cpp` | `contents=` resolved beside a netlist file (a temporary directory with a `.bwn` and a `.rom`); bad contents refused with the netlist line |
| `tests/machines/manuals_test.cpp` | `message` in the Machine list (phase 6) |
| `tests/board/boards_test.cpp`, `tests/machine/electrical_test.cpp`, `tests/view/board_scene_test.cpp` | `message` in the per-Machine loops (phase 7) |
| `tests/session/session_test.cpp` | `session: message with every instrument` (phase 8) |
| `docs/GLOSSARY.md` | **ROM Contents** under Hardware: the words of a ROM Part, kept in a `.rom` file in `roms/` and named by `contents=` in the netlist |
| `docs/recipes/ADD_PART.md` | a Part family sharing a class through a spec table (ROMs); a Part with a file parameter; a DIP-24 needs a `.place` rule |
| `docs/recipes/ADD_MACHINE.md` | 080 commits; ROM contents files in `roms/` |

## 8. Tests

### Parts

- `tests/parts/rom_contents_test.cpp` `[parts]`: hex words, `*N`, `@` jumps, strings with escapes, dot rows (`.##.#` = 0x0D), comments, several tokens a line; unwritten words 0xFF masked to the width (0x1F for 5 bits); every error text of §2 with its line; `ReadRomContents` on a missing file.
- `tests/parts/rom_test.cpp` `[parts]`: catalogue rows and pins (kinds: `VGG`, `VDD`, `NC2` no-connect, `VPP` input, `O0` output); a `Bench` of one ROM built with a known pattern (word = address × 7 mod 256, masked) and a `Stimulus` per input: each data pin follows the word after the address delay (2716 450 ns; 2513 450 ns after a row bit, 500 ns after a character bit, 500 ns when both change); `CE_N` high floats the outputs after the off delay, low gives them back after the enable delay; 2716 `OE_N` 120 ns; one X address bit: data bits where both words agree stay known, the others X; X on `CE_N`: X. The factory: `contents` missing, unknown parameter, unreadable file, word too wide for the 2513 (from a temporary file).
- `tests/parts/ls165_test.cpp` `[parts]`: catalogue row and pins; `SH_LD_N` low: `QH` follows `H` (`kLs165Data`) and loads on the fall (`kLs165Load`); eight rising `CLK` edges after a load shift out `H`…`A` then `SER`; `CLK_INH` high holds; `CLK_INH` rising with `CLK` low shifts; a clock edge with `SH_LD_N` low does not shift; `QH_N` inverse; X on `SH_LD_N` keeps stages equal to their inputs; `CLK_INH` to X with `CLK` low.

### The Machine (`tests/machines/message_test.cpp`)

Copies the shape of [ball_test.cpp](../../tests/machines/ball_test.cpp): power on, run the TV in 1 ms steps, read the completed frame.

- Expected page: `ReadRomContents(std::filesystem::path(BW_ROOT_DIR) / "roms" / "message.rom", 2048, 8)` and `"2513.rom"` (512, 5); cell (row, column) has code `message[row × 16 + column] & 0x3F`; its dot (x, line) (x 0–5, line 0–15) is lit when x < 5 and bit (4 − x) of `font[code × 8 + line / 2]` is set.
- Reading: a dot is 4 TV columns, a line one row; dot (X, Y) of the page (X 0–95, Y 0–255) is the cell at row `top + Y`, column `left + 4X + 2`, lit at ≥ 0.5. The test tries `left` 0–127 and `top` 0–63, each offset abandoned at its first mismatch (an expected-lit dot dark or an expected-dark dot lit). A dot is 4 columns wide, so up to 4 consecutive `left` values match with one `top` (`V0` doubling makes `top` unique): the matches must be one such run, and its middle is the offset. Rows 0 of every glyph are blank, so the TV's free-running first line after the vertical sync does not disturb the page.
- Cases:

| Case | Checks |
|---|---|
| `message: netlist loads without warnings` | the design netlist (`LoadNetlistFile`), no warnings |
| `message: the TV shows the page` | seeds 1–3 (`GENERATE`): powered on, run until the TV is locked and two more frames completed (at most 1 s): the page is found (one run of offsets), `left` 44 ± 3 (MESSAGE_DESIGN.md: the text starts 11 dots after the line start), `top` about 4; nothing lit outside the 384 × 256 text rectangle; the next frame is the same |

- The netlist at first (`LoadNetlistFile`), then the Board (`BoardCircuit`, as in `ball_test.cpp`) once it exists (ADD_MACHINE.md step 3); the warnings case keeps the netlist.
- `Watch` and `Lit` are local to `ball_test.cpp`: copied. Simulated time about 0.6 s a seed at most (the TV locks within about 100 ms); debug runs a TV Machine at about 0.03×: about 20 s each, tagged `[machines][slow]`.

### Speed

`bw run netlists/message.bwn --time 2s` in release (060: 2.12× idle). The gate is the release `bw_ui` with every instrument attached at 1.00× (last phase). Measure on an idle machine (ADD_MACHINE.md gotchas).

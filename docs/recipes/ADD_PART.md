# Recipe — Add a chip Part

A new IC in the catalogue (e.g. 74LS90, 74LS47). For an on-board LED, digit or button, see [ADD_BOARD_DEVICE.md](ADD_BOARD_DEVICE.md) instead.

Reference commit: `7fb9693` (74LS164: sequential chip). Combinational gate package: `74LS30` in `src/parts/gate_package.cpp` (a new one: `09fbb1a`, 74LS27). Falling-edge JK flip-flop with `J`/`K` sampled before the edge (`readBefore`) and X cases: `4b7cf99` (74LS107). Register of flip-flops on one shared clock and clear: `2bb1a10` (74LS175). Multiplexer whose select at X folds by trying both values, delay chosen by the input that changed: `422d284` (74LS157). Variant of an existing chip (same pinout and glyphs, other output stage), sharing its code: `e3b596b` (74LS48 beside the 74LS47). Parallel-load shift register with an asynchronous load and a gated clock: `8b587d1` (74LS165). ROM family, one class behind a spec table, contents from a file, a new package (DIP-24): `9cd6bcb` (2716, 2513), `19a244f` (DIP-24). Static RAM with shared data pins (`PinKind::Bidirectional`, MOS pins, random contents, a write kept as held just before it ends): `5356e5e` (2114). Tri-state buffers in groups, with data, enable and disable delays: `a52b167` (74LS244).

## Which kind

- **Gate package** (only independent gates, no state): add a pin-name array, a `GateUnit` array and a `GatePackageSpec` row in `src/parts/gate_package.cpp`, then `&CreateGatePackage<"74LSxx">` in the catalogue. No new class.
- **ROM** (2716, 2513): a `RomSpec` row in `src/parts/rom.cpp` (pin names, address pins low bit first with their delays, data pins, enables with their delays, the float delay), then `&CreateRom<"NNNN">` in the catalogue. No new class. The contents come from `contents=<file>` ([ROM_PARTS.md](../p4l-rom-parts/ROM_PARTS.md) for the `.rom` format); `PartPinNames` reads the names from the spec.
- **Chip with state or special logic**: a new `Component` subclass `LsNN` in `src/parts/lsNN.{hpp,cpp}`, registered with `&CreateChip<LsNN, "74LSNN">`.

## Files

| File | Change |
|---|---|
| `src/parts/lsNN.hpp` | `class LsNN final : public Component`; comment with the datasheet pinout; `static constexpr PinIndex kX = …` (index = package pin − 1); `powerOn`, `evaluate`; state as private members |
| `src/parts/lsNN.cpp` | `LsNNPins()` builds `std::vector<PinSpec>` in pin order (`in`/`out`/`power` lambdas; clock pins: `PinSpec{"CLK", Input, Ttl, true}` = rising-edge-only); `powerOn` seeds state from `ctx.randomBit()` and `setOutput`s it; `evaluate` reads with `ctx.read`, `ctx.rose(kClk)`, `ctx.readBefore` (value before the edge, for data sampled on a clock), drives with `ctx.drive(pin, value, DelayFor(delays, value))` |
| `src/parts/delays.hpp` | `inline constexpr Delays kLsNNSomething{rise_ps, fall_ps};` nominal datasheet typicals |
| `src/parts/CMakeLists.txt` | add `lsNN.cpp` (keep sorted) |
| `src/parts/catalogue.cpp` | include + `PartInfo{"74LSNN", "lowercase description", "DIP-14"/"DIP-16", Chip, …}` in `s_parts`, **sorted by name** |
| `src/board/kicad_netlist.cpp` | only for a new package: add it to `kFootprints` (and resize the array) |
| `datasheets/74LSNN.md` | `# 74LSNN — description`, `## Function` (plain words), `## Pins` (one bullet per pin or range `` `QA`–`QH` ``, every pin name covered, `VCC`/`GND` included). Checked by `manuals: every catalogue Part has a Datasheet` |
| `tests/parts/lsNN_test.cpp` | Catch2, tag `[parts]`; `Bench` = one chip + one `Stimulus` per driven input in a `Circuit`; check truth table / sequence, clear/load priority, delays against `delays::`, X handling. Copy `tests/parts/ls164_test.cpp` |
| `tests/CMakeLists.txt` | add `parts/lsNN_test.cpp` (sorted) |
| `docs/NAMING.md` | only if a new abbreviation is used |

## Rules and gotchas

- Pin names: datasheet names, active-low suffix `_N` (`CLR_N`, `LOAD_N`); gates `1A`, `1Y`; unused pins `NCn` with `PinKind::NoConnect`.
- Shared data pins (a RAM's `IO`, a transceiver): `PinKind::Bidirectional`. The pin is a Driver and a reader of its Net, starts released (Z) at power-on, and its chip is evaluated when its own drive changes the Net. MOS chips (RAM) read inputs with `ctx.readBit` / `ctx.readBitBefore` (X → random bit); a chip whose pins are MOS goes in `kMosSignals` in `tests/parts/catalogue_test.cpp`.
- Open-collector outputs: `PinKind::OpenCollector` (needs a pull-up on the Board); drive 1 to release (`DriveOf` maps 1 to no drive).
- Falling-edge clocks (74LS90, 74LS93, 74LS107): `ctx.fell(pin)`; leave the pin's `risingEdgeOnly` false, so it is evaluated on both edges.
- Power Pins not at the DIP corners (74LS90, 74LS93: `VCC` pin 5, `GND` pin 10): add the Part to `kOffCornerPower` in `tests/parts/catalogue_test.cpp`.
- X on a control input makes the affected state X; never invent a 0/1.
- No zero delays, no delta cycles: every `drive` has a datasheet delay.
- Hot path: `evaluate` must not allocate (use `std::array`, no `std::vector`/`std::string`).
- Several units in one Part (74LS107): test them with `GENERATE`, not `SECTION`s inside a loop (Catch2 sections in loops are unreliable).
- `-Werror` includes unused private fields. Run `clang-format` on C++ files only, never on `CMakeLists.txt`.
- A Part with a file parameter (ROM `contents=`): the catalogue test, the footprint test and any test that builds every Part must pass one (`tests/data/empty.rom`); paths resolve against the netlist's directory (`ResolveContentsPaths`), so a test that parses a netlist itself calls it too.
- A new package: copy the footprint from the installed KiCad into `boards/footprints/` unchanged and add a `kFootprints` row before any catalogue row uses it (the footprint test loads every catalogue package). A DIP-14/16-wide package (DIP-18, DIP-20) is also a grid package: add it to `IsGridPackage` in `src/board/placer.cpp`. A package wider than DIP-16 (DIP-24) has no slot in the chip grid of `bw place`: the Machine's `.place` must place it with a rule (`row`, `edge`).
- Old MOS datasheets (2513) are in the 1972 Signetics MOS data book on bitsavers; EPROMs (2716) publish only maximum delays: use them and say so.
- TI datasheet PDFs can't be fetched: use known typical values and say so in the commit.

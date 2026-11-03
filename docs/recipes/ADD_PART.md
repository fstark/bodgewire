# Recipe — Add a chip Part

A new IC in the catalogue (e.g. 74LS90, 74LS47). For an on-board LED, digit or button, see [ADD_BOARD_DEVICE.md](ADD_BOARD_DEVICE.md) instead.

Reference commit: `7fb9693` (74LS164: sequential chip). Combinational gate package: `74LS30` in `src/parts/gate_package.cpp`.

## Which kind

- **Gate package** (only independent gates, no state): add a pin-name array, a `GateUnit` array and a `GatePackageSpec` row in `src/parts/gate_package.cpp`, then `&CreateGatePackage<"74LSxx">` in the catalogue. No new class.
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
- Open-collector outputs: `PinKind::OpenCollector` (needs a pull-up on the Board).
- X on a control input makes the affected state X; never invent a 0/1.
- No zero delays, no delta cycles: every `drive` has a datasheet delay.
- Hot path: `evaluate` must not allocate (use `std::array`, no `std::vector`/`std::string`).
- `-Werror` includes unused private fields. Run `clang-format` on C++ files only, never on `CMakeLists.txt`.
- TI datasheet PDFs can't be fetched: use known typical values and say so in the commit.
